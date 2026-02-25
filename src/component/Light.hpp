#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "Renderer.hpp"
#include "Transform.hpp"
#include "TransformID.hpp"
#include "Persistent.hpp"
#include "Hidden.hpp"

namespace Lca {
    namespace Component {

        // -------------------------------------------------------
        // Light Components  (user-facing, friendly fields)
        // -------------------------------------------------------

        struct PointLight {
            glm::vec3 color{1.0f};
            float     intensity{1.0f};
            float     radius{10.0f};
        };

        struct SpotLight {
            glm::vec3 color{1.0f};
            float     intensity{1.0f};
            float     radius{10.0f};
            float     innerCutoffDeg{25.0f};
            float     outerCutoffDeg{35.0f};
        };

        struct DirectionalLight {
            glm::vec3 color{1.0f};
            float     intensity{1.0f};
        };

        // Internal bookkeeping — holds the slot ID returned by the Renderer.
        struct LightID {
            uint32_t id{UINT32_MAX};
        };
    }

    namespace Module {

        // -------------------------------------------------------
        // Helper: build a Core::Light from component data + transform
        // -------------------------------------------------------
        namespace detail {

            inline Core::Light makePointLight(const Component::PointLight& pl,
                                              const Component::Transform& t) {
                Core::Light l{};
                l.position  = glm::vec4(t.position, Core::LIGHT_TYPE_POINT);
                l.direction = glm::vec4(0.0f);
                l.color     = glm::vec4(pl.color, pl.intensity);
                l.params    = glm::vec4(pl.radius, 0.0f, 0.0f, 0.0f);
                return l;
            }

            inline Core::Light makeSpotLight(const Component::SpotLight& sl,
                                             const Component::Transform& t) {
                Core::Light l{};
                l.position  = glm::vec4(t.position, Core::LIGHT_TYPE_SPOT);
                // Forward direction from the transform's rotation
                glm::vec3 dir = t.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
                l.direction = glm::vec4(glm::normalize(dir), 0.0f);
                l.color     = glm::vec4(sl.color, sl.intensity);
                l.params    = glm::vec4(sl.radius,
                                        glm::cos(glm::radians(sl.innerCutoffDeg)),
                                        glm::cos(glm::radians(sl.outerCutoffDeg)),
                                        0.0f);
                return l;
            }

            inline Core::Light makeDirectionalLight(const Component::DirectionalLight& dl,
                                                    const Component::Transform& t) {
                Core::Light l{};
                l.position  = glm::vec4(0.0f, 0.0f, 0.0f, Core::LIGHT_TYPE_DIRECTIONAL);
                glm::vec3 dir = t.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
                l.direction = glm::vec4(glm::normalize(dir), 0.0f);
                l.color     = glm::vec4(dl.color, dl.intensity);
                l.params    = glm::vec4(0.0f);  // radius = 0 → infinite
                return l;
            }
        }

        // -------------------------------------------------------
        // Module: registers components, observers, and systems
        // -------------------------------------------------------
        struct Light {
            Light(flecs::world& world) {
                world.component<Component::PointLight>();
                world.component<Component::SpotLight>();
                world.component<Component::DirectionalLight>();
                world.component<Component::LightID>();

                // ==================================================
                //  POINT LIGHT
                // ==================================================

                // OnSet: add or update the light in the renderer
                world.observer<const Component::PointLight, const Component::Transform>()
                    .event(flecs::OnSet)
                    .each([](flecs::entity e,
                             const Component::PointLight& pl,
                             const Component::Transform& t) {
                        if (e.has<Component::Hidden>()) return;

                        Core::Light light = detail::makePointLight(pl, t);

                        if (e.has<Component::LightID>()) {
                            auto lid = e.get<Component::LightID>();
                            Core::GetRenderer().updateLight(lid.id, light);
                        } else {
                            uint32_t id = Core::GetRenderer().addLight(light);
                            e.set<Component::LightID>({id});
                        }
                    })
                    .add<Component::PersistentSystem>();

                // OnRemove
                world.observer<const Component::PointLight>()
                    .event(flecs::OnRemove)
                    .each([](flecs::entity e, const Component::PointLight&) {
                        if (e.has<Component::LightID>()) {
                            Core::GetRenderer().removeLight(e.get<Component::LightID>().id);
                            e.remove<Component::LightID>();
                        }
                    })
                    .add<Component::PersistentSystem>();

                // ==================================================
                //  SPOT LIGHT
                // ==================================================

                world.observer<const Component::SpotLight, const Component::Transform>()
                    .event(flecs::OnSet)
                    .each([](flecs::entity e,
                             const Component::SpotLight& sl,
                             const Component::Transform& t) {
                        if (e.has<Component::Hidden>()) return;

                        Core::Light light = detail::makeSpotLight(sl, t);

                        if (e.has<Component::LightID>()) {
                            auto lid = e.get<Component::LightID>();
                            Core::GetRenderer().updateLight(lid.id, light);
                        } else {
                            uint32_t id = Core::GetRenderer().addLight(light);
                            e.set<Component::LightID>({id});
                        }
                    })
                    .add<Component::PersistentSystem>();

                world.observer<const Component::SpotLight>()
                    .event(flecs::OnRemove)
                    .each([](flecs::entity e, const Component::SpotLight&) {
                        if (e.has<Component::LightID>()) {
                            Core::GetRenderer().removeLight(e.get<Component::LightID>().id);
                            e.remove<Component::LightID>();
                        }
                    })
                    .add<Component::PersistentSystem>();

                // ==================================================
                //  DIRECTIONAL LIGHT
                // ==================================================

                world.observer<const Component::DirectionalLight, const Component::Transform>()
                    .event(flecs::OnSet)
                    .each([](flecs::entity e,
                             const Component::DirectionalLight& dl,
                             const Component::Transform& t) {
                        if (e.has<Component::Hidden>()) return;

                        Core::Light light = detail::makeDirectionalLight(dl, t);

                        if (e.has<Component::LightID>()) {
                            auto lid = e.get<Component::LightID>();
                            Core::GetRenderer().updateLight(lid.id, light);
                        } else {
                            uint32_t id = Core::GetRenderer().addLight(light);
                            e.set<Component::LightID>({id});
                        }
                    })
                    .add<Component::PersistentSystem>();

                world.observer<const Component::DirectionalLight>()
                    .event(flecs::OnRemove)
                    .each([](flecs::entity e, const Component::DirectionalLight&) {
                        if (e.has<Component::LightID>()) {
                            Core::GetRenderer().removeLight(e.get<Component::LightID>().id);
                            e.remove<Component::LightID>();
                        }
                    })
                    .add<Component::PersistentSystem>();

                // ==================================================
                //  Per-frame systems: update moving lights + GPU copy
                // ==================================================

                // Update non-static point lights from their transform
                world.system<const Component::PointLight,
                             const Component::Transform,
                             const Component::LightID>("Non Static Point Light Update")
                    .without<Component::Static>()
                    .without<Component::Hidden>()
                    .each([](flecs::entity e,
                             const Component::PointLight& pl,
                             const Component::Transform& t,
                             const Component::LightID& lid) {
                        Core::GetRenderer().updateLight(
                            lid.id, detail::makePointLight(pl, t));
                    })
                    .add<Component::PersistentSystem>();

                // Update non-static spot lights
                world.system<const Component::SpotLight,
                             const Component::Transform,
                             const Component::LightID>("Non Static Spot Light Update")
                    .without<Component::Static>()
                    .without<Component::Hidden>()
                    .each([](flecs::entity e,
                             const Component::SpotLight& sl,
                             const Component::Transform& t,
                             const Component::LightID& lid) {
                        Core::GetRenderer().updateLight(
                            lid.id, detail::makeSpotLight(sl, t));
                    })
                    .add<Component::PersistentSystem>();

                // Update non-static directional lights
                world.system<const Component::DirectionalLight,
                             const Component::Transform,
                             const Component::LightID>("Non Static Directional Light Update")
                    .without<Component::Static>()
                    .without<Component::Hidden>()
                    .each([](flecs::entity e,
                             const Component::DirectionalLight& dl,
                             const Component::Transform& t,
                             const Component::LightID& lid) {
                        Core::GetRenderer().updateLight(
                            lid.id, detail::makeDirectionalLight(dl, t));
                    })
                    .add<Component::PersistentSystem>();

                // ==================================================
                //  Hidden handling: remove/re-add lights
                // ==================================================

                world.observer<const Component::Hidden>()
                    .event(flecs::OnAdd)
                    .each([](flecs::entity e, const Component::Hidden&) {
                        if (e.has<Component::LightID>()) {
                            Core::GetRenderer().removeLight(e.get<Component::LightID>().id);
                            e.remove<Component::LightID>();
                        }
                    })
                    .add<Component::PersistentSystem>();

                world.observer<const Component::Hidden>()
                    .event(flecs::OnRemove)
                    .each([](flecs::entity e, const Component::Hidden&) {
                        if (e.has<Component::Transform>()) {
                            if (e.has<Component::PointLight>())
                                e.modified<Component::PointLight>();
                            else if (e.has<Component::SpotLight>())
                                e.modified<Component::SpotLight>();
                            else if (e.has<Component::DirectionalLight>())
                                e.modified<Component::DirectionalLight>();
                        }
                    })
                    .add<Component::PersistentSystem>();

                // ==================================================
                //  GPU Copy (runs after all per-frame updates)
                // ==================================================
                world.system("Copy Lights to GPU")
                    .each([]() {
                        Core::GetRenderer().copyLightsToGPU(Core::currentFrameIndex);
                    })
                    .add<Component::PersistentSystem>();
            }
        };
    }
}
