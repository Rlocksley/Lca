#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "TransformID.hpp"
#include "Transform.hpp"
#include "Renderer.hpp"
#include "Hidden.hpp"

namespace Lca{
    namespace Component{

        struct GuiRect{
            glm::vec4 rect{0.0f};           // x, y, width, height (pixels, local to parent)
            glm::vec4 clipRect{0.0f, 0.0f, 99999.0f, 99999.0f}; // minX, minY, maxX, maxY (screen pixels)
            float borderWidth{0.0f};
            float cornerRadius{0.0f};
            int32_t materialId{-1};         // fill: -1 = white, >= 0 = material texture
            int32_t borderMaterialId{-1};   // border: -1 = no border, >= 0 = material texture
            uint32_t pipelineID{0};
        };

        struct GuiRectRender{
            uint32_t rectInstanceID;
        };
    }

    namespace Module{
        struct GuiRect{
            GuiRect(flecs::world& world){
                world.component<Lca::Component::GuiRect>();

                world.observer<const Lca::Component::GuiRect, const Lca::Component::Transform, const Lca::Component::TransformID>()
                    .term_at(1).parent()
                    .term_at(2).parent()
                    .event(flecs::OnSet)
                    .each([](flecs::entity e, const Lca::Component::GuiRect& rect, const Lca::Component::Transform& transform, const Lca::Component::TransformID& transformID) {
                        if(e.has<Lca::Component::Hidden>()) return;

                        Core::GuiRectInstance instance;
                        instance.rect           = rect.rect;
                        instance.clipRect       = rect.clipRect;
                        instance.borderWidth    = rect.borderWidth;
                        instance.cornerRadius   = rect.cornerRadius;
                        instance.materialId     = rect.materialId;
                        instance.borderMaterialId = rect.borderMaterialId;
                        instance.pipelineID     = rect.pipelineID;
                        instance.transformID    = transformID.id;

                        if(e.has<Lca::Component::GuiRectRender>()){
                            const auto& rr = e.get<Lca::Component::GuiRectRender>();
                            Core::GetRenderer().updateGuiRectInstance(rr.rectInstanceID, instance);
                        }
                        else{
                            uint32_t id = Core::GetRenderer().addGuiRectInstance(instance);
                            e.set<Lca::Component::GuiRectRender>({ id });
                        }
                    })
                    .add<Lca::Component::PersistentSystem>();

                world.observer<const Lca::Component::GuiRect>()
                    .event(flecs::OnRemove)
                    .each([](flecs::entity e, const Lca::Component::GuiRect& rect) {
                        if(e.has<Lca::Component::GuiRectRender>()){
                            const auto& rr = e.get<Lca::Component::GuiRectRender>();
                            Core::GetRenderer().removeGuiRectInstance(rr.rectInstanceID);
                            e.remove<Lca::Component::GuiRectRender>();
                        }
                    })
                    .add<Lca::Component::PersistentSystem>();

                world.observer<const Lca::Component::Hidden>()
                    .with<Lca::Component::GuiRect>()
                    .event(flecs::OnAdd)
                    .each([](flecs::entity e, const Lca::Component::Hidden&) {
                        if(e.has<Lca::Component::GuiRectRender>()){
                            const auto& rr = e.get<Lca::Component::GuiRectRender>();
                            Core::GetRenderer().removeGuiRectInstance(rr.rectInstanceID);
                            e.remove<Lca::Component::GuiRectRender>();
                        }
                    })
                    .add<Lca::Component::PersistentSystem>();

                world.observer<const Lca::Component::Hidden>()
                    .with<Lca::Component::GuiRect>()
                    .event(flecs::OnRemove)
                    .each([](flecs::entity e, const Lca::Component::Hidden&) {
                        if(e.has<Lca::Component::GuiRect>()){
                            e.modified<Lca::Component::GuiRect>();
                        }
                    })
                    .add<Lca::Component::PersistentSystem>();
            }
        };
    }
}
