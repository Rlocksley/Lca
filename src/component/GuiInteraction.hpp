#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "Input.hpp"
#include "GuiRect.hpp"
#include "Transform.hpp"
#include "TransformID.hpp"
#include "Renderer.hpp"

namespace Lca{
    namespace Component{

        struct GuiInteraction{
            bool hovered{false};
            bool pressed{false};      // button held down while hovered
            bool clicked{false};       // rising edge: pressed this frame
            bool released{false};      // falling edge: released this frame while hovered
        };
    }

    namespace Module{
        struct GuiInteraction{
            GuiInteraction(flecs::world& world){
                world.component<Lca::Component::GuiInteraction>();

                // System runs each frame, tests cursor against GuiRect AABB
                world.system<Lca::Component::GuiInteraction, const Lca::Component::GuiRect, const Lca::Component::Transform, const Lca::Component::TransformID>()
                    .term_at(2).parent()
                    .term_at(3).parent()
                    .each([](flecs::entity e, Lca::Component::GuiInteraction& interaction,
                             const Lca::Component::GuiRect& rect,
                             const Lca::Component::Transform& transform,
                             const Lca::Component::TransformID& transformID) {

                        // Compute screen-space AABB from parent position + rect offset/size
                        glm::vec2 parentPos = glm::vec2(transform.position.x, transform.position.y);
                        glm::vec2 rectMin = parentPos + glm::vec2(rect.rect.x, rect.rect.y);
                        glm::vec2 rectMax = rectMin + glm::vec2(rect.rect.z, rect.rect.w);

                        glm::vec2 cursor = Input::cursor.position;
                        bool wasPressed = interaction.pressed;

                        interaction.hovered = (cursor.x >= rectMin.x && cursor.x <= rectMax.x &&
                                              cursor.y >= rectMin.y && cursor.y <= rectMax.y);

                        interaction.clicked = false;
                        interaction.released = false;

                        if(interaction.hovered){
                            if(Input::buttonLeft.pressed && !wasPressed){
                                interaction.clicked = true;
                            }
                            interaction.pressed = Input::buttonLeft.down;
                            if(wasPressed && !Input::buttonLeft.down){
                                interaction.released = true;
                            }
                        } else {
                            interaction.pressed = false;
                        }
                    })
                    .add<Lca::Component::PersistentSystem>();
            }
        };
    }
}
