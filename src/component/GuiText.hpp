#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "TransformID.hpp"
#include "Transform.hpp"
#include "Renderer.hpp"
#include "Hidden.hpp"

namespace Lca{
    namespace Component{

        struct GuiText{
            std::string text;
            uint32_t fontID;
            uint32_t pipelineID;
            glm::mat4 localTransform{glm::mat4(1.0f)};
            uint32_t maxLetters{64};
            glm::vec4 clipRect{0.0f, 0.0f, 99999.0f, 99999.0f}; // minX, minY, maxX, maxY (screen pixels)
        };

        struct GuiTextRender{
            uint32_t textInstanceID;
        };
    }

    namespace Module{
        struct GuiText{
            GuiText(flecs::world& world){
                world.component<Lca::Component::GuiText>();

                world.observer<const Lca::Component::GuiText, const Lca::Component::Transform, const Lca::Component::TransformID>()
                    .term_at(1).parent()
                    .term_at(2).parent()
                    .event(flecs::OnSet)
                    .each([](flecs::entity e, const Lca::Component::GuiText& text, const Lca::Component::Transform& transform, const Lca::Component::TransformID& transformID) {
                        if(e.has<Lca::Component::Hidden>()) return;

                        Core::TextInstance instance;
                        instance.text           = text.text;
                        instance.fontID         = text.fontID;
                        instance.pipelineID     = text.pipelineID;
                        instance.transformID    = transformID.id;
                        instance.localTransform = text.localTransform;
                        instance.clipRect       = text.clipRect;

                        if(e.has<Lca::Component::GuiTextRender>()){
                            const auto& tr = e.get<Lca::Component::GuiTextRender>();
                            Core::GetRenderer().updateGuiTextInstance(tr.textInstanceID, instance);
                        }
                        else{
                            uint32_t id = Core::GetRenderer().addGuiTextInstance(instance, text.maxLetters);
                            e.set<Lca::Component::GuiTextRender>({ id });
                        }
                    })
                    .add<Lca::Component::PersistentSystem>();

                world.observer<const Lca::Component::GuiText>()
                    .event(flecs::OnRemove)
                    .each([](flecs::entity e, const Lca::Component::GuiText& text) {
                        if(e.has<Lca::Component::GuiTextRender>()){
                            const auto& tr = e.get<Lca::Component::GuiTextRender>();
                            Core::GetRenderer().removeGuiTextInstance(tr.textInstanceID);
                            e.remove<Lca::Component::GuiTextRender>();
                        }
                    })
                    .add<Lca::Component::PersistentSystem>();

                world.observer<const Lca::Component::Hidden>()
                    .with<Lca::Component::GuiText>()
                    .event(flecs::OnAdd)
                    .each([](flecs::entity e, const Lca::Component::Hidden&) {
                        if(e.has<Lca::Component::GuiTextRender>()){
                            const auto& tr = e.get<Lca::Component::GuiTextRender>();
                            Core::GetRenderer().removeGuiTextInstance(tr.textInstanceID);
                            e.remove<Lca::Component::GuiTextRender>();
                        }
                    })
                    .add<Lca::Component::PersistentSystem>();

                world.observer<const Lca::Component::Hidden>()
                    .with<Lca::Component::GuiText>()
                    .event(flecs::OnRemove)
                    .each([](flecs::entity e, const Lca::Component::Hidden&) {
                        if(e.has<Lca::Component::GuiText>()){
                            e.modified<Lca::Component::GuiText>();
                        }
                    })
                    .add<Lca::Component::PersistentSystem>();
            }
        };
    }
}
