#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "GuiRect.hpp"
#include "GuiText.hpp"
#include "GuiInteraction.hpp"

namespace Lca{
    namespace Component{

        struct GuiButton{
            int32_t normalMaterialId{-1};   // fill material when idle
            int32_t hoverMaterialId{-1};    // fill material when hovered
            int32_t pressMaterialId{-1};    // fill material when pressed
        };

        // Tag applied to fire a one-shot observer when a button is clicked
        struct GuiClicked{};
    }

    namespace Module{
        struct GuiButton{
            GuiButton(flecs::world& world){
                world.component<Lca::Component::GuiButton>();
                world.component<Lca::Component::GuiClicked>();

                // System: update rect color based on interaction state
                world.system<const Lca::Component::GuiButton, const Lca::Component::GuiInteraction, Lca::Component::GuiRect>()
                    .each([](flecs::entity e,
                             const Lca::Component::GuiButton& button,
                             const Lca::Component::GuiInteraction& interaction,
                             Lca::Component::GuiRect& rect) {

                        int32_t newMaterialId;
                        if(interaction.pressed){
                            newMaterialId = button.pressMaterialId;
                        } else if(interaction.hovered){
                            newMaterialId = button.hoverMaterialId;
                        } else {
                            newMaterialId = button.normalMaterialId;
                        }

                        // Only trigger modified when the material actually changed
                        if(newMaterialId != rect.materialId){
                            rect.materialId = newMaterialId;
                            e.modified<Lca::Component::GuiRect>();
                        }

                        // Add GuiClicked tag on click (observers can react to this)
                        if(interaction.released){
                            e.add<Lca::Component::GuiClicked>();
                        }
                    })
                    .add<Lca::Component::PersistentSystem>();

                // Auto-remove GuiClicked tag after one frame so it's a one-shot event
                world.system<const Lca::Component::GuiClicked>()
                    .each([](flecs::entity e, const Lca::Component::GuiClicked&) {
                        e.remove<Lca::Component::GuiClicked>();
                    })
                    .add<Lca::Component::PersistentSystem>();
            }
        };
    }
}
