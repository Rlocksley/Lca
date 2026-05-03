#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "Transform.hpp"
#include "TransformID.hpp"
#include "Input.hpp"
#include "Time.hpp"
#include "Persistent.hpp"
#include <cmath>
#include <algorithm>

// ──────────────────────────────────────────────────────────────────────────────
// GuiScrollList / GuiScrollItem
//
// A general-purpose scrollable list for the GUI layer.
//
// Usage
// ──────
// 1. Create a "list container" entity and add Component::GuiScrollList.
//    This entity has NO Transform/TransformID of its own — it is purely a
//    controller/state holder.
//
//        auto listE = world.entity("MyList");
//        listE.set<Component::GuiScrollList>({
//            .position      = {100.f, 200.f},   // viewport top-left (pixels)
//            .size          = {400.f, 500.f},    // viewport width × height
//            .contentHeight = N * rowHeight,     // total scrollable content
//            .scrollSpeed   = 40.f,
//            .wrapAround    = false,
//        });
//
// 2. For each row, create ONE OR MORE "item transform" entities that are
//    direct children of the list entity, each carrying Component::GuiScrollItem:
//
//        auto rowBg = world.entity().child_of(listE);
//        rowBg.add<Component::TransformID>();
//        rowBg.set<Component::Transform>({
//            .position = {listX + 0, listY + rowI * rowH, zBg},
//            .scale    = {1.f, 1.f, 1.f},
//        });
//        rowBg.set<Component::GuiScrollItem>({
//            .baseY   = rowI * rowH,   // unscrolled Y in content space
//            .offsetX = 0.f,           // X offset from list.position.x
//            .offsetY = 0.f,           // additional Y offset within the row
//        });
//        // attach a GuiRect child as usual
//        auto bg = world.entity().child_of(rowBg);
//        bg.set<Component::GuiRect>({ .rect={0,0,400,rowH-2}, .clipRect=listClip, ... });
//
// 3. Set .clipRect on every GuiRect / GuiText to the list's viewport bounds so
//    the shaders discard fragments that scroll outside the visible area.
//
// Scrolling
// ──────────
// The Module::GuiScrollList system runs in PreUpdate (before the TransformID
// Non-Static-Update system) so transform changes are visible within the same frame.
// Mouse-wheel scrolling is detected when the cursor is inside the viewport.
// ──────────────────────────────────────────────────────────────────────────────

namespace Lca {

    namespace Component {

        // ── Scrollable viewport / state ──────────────────────────────────────
        struct GuiScrollList {
            glm::vec2 position;                // top-left of the visible viewport (screen px)
            glm::vec2 size;                    // width × height of the visible viewport
            float scrollOffset{0.f};           // current rendered scroll position (smoothed)
            float targetScrollOffset{0.f};     // desired scroll position (set by wheel input)
            float contentHeight{0.f};          // total height of all content rows
            float scrollSpeed{40.f};           // pixels added to target per wheel tick
            float smoothing{12.f};             // exponential decay rate (higher = snappier)
            bool  wrapAround{false};           // true = circular; false = clamped
        };

        // ── Row / sub-element position descriptor ────────────────────────────
        // Attach this to every entity whose Transform the scroll system should
        // drive.  All such entities must be DIRECT children of the list entity
        // (added via .child_of(listEntity) or .add(flecs::ChildOf, listEntity)).
        struct GuiScrollItem {
            float baseY{0.f};    // row's Y in list content space (px from content top)
            float offsetX{0.f};  // X offset from list.position.x  (px)
            float offsetY{0.f};  // additional Y offset within the row (px)
        };

    }  // namespace Component

    namespace Module {

        struct GuiScrollList {
            GuiScrollList(flecs::world& world) {

                world.component<Lca::Component::GuiScrollList>();
                world.component<Lca::Component::GuiScrollItem>();

                // Run in PreUpdate so that the OnUpdate "Non Static Transform Update"
                // system (registered in Module::TransformID) picks up our Transform
                // writes in the same frame.
                world.system<Lca::Component::GuiScrollList>("GuiScrollListSystem")
                    .kind(flecs::PreUpdate)
                    .each([](flecs::entity listEntity,
                             Lca::Component::GuiScrollList& list)
                    {
                        // ── Mouse-wheel scroll when cursor is inside viewport ──
                        const glm::vec2 cursor = Lca::Input::cursor.position;
                        const bool hovered =
                            cursor.x >= list.position.x &&
                            cursor.x <= list.position.x + list.size.x &&
                            cursor.y >= list.position.y &&
                            cursor.y <= list.position.y + list.size.y;

                        if (hovered && std::abs(Lca::Input::scroll.deltaScroll) > 0.001f) {
                            list.targetScrollOffset -= Lca::Input::scroll.deltaScroll * list.scrollSpeed;
                        }

                        // ── Clamp or wrap the TARGET ───────────────────────────
                        const float maxScroll =
                            std::max(0.f, list.contentHeight - list.size.y);

                        if (list.wrapAround && list.contentHeight > 0.f) {
                            const float c = list.contentHeight;
                            list.targetScrollOffset =
                                std::fmod(list.targetScrollOffset + c * 4096.f, c);
                        } else {
                            list.targetScrollOffset =
                                std::max(0.f, std::min(list.targetScrollOffset, maxScroll));
                        }

                        // ── Smooth scrollOffset toward target (frame-rate independent) ──
                        // Exponential decay: gap halves every 1/smoothing seconds.
                        const float dt     = Lca::Time::deltaTime;
                        const float alpha  = 1.f - std::exp(-list.smoothing * dt);
                        list.scrollOffset += (list.targetScrollOffset - list.scrollOffset) * alpha;

                        // Snap to target when visually indistinguishable (< 0.1 px)
                        if (std::abs(list.targetScrollOffset - list.scrollOffset) < 0.1f) {
                            list.scrollOffset = list.targetScrollOffset;
                        }

                        // ── Update all GuiScrollItem child transforms ──────────
                        // Only DIRECT children of listEntity are iterated.
                        listEntity.children([&](flecs::entity child) {
                            if (!child.has<Lca::Component::GuiScrollItem>()) return;
                            if (!child.has<Lca::Component::Transform>())     return;

                            const auto item =
                                child.get<Lca::Component::GuiScrollItem>();
                            auto& t =
                                child.ensure<Lca::Component::Transform>();

                            // Update screen-space X and Y; leave Z (depth) intact.
                            t.position.x =
                                list.position.x + item.offsetX;
                            t.position.y =
                                list.position.y + item.baseY + item.offsetY
                                - list.scrollOffset;
                        });
                    })
                    .add<Lca::Component::PersistentSystem>();

            }
        };

    }  // namespace Module

}  // namespace Lca
