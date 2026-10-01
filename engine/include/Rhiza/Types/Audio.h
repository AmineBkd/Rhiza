#pragma once

#include <Rhiza/Types/Math.h>

namespace Rhiza
{

enum class AudioBus
{
    Sfx,
    Music,
    // Keeps playing when Sfx is paused - menu clicks over a paused game.
    Ui,
};

struct PlayDesc
{
    float volume = 1.0f;

    // 0.5 is an octave down.
    float pitch = 1.0f;

    // -1 left, +1 right. Ignored when positional.
    float pan = 0.0f;

    bool loop = false;

    AudioBus bus = AudioBus::Sfx;

    // Heard from `position` relative to the listener: panned by direction,
    // quieter with distance. Meant for mono sounds - a stereo recording
    // cannot come from one point.
    bool positional = false;
    Vec3 position;

    // Drives the Doppler effect.
    Vec3 velocity;

    // Full volume within minDistance; no further fading beyond maxDistance.
    float minDistance = 10.0f;
    float maxDistance = 200.0f;
};

// The ears for positional audio. By default the listener follows the
// camera; setting one stops that.
struct ListenerDesc
{
    Vec3 position;

    // Identity faces -Z with +Y up, like the camera.
    Quat orientation;

    Vec3 velocity;
};

}  // namespace Rhiza
