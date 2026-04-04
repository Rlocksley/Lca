#include "Physics.hpp"
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <algorithm>

namespace Lca {
namespace Core {

void Physics::init() {
    if (physicsSystem) return; // already initialized

    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    broadPhaseLayerInterface = std::make_unique<BPLayerInterfaceImpl>();
    objectVsBroadPhaseLayerFilter = std::make_unique<ObjectVsBroadPhaseLayerFilterImpl>();
    objectLayerPairFilter = std::make_unique<ObjectLayerPairFilterImpl>();

    physicsSystem = std::make_unique<JPH::PhysicsSystem>();
    physicsSystem->Init(
        maxBodies,
        numBodyMutexes,
        maxBodyPairs,
        maxContactConstraints,
        *broadPhaseLayerInterface,
        *objectVsBroadPhaseLayerFilter,
        *objectLayerPairFilter
    );

    physicsSystem->SetGravity(Core::toJoltVec3(glm::vec3(0.0f, -9.81f, 0.0f)));

    unsigned int threadCount = std::max(1u, std::thread::hardware_concurrency() - 1);
    jobSystemThreadPool = std::make_unique<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, threadCount);

    tempAllocator = std::make_unique<JPH::TempAllocatorImpl>(tempAllocatorSize);

    contactListener = std::make_unique<FlecsContactListener>();
    physicsSystem->SetContactListener(contactListener.get());

    characterContactListener = std::make_unique<FlecsCharacterContactListener>(physicsSystem.get());
}

void Physics::shutdown() {
    if (!physicsSystem) return;

    // Reset Jolt-owned objects in reverse order
    characterContactListener.reset();
    contactListener.reset();
    jobSystemThreadPool.reset();
    tempAllocator.reset();
    physicsSystem.reset();
    broadPhaseLayerInterface.reset();
    objectVsBroadPhaseLayerFilter.reset();
    objectLayerPairFilter.reset();

    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
}

JPH::PhysicsSystem& Physics::getPhysicsSystem() {
    JPH_ASSERT(physicsSystem != nullptr);
    return *physicsSystem;
}

JPH::JobSystemThreadPool& Physics::getJobSystem() {
    JPH_ASSERT(jobSystemThreadPool != nullptr);
    return *jobSystemThreadPool;
}

JPH::TempAllocator* Physics::getTempAllocator() {
    return tempAllocator.get();
}

JPH::BodyInterface& Physics::getBodyInterface() {
    JPH_ASSERT(physicsSystem != nullptr);
    return physicsSystem->GetBodyInterface();
}

JPH::BodyID Physics::createBody(const JPH::BodyCreationSettings& settings) {
    JPH::Body* body = getBodyInterface().CreateBody(settings);
    if (!body) return JPH::BodyID();
    return body->GetID();
}

void Physics::addBody(JPH::BodyID id) {
    getBodyInterface().AddBody(id, JPH::EActivation::Activate);
}

void Physics::removeBody(JPH::BodyID id) {
    getBodyInterface().RemoveBody(id);
}

void Physics::destroyBody(JPH::BodyID id) {
    getBodyInterface().DestroyBody(id);
}

void Physics::setUserData(JPH::BodyID id, flecs::entity_t ent) {
    getBodyInterface().SetUserData(id, ent);
}

void Physics::step(float deltaTime) {
    if (!physicsSystem) return;
    // Determine substep count heuristically
    int numSteps = (1.0f / deltaTime) > 60.0f ? 2 : 1;
    physicsSystem->Update(deltaTime, numSteps, tempAllocator.get(), jobSystemThreadPool.get());
}

} // namespace Core
} // namespace Lca
