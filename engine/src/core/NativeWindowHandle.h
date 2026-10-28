#pragma once

#include <cstdint>

namespace Rhiza
{

// The only thing crossing the Window/Renderer boundary: SDL creates the
// window, Ogre receives this as its "externalWindowHandle". Every platform
// Ogre-Next supports reduces that to one native value - HWND on Windows, the
// X11 Window XID on Linux/GLX, NSWindow* on macOS/Metal, ANativeWindow* on
// Android - so one field covers them all, and Window::getNativeHandle() is
// the only place that needs to know which.
struct NativeWindowHandle
{
    uintptr_t value = 0;

    // Android only: the AAssetManager reading the APK, where the shaders are.
    uintptr_t assetManager = 0;
};

}  // namespace Rhiza
