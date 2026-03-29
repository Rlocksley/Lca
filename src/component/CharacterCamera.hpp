#pragma once

#include "Global.hpp"
#include "Time.hpp"
#include "Input.hpp"
#include "Renderer.hpp"
#include "Transform.hpp"
#include "Hidden.hpp"
#include "Persistent.hpp"

namespace Lca
{
    namespace Component
    {
        struct CharacterCamera
        {
            flecs::entity target;

            float yaw   = 0.0f;
            float pitch = 0.15f;

            float distance    = 6.0f;
            float minDistance  = 2.0f;
            float maxDistance  = 20.0f;
            float zoomSpeed   = 2.0f;

            float rotationSpeed = 0.005f;
            float fov       = glm::radians(60.0f);
            float nearClip  = 0.1f;
            float farClip   = 500.0f;

            glm::vec3 targetOffset = glm::vec3(0.0f, 1.5f, 0.0f);

        public:
            void update(Component::Transform& cameraTransform)
            {
                if (!target.is_alive()) return;
                if (!target.has<Component::Transform>()) return;
                const Component::Transform& targetTransform = target.get<Component::Transform>();

                // Rotate only while right mouse button is held
                if (Input::buttonRight.down) {
                    yaw   -= Input::cursor.deltaPosition.x * rotationSpeed;
                    pitch -= Input::cursor.deltaPosition.y * rotationSpeed;
                }

                // Clamp pitch to avoid flipping
                const float maxPitch = glm::radians(89.0f);
                pitch = glm::clamp(pitch, -maxPitch, maxPitch);

                // Scroll wheel zoom
                distance -= Input::scroll.deltaScroll * zoomSpeed;
                distance  = glm::clamp(distance, minDistance, maxDistance);

                // Quaternion rotation: yaw around world Y, then pitch around local X
                glm::quat qYaw   = glm::angleAxis(yaw,   glm::vec3(0.0f, 1.0f, 0.0f));
                glm::quat qPitch = glm::angleAxis(pitch,  glm::vec3(1.0f, 0.0f, 0.0f));
                glm::quat orientation = qYaw * qPitch;

                // Camera sits behind the target along -Z in camera space
                glm::vec3 offset = orientation * glm::vec3(0.0f, 0.0f, -distance);

                glm::vec3 focusPoint = targetTransform.position + targetOffset;
                glm::vec3 camPos = focusPoint + offset;

                // Forward direction: camera looks at target
                glm::vec3 direction = glm::normalize(focusPoint - camPos);

                cameraTransform.position = camPos;
                cameraTransform.rotation = orientation;

                Core::GetRenderer().updateCamera(
                    Core::currentFrameIndex, camPos, direction, fov, nearClip, farClip);
            }
        };
    }

    namespace Module {
        struct CharacterCamera {
            CharacterCamera(flecs::world& world) {
                world.component<Lca::Component::CharacterCamera>();

                world.system<Lca::Component::CharacterCamera, Lca::Component::Transform>(
                    "Character Camera Update")
                    .without<Lca::Component::Hidden>()
                    .each([](flecs::entity e,
                             Lca::Component::CharacterCamera& camera,
                             Lca::Component::Transform& transform) {
                        camera.update(transform);
                    })
                    .add<Lca::Component::PersistentSystem>();
            }
        };
    }
}
