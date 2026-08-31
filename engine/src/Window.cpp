#include "Window.h"
#include <SDL3/SDL.h>

namespace Rhiza
{

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

bool Window::pollEvents()
{
    SDL_Event event;
    while( SDL_PollEvent( &event ) )
    {
        if( event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED )
            return false;
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
