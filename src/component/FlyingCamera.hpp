#pragma once

#include "Global.hpp"
#include "Time.hpp"
#include "Input.hpp"
#include "Renderer.hpp"
#include "Transform.hpp"

namespace Lca
{
    namespace Component
    {
        struct FlyingCamera
        {
            glm::vec3 position;
            glm::vec2 angle;
            float speed;
            float rotationSpeed;
            float fov;
            float nearClip;
            float farClip;
            glm::vec3 direction;

        public:
            void update(Lca::Component::Transform& transform)
            {
                // apply mouse look
                angle += Input::cursor.deltaPosition * (1.f * Input::buttonRight.down) * rotationSpeed;

                // compute forward direction from yaw (angle.x) and pitch (angle.y)
                direction = (glm::angleAxis(angle[0], glm::vec3(0.0f, -1.0f, 0.0f)) * glm::angleAxis(angle[1], glm::vec3(1.0f, 0.0f, 0.0f))) *
                            glm::vec3(0.0f, 0.0f, -1.0f);

                // movement input
                glm::vec3 deltaPosition = direction * (1.f * (Input::keyW.down - Input::keyS.down));
                glm::vec3 right = glm::cross(direction, glm::vec3(0.f, 1.f, 0.f));
                deltaPosition += right * (1.f * (Input::keyD.down - Input::keyA.down));

                if (deltaPosition.length() > 0.01f) {
                    glm::vec3 move = deltaPosition * (1.f / deltaPosition.length()) * speed * Time::deltaTime;
                    position += move;
                }

                transform.position = position;
                // update transform rotation to match camera angles
                transform.rotation = glm::angleAxis(angle[0], glm::vec3(0.0f, -1.0f, 0.0f)) * glm::angleAxis(angle[1], glm::vec3(1.0f, 0.0f, 0.0f));

                Core::GetRenderer().updateCamera(Core::currentFrameIndex, transform.position, direction, fov, nearClip, farClip);
            }
            // new static lookAt()
            static glm::vec2 lookAt(const glm::vec3& position,
                                    const glm::vec3& viewTarget)
            {
                // 1) direction from cam → target
                glm::vec3 dir = glm::normalize(viewTarget - position);

                // 2) yaw  = rotation around Y so that forward (-Z) points towards dir
                //TBD test if -Z or +Z is forward and adjust accordingly
                float yaw   = std::atan2(dir.x, -dir.z);

                // 3) pitch = rotation around X so that forward (in Y) points towards dir
                float horizontalDist = std::sqrt(dir.x*dir.x + dir.z*dir.z);
                float pitch = std::atan2(dir.y, horizontalDist);

                return glm::vec2(yaw, pitch);
            }   
        };
    }

    namespace Module{
        struct FlyingCamera{
            FlyingCamera(flecs::world& world){
                world.component<Lca::Component::FlyingCamera>();

                world.system<Lca::Component::FlyingCamera, Lca::Component::Transform>("Flying Camera Update")
                    .without<Lca::Component::Hidden>()
                    .each([](flecs::entity e, Lca::Component::FlyingCamera& camera, Lca::Component::Transform& transform) {
                        camera.update(transform);
                    })
                    .add<Lca::Component::PersistentSystem>();
            }
        };
    }
}