// The one place miniaudio and stb_vorbis are compiled. miniaudio enables its
// Ogg Vorbis decoder only if it sees stb_vorbis's declarations first, so the
// order of these includes matters.
#define STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>

#define MINIAUDIO_IMPLEMENTATION
#include "Miniaudio.h"

#undef STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>
