#include "Application.hpp"
#include "Light.hpp"
#include "Shape.hpp"
#include "Lev.hpp"

class ExampleApp : public Lca::Application {
public:
    ExampleApp(const std::string& title, int width, int height) : Application(title, width, height) {}

protected:
    void onInit() override {

        Core::GraphicsPipelineConfig pipelineConfig{};
        pipelineConfig.vertexShader = "shader/mesh_basic.vert.spv";
        pipelineConfig.fragmentShader = "shader/mesh_basic.frag.spv";

        Core::MeshPipeline meshPipeline(pipelineConfig);
        Core::GetRenderer().addMeshPipeline("basic", std::move(meshPipeline), 4096);

        // Loading screen cube
        Shape::Cube _cube(glm::vec3(1.0f), glm::vec3(0.0f), glm::vec4(1.0f));
        Core::GetAssetManager().addMesh("cube", _cube.getVertices(), _cube.getIndices());

        Core::Material material{};
        material.metallic = 1.0f;
        material.roughness = 0.5f;
        material.textureId = -1;

        Core::GetAssetManager().addMaterial("loadingCubeMaterial", material);

        world.import<flecs::stats>();
        world.set<flecs::Rest>({});

        world.import<Lca::Module::Persistent>();
        world.import<Lca::Module::Hidden>();
        world.import<Lca::Module::Transform>();
        world.import<Lca::Module::TransformID>();
        world.import<Lca::Module::Mesh>();
        world.import<Lca::Module::Light>();
        world.import<Lca::Module::FlyingCamera>();

        std::shared_ptr<Lca::Level> level = std::make_shared<GameLevel>();
        loadLevel(level);
    }

    void onLoadingScreenSetup() override {

        world.entity("LoadingScreenCamera").set<Lca::Component::FlyingCamera>({
            .position = glm::vec3(0.0f, 0.0f, 5.0f),
            .angle = Lca::Component::FlyingCamera::lookAt(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f)),
            .speed = 10.0f,
            .rotationSpeed = 0.005f,
            .fov = glm::radians(60.0f),
            .nearClip = 0.1f,
            .farClip = 1000.0f
        })
        .add<Lca::Component::Transform>();

        auto cube = world.entity("LoadingScreenCube");
        cube.set(Component::Transform{glm::vec3(0,0,0), 0.0f, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f)});
        cube.add<Component::TransformID>();
        auto cubeMesh = world.entity("LoadingScreenCubeMesh");
        cubeMesh.add(flecs::ChildOf, cube);
        cubeMesh.set<Component::Mesh>({
            Core::GetAssetManager().getMeshId("cube"),
            Core::GetAssetManager().getMaterialId("loadingCubeMaterial"),
            Core::GetRenderer().getMeshPipelineId("basic")
        });

        world.system<Component::Transform>("Rotate Loading Screen Cube")
            .without<Lca::Component::Hidden>()
            .each([](flecs::entity e, Component::Transform& transform) {
                glm::quat deltaRot = glm::angleAxis(Time::deltaTime, glm::vec3(0.0f, 1.0f, 0.0f));
                transform.rotation = deltaRot * transform.rotation;
            });
    }

    void onLevelReady() override {}
    void onUpdate() override {}
};
