#pragma once

// Every include of miniaudio goes through here, so all of them see the same
// switches. SDL drives the speakers and assets arrive as bytes, so
// miniaudio's device and resource-manager code is compiled out - and with
// it every platform audio library and thread it would otherwise pull in.
#define MA_NO_DEVICE_IO
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_THREADING
#define MA_NO_ENCODING
#define MA_NO_GENERATION

#include <miniaudio.h>
