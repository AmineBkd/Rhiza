#pragma once

#include <Rhiza/Types/Physics.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Rhiza
{

// Owns the Box2D world. Knows nothing about rendering, audio or the ECS, and
// keeps Box2D's headers to Physics.cpp.
class Physics
{
public:
    // A stall banks at most this many steps, so the simulation slows down
    // afterwards rather than spending frames catching up.
    static constexpr int kMaxBankedSteps = 8;

    Physics();
    ~Physics();

    bool initialize( Vec2 gravity, float stepSeconds );
    void shutdown();

    void setGravity( Vec2 gravity );

    // 0 if physics isn't running.
    uint32_t createBody( const BodyDesc &desc );
    void destroyBody( uint32_t body );
    bool addShape( uint32_t body, const ShapeDesc &desc, std::string &error );

    BodyPose pose( uint32_t body ) const;
    void setPose( uint32_t body, const BodyPose &pose );
    Vec2 linearVelocity( uint32_t body ) const;
    void setLinearVelocity( uint32_t body, Vec2 velocity );
    float angularVelocity( uint32_t body ) const;
    void setAngularVelocity( uint32_t body, float velocity );
    void applyForce( uint32_t body, Vec2 force );
    void applyImpulse( uint32_t body, Vec2 impulse );
    void applyTorque( uint32_t body, float torque );

    // Banks gameplay time to be spent in fixed steps. Negative time (a
    // reversed clock) banks nothing: physics cannot run backward.
    void bank( float seconds );
    bool stepDue() const;

    // Advances one fixed step, spending it from the bank, and collects its
    // contact and sensor events.
    void step();

    const std::vector<ContactEvent> &events() const;
    void clearEvents();

    RaycastHit raycast( Vec2 from, Vec2 to, uint64_t maskBits ) const;

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

}  // namespace Rhiza
