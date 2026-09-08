#pragma once

#include <string>

#include "Input.h"
#include "NativeWindowHandle.h"

struct SDL_Window;

namespace Rhiza
{

// Thin SDL3 wrapper: owns the OS window and the event pump, knows nothing
// about rendering (Renderer.h must never be included here). The one place
// allowed to know SDL's key and window-handle types, which it translates
// before anything else sees them.
class Window
{
public:
    ~Window();

    bool initialize( const std::string &title, int width, int height );
    void shutdown();

    // Feeds every key transition to `input`, releasing all keys on focus
    // loss. False once the user has asked to close the window.
    bool pollEvents( Input &input );

    NativeWindowHandle getNativeHandle() const;

private:
    SDL_Window *mWindow = nullptr;
};

}  // namespace Rhiza
