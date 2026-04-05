# Lca

Lca is a Vulkan AZDO forward renderer with a simple, high-performance design. 

It uses flecs as the GameLogic ECS API so you can author game logic and scene graphs with flecs while the Renderer and AssetManager provide the GPU resources and pipelines.

It is tested on Windows but should be easy to port to Linux and Mac because all libraries used are cross platform.

- Renderer: Vulkan AZDO forward renderer with dynamic rendering and GPU-driven culling.
- Game API: flecs (ECS) for entities, components and relationships.
- AssetManager: centralized loader/creator for meshes, vertex/index buffers, textures and materials.

**Key files**
- Application and Level Class [src/application](src/application)  (See the Example 2 & 3 for usage)
- Renderer implementation: [src/core/Renderer.cpp](src/core/Renderer.cpp#L1)
- AssetManager implementation [src/core/AssetManager.cpp](src/core/AssetManager.cpp#L1)

**Prerequisites**
- glfw3
- Vulkan SDK (tested with 1.4.x)
- flecs (C++ API)
- glm
- CMake toolchain to build the project

**Quick Start for Windows**
- Download Visual Studio Code and install the CMake and C++ plugins
- Download the Vulkan SDK
- clone vcpkg in the same directory as your Lca Folder and build vcpkg
- install glfw3 flecs and glm through vcpkg
- build Lca + examples with the top layer CMakeLists.txt

**Quick usage**

**1) Using flecs to create a simple parent `Transform` and child `Mesh`**

This shows the game-logic side: create an entity with a `Transform` component, create a child entity with a `Mesh` component and attach it to the parent.

```cpp
        auto cube = world.entity("Cube");
        cube.set(Component::Transform{glm::vec3(0,0,0), 0.0f, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f)});
        cube.add<Component::TransformID>(); // this registers the Transform for the GPU

        auto cubeMesh = world.entity("CubeMesh");
        cubeMesh.add(flecs::ChildOf, cube); // always set to child before adding Mesh Component
        cubeMesh.set<Component::Mesh>({
            Core::GetAssetManager().getMeshId("cube"),
            Core::GetAssetManager().getMaterialId("MyMaterial"),
            Core::GetRenderer().getMeshPipelineId("MyPipeline")
        });
```

Notes:
- `Transform` and `Mesh` are application components stored in `src/component/`.

**2) Creating a Mesh with the AssetManager**

```cpp
        Shape::Cube cube(glm::vec3(1.0f), glm::vec3(0.0f), glm::vec4(1.0f));
        Core::GetAssetManager().addMesh("cube", cube.getVertices(), cube.getIndices());
```


**3) Creating a MeshPipeline with the `Renderer`**

`Renderer::addMeshPipeline` registers a graphics pipeline and allocates GPU-side indirect buffers.

```cpp
        Core::GraphicsPipelineConfig pipelineConfig{};   
        pipelineConfig.vertexShader = "shader/mesh_basic.vert.spv";
        pipelineConfig.fragmentShader = "shader/mesh_basic.frag.spv";

        Core::MeshPipeline meshPipeline(pipelineConfig);
        Core::GetRenderer().addMeshPipeline("basic", std::move(meshPipeline), 250000);
```

**4) Creating Shaders**

You can use the bound buffers, textures and vertices of mesh_basic.vert and mesh_basic.frag to write your own shaders.

notes:
- The renderer uses GPU-driven indirect draw/count buffers; when adding pipelines you must provide an upper bound (`maxObjects`) so the renderer can allocate per-frame indirect buffers.

**5) Creating a SkeletonMesh**

Skeleton meshes support vertex-skinned rendering with GPU-driven culling, driven by an `AnimationStateMachine`.

**5a) Register the pipeline (once, in `onInit`)**

```cpp
        Core::GraphicsPipelineConfig skelConfig{};
        skelConfig.vertexShader   = "shader/skeleton_mesh_pbr.vert.spv";
        skelConfig.fragmentShader = "shader/mesh_pbr.frag.spv";
        Core::SkeletonMeshPipeline skelPipeline(skelConfig);
        Core::GetRenderer().addSkeletonMeshPipeline("skeletonPBR", std::move(skelPipeline), 2 * 4096);
```

**5b) Import ECS modules**

```cpp
        world.import<Module::SkeletonMesh>();
        world.import<Module::AnimationStateMachine>();
```

**5c) Load assets (in `loadAssets`)**

`loadSkeletonModel` parses the file with Assimp, uploads all sub-meshes to the GPU skeleton vertex/index buffers, extracts the skeleton hierarchy and returns a `Model` — a flat list of (`meshName`, `materialName`, `meshId`, `materialId`, `nodeIndex`) entries, one per sub-mesh.

```cpp
        auto& am = Core::GetAssetManager();
        Model model = am.loadSkeletonModel("Wizard", "path/to/Wizard.gltf");

        // Load animation clips (returns clip names such as "Wizard/Idle")
        std::vector<std::string> animNames = am.loadAnimations("Wizard", "path/to/Wizard.gltf");

        // Re-index each animation to match the skeleton's node ordering
        for (const auto& animName : animNames) {
            am.extractAnimationForSkeleton(animName, "Wizard");
        }
```

**5d) Create the entity hierarchy (in `setupScene`)**

The parent entity owns the skeleton instance (bone matrices). Child entities, one per sub-mesh, hold the `SkeletonMesh` component.

```cpp
        uint32_t pipelineId = Core::GetRenderer().getSkeletonMeshPipelineId("skeletonPBR");

        auto character = world.entity("Character");
        character.set(Component::Transform{glm::vec3(0,0,0), 0.0f, glm::vec3(0,1,0), glm::vec3(1.0f)});
        character.add<Component::TransformID>();
        character.add<Component::SkeletonInstanceID>(); // allocates a bone-matrix slot on the GPU

        for (size_t k = 0; k < model.size(); ++k) {
            auto child = world.entity(("CharMesh_" + std::to_string(k)).c_str());
            child.add(flecs::ChildOf, character);
            child.set<Component::SkeletonMesh>({
                .skeletonName = "Wizard",
                .meshID       = model[k].meshId,
                .materialID   = model[k].materialId,
                .pipelineID   = pipelineId,
                .nodeIndex    = model[k].nodeIndex  // -1 = deformed, >= 0 = node-attached (rigid)
            });
        }
```

**5e) Set up an AnimationStateMachine**

```cpp
        Component::AnimationStateMachine asm_;
        asm_.skeletonName = "Wizard";

        // Blend space: smoothly interpolates Idle → Walk → Run
        Component::BlendSpace1D blendSpace;
        blendSpace.pointCount        = 3;
        blendSpace.animationNames[0] = "Wizard:Idle";
        blendSpace.animationNames[1] = "Wizard:Walk";
        blendSpace.animationNames[2] = "Wizard:Run";
        blendSpace.animationPoints[0] = 0.0f;
        blendSpace.animationPoints[1] = 0.5f;
        blendSpace.animationPoints[2] = 1.0f;
        blendSpace.blendParameter    = 0.0f;   // drive this at runtime
        asm_.addState("IdleWalkRun", blendSpace);
        asm_.setCurrentState("IdleWalkRun");

        character.set(asm_);
```

Notes:
- Animation clip names are prefixed with the skeleton name after `extractAnimationForSkeleton`: `"Wizard:Idle"`, `"Wizard:Walk"`, etc.
- `SkeletonInstanceID` on the parent allocates a per-instance bone-matrix buffer slot; the `AnimationStateMachine` writes into it every frame.
- `nodeIndex == -1` means the mesh is fully deformed by the skeleton; `nodeIndex >= 0` means the mesh is rigidly attached to a single node (useful for rigid accessories).

---

**6) Jolt Physics Components**

Physics integration uses [JoltPhysics](https://github.com/jrouwe/JoltPhysics). The high-level manager is accessible via `Core::GetPhysics()`. Components live in `src/component/` and register flecs observers that create/destroy Jolt bodies automatically.

**6a) Object layers**

Define object layers and broad-phase layers in your app (see `src/core/Physics.hpp` for the built-in `Layers` namespace):

```cpp
namespace Layers {
    static constexpr JPH::ObjectLayer STATIC_ENVIRONMENT = 0;
    static constexpr JPH::ObjectLayer PLAYER_BODY        = 1;
    // ...
}
```

**6b) `BoxCollider` — collision shape**

Describes a box half-extents used by rigid body and sensor observers to create the Jolt `BoxShape`.

```cpp
        entity.set<Component::BoxCollider>({ glm::vec3(1.0f, 0.5f, 1.0f) });
```

**6c) Rigid bodies**

All three variants require a `Transform` and a `BoxCollider` on the same entity. The corresponding flecs observer fires `OnSet` and creates the Jolt body automatically. When the component is removed the body is cleaned up via the `PhysicsBody` observer.

```cpp
        // Static — never moves, zero simulation cost
        entity.set<Component::BoxCollider>({ halfExtent });
        entity.set<Component::StaticRigidBody>({
            .objectLayer  = Layers::STATIC_ENVIRONMENT,
            .friction     = 0.8f,
            .restitution  = 0.0f
        });

        // Dynamic — fully simulated; requires Component::Velocity as well
        entity.set<Component::Velocity>({});
        entity.set<Component::BoxCollider>({ halfExtent });
        entity.set<Component::DynamicRigidBody>({
            .objectLayer  = Layers::DYNAMIC_OBJECTS,
            .mass         = 5.0f,
            .friction     = 0.5f,
            .restitution  = 0.2f
        });
        // SyncFlecsFromJolt writes position/velocity back to Transform & Velocity each frame.

        // Kinematic — moved by game code, collides with dynamic bodies
        entity.set<Component::BoxCollider>({ halfExtent });
        entity.set<Component::KinematicRigidBody>({
            .objectLayer = Layers::KINEMATIC_OBJECTS,
            .friction    = 0.5f
        });
```

**6d) `CharacterCapsule` — player/NPC character controller**

Wraps Jolt `CharacterVirtual`, providing walking, running, turning, and jumping without a traditional rigid body. Set it together with `Transform` on the same entity; the observer creates the `JPH::CharacterVirtual` on `OnSet`.

```cpp
        entity.set(Component::Transform{startPos, 0.0f, glm::vec3(0,1,0), glm::vec3(1.0f)});
        entity.set<Component::CharacterCapsule>({
            .capsuleHeight  = 1.8f,
            .capsuleRadius  = 0.3f,
            .layer          = Layers::PLAYER_BODY,
            .maxWalkSpeed   = 2.0f,
            .maxRunSpeed    = 5.0f,
            .jumpSpeed      = 6.0f,
            .speedDecrement = 4.0f
        });
```

Drive it at runtime from a controller system:

```cpp
        capsule.turn(direction);          // rotate around Y
        capsule.move(forwardDir);         // set horizontal velocity
        capsule.incrSpeed();              // accelerate toward walk/run limit
        capsule.jump();                   // apply vertical impulse if grounded
```

`capsule.updateTransform(transform)` writes the capsule's Jolt position back to the flecs `Transform` (called automatically by `CharacterCapsuleModule`).

**6e) Sensor volumes**

Sensors detect overlaps but do not block movement. They also require a `BoxCollider` and `Transform`.

```cpp
        // Static sensor (does not move)
        entity.set<Component::BoxCollider>({ halfExtent });
        entity.set<Component::StaticSensor>({ .objectLayer = Layers::SENSOR });

        // Kinematic sensor (moves with the entity, requires Component::Velocity)
        entity.set<Component::Velocity>({});
        entity.set<Component::BoxCollider>({ halfExtent });
        entity.set<Component::KinematicSensor>({ .objectLayer = Layers::SENSOR });
```

Overlap events flow through `Core::collisionQueue` (a `CollisionQueue`) as `CollisionEvent` structs. Poll it in a system to react to touches.

**6f) Registering physics modules**

```cpp
        world.import<Module::PhysicsBodyModule>();
        world.import<Module::BoxColliderModule>();
        world.import<Module::RigidBodyModule>();
        world.import<Module::CharacterCapsuleModule>();
        world.import<Module::SensorModule>();
```
---

Contributing
- PRs welcome.

TODO
- Path-Tracing
- GUI
- Particle-System
    
License
- See the top-level `LICENSE` file in this repo.
