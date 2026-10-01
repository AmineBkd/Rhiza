#pragma once

#include <memory>
#include <Rhiza/Types/Audio.h>
#include <Rhiza/Types/Camera.h>
#include <Rhiza/Types/EngineSettings.h>
#include <Rhiza/Types/Handles.h>
#include <Rhiza/Types/Input.h>
#include <Rhiza/Types/Light.h>
#include <Rhiza/Types/Material.h>
#include <Rhiza/Types/Math.h>
#include <Rhiza/Types/Mesh.h>
#include <Rhiza/Types/Texture.h>

namespace Rhiza
{

// Facade over the engine's window and input (SDL3), renderer (Ogre-Next) and
// audio (miniaudio). Nothing outside engine/src ever includes their headers:
// this class and Rhiza/Types/ are the entire public surface.
class Engine
{
public:
    Engine();
    ~Engine();

    Engine(const Engine &) = delete;
    Engine &operator=(const Engine &) = delete;

    // False on failure, with the reason in the log; the engine is unusable
    // until this succeeds.
    bool initialize(const EngineSettings &settings = {});

    // Safe to call more than once; the destructor calls it too.
    void shutdown();

    // Starts a frame: advances the clock, then pumps window and input
    // events. False once the user has asked to close the window - stop the
    // loop without calling endFrame().
    bool beginFrame();

    // True once the GPU device has been lost - a driver reset, a GPU
    // removed, or VK_ERROR_DEVICE_LOST on Android. beginFrame() returns
    // false from then on, so the loop exits; check this afterwards to tell a
    // crash apart from the user closing the window. The engine does not
    // recover in place: save and restart the process.
    bool deviceLost() const;

    // True once rendering has failed for any other reason, such as a shader
    // that would not compile. beginFrame() returns false from then on too,
    // but a restart would hit the same error: exit and report it instead.
    bool renderFailed() const;

    // Renders the frame. Call once per successful beginFrame(), after game
    // code has finished mutating the scene - anything changed after this
    // lands on screen a frame late.
    //
    //   while( engine.beginFrame() )
    //   {
    //       ... update the world ...
    //       engine.endFrame();
    //   }
    void endFrame();

    // Uploads geometry once; place copies with createInstance(). Invalid
    // handle if the description is unusable - empty, not a triangle list, or
    // indices past the end of the vertex list.
    MeshHandle createMeshAsset(const MeshDesc &desc);

    // Re-uploads geometry in place. Requires MeshDesc::isMutable; every
    // instance picks up the change with nothing to update per-instance.
    void updateMesh(MeshHandle mesh, const MeshDesc &desc);

    // Loading: paths are relative to EngineSettings::assetRoot, and the same
    // path returns the same handle without loading again. Every load needs
    // its own destroy; the last one frees the asset.
    //
    // Paths are case-sensitive. Windows and macOS forgive "Ship.png" for
    // ship.png - with a warning in the log - but Linux and Android won't.

    // glTF (.gltf or .glb), static geometry only. Never mutable: updateMesh
    // on a shared mesh would change it for every user.
    MeshHandle loadMesh(const char *path);

    // Refused while any instance still references it.
    void destroyMeshAsset(MeshHandle mesh);

    // PNG, JPG, TGA - anything FreeImage decodes.
    TextureHandle loadTexture(const char *path, const TextureDesc &desc = {});

    // Refused while any material still references it.
    void destroyTexture(TextureHandle texture);

    // A material (Ogre's "datablock"), shared across instances like a mesh.
    MaterialHandle createMaterial(const MaterialDesc &desc);

    // Refused while any instance still references it.
    void destroyMaterial(MaterialHandle material);

    InstanceHandle createInstance(MeshHandle mesh, MaterialHandle material);

    // The mesh asset and material survive; others may still reference them.
    void destroyInstance(InstanceHandle instance);

    void setTransform(InstanceHandle handle, const Transform &transform);

    // Only ShadingModel::Lit materials respond to lights.
    LightHandle createLight(const LightDesc &desc);
    void destroyLight(LightHandle handle);

    // Stands in for bounced light the renderer doesn't simulate; without it,
    // surfaces facing away from every light are pure black. Ogre blends the
    // two colours by how far a surface tilts toward sky or ground.
    void setAmbientLight(Color skyColor, Color groundColor);

    // Look-at; leaves the projection as it was.
    void setCamera(Vec3 position, Vec3 target);
    void setCamera(const CameraDesc &camera);

    // Sounds load like meshes and textures: cached by path, one destroy per
    // load. Mono sounds are for the world, stereo for UI. Up to
    // EngineSettings::defaultMaxCopies copies of one sound play at once.
    SoundHandle loadSound(const char *path);

    // Overrides how many copies may play at once - 1 for a UI click that
    // should never overlap itself. The cap belongs to the sound, so the last
    // value given wins.
    SoundHandle loadSound(const char *path, int maxCopies);
    void destroySound(SoundHandle sound);

    // At the sound's copy cap this restarts its oldest copy; when every slot
    // holds a different sound, the oldest slot is taken over.
    VoiceHandle playSound(SoundHandle sound, const PlayDesc &desc = {});
    void stopVoice(VoiceHandle voice);
    void setVoicePosition(VoiceHandle voice, Vec3 position, Vec3 velocity = {});
    bool isVoicePlaying(VoiceHandle voice) const;

    // Ogg Vorbis, kept compressed and decoded while playing. Loops, and a new
    // track crossfades from the previous one over `fadeSeconds`.
    void playMusic(const char *path, float fadeSeconds = 0.0f);
    void stopMusic(float fadeSeconds = 0.0f);

    void setBusVolume(AudioBus bus, float volume);
    void setBusPaused(AudioBus bus, bool paused);

    // Scales every bus, music included, on top of their own volumes.
    void setMasterVolume(float volume);

    // The listener follows the camera until this is called, and again after
    // followCameraWithListener().
    void setListener(const ListenerDesc &listener);
    void followCameraWithListener();

    // Keys are identified by physical position - see Key in Types/Input.h.
    // wasKeyPressed/Released are true only on the frame of the transition;
    // holding a key does not repeat them.
    bool isKeyDown(Key key) const;
    bool wasKeyPressed(Key key) const;
    bool wasKeyReleased(Key key) const;

    // deltaSeconds drives gameplay and in-world animation - this is what
    // hitstop freezes. unscaledDeltaSeconds drives input, UI and anything
    // else that must keep moving through one. Both zero before the first
    // beginFrame(). timeScale: 1 = normal, 0 = frozen, negative = backward.
    float deltaSeconds() const;
    float unscaledDeltaSeconds() const;
    float timeScale() const;
    void setTimeScale(float scale);

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

}  // namespace Rhiza
