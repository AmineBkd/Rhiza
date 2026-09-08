#include "Window.h"
#include <SDL3/SDL.h>

namespace Rhiza
{

namespace
{

// The only function in the engine that knows SDL's scancode numbering.
// Anything unlisted falls through to Key::Unknown, which Input ignores.
Key toRhizaKey( SDL_Scancode scancode )
{
    switch( scancode )
    {
    case SDL_SCANCODE_A: return Key::A;
    case SDL_SCANCODE_B: return Key::B;
    case SDL_SCANCODE_C: return Key::C;
    case SDL_SCANCODE_D: return Key::D;
    case SDL_SCANCODE_E: return Key::E;
    case SDL_SCANCODE_F: return Key::F;
    case SDL_SCANCODE_G: return Key::G;
    case SDL_SCANCODE_H: return Key::H;
    case SDL_SCANCODE_I: return Key::I;
    case SDL_SCANCODE_J: return Key::J;
    case SDL_SCANCODE_K: return Key::K;
    case SDL_SCANCODE_L: return Key::L;
    case SDL_SCANCODE_M: return Key::M;
    case SDL_SCANCODE_N: return Key::N;
    case SDL_SCANCODE_O: return Key::O;
    case SDL_SCANCODE_P: return Key::P;
    case SDL_SCANCODE_Q: return Key::Q;
    case SDL_SCANCODE_R: return Key::R;
    case SDL_SCANCODE_S: return Key::S;
    case SDL_SCANCODE_T: return Key::T;
    case SDL_SCANCODE_U: return Key::U;
    case SDL_SCANCODE_V: return Key::V;
    case SDL_SCANCODE_W: return Key::W;
    case SDL_SCANCODE_X: return Key::X;
    case SDL_SCANCODE_Y: return Key::Y;
    case SDL_SCANCODE_Z: return Key::Z;

    case SDL_SCANCODE_0: return Key::Num0;
    case SDL_SCANCODE_1: return Key::Num1;
    case SDL_SCANCODE_2: return Key::Num2;
    case SDL_SCANCODE_3: return Key::Num3;
    case SDL_SCANCODE_4: return Key::Num4;
    case SDL_SCANCODE_5: return Key::Num5;
    case SDL_SCANCODE_6: return Key::Num6;
    case SDL_SCANCODE_7: return Key::Num7;
    case SDL_SCANCODE_8: return Key::Num8;
    case SDL_SCANCODE_9: return Key::Num9;

    case SDL_SCANCODE_UP: return Key::Up;
    case SDL_SCANCODE_DOWN: return Key::Down;
    case SDL_SCANCODE_LEFT: return Key::Left;
    case SDL_SCANCODE_RIGHT: return Key::Right;

    case SDL_SCANCODE_SPACE: return Key::Space;
    case SDL_SCANCODE_RETURN: return Key::Return;
    case SDL_SCANCODE_ESCAPE: return Key::Escape;
    case SDL_SCANCODE_TAB: return Key::Tab;
    case SDL_SCANCODE_BACKSPACE: return Key::Backspace;

    case SDL_SCANCODE_LSHIFT: return Key::LeftShift;
    case SDL_SCANCODE_RSHIFT: return Key::RightShift;
    case SDL_SCANCODE_LCTRL: return Key::LeftCtrl;
    case SDL_SCANCODE_RCTRL: return Key::RightCtrl;
    case SDL_SCANCODE_LALT: return Key::LeftAlt;
    case SDL_SCANCODE_RALT: return Key::RightAlt;

    default: return Key::Unknown;
    }
}

}  // namespace

Window::~Window() {
    shutdown();
}

bool Window::initialize( const std::string &title, int width, int height )
{
#if defined( __linux__ )
    // GL3Plus reaches the window through GLX, which needs a real X11
    // window - under SDL3's default Wayland backend there isn't one, and
    // getNativeHandle() comes back empty. XWayland makes this work anyway.
    // https://wiki.libsdl.org/SDL3/SDL_HINT_VIDEO_DRIVER
    SDL_SetHint( SDL_HINT_VIDEO_DRIVER, "x11" );
#endif

    if( !SDL_Init( SDL_INIT_VIDEO ) )
    {
        SDL_Log( "SDL_Init failed: %s", SDL_GetError() );
        return false;
    }

    // Intentionally not resizable: the renderer does not yet re-create its
    // swapchain on resize, so a resize would just stretch the last frame.
    mWindow = SDL_CreateWindow( title.c_str(), width, height, 0 );
    if( !mWindow )
    {
        SDL_Log( "SDL_CreateWindow failed: %s", SDL_GetError() );
        SDL_Quit();
        return false;
    }

    return true;
}

void Window::shutdown()
{
    if( mWindow )
    {
        SDL_DestroyWindow( mWindow );
        mWindow = nullptr;
        SDL_Quit();
    }
}

bool Window::pollEvents( Input &input )
{
    input.beginFrame();

    SDL_Event event;
    while( SDL_PollEvent( &event ) )
    {
        if( event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED )
            return false;

        if( event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP )
        {
            // SDL repeats KEY_DOWN for a held key. Dropping those here
            // keeps Input's contract simple: every event is a transition.
            if( !event.key.repeat )
                input.handleKeyEvent( toRhizaKey( event.key.scancode ), event.key.down );
        }
        else if( event.type == SDL_EVENT_WINDOW_FOCUS_LOST )
        {
            input.releaseAll();
        }
    }
    return true;
}

NativeWindowHandle Window::getNativeHandle() const
{
    NativeWindowHandle handle;
    SDL_PropertiesID props = SDL_GetWindowProperties( mWindow );

    // Only the property matching the running driver is non-zero, and
    // querying the others just returns the default, so trying each in turn
    // avoids a compile-time platform switch.
    // https://wiki.libsdl.org/SDL3/SDL_GetWindowProperties
    if( void *hwnd = SDL_GetPointerProperty( props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr ) )
        handle.value = reinterpret_cast<uintptr_t>( hwnd );
    else if( Sint64 xid = SDL_GetNumberProperty( props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0 ) )
        handle.value = static_cast<uintptr_t>( xid );
    else if( void *nsWindow =
                 SDL_GetPointerProperty( props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr ) )
        handle.value = reinterpret_cast<uintptr_t>( nsWindow );

    return handle;
}

}  // namespace Rhiza
