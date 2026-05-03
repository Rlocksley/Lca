#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "Input.hpp"
#include "GuiInteraction.hpp"
#include "GuiRect.hpp"

namespace Lca{
    namespace Component{

        // Tag: slot can spawn drag payload/icon as drag source.
        struct GuiDragSourceSlot{
            flecs::entity payload;    // Entity to copy into payload when drag starts.
            int32_t materialId{-1};
        };

        struct GuiDragDropTarget{};
        // Tag: slot can receive dropped payload as drag target.
        struct GuiDragDropTargetSlot{
            flecs::entity payload;
        };

        // Singleton: tracks the active drag-and-drop operation
        struct GuiDragState{
            bool active{false};
            flecs::entity dragSourceSlot{};
            flecs::entity dragIcon{};
            flecs::entity draggedPayload{};
            int32_t draggedMaterialId{-1};
            flecs::entity hoveredTargetSlot{};
            bool justStarted{false};
        };
    }

    namespace Module{
        struct GuiDragDrop{
            GuiDragDrop(flecs::world& world){
                world.component<Lca::Component::GuiDragSourceSlot>();
                world.component<Lca::Component::GuiDragDropTarget>();
                world.component<Lca::Component::GuiDragDropTargetSlot>();
                world.component<Lca::Component::GuiDragState>();

                // Initialize singleton
                world.set<Lca::Component::GuiDragState>({});

                // Build queries for multi-component iteration
                auto dragQ = world.query_builder<Lca::Component::GuiDragSourceSlot, Lca::Component::GuiInteraction>()
                    .build();

                auto dropQ = world.query_builder<Lca::Component::GuiInteraction>()
                    .with<Lca::Component::GuiDragDropTarget>()
                    .build();

                // System: detect drag start from source slots, spawn/track icon, assign to target slots.
                world.system("GuiDragDropSystem")
                    .run([dragQ, dropQ](flecs::iter& it){
                        while(it.next()){
                            auto& state = it.world().ensure<Lca::Component::GuiDragState>();

                
                            if(!state.active){
                                // Not dragging: source slot click starts a drag payload/icon operation.
                                dragQ.each([&](flecs::entity e, Lca::Component::GuiDragSourceSlot sourceSlot,
                                                                Lca::Component::GuiInteraction interaction){
                                    if(interaction.clicked){

                                        state.active = true;
                                        state.dragSourceSlot = e;
                                        state.draggedPayload = sourceSlot.payload;
                                        state.draggedMaterialId = sourceSlot.materialId;
                                        state.hoveredTargetSlot = flecs::entity{};

                                        // Spawn a transient icon entity that carries copied payload and visual.
                                        auto icon = it.world().entity("DragIcon");
                                        icon.add<Lca::Component::TransformID>();
                                        Lca::Component::Transform iconTransform;
                                        iconTransform.position = glm::vec3(Input::cursor.position, 0.5f);
                                        icon.set<Lca::Component::Transform>(iconTransform);
                                        
                                        auto iconRect = it.world().entity("DragIconRect");
                                        iconRect.add(flecs::ChildOf, icon);
                                        Lca::Component::GuiRect iconRectComp;
                                        iconRectComp.rect = {0.f, 0.f, 64.f, 64.f};
                                        iconRectComp.materialId = sourceSlot.materialId;
                                        iconRectComp.pipelineID = e.has<Lca::Component::GuiRect>()
                                            ? e.get<Lca::Component::GuiRect>().pipelineID : 0u;
                                        iconRect.set<Lca::Component::GuiRect>(iconRectComp);

                                        state.dragIcon = icon;
                                    
                                    }
                                });
                            }
                            else{
                                // Currently dragging — track hovered target slot.
                                state.hoveredTargetSlot = flecs::entity{};

                                dropQ.each([&](flecs::entity e, Lca::Component::GuiInteraction interaction){
                                    if(interaction.hovered){
                                        state.hoveredTargetSlot = e;
                                    }
                                });

                                // Detect drop (mouse released) and assign payload + texture to target slot.
                                if(!Input::buttonLeft.down){
                                    if(state.hoveredTargetSlot.is_alive()){
                                        state.hoveredTargetSlot.parent().set<Lca::Component::GuiDragDropTargetSlot>({ .payload = state.draggedPayload });
                                        auto rect = state.hoveredTargetSlot.get<Lca::Component::GuiRect>();
                                        rect.materialId = state.draggedMaterialId;
                                        state.hoveredTargetSlot.set<Lca::Component::GuiRect>(rect);
                                    }

                                    if(state.dragIcon.is_alive()){
                                        state.dragIcon.children([&](flecs::entity e){
                                            e.destruct();
                                        });
                                        state.dragIcon.destruct();
                                    }

                                    state.active = false;
                                    state.dragSourceSlot = flecs::entity{};
                                    state.dragIcon = flecs::entity{};
                                    state.draggedPayload = flecs::entity{};
                                    state.draggedMaterialId = -1;
                                    state.hoveredTargetSlot = flecs::entity{};
                                }else{
                                    Lca::Component::Transform dragTransform;
                                    dragTransform.position = glm::vec3(Input::cursor.position, 0.5f);
                                    state.dragIcon.set<Lca::Component::Transform>(dragTransform);
                                }
                            }
                        }
                    })
                    .add<Lca::Component::PersistentSystem>();
            }
        };
    }
}
