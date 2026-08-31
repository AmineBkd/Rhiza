#pragma once

#include <string>

#include "NativeWindowHandle.h"

struct SDL_Window;

namespace Rhiza
{

// Thin SDL3 wrapper. Owns the OS window and the SDL event pump; knows
// nothing about rendering. Renderer.h must never be included here.
class Window
{
public:
    ~Window();

    bool initialize( const std::string &title, int width, int height );
    void shutdown();

    // Pumps pending OS events. Returns false once the user has asked to
    // close the window.
    bool pollEvents();

    NativeWindowHandle getNativeHandle() const;

private:
    SDL_Window *mWindow = nullptr;
};

}  // namespace Rhiza
