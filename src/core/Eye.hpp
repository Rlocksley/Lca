#pragma once

#include "Global.hpp"
#include "Buffer.hpp"

namespace Lca
{
    namespace Core
    {
        struct Eye
        {
            glm::vec4 position;
            glm::vec4 direction;
            glm::mat4 projView;
        };

        struct OrthoEye
        {
            glm::vec4 position;
            glm::vec4 direction;
            glm::mat4 projView;
        };

        inline BufferInterface eye;
        inline BufferInterface orthoEye;
        
        void createEye();
        void destroyEye();
        void updateEye(glm::vec3 position, glm::vec3 direction, float fov, float nearClip, float farClip);
        void updateOrthoEye(glm::vec3 position, glm::vec3 direction, float nearClip, float farClip);
        std::pair<glm::mat4, glm::mat4> getEyeMatrices(glm::vec3 position, glm::vec3 direction, glm::vec3 up,  float fov, float nearClip, float farClip);
        std::pair<glm::mat4, glm::mat4> getEyeMatrices(glm::vec3 position, glm::vec3 direction, glm::vec3 up,  float fov, float aspectRatio, float nearClip, float farClip);

    }
}