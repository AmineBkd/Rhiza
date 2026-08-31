#include "Window.h"
#include <SDL3/SDL.h>

namespace Rhiza
{

namespace
{

// The only function in the engine that knows SDL's scancode numbering.
// Physical position, not typed character - see the Key comment in Types.h
// for why that's the right identity for gameplay bindings. Anything not
// listed falls through to Key::Unknown, which Input silently ignores.
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
    // Ogre-Next's GL3Plus render system talks to the window through GLX,
    // which needs a real X11 window. Wayland is SDL3's default on most
    // modern distros, and under Wayland there is no X11 window at all, so
    // Window::getNativeHandle() would come back empty. Forcing x11 here
    // trades native Wayland for something Ogre-Next can actually attach to;
    // XWayland (present on effectively all Wayland desktops) makes this an
    // X11 window under the hood either way.
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
            // SDL fires repeated KEY_DOWN events for a held key. Input's
            // wasKeyPressed() already derives "just pressed" from the
            // up->down transition, so letting repeats through would just
            // mean re-setting a bit that is already set - harmless, but
            // filtering them here keeps Input's contract simple: every
            // event it sees is a real transition.
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

    // Every platform Ogre-Next supports needs a different SDL property, but
    // only the one matching the driver actually running will be non-zero -
    // querying the others is harmless (SDL just returns the default value),
    // so trying each in turn avoids needing a compile-time platform switch.
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
