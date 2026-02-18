#include "Core.hpp"
#include "Renderer.hpp"
#include "MeshPipeline.hpp"
#include "AssetManager.hpp"
#include "flecs.h"
#include "Transform.hpp"
#include "Mesh.hpp"
#include "Shape.hpp"
#include "Image.hpp"
#include "FlyingCamera.hpp"

using namespace Lca;

int main() {
    Core::windowTitle = "RxWorld";
    Core::windowHeight = 600;
    Core::windowWidth = 1200;
    Core::vkSampleCountFlagBits = VK_SAMPLE_COUNT_4_BIT;    
    Core::createCore();
    Core::GetAssetManager().init();
    Core::GetRenderer().init();

    Core::GraphicsPipelineConfig pipelineConfig{};   
    pipelineConfig.vertexShader = "shader/mesh_basic.vert.spv";
    pipelineConfig.fragmentShader = "shader/mesh_basic.frag.spv";

    Core::MeshPipeline meshPipeline(pipelineConfig);
    Core::GetRenderer().addMeshPipeline("basic", std::move(meshPipeline), 250000);

    Shape::Cube cube(glm::vec3(1.0f), glm::vec3(0.0f), glm::vec4(1.0f));
    Core::GetAssetManager().addMesh("cube", cube.getVertices(), cube.getIndices());

    Shape::Sphere sphere(1, 16, 16, glm::vec3(10.0f), glm::vec4(1.0f));
    Core::GetAssetManager().addMesh("sphere", sphere.getVertices(), sphere.getIndices());


    Core::GetAssetManager().loadTexture("fire", "asset/fire.jpg");


    Core::Material material{};
    material.metallic = 1.0f;
    material.roughness = 0.5f;
    material.textureId = Core::GetAssetManager().getTextureId("fire");

    Core::GetAssetManager().addMaterial("fire_material", material);

    flecs::world world;
    // Optional, gather statistics for explorer
    world.import<flecs::stats>();
    
    // Creates REST server on default port (27750)
    world.set<flecs::Rest>({});

    world.import<Lca::Module::Transform>();
    world.import<Lca::Module::TransformID>();
    world.import<Lca::Module::Mesh>();
    world.import<Lca::Module::FlyingCamera>();

    world.entity().set<Lca::Component::FlyingCamera>({
        .position = glm::vec3(0.0f, 0.0f, 5.0f),
        .angle = glm::vec2(0.0f, 0.0f),
        .speed = 10.0f,
        .rotationSpeed = 0.005f,
        .fov = glm::radians(60.0f),
        .nearClip = 0.1f,
        .farClip = 1000.0f
    });
    
    init_random();
    uint32_t numModels = 50000;
    for (uint32_t i = 0; i < numModels; i++) {

        auto model = world.entity();
        model.set(Component::Transform{glm::vec3(random(-100.0f, 100.0f), random(-100.0f, 100.0f), random(-100.0f, 100.0f)), 0.0f, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f)});
        model.add<Component::TransformID>();
        model.add<Component::Static>();
        
        auto cube = world.entity();
        cube.set(Component::Mesh{
                    Core::GetAssetManager().getMeshId("cube"),
                    Core::GetAssetManager().getMaterialId("fire_material"),
                    Core::GetRenderer().getMeshPipelineId("basic")
                });
        cube.add(flecs::ChildOf, model);

         auto sphere = world.entity();
         sphere.set(Component::Mesh{
                     Core::GetAssetManager().getMeshId("sphere"),
                     Core::GetAssetManager().getMaterialId("fire_material"),
                     Core::GetRenderer().getMeshPipelineId("basic")
                 });
         sphere.add(flecs::ChildOf, model);
    }

    // Main loop
    while (Core::updateCore()) {
        std::cout << "Framerate: " << 1.f / Time::deltaTime << " FPS" << std::endl;
        world.progress();
        Core::GetRenderer().recordFrame();
        Core::GetRenderer().submitFrame();
    }

    Core::GetAssetManager().shutdown();
    Core::GetRenderer().shutdown();
    Core::destroyCore();
    return 0;
}