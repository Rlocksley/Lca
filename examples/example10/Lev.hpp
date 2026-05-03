#pragma once

#include "Level.hpp"
#include "AssetManager.hpp"
#include "Image.hpp"
#include "Renderer.hpp"
#include "FlyingCamera.hpp"
#include "Light.hpp"
#include "MeshTransform.hpp"
#include "Mesh.hpp"
#include "Text.hpp"
#include "GuiText.hpp"
#include "GuiRect.hpp"
#include "GuiInteraction.hpp"
#include "GuiButton.hpp"
#include "GuiDragDrop.hpp"
#include "GuiScrollList.hpp"
#include <random>
#include <array>
#include <string>
#include <unordered_map>
#include <cmath>
#include <cstdio>

using namespace Lca;

class TextLevel : public Lca::Level {
public:
    void loadAssets() override {
        // Pre-create ALL solid-colour materials so that setupScene() only reads
        // from the cache and never allocates GPU resources itself.

        // Panel / HUD
        getSolidMaterialId({0.1f,  0.1f,  0.15f, 0.85f});
        getSolidMaterialId({0.4f,  0.6f,  1.0f,  1.0f });
        // Button
        getSolidMaterialId({0.2f,  0.2f,  0.25f, 1.0f });
        getSolidMaterialId({0.5f,  0.7f,  1.0f,  1.0f });
        getSolidMaterialId({0.3f,  0.35f, 0.45f, 1.0f });
        getSolidMaterialId({0.1f,  0.1f,  0.12f, 1.0f });
        // Equipment panel
        getSolidMaterialId({0.08f, 0.05f, 0.05f, 0.96f});
        getSolidMaterialId({0.82f, 0.66f, 0.36f, 0.95f});
        getSolidMaterialId({0.11f, 0.07f, 0.07f, 0.93f});
        getSolidMaterialId({0.42f, 0.30f, 0.16f, 0.82f});
        getSolidMaterialId({0.16f, 0.10f, 0.10f, 0.45f});
        getSolidMaterialId({0.50f, 0.36f, 0.18f, 0.35f});
        getSolidMaterialId({0.22f, 0.16f, 0.16f, 0.98f});
        getSolidMaterialId({0.90f, 0.74f, 0.42f, 0.96f});
        // Inventory panel
        getSolidMaterialId({0.05f, 0.07f, 0.12f, 0.93f});
        getSolidMaterialId({0.30f, 0.50f, 0.90f, 1.0f });
        getSolidMaterialId({0.30f, 0.50f, 0.90f, 0.60f});
        getSolidMaterialId({0.10f, 0.12f, 0.18f, 0.80f});
        getSolidMaterialId({0.25f, 0.40f, 0.70f, 0.50f});
        getSolidMaterialId({0.35f, 0.55f, 0.95f, 0.85f});
        getSolidMaterialId({0.50f, 0.70f, 1.00f, 0.60f});
        getSolidMaterialId({0.08f, 0.10f, 0.16f, 0.90f});
        getSolidMaterialId({0.11f, 0.13f, 0.20f, 0.90f});
        getSolidMaterialId({0.0f,  0.0f,  0.0f,  0.4f });
        // Item icon colours / textures
        for (const auto& item : kItems) {
            if (item.texturePath)
                getTextureMaterialId(item.texturePath);
            else
                getSolidMaterialId(item.iconColor);
        }
    }

    void setupScene(flecs::world& world) override {
        const uint32_t textPipeID = Core::GetRenderer().getTextPipelineId("text");
        const uint32_t fontID     = Core::GetAssetManager().getFontId("arial");

        // ── Flying camera ────────────────────────────────────────────────────
        world.entity("FlyCamera")
            .set<Component::FlyingCamera>({
                .position      = glm::vec3(0.0f, 0.0f, 0.0f),
                .angle         = Component::FlyingCamera::lookAt(
                                     glm::vec3(0.0f, 0.0f, 0.0f),
                                     glm::vec3(0.0f, 0.0f, 1.0f)),
                .speed         = 8.0f,
                .rotationSpeed = 0.003f,
                .fov           = glm::radians(60.0f),
                .nearClip      = 0.01f,
                .farClip       = 800.0f
            })
            .add<Component::Transform>();

        // ── Sun ──────────────────────────────────────────────────────────────
        world.entity("Sun")
            .set<Component::DirectionalLight>({
                .color     = glm::vec3(1.0f, 0.95f, 0.85f),
                .intensity = 2.5f,
            });

        // ── Word corpus ──────────────────────────────────────────────────────
        static const char* phrases[] = {
            "hello world", "the quick brown fox", "jumps over the lazy dog",
            "once upon a time", "in a galaxy far away", "to be or not to be",
            "knowledge is power", "time flies like an arrow",
            "all that glitters is not gold", "the stars are bright tonight",
            "dream big", "never give up", "courage", "freedom", "imagine",
            "wonder", "explore", "create", "discover", "believe",
            "infinite possibilities", "beyond the horizon", "whispers in the wind",
            "echoes of eternity", "fragments of light", "rivers of thought",
            "mountains of memory", "oceans of time", "fields of dreams",
            "castles in the sky", "wanderlust", "serendipity", "ephemeral",
            "luminous", "ethereal", "transcend", "horizon", "velocity",
            "gravity", "entropy", "quantum", "nebula", "cosmos",
            "the universe is vast", "silence speaks volumes",
            "every moment matters", "lost in translation",
            "between the lines", "under the surface", "above the clouds",
            "through the looking glass", "into the unknown", "out of thin air",
            "a spark of genius", "the fabric of reality",
            "pixels and polygons", "bits and bytes", "code is poetry",
            "vulkan rendering", "vertex shader magic", "fragment of light",
            "compute the future", "parallel worlds", "synchronize",
            "buffer overflow of joy", "pipeline dreams",
            "the art of programming", "algorithms dance",
            "data structures sing", "memory is fleeting",
            "pointers to the truth", "recursion is recursion",
            "sunrise", "moonlight", "starfall", "thunderstorm",
            "whirlwind", "avalanche", "blizzard", "tsunami",
            "earthquake", "volcano", "wildfire", "rainbow",
            "the pen is mightier", "actions speak louder",
            "fortune favors the bold", "carpe diem",
            "memento mori", "amor fati", "per aspera ad astra",
            "cogito ergo sum", "veni vidi vici", "alea iacta est",
            "wandering not lost", "the road less traveled",
            "footprints in the sand", "bridges not walls",
            "open your mind", "break the mold", "think different",
            "stay curious", "keep moving forward", "rise above",
            "stand your ground", "light the way", "find your path",
            "chase the dawn", "embrace the chaos", "seek the truth",
            "paint the sky", "write your story", "sing your song",
            "dance in the rain", "laugh out loud", "breathe deeply",
            "let it go", "hold on tight", "reach for the stars",
            "fly higher", "dive deeper", "run faster",
            "grow stronger", "live fully", "love freely",
            "dream wildly", "hope endlessly", "shine brightly",
            "the journey continues", "adventure awaits",
            "new horizons call", "tomorrow is unwritten"
        };

        std::mt19937 rng(42);
        std::uniform_real_distribution<float> distPos(-120.0f, 120.0f);
        std::uniform_real_distribution<float> distY(-40.0f, 60.0f);
        std::uniform_real_distribution<float> distAngle(0.0f, glm::two_pi<float>());
        std::uniform_real_distribution<float> distScale(0.01f, 0.06f);
        std::uniform_real_distribution<float> distTilt(-0.4f, 0.4f);
        std::uniform_int_distribution<int>    distPhrase(0, static_cast<int>(std::size(phrases)) - 1);

        const int totalTexts = 300;

        for (int i = 0; i < totalTexts; i++) {
            const char* phrase = phrases[distPhrase(rng)];
            uint32_t maxL = static_cast<uint32_t>(std::strlen(phrase)) + 1;

            float px = distPos(rng);
            float py = distY(rng);
            float pz = distPos(rng);
            float yaw   = distAngle(rng);
            float pitch  = distTilt(rng);
            float s  = distScale(rng);

            std::string parentName = "TextParent_" + std::to_string(i);
            std::string childName  = "Text_" + std::to_string(i);

            glm::quat rot = glm::angleAxis(yaw, glm::vec3(0, 1, 0))
                          * glm::angleAxis(pitch, glm::vec3(1, 0, 0));

            auto parent = world.entity(parentName.c_str());
            Component::Transform t;
            t.position = glm::vec3(px, py, pz);
            t.rotation = rot;
            t.scale    = glm::vec3(s);
            parent.set<Component::Transform>(t);
            parent.add<Component::TransformID>();

            auto child = world.entity(childName.c_str()).add(flecs::ChildOf, parent);
            child.set<Component::Text>({
                .text       = phrase,
                .fontID     = fontID,
                .pipelineID = textPipeID,
                .maxLetters = maxL
            });
        }

        // ── Billboard text (always faces camera, like damage numbers) ────────
        const uint32_t billboardPipeID = Core::GetRenderer().getTextPipelineId("text_billboard");

        static const char* billboardTexts[] = {
            "-42", "+100 HP", "CRITICAL!", "Miss", "Level Up!"
        };

        for (int i = 0; i < 5; i++) {
            float bx = -10.0f + i * 5.0f;
            float by = 3.0f;
            float bz = 0.0f;

            std::string bParentName = "BillboardParent_" + std::to_string(i);
            std::string bChildName  = "BillboardText_"   + std::to_string(i);

            auto bParent = world.entity(bParentName.c_str());
            Component::Transform bt;
            bt.position = glm::vec3(bx, by, bz);
            bt.scale    = glm::vec3(0.04f);
            bParent.set<Component::Transform>(bt);
            bParent.add<Component::TransformID>();

            auto bChild = world.entity(bChildName.c_str()).add(flecs::ChildOf, bParent);
            bChild.set<Component::Text>({
                .text       = billboardTexts[i],
                .fontID     = fontID,
                .pipelineID = billboardPipeID,
                .maxLetters = static_cast<uint32_t>(std::strlen(billboardTexts[i])) + 1
            });
        }

        // ── GUI text (2D screen-space overlay) ───────────────────────────────
        const uint32_t guiPipeID = Core::GetRenderer().getGuiTextPipelineId("text_gui");

        // Top-left HUD text
        {
            auto guiParent = world.entity("GuiParent_HP");
            Component::Transform gt;
            gt.position = glm::vec3(20.0f, 50.0f, 0.1f);  // 20px from left, 50px from top; z=0.1 background layer
            gt.scale    = glm::vec3(0.375f);                // 24px text (24/64 font render size)
            guiParent.set<Component::Transform>(gt);
            guiParent.add<Component::TransformID>();

            auto guiChild = world.entity("GuiText_HP").add(flecs::ChildOf, guiParent);
            guiChild.set<Component::GuiText>({
                .text       = "HP: 100 / 100",
                .fontID     = fontID,
                .pipelineID = guiPipeID,
                .maxLetters = 20
            });
        }

        // Center notification
        {
            auto guiParent2 = world.entity("GuiParent_Notify");
            Component::Transform gt2;
            gt2.position = glm::vec3(500.0f, 60.0f, 0.1f); // Roughly center-top; z=0.1 background layer
            gt2.scale    = glm::vec3(0.5f);                 // 32px text (32/64 font render size)
            guiParent2.set<Component::Transform>(gt2);
            guiParent2.add<Component::TransformID>();

            auto guiChild2 = world.entity("GuiText_Notify").add(flecs::ChildOf, guiParent2);
            guiChild2.set<Component::GuiText>({
                .text       = "Welcome to the World!",
                .fontID     = fontID,
                .pipelineID = guiPipeID,
                .maxLetters = 25
            });
        }

        // ── GUI rect demo (2D panel background) ──────────────────────────────
        const uint32_t guiRectPipeID = Core::GetRenderer().getGuiRectPipelineId("gui_rect");
        {
            auto rectParent = world.entity("GuiRectParent_Panel");
            Component::Transform rt;
            rt.position = glm::vec3(10.0f, 30.0f, 0.1f);  // z=0.1 background layer
            rt.scale    = glm::vec3(1.0f);
            rectParent.set<Component::Transform>(rt);
            rectParent.add<Component::TransformID>();

            auto rectChild = world.entity("GuiRect_Panel").add(flecs::ChildOf, rectParent);
            rectChild.set<Component::GuiRect>({
                .rect        = glm::vec4(0.0f, 0.0f, 300.0f, 40.0f),
                .borderWidth = 2.0f,
                .cornerRadius = 8.0f,
                .materialId  = getSolidMaterialId(glm::vec4(0.1f, 0.1f, 0.15f, 0.85f)),
                .borderMaterialId = getSolidMaterialId(glm::vec4(0.4f, 0.6f, 1.0f, 1.0f)),
                .pipelineID  = guiRectPipeID,
            });
        }

        // ── GUI button demo ──────────────────────────────────────────────────
        {
            auto btnParent = world.entity("GuiButtonParent");
            Component::Transform bt;
            bt.position = glm::vec3(10.0f, 80.0f, 0.2f);  // z=0.2 interactive widget layer
            bt.scale    = glm::vec3(1.0f);
            btnParent.set<Component::Transform>(bt);
            btnParent.add<Component::TransformID>();

            auto btnChild = world.entity("GuiButton_Start").add(flecs::ChildOf, btnParent);
            btnChild.set<Component::GuiRect>({
                .rect        = glm::vec4(0.0f, 0.0f, 120.0f, 36.0f),
                .borderWidth = 1.5f,
                .cornerRadius = 6.0f,
                .materialId  = getSolidMaterialId(glm::vec4(0.2f, 0.2f, 0.25f, 1.0f)),
                .borderMaterialId = getSolidMaterialId(glm::vec4(0.5f, 0.7f, 1.0f, 1.0f)),
                .pipelineID  = guiRectPipeID,
            });
            btnChild.set<Component::GuiInteraction>({});
            btnChild.set<Component::GuiButton>({
                .normalMaterialId = getSolidMaterialId(glm::vec4(0.2f, 0.2f, 0.25f, 1.0f)),
                .hoverMaterialId  = getSolidMaterialId(glm::vec4(0.3f, 0.35f, 0.45f, 1.0f)),
                .pressMaterialId  = getSolidMaterialId(glm::vec4(0.1f, 0.1f, 0.12f, 1.0f)),
            });

            // Button label — separate text parent for scale control
            auto btnTextParent = world.entity("GuiButtonTextParent");
            Component::Transform btt;
            btt.position = glm::vec3(22.0f, 104.0f, 0.2f); // centered inside button rect; same z as button
            btt.scale    = glm::vec3(0.375f);               // 24px text
            btnTextParent.set<Component::Transform>(btt);
            btnTextParent.add<Component::TransformID>();

            auto btnText = world.entity("GuiButton_Start_Text").add(flecs::ChildOf, btnTextParent);
            btnText.set<Component::GuiText>({
                .text       = "Press Me",
                .fontID     = fontID,
                .pipelineID = guiPipeID,
                .maxLetters = 12
            });

            // System: update button label text based on interaction state
            world.system<const Component::GuiInteraction, const Component::GuiButton>("ButtonTextUpdate")
                .each([btnText, fontID, guiPipeID](flecs::entity e,
                            const Component::GuiInteraction& interaction,
                            const Component::GuiButton&) {
                    const char* label = "Press Me";
                    if (interaction.pressed) {
                        label = "Pressed";
                    } else if (interaction.hovered) {
                        label = "Hovered";
                    }
                    if (const auto* current = btnText.try_get<Component::GuiText>()) {
                        if (current->text == label) return;
                    }
                    btnText.set<Component::GuiText>({
                        .text       = label,
                        .fontID     = fontID,
                        .pipelineID = guiPipeID,
                        .maxLetters = 12
                    });
                });
        }

        // ── RPG Inventory (scrollable list demo) ─────────────────────────────
        setupEquipmentGui(world, guiRectPipeID, guiPipeID, fontID);
        setupInventory(world, guiRectPipeID, guiPipeID, fontID);
    }

private:

    static int32_t getTextureMaterialId(const char* texPath) {
        static std::unordered_map<std::string, int32_t> cache;

        auto it = cache.find(texPath);
        if (it != cache.end()) return it->second;

        std::string texName = std::string("ex10_file_tex_") + texPath;
        for (char& c : texName) if (c == '/' || c == '\\') c = '_';

        // Load full-resolution pixels, then upload with a proper mip-chain.
        // createTextureMipmapped generates all LODs via vkCmdBlitImage and
        // sets maxLod to the full chain, so the GPU picks the correct detail
        // level regardless of the displayed rect size.
        Core::TextureCPU full = Core::loadTexture(texPath);
        Core::Texture tex = Core::createTextureMipmapped(full.width, full.height, full.pixels);
        Core::freeTexture(full);
        Core::GetAssetManager().addTexture(texName, tex);

        std::string matName = std::string("ex10_file_mat_") + texName;
        Core::Material mat{};
        mat.textureId = static_cast<int32_t>(Core::GetAssetManager().getTextureId(texName));
        mat.roughness = 1.0f;
        mat.metallic  = 0.0f;
        int32_t matId = static_cast<int32_t>(Core::GetAssetManager().addMaterial(matName, mat));

        cache[texPath] = matId;
        return matId;
    }

    static int32_t getSolidMaterialId(const glm::vec4& color) {
        static std::unordered_map<uint32_t, int32_t> cache;

        const auto toSrgbByte = [](float linear) -> uint8_t {
            float c = glm::clamp(linear, 0.0f, 1.0f);
            float s = (c <= 0.0031308f) ? (c * 12.92f)
                                         : (1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f);
            return static_cast<uint8_t>(s * 255.0f + 0.5f);
        };
        const auto toLinearByte = [](float value) -> uint8_t {
            return static_cast<uint8_t>(glm::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
        };

        uint8_t r = toSrgbByte(color.r);
        uint8_t g = toSrgbByte(color.g);
        uint8_t b = toSrgbByte(color.b);
        uint8_t a = toLinearByte(color.a);
        uint32_t key = (uint32_t(r) << 24) | (uint32_t(g) << 16) | (uint32_t(b) << 8) | uint32_t(a);

        auto it = cache.find(key);
        if (it != cache.end()) {
            return it->second;
        }

        char matName[32];
        char texName[40];
        std::snprintf(matName, sizeof(matName), "ex10_gui_mat_%08X", key);
        std::snprintf(texName, sizeof(texName), "ex10_gui_tex_%08X", key);

        uint8_t pixel[4] = { r, g, b, a };
        Core::Texture tex = Core::createTexture(1, 1, pixel);
        Core::GetAssetManager().addTexture(texName, tex);

        Core::Material mat{};
        mat.textureId = static_cast<int32_t>(Core::GetAssetManager().getTextureId(texName));
        mat.roughness = 1.0f;
        mat.metallic = 0.0f;
        int32_t matId = static_cast<int32_t>(Core::GetAssetManager().addMaterial(matName, mat));

        cache[key] = matId;
        return matId;
    }

    // ── Inventory item data ───────────────────────────────────────────────────
    struct InvItem {
        const char* name;
        const char* type;
        int         stack;
        glm::vec4   iconColor;  // fallback solid color when texturePath is nullptr
        const char* texturePath = nullptr; // if set, use this texture for the icon
    };

    static constexpr InvItem kItems[] = {
        { "Iron Sword",       "Weapon",      1,  {0.72f, 0.72f, 0.75f, 1.f} },
        { "Health Potion",    "Consumable",  5,  {0.85f, 0.18f, 0.18f, 1.f} },
        { "Leather Armor",    "Armor",       1,  {0.60f, 0.44f, 0.28f, 1.f} },
        { "Wooden Shield",    "Armor",       1,  {0.55f, 0.38f, 0.22f, 1.f} },
        { "Mana Potion",      "Consumable",  3,  {0.22f, 0.30f, 0.85f, 1.f} },
        { "Fire Staff",       "Weapon",      1,  {0.90f, 0.40f, 0.10f, 1.f} },
        { "Steel Chestplate", "Armor",       1,  {0.50f, 0.52f, 0.56f, 1.f} },
        { "Elven Dagger",     "Weapon",      2,  {0.30f, 0.75f, 0.55f, 1.f} },
        { "Arrow",            "Ammo",        24, {0.80f, 0.72f, 0.20f, 1.f} },
        { "Short Bow",        "Weapon",      1,  {0.58f, 0.40f, 0.18f, 1.f} },
        { "Ancient Scroll",   "Spellbook",   2,  {0.82f, 0.72f, 0.28f, 1.f} },
        { "Ruby Gem",         "Material",    4,  {0.85f, 0.10f, 0.22f, 1.f} },
        { "Iron Ore",         "Material",    12, {0.38f, 0.38f, 0.40f, 1.f} },
        { "Lock Pick Set",    "Tool",        1,  {0.42f, 0.52f, 0.74f, 1.f} },
        { "Torch",            "Equipment",   6,  {0.90f, 0.58f, 0.12f, 1.f} },
        { "Rope (50ft)",      "Equipment",   1,  {0.60f, 0.52f, 0.36f, 1.f} },
        { "Dragon Scale",     "Material",    2,  {0.18f, 0.58f, 0.28f, 1.f} },
        { "Enchanted Ring",   "Accessory",   1,  {0.65f, 0.20f, 0.85f, 1.f} },
        { "Traveler's Map",   "Quest",       1,  {0.88f, 0.82f, 0.52f, 1.f} },
        { "Compass",          "Equipment",   1,  {0.80f, 0.65f, 0.28f, 1.f} },
        { "Poison Vial",      "Consumable",  2,  {0.28f, 0.72f, 0.20f, 1.f} },
        { "Stone of Return",  "Quest",       1,  {0.50f, 0.50f, 0.90f, 1.f} },
        { "Skeleton Key",     "Tool",        1,  {0.72f, 0.70f, 0.60f, 1.f} },
        { "Elixir of Speed",  "Consumable",  1,  {0.88f, 0.60f, 0.18f, 1.f} },
        { "War Axe",          "Weapon",      1,  {0.68f, 0.65f, 0.68f, 1.f} },
        { "Knight Armour",    "Armor",       1,  {0.72f, 0.72f, 0.75f, 1.f}, "asset/Armour_1.png" },
        { "Knight Helmet",    "Armor",       1,  {0.72f, 0.72f, 0.75f, 1.f}, "asset/Helmet_1.png" },
    };

    static constexpr float kListX    = 852.f;   // viewport left (inside panel)
    static constexpr float kListY    = 92.f;    // viewport top  (below header bar)
    static constexpr float kListW    = 402.f;   // viewport width
    static constexpr float kListH    = 570.f;   // viewport height
    static constexpr float kRowH     = 54.f;    // height of one row
    static constexpr float kPanelX   = 840.f;
    static constexpr float kPanelY   = 20.f;
    static constexpr float kPanelW   = 430.f;
    static constexpr float kPanelH   = 670.f;

    static constexpr float kEquipPanelX = 470.f;
    static constexpr float kEquipPanelY = 70.f;
    static constexpr float kEquipPanelW = 300.f;
    static constexpr float kEquipPanelH = 560.f;

    void setupEquipmentGui(flecs::world& world,
                           uint32_t rectPipeID, uint32_t textPipeID, uint32_t fontID)
    {
        using namespace Lca::Component;
        using namespace glm;

        // Main panel
        {
            auto p = world.entity("EquipPanelBg_P");
            p.add<TransformID>();
            Transform t;
            t.position = vec3(kEquipPanelX, kEquipPanelY, 0.08f);
            t.scale    = vec3(1.f);
            p.set<Transform>(t);

            world.entity("EquipPanelBg").add(flecs::ChildOf, p)
                .set<GuiRect>({
                    .rect         = vec4(0, 0, kEquipPanelW, kEquipPanelH),
                    .borderWidth  = 2.5f,
                    .cornerRadius = 12.0f,
                    .materialId   = getSolidMaterialId(vec4(0.08f, 0.05f, 0.05f, 0.96f)),
                    .borderMaterialId = getSolidMaterialId(vec4(0.82f, 0.66f, 0.36f, 0.95f)),
                    .pipelineID   = rectPipeID,
                });
        }

        // Title
        {
            auto p = world.entity("EquipTitle_P");
            p.add<TransformID>();
            Transform t;
            t.position = vec3(kEquipPanelX + 58.f, kEquipPanelY + 40.f, 0.15f);
            t.scale    = vec3(0.421875f); // 27 px
            p.set<Transform>(t);

            world.entity("EquipTitle_T").add(flecs::ChildOf, p)
                .set<GuiText>({
                    .text       = "EQUIPMENT",
                    .fontID     = fontID,
                    .pipelineID = textPipeID,
                    .maxLetters = 10,
                });
        }

        // Inner backdrop to frame the silhouette area.
        {
            auto p = world.entity("EquipInnerFrame_P");
            p.add<TransformID>();
            Transform t;
            t.position = vec3(kEquipPanelX + 20.f, kEquipPanelY + 66.f, 0.10f);
            t.scale    = vec3(1.f);
            p.set<Transform>(t);

            world.entity("EquipInnerFrame_R").add(flecs::ChildOf, p)
                .set<GuiRect>({
                    .rect         = vec4(0, 0, kEquipPanelW - 40.f, kEquipPanelH - 88.f),
                    .borderWidth  = 1.8f,
                    .cornerRadius = 8.0f,
                    .materialId   = getSolidMaterialId(vec4(0.11f, 0.07f, 0.07f, 0.93f)),
                    .borderMaterialId = getSolidMaterialId(vec4(0.42f, 0.30f, 0.16f, 0.82f)),
                    .pipelineID   = rectPipeID,
                });
        }

        // Subtle silhouette backdrop to emphasize center alignment.
        {
            auto p = world.entity("EquipSilhouette_P");
            p.add<TransformID>();
            Transform t;
            t.position = vec3(kEquipPanelX + 104.f, kEquipPanelY + 146.f, 0.12f);
            t.scale    = vec3(1.f);
            p.set<Transform>(t);

            world.entity("EquipSilhouette_R").add(flecs::ChildOf, p)
                .set<GuiRect>({
                    .rect         = vec4(0, 0, 92.f, 320.f),
                    .borderWidth  = 1.0f,
                    .cornerRadius = 20.0f,
                    .materialId   = getSolidMaterialId(vec4(0.16f, 0.10f, 0.10f, 0.45f)),
                    .borderMaterialId = getSolidMaterialId(vec4(0.50f, 0.36f, 0.18f, 0.35f)),
                    .pipelineID   = rectPipeID,
                });
        }

        auto addSlot = [&](const char* slotName, const char* slotLabel,
                           float x, float y, float size) {
            auto p = world.entity((std::string("EquipSlot_") + slotName + "_P").c_str());
            p.add<TransformID>();
            Transform t;
            t.position = vec3(kEquipPanelX + x, kEquipPanelY + y, 0.20f);
            t.scale    = vec3(1.f);
            p.set<Transform>(t);

            world.entity((std::string("EquipSlot_") + slotName + "_R").c_str())
                .add(flecs::ChildOf, p)
                .set<GuiRect>({
                    .rect         = vec4(0, 0, size, size),
                    .borderWidth  = 2.0f,
                    .cornerRadius = 5.0f,
                    .materialId   = getSolidMaterialId(vec4(0.22f, 0.16f, 0.16f, 0.98f)),
                    .borderMaterialId = getSolidMaterialId(vec4(0.90f, 0.74f, 0.42f, 0.96f)),
                    .pipelineID   = rectPipeID,
                })
                .set<GuiInteraction>({})
                .add<GuiDragDropTarget>();

            auto lp = world.entity((std::string("EquipSlot_") + slotName + "_LblP").c_str());
            lp.add<TransformID>();
            Transform lt;
            const float approxLabelW = static_cast<float>(std::strlen(slotLabel)) * 7.0f;
            lt.position = vec3(kEquipPanelX + x + (size - approxLabelW) * 0.5f, kEquipPanelY + y - 10.f, 0.22f);
            lt.scale    = vec3(0.1875f); // 12 px
            lp.set<Transform>(lt);

            world.entity((std::string("EquipSlot_") + slotName + "_LblT").c_str())
                .add(flecs::ChildOf, lp)
                .set<GuiText>({
                    .text       = slotLabel,
                    .fontID     = fontID,
                    .pipelineID = textPipeID,
                    .maxLetters = static_cast<uint32_t>(std::strlen(slotLabel)) + 1,
                });
        };

        // Symmetric square-slot layout:
        //    x
        // x  x  x
        //    x
        // x     x
        addSlot("Helmet",  "HELMET", 115.f, 104.f, 70.f);
        addSlot("Weapon1", "WEAPON1", 34.f, 206.f, 64.f);
        addSlot("Armour",  "ARMOUR", 107.f, 195.f, 86.f); // Armour intentionally larger than belt
        addSlot("Weapon2", "WEAPON2", 202.f, 206.f, 64.f);
        addSlot("Belt",    "BELT",   119.f, 316.f, 62.f);
        addSlot("Gloves",  "GLOVES", 34.f, 416.f, 64.f);
        addSlot("Boots",   "BOOTS",  202.f, 416.f, 64.f);
    }

    void setupInventory(flecs::world& world,
                        uint32_t rectPipeID, uint32_t textPipeID, uint32_t fontID)
    {
        using namespace Lca::Component;
        using namespace glm;

        // ── Item database (defined as kItems class-static below) ─────────────
        constexpr int kItemCount = static_cast<int>(std::size(kItems));
        const auto& items = kItems;
        const float contentH = kItemCount * kRowH;

        // clipRect covers the scrollable viewport — used by every row's GuiRect/GuiText
        const vec4 clip = { kListX, kListY, kListX + kListW, kListY + kListH };

        // ── Outer panel background ────────────────────────────────────────────
        {
            auto p = world.entity("InvPanelBg_P");
            p.add<TransformID>();
            Transform t;
            t.position = vec3(kPanelX, kPanelY, 0.08f);
            t.scale    = vec3(1.f);
            p.set<Transform>(t);

            world.entity("InvPanelBg").add(flecs::ChildOf, p)
                .set<GuiRect>({
                    .rect         = vec4(0, 0, kPanelW, kPanelH),
                    .borderWidth  = 2.0f,
                    .cornerRadius = 8.0f,
                    .materialId   = getSolidMaterialId(vec4(0.05f, 0.07f, 0.12f, 0.93f)),
                    .borderMaterialId = getSolidMaterialId(vec4(0.30f, 0.50f, 0.90f, 1.0f)),
                    .pipelineID   = rectPipeID,
                });
        }

        // ── Panel title ───────────────────────────────────────────────────────
        {
            auto p = world.entity("InvTitle_P");
            p.add<TransformID>();
            Transform t;
            t.position = vec3(kPanelX + 26.f, kPanelY + 40.f, 0.15f);
            t.scale    = vec3(0.46875f);   // 30 px  (30/64)
            p.set<Transform>(t);

            world.entity("InvTitle_T").add(flecs::ChildOf, p)
                .set<GuiText>({
                    .text       = "INVENTORY",
                    .fontID     = fontID,
                    .pipelineID = textPipeID,
                    .maxLetters = 10,
                });
        }

        // ── Column header labels ──────────────────────────────────────────────
        auto addHeaderText = [&](const char* name, const char* txt,
                                 float x, float y) {
            auto p = world.entity(name);
            p.add<TransformID>();
            Transform t;
            t.position = vec3(x, y, 0.15f);
            t.scale    = vec3(0.234375f);   // 15 px
            p.set<Transform>(t);

            world.entity().add(flecs::ChildOf, p)
                .set<GuiText>({
                    .text       = txt,
                    .fontID     = fontID,
                    .pipelineID = textPipeID,
                    .maxLetters = static_cast<uint32_t>(std::strlen(txt) + 1),
                });
        };

        addHeaderText("InvHdr_Item", "ITEM",  kListX + 52.f, kListY - 18.f);
        addHeaderText("InvHdr_Type", "TYPE",  kListX + 214.f, kListY - 18.f);
        addHeaderText("InvHdr_Qty",  "QTY",   kListX + 366.f, kListY - 18.f);

        // ── Thin separator line beneath the header ────────────────────────────
        {
            auto p = world.entity("InvSep_P");
            p.add<TransformID>();
            Transform t;
            t.position = vec3(kListX, kListY - 4.f, 0.15f);
            t.scale    = vec3(1.f);
            p.set<Transform>(t);

            world.entity("InvSep_R").add(flecs::ChildOf, p)
                .set<GuiRect>({
                    .rect        = vec4(0, 0, kListW, 1.5f),
                    .borderWidth = 0.f,
                    .materialId  = getSolidMaterialId(vec4(0.30f, 0.50f, 0.90f, 0.60f)),
                    .pipelineID  = rectPipeID,
                });
        }

        // ── Scrollbar track ───────────────────────────────────────────────────
        const float kBarX = kListX + kListW + 4.f;
        {
            auto p = world.entity("InvBarTrack_P");
            p.add<TransformID>();
            Transform t;
            t.position = vec3(kBarX, kListY, 0.15f);
            t.scale    = vec3(1.f);
            p.set<Transform>(t);

            world.entity("InvBarTrack_R").add(flecs::ChildOf, p)
                .set<GuiRect>({
                    .rect         = vec4(0, 0, 8.f, kListH),
                    .borderWidth  = 1.f,
                    .cornerRadius = 4.f,
                    .materialId   = getSolidMaterialId(vec4(0.10f, 0.12f, 0.18f, 0.80f)),
                    .borderMaterialId = getSolidMaterialId(vec4(0.25f, 0.40f, 0.70f, 0.50f)),
                    .pipelineID   = rectPipeID,
                });
        }

        // ── Scrollbar thumb (dynamic — updated every frame by system below) ──
        const float thumbH = std::max(20.f, kListH * kListH / contentH);
        auto thumbP = world.entity("InvBarThumb_P");
        {
            thumbP.add<TransformID>();
            Transform t;
            t.position = vec3(kBarX, kListY, 0.16f);
            t.scale    = vec3(1.f);
            thumbP.set<Transform>(t);

            world.entity("InvBarThumb_R").add(flecs::ChildOf, thumbP)
                .set<GuiRect>({
                    .rect         = vec4(0, 0, 8.f, thumbH),
                    .borderWidth  = 1.f,
                    .cornerRadius = 4.f,
                    .materialId   = getSolidMaterialId(vec4(0.35f, 0.55f, 0.95f, 0.85f)),
                    .borderMaterialId = getSolidMaterialId(vec4(0.50f, 0.70f, 1.00f, 0.60f)),
                    .pipelineID   = rectPipeID,
                });
        }

        // ── Scroll list container entity ──────────────────────────────────────
        auto listE = world.entity("InvList");
        listE.set<Component::GuiScrollList>({
            .position      = vec2(kListX, kListY),
            .size          = vec2(kListW, kListH),
            .scrollOffset  = 0.f,
            .contentHeight = contentH,
            .scrollSpeed   = 42.f,
            .smoothing     = 5.f,
            .wrapAround    = false,
        });

        // ── Row entities (children of listE) ─────────────────────────────────
        for (int i = 0; i < kItemCount; i++) {
            const InvItem& item = items[i];
            const float baseY = i * kRowH;

            // Alternating row background color
            const vec4 rowColor = (i % 2 == 0)
                ? vec4(0.08f, 0.10f, 0.16f, 0.90f)
                : vec4(0.11f, 0.13f, 0.20f, 0.90f);

            const std::string si = std::to_string(i);

            // ── Row background ────────────────────────────────────────────────
            {
                auto p = world.entity(("InvRow_P" + si).c_str()).child_of(listE);
                p.add<TransformID>();
                Transform t;
                t.position = vec3(kListX, kListY + baseY, 0.20f);
                t.scale    = vec3(1.f);
                p.set<Transform>(t);
                p.set<Component::GuiScrollItem>({ .baseY = baseY, .offsetX = 0.f, .offsetY = 0.f });

                world.entity(("InvRow_R" + si).c_str()).add(flecs::ChildOf, p)
                    .set<GuiRect>({
                        .rect        = vec4(0, 0, kListW, kRowH - 2.f),
                        .clipRect    = clip,
                        .borderWidth = 0.f,
                        .materialId  = getSolidMaterialId(rowColor),
                        .pipelineID  = rectPipeID,
                    });
            }

            // ── Item icon (colored square) ────────────────────────────────────
            {
                auto p = world.entity(("InvIcon_P" + si).c_str()).child_of(listE);
                p.add<TransformID>();
                Transform t;
                t.position = vec3(kListX + 6.f, kListY + baseY + 7.f, 0.21f);
                t.scale    = vec3(1.f);
                p.set<Transform>(t);
                p.set<Component::GuiScrollItem>({ .baseY = baseY, .offsetX = 6.f, .offsetY = 7.f });

                const int32_t iconMatId = item.texturePath
                    ? getTextureMaterialId(item.texturePath)
                    : getSolidMaterialId(item.iconColor);
                world.entity(("InvIcon_R" + si).c_str()).add(flecs::ChildOf, p)
                    .set<GuiRect>({
                        .rect         = vec4(0, 0, 38.f, 38.f),
                        .clipRect     = clip,
                        .borderWidth  = 1.f,
                        .cornerRadius = 4.f,
                        .materialId   = iconMatId,
                        .borderMaterialId = getSolidMaterialId(vec4(0.f, 0.f, 0.f, 0.4f)),
                        .pipelineID   = rectPipeID,
                    })
                    .set<Component::GuiInteraction>({})
                    .set<Component::GuiDragSourceSlot>({
                        .payload    = flecs::entity{},
                        .materialId = iconMatId,
                    });
            }

            // ── Item name text ────────────────────────────────────────────────
            {
                auto p = world.entity(("InvName_P" + si).c_str()).child_of(listE);
                p.add<TransformID>();
                Transform t;
                t.position = vec3(kListX + 52.f, kListY + baseY + 33.f, 0.22f);
                t.scale    = vec3(0.28125f);  // 18 px
                p.set<Transform>(t);
                p.set<Component::GuiScrollItem>({ .baseY = baseY, .offsetX = 52.f, .offsetY = 33.f });

                const uint32_t nameLen = static_cast<uint32_t>(std::strlen(item.name)) + 1;
                world.entity(("InvName_T" + si).c_str()).add(flecs::ChildOf, p)
                    .set<GuiText>({
                        .text       = item.name,
                        .fontID     = fontID,
                        .pipelineID = textPipeID,
                        .maxLetters = nameLen,
                        .clipRect   = clip,
                    });
            }

            // ── Item type text ────────────────────────────────────────────────
            {
                auto p = world.entity(("InvType_P" + si).c_str()).child_of(listE);
                p.add<TransformID>();
                Transform t;
                t.position = vec3(kListX + 214.f, kListY + baseY + 34.f, 0.22f);
                t.scale    = vec3(0.21875f);  // 14 px
                p.set<Transform>(t);
                p.set<Component::GuiScrollItem>({ .baseY = baseY, .offsetX = 214.f, .offsetY = 34.f });

                const uint32_t typeLen = static_cast<uint32_t>(std::strlen(item.type)) + 1;
                world.entity(("InvType_T" + si).c_str()).add(flecs::ChildOf, p)
                    .set<GuiText>({
                        .text       = item.type,
                        .fontID     = fontID,
                        .pipelineID = textPipeID,
                        .maxLetters = typeLen,
                        .clipRect   = clip,
                    });
            }

            // ── Stack count text ──────────────────────────────────────────────
            {
                auto p = world.entity(("InvQty_P" + si).c_str()).child_of(listE);
                p.add<TransformID>();
                Transform t;
                t.position = vec3(kListX + 366.f, kListY + baseY + 33.f, 0.22f);
                t.scale    = vec3(0.28125f);  // 18 px
                p.set<Transform>(t);
                p.set<Component::GuiScrollItem>({ .baseY = baseY, .offsetX = 366.f, .offsetY = 33.f });

                const std::string qtyStr = "x" + std::to_string(item.stack);
                world.entity(("InvQty_T" + si).c_str()).add(flecs::ChildOf, p)
                    .set<GuiText>({
                        .text       = qtyStr,
                        .fontID     = fontID,
                        .pipelineID = textPipeID,
                        .maxLetters = static_cast<uint32_t>(qtyStr.size()) + 1,
                        .clipRect   = clip,
                    });
            }
        }

        // ── Scrollbar thumb update system (visual only, runs per frame) ───────
        // Captures listE and thumbP so it can read the list scroll state and
        // reposition the thumb transform accordingly.
        const float kTrackH    = kListH;
        const float kThumbH    = thumbH;
        const float kThumbMinY = kListY;

        world.system<const Component::GuiScrollList>("InvScrollBarUpdate")
            .each([listE, thumbP, kTrackH, kThumbH, kThumbMinY]
                  (flecs::entity e, const Component::GuiScrollList& list)
            {
                if (e != listE) return;

                const float maxScroll = std::max(0.f, list.contentHeight - list.size.y);
                const float thumbRange = kTrackH - kThumbH;
                const float fraction   = maxScroll > 0.f
                    ? std::min(1.f, list.scrollOffset / maxScroll)
                    : 0.f;

                auto& t = thumbP.ensure<Component::Transform>();
                t.position.y = kThumbMinY + fraction * thumbRange;
            })
            .add<Component::PersistentSystem>();


        auto model = world.entity("cube");
        model.set(Component::Transform{glm::vec3(0.0f, 0.0f, 0.0f), 0.0f, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f)});
        model.add<Component::TransformID>();
        model.add<Component::Static>();

        auto cube = world.entity("cubeMesh");
        cube.add(flecs::ChildOf, model);
        cube.set(Component::Mesh{
                    Core::GetAssetManager().getMeshId("cube"),
                    static_cast<uint32_t>(getTextureMaterialId("asset/Armour_1.png")),
                    Core::GetRenderer().getMeshPipelineId("basic")
                });

    }
};
