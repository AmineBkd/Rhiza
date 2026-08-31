#pragma once

#include <cstdint>

namespace Rhiza
{

// Platform window handle, passed from Window (which creates it via SDL) to
// Renderer (which hands it to Ogre as an "externalWindowHandle"). Neither
// Window.h nor Renderer.h includes the other's library headers; this struct
// is the only thing that crosses the boundary.
//
// Every platform Ogre-Next supports reduces "externalWindowHandle" to a
// single native handle, stringified as a decimal integer:
//   - Windows: the HWND, as a pointer value.
//   - Linux (GLX): the X11 Window XID (an unsigned long).
//   - macOS (Metal): the NSWindow*, as a pointer value.
// So one field covers all three; Window::getNativeHandle() is the only
// place that needs to know which platform-specific value goes in it.
struct NativeWindowHandle
{
    uintptr_t value = 0;
};

}  // namespace Rhiza
