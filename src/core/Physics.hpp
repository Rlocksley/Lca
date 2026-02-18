#pragma once
#include "Global.hpp"
#include "flecs.h"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
namespace Lca{

namespace Layers
{
    // We have 8 object layers. Feel free to add more.
    // Note: Layers are defined as a contiguous integer range [0, N-1]
    static constexpr JPH::ObjectLayer STATIC_ENVIRONMENT = 0; // Walls, floors, non-moving geometry
    static constexpr JPH::ObjectLayer DYNAMIC_ENVIRONMENT = 1; // Moving platforms, elevators, etc.
    static constexpr JPH::ObjectLayer PLAYER_BODY = 2;        // The player character
    static constexpr JPH::ObjectLayer ENEMY_BODY = 3;         // Enemy characters
    static constexpr JPH::ObjectLayer PLAYER_SPELL = 4;       // Projectiles, AoEs from the player
    static constexpr JPH::ObjectLayer ENEMY_SPELL = 5;        // Projectiles, AoEs from enemies
    static constexpr JPH::ObjectLayer DESTRUCTIBLE = 6;       // Barrels, crates, etc.
    static constexpr JPH::ObjectLayer LOOT_PICKUP = 7;        // Non-colliding sensors for items on ground

    static constexpr JPH::ObjectLayer NUM_OBJECT_LAYERS = 8; // Total number of object layers
};

// Jolt's Broad-Phase is a high-level grouping of objects to quickly discard collision checks.
// We'll group objects into "things that don't move" and "things that do".
namespace BroadPhaseLayers
{
    static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
    static constexpr JPH::BroadPhaseLayer MOVING(1);
    static constexpr JPH::uint NUM_BROAD_PHASE_LAYERS = 2;
};

// This class maps our specific ObjectLayers to a broader BroadPhaseLayer.
class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
{
public:
    BPLayerInterfaceImpl()
    {
        // Create a mapping table from object to broad-phase layer
        mObjectToBroadPhase[Layers::STATIC_ENVIRONMENT] = BroadPhaseLayers::NON_MOVING;
        mObjectToBroadPhase[Layers::DYNAMIC_ENVIRONMENT] = BroadPhaseLayers::MOVING; // Dynamic environment can move, so it's in the moving layer
        mObjectToBroadPhase[Layers::PLAYER_BODY] = BroadPhaseLayers::MOVING;
        mObjectToBroadPhase[Layers::ENEMY_BODY] = BroadPhaseLayers::MOVING;
        mObjectToBroadPhase[Layers::PLAYER_SPELL] = BroadPhaseLayers::MOVING;
        mObjectToBroadPhase[Layers::ENEMY_SPELL] = BroadPhaseLayers::MOVING;
        mObjectToBroadPhase[Layers::DESTRUCTIBLE] = BroadPhaseLayers::MOVING; // Or non-moving if they are static until broken
        mObjectToBroadPhase[Layers::LOOT_PICKUP] = BroadPhaseLayers::MOVING; // It's a sensor, but needs to check against a moving player
    }

    virtual JPH::uint GetNumBroadPhaseLayers() const override
    {
        return BroadPhaseLayers::NUM_BROAD_PHASE_LAYERS;
    }

    virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
    {
        JPH_ASSERT(inLayer < Layers::NUM_OBJECT_LAYERS);
        return mObjectToBroadPhase[inLayer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
    {
        switch ((JPH::BroadPhaseLayer::Type)inLayer)
        {
        case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING: return "NON_MOVING";
        case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING: return "MOVING";
        default: JPH_ASSERT(false); return "INVALID";
        }
    }
#endif

private:
    JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_OBJECT_LAYERS];
};


// This class defines which BroadPhaseLayers can collide with each other.
// It's a high-level filter.
class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter
{
public:
    virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
    {
        // The logic here is simple: moving things need to be checked against both static and other moving things.
        // Static things only need to be checked against moving things.
        switch (inLayer1)
        {
        case Layers::STATIC_ENVIRONMENT:
            return inLayer2 == BroadPhaseLayers::MOVING;
        
        case Layers::DYNAMIC_ENVIRONMENT:
        case Layers::PLAYER_BODY:
        case Layers::ENEMY_BODY:
        case Layers::PLAYER_SPELL:
        case Layers::ENEMY_SPELL:
        case Layers::DESTRUCTIBLE:
        case Layers::LOOT_PICKUP:
            return true; // Moving things can collide with everything.

        default:
            JPH_ASSERT(false);
            return false;
        }
    }
};


// This class defines the fine-grained collision rules between specific ObjectLayers.
// This is where your core game logic lives.
class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter
{
public:
    virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::ObjectLayer inLayer2) const override
    {
        switch (inLayer1)
        {
        case Layers::STATIC_ENVIRONMENT:
            // Static environment collides with all bodies and spells, but not with loot sensors
            return inLayer2 == Layers::DYNAMIC_ENVIRONMENT || inLayer2 == Layers::PLAYER_BODY || inLayer2 == Layers::ENEMY_BODY || inLayer2 == Layers::PLAYER_SPELL || inLayer2 == Layers::ENEMY_SPELL || inLayer2 == Layers::DESTRUCTIBLE;

        case Layers::DYNAMIC_ENVIRONMENT:
            // Dynamic environment collides with all bodies and spells, but not with loot sensors
            return inLayer2 == Layers::STATIC_ENVIRONMENT || inLayer2 == Layers::DYNAMIC_ENVIRONMENT || inLayer2 == Layers::PLAYER_BODY || inLayer2 == Layers::ENEMY_BODY || inLayer2 == Layers::PLAYER_SPELL || inLayer2 == Layers::ENEMY_SPELL || inLayer2 == Layers::DESTRUCTIBLE;

        case Layers::PLAYER_BODY:
            // Player collides with environment, enemies, enemy spells, destructibles, and loot
            return inLayer2 == Layers::STATIC_ENVIRONMENT || inLayer2 == Layers::DYNAMIC_ENVIRONMENT || inLayer2 == Layers::ENEMY_BODY || inLayer2 == Layers::ENEMY_SPELL || inLayer2 == Layers::DESTRUCTIBLE || inLayer2 == Layers::LOOT_PICKUP;
        
        case Layers::ENEMY_BODY:
            // Enemy collides with environment, player, player spells, and destructibles
            // IMPORTANT: Enemies do NOT collide with other enemies or enemy spells.
            return inLayer2 == Layers::STATIC_ENVIRONMENT || inLayer2 == Layers::DYNAMIC_ENVIRONMENT || inLayer2 == Layers::PLAYER_BODY || inLayer2 == Layers::ENEMY_BODY || inLayer2 == Layers::PLAYER_SPELL || inLayer2 == Layers::DESTRUCTIBLE;

        case Layers::PLAYER_SPELL:
            // Player spells collide with environment, enemies, and destructibles
            // IMPORTANT: They pass through the player and other player spells.
            return inLayer2 == Layers::STATIC_ENVIRONMENT || inLayer2 == Layers::DYNAMIC_ENVIRONMENT || inLayer2 == Layers::ENEMY_BODY || inLayer2 == Layers::DESTRUCTIBLE;
        
        case Layers::ENEMY_SPELL:
            // Enemy spells collide with environment, the player, and destructibles
            return inLayer2 == Layers::STATIC_ENVIRONMENT || inLayer2 == Layers::PLAYER_BODY || inLayer2 == Layers::DESTRUCTIBLE;
        
        case Layers::DESTRUCTIBLE:
            // Destructibles are hit by everything that can cause damage.
            return inLayer2 == Layers::STATIC_ENVIRONMENT || inLayer2 == Layers::PLAYER_BODY || inLayer2 == Layers::ENEMY_BODY || inLayer2 == Layers::PLAYER_SPELL || inLayer2 == Layers::ENEMY_SPELL;

        case Layers::LOOT_PICKUP:
            // Loot sensors only need to check for overlap with the player.
            return inLayer2 == Layers::PLAYER_BODY;
        
        default:
            // If a layer isn't handled above, it means we have a symmetric case
            // that is already handled by one of the other `case` statements.
            // If we reach here, it means we have an unhandled pair, so we assert.
            JPH_ASSERT(false);
            return false;
        }
    }
};


namespace Core{
    inline JPH::Vec3 toJoltVec3(const glm::vec3& vec) {
        return JPH::Vec3(vec.x, vec.y, vec.z);
    }

    inline glm::vec3 toGlmVec3(const JPH::Vec3& vec) {
        return glm::vec3(vec.GetX(), vec.GetY(), vec.GetZ());
    }

    inline JPH::Quat toJoltQuat(const glm::quat& quat) {
        return JPH::Quat(quat.x, quat.y, quat.z, quat.w);
    }

    inline glm::quat toGlmQuat(const JPH::Quat& quat) {
        return glm::quat(quat.GetW(), quat.GetX(), quat.GetY(), quat.GetZ());
    }
}


struct CollisionEvent{
    flecs::entity_t entity1;
    flecs::entity_t entity2;
    glm::vec3 contactPoint;
    glm::vec3 contactNormal;
};


class CollisionQueue
{
public:
    void push(const CollisionEvent& event) {
        std::lock_guard<std::mutex> lock(mutex);
        queue.push_back(event);
    }

    void drain(std::vector<CollisionEvent>& out_events) {
        std::lock_guard<std::mutex> lock(mutex);
        out_events.swap(queue);
        queue.clear();
    }

private:
    std::mutex mutex;
    std::vector<CollisionEvent> queue;
};


namespace Core{
inline CollisionQueue collisionQueue; // Global queue for collision events
}

class FlecsContactListener : public JPH::ContactListener {
public:
    // We pass a reference to our queue
    FlecsContactListener() {}

    // We only care about when two bodies start touching.
    void OnContactAdded(const JPH::Body &inBody1, const JPH::Body &inBody2, const JPH::ContactManifold &inManifold, JPH::ContactSettings &ioSettings) override {

        // The world space normal, pointing from body 2 to body 1.
        JPH::Vec3 jolt_normal = inManifold.mWorldSpaceNormal;

        // Get the first contact point on the surface of body 2.
        JPH::Vec3 jolt_point = inManifold.GetWorldSpaceContactPointOn2(0);

        // We get the flecs::entity_t from the user data we stored earlier.
        auto e1_id = static_cast<flecs::entity_t>(inBody1.GetUserData());
        auto e2_id = static_cast<flecs::entity_t>(inBody2.GetUserData());

        // We only push if both are valid entities
        if (e1_id != 0 && e2_id != 0) {
            Core::collisionQueue.push({e1_id, e2_id, Core::toGlmVec3(jolt_point), Core::toGlmVec3(jolt_normal)});
        }
    }
};

class FlecsCharacterContactListener : public JPH::CharacterContactListener {
private:
    JPH::PhysicsSystem* pPhysicsSystem;
public:
    // We pass a reference to our queue
    FlecsCharacterContactListener(JPH::PhysicsSystem* physicsSystem) : pPhysicsSystem(physicsSystem) {}

    // We only care about when two bodies start touching.
    void OnContactAdded(const JPH::CharacterVirtual *inCharacter, const JPH::BodyID &inBodyID2, const JPH::SubShapeID &inSubShapeID2, JPH::RVec3Arg inContactPosition, JPH::Vec3Arg inContactNormal, JPH::CharacterContactSettings &ioSettings) override {
        // The world space normal, pointing from body 2 to body 1.
        JPH::Vec3 jolt_normal = inContactNormal;

        // Get the first contact point on the surface of body 2.
        JPH::Vec3 jolt_point = inContactPosition;

        // We get the flecs::entity_t from the user data we stored earlier.
        auto e1_id = static_cast<flecs::entity_t>(inCharacter->GetUserData());
        const JPH::BodyLockInterfaceNoLock& body_interface = pPhysicsSystem->GetBodyLockInterfaceNoLock();
        auto e2_id = static_cast<flecs::entity_t>(body_interface.TryGetBody(inBodyID2)->GetUserData());

        // We only push if both are valid entities
        if (e1_id != 0 && e2_id != 0) {
            Core::collisionQueue.push({e1_id, e2_id, Core::toGlmVec3(jolt_point), Core::toGlmVec3(jolt_normal)});
        }
    }
};


}