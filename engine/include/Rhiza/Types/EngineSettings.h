#pragma once

#include <Rhiza/Types/Math.h>

namespace Rhiza
{

struct EngineSettings
{
    const char *windowTitle = "Rhiza";
    int windowWidth = 1280;
    int windowHeight = 720;

    // Off-axis by default so a solid object shows more than one face, which
    // makes lighting legible.
    Vec3 cameraPosition{ 3.5f, 3.0f, 5.0f };
    Vec3 cameraTarget{ 0.0f, 0.0f, 0.0f };

    // Where loadTexture and loadMesh paths start. Null: the "assets" folder
    // next to the executable.
    const char *assetRoot = nullptr;

    // How many different sounds can play at once - mono for positional
    // world sounds, stereo for UI. All copies of one sound share its slot.
    int monoSoundSlots = 56;
    int stereoSoundSlots = 8;

    // Copies of one sound playing at once, unless loadSound overrides it.
    int defaultMaxCopies = 4;
};

}  // namespace Rhiza
