#pragma once

#include <Rhiza/Types/Handles.h>
#include <Rhiza/Types/Math.h>

namespace Rhiza
{

// Non-opaque modes write no depth and cost full overdraw per layer.
enum class BlendMode
{
    Opaque,
    AlphaBlend,
    Additive,
};

// Picks the shading path per material rather than globally, so a lit 3D
// world with a flat 2D HUD over it is the normal case. Maps onto Ogre-Next's
// two Hlms implementations: Lit -> HlmsPbs, Unlit -> HlmsUnlit ("great for
// GUI, billboards, particle FXs"), which is Rhiza's 2D/effects path.
// https://ogrecave.github.io/ogre-next/api/latest/hlms.html
enum class ShadingModel
{
    Lit,
    Unlit,
};

struct MaterialDesc
{
    ShadingModel shading = ShadingModel::Lit;

    // Tints the texture; white leaves it as authored.
    Color color;

    TextureHandle texture;

    BlendMode blend = BlendMode::Opaque;

    // Lit only. 0 = mirror-smooth, 1 = fully diffuse.
    float roughness = 0.6f;

    // Lit only. 0 = dielectric, 1 = raw metal. Values in between are
    // physically meaningless, so prefer the extremes.
    float metalness = 0.0f;

    // Defaults to true because MeshDesc makes no promise about winding
    // order; false enables backface culling.
    bool doubleSided = true;
};

}  // namespace Rhiza
