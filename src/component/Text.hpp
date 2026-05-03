#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "TransformID.hpp"
#include "Transform.hpp"
#include "Renderer.hpp"
#include "Hidden.hpp"

namespace Lca{
    namespace Component{

        struct Text{
            std::string text;
            uint32_t fontID;
            uint32_t pipelineID;
            glm::mat4 localTransform{glm::mat4(1.0f)};
            uint32_t maxLetters{64};
        };

        struct TextRender{
            uint32_t textInstanceID;
        };
    }

    namespace Module{
        struct Text{
            Text(flecs::world& world){
                world.component<Lca::Component::Text>();

                world.observer<const Lca::Component::Text, const Lca::Component::Transform, const Lca::Component::TransformID>()
                    .term_at(1).parent()
                    .term_at(2).parent()
                    .event(flecs::OnSet)
                    .each([](flecs::entity e, const Lca::Component::Text& text, const Lca::Component::Transform& transform, const Lca::Component::TransformID& transformID) {
                        if(e.has<Lca::Component::Hidden>()) return;

                        Core::TextInstance instance;
                        instance.text           = text.text;
                        instance.fontID         = text.fontID;
                        instance.pipelineID     = text.pipelineID;
                        instance.transformID    = transformID.id;
                        instance.localTransform = text.localTransform;

                        if(e.has<Lca::Component::TextRender>()){
                            const auto& tr = e.get<Lca::Component::TextRender>();
                            Core::GetRenderer().updateTextInstance(tr.textInstanceID, instance);
                        }
                        else{
                            uint32_t id = Core::GetRenderer().addTextInstance(instance, text.maxLetters);
                            e.set<Lca::Component::TextRender>({ id });
                        }
                    })
                    .add<Lca::Component::PersistentSystem>();

                world.observer<const Lca::Component::Text>()
                    .event(flecs::OnRemove)
                    .each([](flecs::entity e, const Lca::Component::Text& text) {
                        if(e.has<Lca::Component::TextRender>()){
                            const auto& tr = e.get<Lca::Component::TextRender>();
                            Core::GetRenderer().removeTextInstance(tr.textInstanceID);
                            e.remove<Lca::Component::TextRender>();
                        }
                    })
                    .add<Lca::Component::PersistentSystem>();

                world.observer<const Lca::Component::Hidden>()
                    .event(flecs::OnAdd)
                    .each([](flecs::entity e, const Lca::Component::Hidden&) {
                        if(e.has<Lca::Component::TextRender>()){
                            const auto& tr = e.get<Lca::Component::TextRender>();
                            Core::GetRenderer().removeTextInstance(tr.textInstanceID);
                            e.remove<Lca::Component::TextRender>();
                        }
                    })
                    .add<Lca::Component::PersistentSystem>();

                world.observer<const Lca::Component::Hidden>()
                    .event(flecs::OnRemove)
                    .each([](flecs::entity e, const Lca::Component::Hidden&) {
                        if(e.has<Lca::Component::Text>()){
                            e.modified<Lca::Component::Text>();
                        }
                    })
                    .add<Lca::Component::PersistentSystem>();
            }
        };
    }
}
