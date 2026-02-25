# Lca

Lca is a Vulkan AZDO forward renderer with a simple, high-performance design. 

It uses flecs as the GameLogic ECS API so you can author game logic and scene graphs with flecs while the renderer and AssetManager provide the GPU resources and pipelines.

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

**Quick Start for Window**
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


notes:
- The renderer uses GPU-driven indirect draw/count buffers; when adding pipelines you must provide an upper bound (`maxObjects`) so the renderer can allocate per-frame indirect buffers.

Contributing
- PRs welcome.

TODO
- light culling
- Skeleton Meshes and Animations
- Asset Loading from File
- GUI
    
License
- See the top-level `LICENSE` file in this repo.
