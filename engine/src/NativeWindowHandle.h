#pragma once

#include <cstdint>

namespace Rhiza
{

// The only thing crossing the Window/Renderer boundary: SDL creates the
// window, Ogre receives this as its "externalWindowHandle". Every platform
// Ogre-Next supports reduces that to one native value - HWND on Windows, the
// X11 Window XID on Linux/GLX, NSWindow* on macOS/Metal - so one field
// covers all three, and Window::getNativeHandle() is the only place that
// needs to know which.
struct NativeWindowHandle
{
    uintptr_t value = 0;
};

}  // namespace Rhiza
