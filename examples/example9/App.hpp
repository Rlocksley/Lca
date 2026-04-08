#pragma once

#include "Application.hpp"
#include "Hidden.hpp"
#include "FlyingCamera.hpp"
#include "Light.hpp"
#include "Mesh.hpp"
#include "MeshTransform.hpp"
#include "ParticleSystem.hpp"
#include "Shape.hpp"
#include "ProceduralTex.hpp"
#include "Image.hpp"
#include "Lev.hpp"

// ──────────────────────────────────────────────────────────────────────────────
// Example 9 — Particle System with Flying Camera
//
// Registers pipelines:
//   "basic"          — mesh_pbr.vert / mesh_pbr.frag   (floor + emitter cube)
//   "particlePBR"    — particle_pbr.vert / frag         (spark draw)
//   "particleSim"    — particle_sim.comp                (spark simulation)
//   "particleFireGfx"— particle_fire.vert / frag        (fire billboard draw)
//   "particleFireSim"— particle_fire.comp               (fire simulation)
//
// The level spawns:
//   • A flying camera  (WASD + right-mouse-button to look)
//   • A sun (directional light)
//   • A flat floor cube
//   • An emitter with a spark particle system (cube mesh)
//   • A fire emitter with a billboard fire particle system (square mesh + texture)
// ──────────────────────────────────────────────────────────────────────────────

class ExampleApp : public Lca::Application {
public:
    ExampleApp(const std::string& title, int width, int height)
        : Application(title, width, height) {}

protected:
    void onInit() override {
        using namespace Lca;

        // ── Mesh PBR pipeline ─────────────────────────────────────────────────
        Core::GraphicsPipelineConfig meshCfg{};
        meshCfg.vertexShader   = "shader/mesh_pbr.vert.spv";
        meshCfg.fragmentShader = "shader/mesh_pbr.frag.spv";
        Core::MeshPipeline meshPipeline(meshCfg);
        Core::GetRenderer().addMeshPipeline("basic", std::move(meshPipeline), 4096);

        // ── Particle compute pipeline ─────────────────────────────────────────
        Core::ParticleSystemCompPipeline simPipeline("shader/particle_sim.comp.spv");
        Core::GetRenderer().addParticleSystemCompPipeline("particleSim", std::move(simPipeline));

        // ── Particle graphics pipeline ────────────────────────────────────────
        Core::GraphicsPipelineConfig particleCfg{};
        particleCfg.vertexShader   = "shader/particle_pbr.vert.spv";
        particleCfg.fragmentShader = "shader/particle_pbr.frag.spv";
        Core::ParticleSystemPipeline particlePipeline(particleCfg);
        Core::GetRenderer().addParticleSystemPipeline("particlePBR", std::move(particlePipeline));

        // ── Fire particle compute pipeline ────────────────────────────────────
        Core::ParticleSystemCompPipeline fireSimPipeline("shader/particle_fire.comp.spv");
        Core::GetRenderer().addParticleSystemCompPipeline("particleFireSim", std::move(fireSimPipeline));

        // ── Fire particle graphics pipeline ───────────────────────────────────
        // Billboard quads: no back-face culling, no depth write (transparent).
        Core::GraphicsPipelineConfig fireCfg{};
        fireCfg.vertexShader    = "shader/particle_fire.vert.spv";
        fireCfg.fragmentShader  = "shader/particle_fire.frag.spv";
        fireCfg.cullMode        = VK_CULL_MODE_NONE;
        fireCfg.depthWriteEnable = false;
        Core::ParticleSystemPipeline firePipeline(fireCfg);
        Core::GetRenderer().addParticleSystemPipeline("particleFireGfx", std::move(firePipeline));

        // ── Meshes ────────────────────────────────────────────────────────────
        Shape::Cube cube(glm::vec3(0.5f), glm::vec3(0.0f), glm::vec4(1.0f));
        Core::GetAssetManager().addMesh("cube", cube.getVertices(), cube.getIndices());

        Shape::Square square(0.5f, glm::vec3(0.0f), glm::vec4(1.0f));
        Core::GetAssetManager().addMesh("square", square.getVertices(), square.getIndices());

        // ── Fire sprite texture ───────────────────────────────────────────────
        {
            auto pixels = ProceduralTex::generateFireSprite(128, 128, 0.0f);
            Core::Texture fireTex = Core::createTexture(128, 128, pixels.data());
            Core::GetAssetManager().addTexture("fireTex", fireTex);
        }

        // ── Materials ─────────────────────────────────────────────────────────
        Core::Material floorMat{};
        floorMat.metallic  = 0.0f;
        floorMat.roughness = 0.9f;
        floorMat.textureId = -1;
        Core::GetAssetManager().addMaterial("floorMat", floorMat);

        // Spark particle material — low roughness, no texture
        Core::Material particleMat{};
        particleMat.metallic  = 0.0f;
        particleMat.roughness = 0.3f;
        particleMat.textureId = -1;
        Core::GetAssetManager().addMaterial("particleMat", particleMat);

        // Fire particle material — uses the procedural fire sprite texture
        Core::Material fireMat{};
        fireMat.metallic   = 1.0f;
        fireMat.roughness  = 1.0f;
        fireMat.textureId  = static_cast<int>(Core::GetAssetManager().getTextureId("fireTex"));
        Core::GetAssetManager().addMaterial("fireMat", fireMat);

        // ── ECS modules ───────────────────────────────────────────────────────
        world.import<flecs::stats>();
        world.set<flecs::Rest>({});
        world.import<Module::Persistent>();
        world.import<Module::Hidden>();
        world.import<Module::Transform>();
        world.import<Module::MeshTransform>();
        world.import<Module::TransformID>();
        world.import<Module::Mesh>();
        world.import<Module::Light>();
        world.import<Module::FlyingCamera>();
        world.import<Module::ParticleSystem>();

        auto level = std::make_shared<ParticleLevel>();
        loadLevel(level);
    }

    void onLoadingScreenSetup() override {
        using namespace Lca;
        // Simple loading screen: same flying camera, no geometry yet
        world.entity("LoadingCamera")
            .set<Component::FlyingCamera>({
                .position      = glm::vec3(0.0f, 3.0f, 8.0f),
                .angle         = Component::FlyingCamera::lookAt(
                                     glm::vec3(0.0f, 3.0f, 8.0f),
                                     glm::vec3(0.0f)),
                .speed         = 8.0f,
                .rotationSpeed = 0.003f,
                .fov           = glm::radians(60.0f),
                .nearClip      = 0.05f,
                .farClip       = 500.0f
            })
            .add<Component::Transform>();
    }

    void onLevelReady() override {
        std::cout << "[Example9] Particle system loaded.\n"
                  << "  WASD        — fly\n"
                  << "  Right-mouse — look\n";
    }
};
