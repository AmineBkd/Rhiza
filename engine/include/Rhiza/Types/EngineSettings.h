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
};

}  // namespace Rhiza
