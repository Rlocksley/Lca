#pragma once

#include "Application.hpp"
#include "Hidden.hpp"
#include "FlyingCamera.hpp"
#include "Light.hpp"
#include "MeshTransform.hpp"
#include "Text.hpp"
#include "GuiText.hpp"
#include "GuiRect.hpp"
#include "GuiRectPipeline.hpp"
#include "GuiInteraction.hpp"
#include "GuiButton.hpp"
#include "GuiDragDrop.hpp"
#include "GuiScrollList.hpp"
#include "TextPipeline.hpp"
#include "Lev.hpp"
#include "Shape.hpp"
#include "Mesh.hpp"
// ──────────────────────────────────────────────────────────────────────────────
// Example 10 — Text Rendering
//
// Registers:
//   "text" pipeline  — text.vert / text.frag (GPU-driven per-letter indirect)
//   "arial" font     — loaded via FreeType from a system .ttf
//
// The level spawns:
//   • A flying camera  (WASD + right-mouse-button to look)
//   • A sun (directional light)
//   • A "Hello World" text entity
// ──────────────────────────────────────────────────────────────────────────────

class ExampleApp : public Lca::Application {
public:
    ExampleApp(const std::string& title, int width, int height)
        : Application(title, width, height) {}

protected:
    void onInit() override {
        using namespace Lca;

        // ── Text pipeline (world-space, transform-driven) ─────────────────────
        Core::GraphicsPipelineConfig textCfg{};
        textCfg.vertexShader   = "shader/text.vert.spv";
        textCfg.fragmentShader = "shader/text.frag.spv";
        textCfg.cullMode       = VK_CULL_MODE_NONE;
        textCfg.depthWriteEnable = false;
        Core::TextPipeline textPipeline(textCfg);
        Core::GetRenderer().addTextPipeline("text", std::move(textPipeline));

        // ── Billboard text pipeline (always faces camera) ─────────────────────
        Core::GraphicsPipelineConfig billboardCfg{};
        billboardCfg.vertexShader   = "shader/text_billboard.vert.spv";
        billboardCfg.fragmentShader = "shader/text.frag.spv";
        billboardCfg.cullMode       = VK_CULL_MODE_NONE;
        billboardCfg.depthWriteEnable = false;
        Core::TextPipeline billboardPipeline(billboardCfg);
        Core::GetRenderer().addTextPipeline("text_billboard", std::move(billboardPipeline));

        // ── GUI text pipeline (2D overlay, screen-space) ──────────────────────
        Core::GraphicsPipelineConfig guiCfg{};
        guiCfg.vertexShader   = "shader/text_gui.vert.spv";
        guiCfg.fragmentShader = "shader/text.frag.spv";
        guiCfg.cullMode       = VK_CULL_MODE_NONE;
        guiCfg.depthWriteEnable = true;
        guiCfg.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL;
        guiCfg.sampleCount    = VK_SAMPLE_COUNT_1_BIT;
        Core::TextPipeline guiTextPipeline(guiCfg);
        Core::GetRenderer().addGuiTextPipeline("text_gui", std::move(guiTextPipeline));

        // ── GUI rect pipeline (2D overlay, screen-space rectangles) ───────────
        Core::GraphicsPipelineConfig guiRectCfg{};
        guiRectCfg.vertexShader   = "shader/gui_rect.vert.spv";
        guiRectCfg.fragmentShader = "shader/gui_rect.frag.spv";
        guiRectCfg.cullMode       = VK_CULL_MODE_NONE;
        guiRectCfg.depthWriteEnable = true;
        guiRectCfg.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL;
        guiRectCfg.sampleCount    = VK_SAMPLE_COUNT_1_BIT;
        Core::GuiRectPipeline guiRectPipeline(guiRectCfg);
        Core::GetRenderer().addGuiRectPipeline("gui_rect", std::move(guiRectPipeline));

        // ── Load font ─────────────────────────────────────────────────────────
        Core::GetAssetManager().loadFont("arial", "C:/Windows/Fonts/arial.ttf", 64);

        Core::GraphicsPipelineConfig pipelineConfig{};   
        pipelineConfig.vertexShader = "shader/mesh_basic.vert.spv";
        pipelineConfig.fragmentShader = "shader/mesh_basic.frag.spv";

        Core::MeshPipeline meshPipeline(pipelineConfig);
        Core::GetRenderer().addMeshPipeline("basic", std::move(meshPipeline), 250000);

        Shape::Cube _cube(glm::vec3(1.0f), glm::vec3(0.0f), glm::vec4(1.0f));
        Core::GetAssetManager().addMesh("cube", _cube.getVertices(), _cube.getIndices());

        // ── ECS modules ───────────────────────────────────────────────────────
        world.import<flecs::stats>();
        world.set<flecs::Rest>({});
        world.import<Module::Persistent>();
        world.import<Module::Hidden>();
        world.import<Module::Transform>();
        world.import<Module::MeshTransform>();
        world.import<Module::TransformID>();
        world.import<Module::Light>();
        world.import<Module::FlyingCamera>();
        world.import<Module::Text>();
        world.import<Module::GuiText>();
        world.import<Module::GuiRect>();
        world.import<Module::GuiInteraction>();
        world.import<Module::GuiButton>();
        world.import<Module::GuiDragDrop>();
        world.import<Module::GuiScrollList>();
        world.import<Module::Mesh>();
        auto level = std::make_shared<TextLevel>();
        loadLevel(level);
    }

    void onLoadingScreenSetup() override {
        using namespace Lca;
        world.entity("LoadingCamera")
            .set<Component::FlyingCamera>({
                .position      = glm::vec3(0.0f, 0.0f, 5.0f),
                .angle         = Component::FlyingCamera::lookAt(
                                     glm::vec3(0.0f, 0.0f, 5.0f),
                                     glm::vec3(0.0f)),
                .speed         = 4.0f,
                .rotationSpeed = 0.003f,
                .fov           = glm::radians(60.0f),
                .nearClip      = 0.05f,
                .farClip       = 500.0f
            })
            .add<Component::Transform>();
    }

    void onLevelReady() override {
        std::cout << "[Example10] Text rendering loaded.\n"
                  << "  WASD        — fly\n"
                  << "  Right-mouse — look\n";
    }
};
