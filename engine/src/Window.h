#pragma once

#include <string>

#include "Input.h"
#include "NativeWindowHandle.h"

struct SDL_Window;

namespace Rhiza
{

// Thin SDL3 wrapper. Owns the OS window and the SDL event pump; knows
// nothing about rendering. Renderer.h must never be included here.
//
// Also the one place in the engine allowed to know SDL's key types: it
// translates SDL_Scancode into Rhiza::Key before anything else ever sees it,
// the same way getNativeHandle() translates SDL's window handle types.
class Window
{
public:
    ~Window();

    bool initialize( const std::string &title, int width, int height );
    void shutdown();

    // Pumps pending OS events, feeding every key transition to `input` and
    // releasing all its keys if focus is lost. Returns false once the user
    // has asked to close the window.
    bool pollEvents( Input &input );

    NativeWindowHandle getNativeHandle() const;

private:
    SDL_Window *mWindow = nullptr;
};

}  // namespace Rhiza
