#include "PipelineLayouts.hpp"
#include "DescriptorSetLayouts.hpp"
#include "Mutex.hpp"
#include "Device.hpp"

namespace Lca
{
    namespace Core
    {
        void createPipelineLayouts()
        {
            createColorMeshPipelineLayout();
            createColorMeshArrayPipelineLayout();
            createInstancedColorMeshPipelineLayout();
            createTextureModelPipelineLayout();
            createSkeletonModelPipelineLayout();
            createSkeletonModelCompPipelineLayout();
            createShadowColorMeshPipelineLayout();
            createShadowColorModelArrayPipelineLayout();
            createShadowTextureModelPipelineLayout();
            createShadowSkeletonModelPipelineLayout();
            createTextModelArrayPipelineLayout();
        }

        void destroyPipelineLayouts()
        {
            destroyTextModelArrayPipelineLayout();
            destroyShadowSkeletonModelPipelineLayout();
            destroyShadowTextureModelPipelineLayout();
            destroyShadowColorModelArrayPipelineLayout();
            destroyShadowColorMeshPipelineLayout();
            destroySkeletonModelCompPipelineLayout();
            destroySkeletonModelPipelineLayout();
            destroyTextureModelPipelineLayout();
            destroyInstancedColorMeshPipelineLayout();
            destroyColorMeshArrayPipelineLayout();
            destroyColorMeshPipelineLayout();
        }

        void createColorMeshPipelineLayout(){
            VkPipelineLayoutCreateInfo layoutInfo{};
            layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            layoutInfo.setLayoutCount = 1;
            layoutInfo.pSetLayouts = &colorMeshDescriptorSetLayout;

            LCA_VK_MUTEX(
            LCA_CHECK_VULKAN(
            vkCreatePipelineLayout
            (Core::vkDevice, 
            &layoutInfo, 
            nullptr, 
            &colorMeshPipelineLayout),
            "createPipelineLayout",
            "vkCreatePipelineLayout"
            ));
        }

        void destroyColorMeshPipelineLayout(){
            LCA_VK_MUTEX(
            vkDestroyPipelineLayout(Core::vkDevice, colorMeshPipelineLayout, nullptr);
            );
        }

        void createColorMeshArrayPipelineLayout(){
            VkPipelineLayoutCreateInfo layoutInfo{};
            layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            layoutInfo.setLayoutCount = 1;
            layoutInfo.pSetLayouts = &colorMeshArrayDescriptorSetLayout;

            LCA_VK_MUTEX(
            LCA_CHECK_VULKAN(
            vkCreatePipelineLayout
            (Core::vkDevice, 
            &layoutInfo, 
            nullptr, 
            &colorMeshArrayPipelineLayout),
            "createColorMeshArrayPipelineLayout",
            "vkCreatePipelineLayout"
            ));
        }

        void destroyColorMeshArrayPipelineLayout(){
            LCA_VK_MUTEX(
            vkDestroyPipelineLayout(Core::vkDevice, colorMeshArrayPipelineLayout, nullptr);
            );
        }


        void createInstancedColorMeshPipelineLayout(){
            VkPipelineLayoutCreateInfo layoutInfo{};
            layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            layoutInfo.setLayoutCount = 1;
            layoutInfo.pSetLayouts = &instancedColorMeshDescriptorSetLayout;

            LCA_VK_MUTEX(
            LCA_CHECK_VULKAN(
            vkCreatePipelineLayout
            (Core::vkDevice, 
            &layoutInfo, 
            nullptr, 
            &instancedColorMeshPipelineLayout),
            "createInstancedColorMeshPipelineLayout",
            "vkCreatePipelineLayout"
            ));
        }

        void destroyInstancedColorMeshPipelineLayout(){
            LCA_VK_MUTEX(
            vkDestroyPipelineLayout
            (Core::vkDevice, 
            instancedColorMeshPipelineLayout, 
            nullptr);)
        }

        void createTextureModelPipelineLayout(){
            VkPipelineLayoutCreateInfo layoutCreateInfo{};
            layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            layoutCreateInfo.setLayoutCount = 1;
            layoutCreateInfo.pSetLayouts = &textureModelDescriptorSetLayout;
            layoutCreateInfo.pushConstantRangeCount = 0;
            layoutCreateInfo.pPushConstantRanges = nullptr;

            LCA_CHECK_VULKAN
            (vkCreatePipelineLayout
            (Core::vkDevice,
            &layoutCreateInfo,
            nullptr,
            &textureModelPipelineLayout),
            "createTextureModelPipelineLayout",
            "vkCreatePipelineLayout")
        }

        void destroyTextureModelPipelineLayout(){
            LCA_VK_MUTEX(
            vkDestroyPipelineLayout
            (Core::vkDevice, 
            textureModelPipelineLayout, 
            nullptr);)
        }

        void createSkeletonModelPipelineLayout(){
            VkPipelineLayoutCreateInfo layoutInfo{};
            layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            layoutInfo.setLayoutCount = 1;
            layoutInfo.pSetLayouts = &skeletonModelDescriptorSetLayout;

            LCA_VK_MUTEX(
            LCA_CHECK_VULKAN(
            vkCreatePipelineLayout
            (Core::vkDevice, 
            &layoutInfo, 
            nullptr, 
            &skeletonModelPipelineLayout),
            "createSkeletonModelPipelineLayout",
            "vkCreatePipelineLayout"
            ));
        }

        void destroySkeletonModelPipelineLayout(){
            LCA_VK_MUTEX(
            vkDestroyPipelineLayout
            (Core::vkDevice, 
            skeletonModelPipelineLayout, 
            nullptr);
            );
        }

        void createSkeletonModelCompPipelineLayout(){
            VkPushConstantRange pushConstantRange{};
            pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            pushConstantRange.offset = 0;
            pushConstantRange.size = sizeof(int); // As used in the system: struct { int numberNodes; }

            VkPipelineLayoutCreateInfo layoutInfo{};
            layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            layoutInfo.setLayoutCount = 1;
            layoutInfo.pSetLayouts = &skeletonModelCompDescriptorSetLayout;
            layoutInfo.pushConstantRangeCount = 1;
            layoutInfo.pPushConstantRanges = &pushConstantRange;

            LCA_VK_MUTEX(
            LCA_CHECK_VULKAN(
            vkCreatePipelineLayout
            (Core::vkDevice, 
            &layoutInfo, 
            nullptr, 
            &skeletonModelCompPipelineLayout),
            "createSkeletonModelCompPipelineLayout",
            "vkCreatePipelineLayout"
            ));
        }

        void destroySkeletonModelCompPipelineLayout(){
            LCA_VK_MUTEX(
            vkDestroyPipelineLayout(Core::vkDevice, skeletonModelCompPipelineLayout, nullptr);
            );
        }

        void createShadowColorMeshPipelineLayout(){
            VkPipelineLayoutCreateInfo layoutInfo{};
            layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            layoutInfo.setLayoutCount = 1;
            layoutInfo.pSetLayouts = &shadowColorMeshDescriptorSetLayout;

            LCA_VK_MUTEX(
            LCA_CHECK_VULKAN(
            vkCreatePipelineLayout
            (Core::vkDevice, 
            &layoutInfo, 
            nullptr, 
            &shadowColorMeshPipelineLayout),
            "createShadowColorMeshPipelineLayout",
            "vkCreatePipelineLayout"
            ));
        }
        void destroyShadowColorMeshPipelineLayout(){
            LCA_VK_MUTEX(
            vkDestroyPipelineLayout(Core::vkDevice, shadowColorMeshPipelineLayout, nullptr);
            );
        }


        
        void createShadowColorModelArrayPipelineLayout(){
            VkPipelineLayoutCreateInfo layoutInfo{};
            layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            layoutInfo.setLayoutCount = 1;
            layoutInfo.pSetLayouts = &shadowColorModelArrayDescriptorSetLayout;

            LCA_VK_MUTEX(
            LCA_CHECK_VULKAN(
            vkCreatePipelineLayout
            (Core::vkDevice, 
            &layoutInfo, 
            nullptr, 
            &shadowColorModelArrayPipelineLayout),
            "createShadowColorModelArrayPipelineLayout",
            "vkCreatePipelineLayout"
            ));
        }

        void destroyShadowColorModelArrayPipelineLayout(){
            LCA_VK_MUTEX(
            vkDestroyPipelineLayout(Core::vkDevice, shadowColorModelArrayPipelineLayout, nullptr);
            );

        }

        void createShadowTextureModelPipelineLayout(){
            VkPipelineLayoutCreateInfo layoutInfo{};
            layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            layoutInfo.setLayoutCount = 1;
            layoutInfo.pSetLayouts = &shadowTextureModelDescriptorSetLayout;

            LCA_VK_MUTEX(
            LCA_CHECK_VULKAN(
            vkCreatePipelineLayout
            (Core::vkDevice, 
            &layoutInfo, 
            nullptr, 
            &shadowTextureModelPipelineLayout),
            "createShadowTextureModelPipelineLayout",
            "vkCreatePipelineLayout"
            ));
        }

        void destroyShadowTextureModelPipelineLayout(){
            LCA_VK_MUTEX(
            vkDestroyPipelineLayout(Core::vkDevice, shadowTextureModelPipelineLayout, nullptr);
            );
        }
     
        
        void createShadowSkeletonModelPipelineLayout(){
            VkPipelineLayoutCreateInfo layoutInfo{};
            layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            layoutInfo.setLayoutCount = 1;
            layoutInfo.pSetLayouts = &shadowSkeletonModelDescriptorSetLayout;

            LCA_VK_MUTEX(
            LCA_CHECK_VULKAN(
            vkCreatePipelineLayout
            (Core::vkDevice, 
            &layoutInfo, 
            nullptr, 
            &shadowSkeletonModelPipelineLayout),
            "createShadowSkeletonModelPipelineLayout",
            "vkCreatePipelineLayout"
            ));
        
        }

        void destroyShadowSkeletonModelPipelineLayout(){
            LCA_VK_MUTEX(
            vkDestroyPipelineLayout(Core::vkDevice, shadowSkeletonModelPipelineLayout, nullptr);
            );
        }

        void createTextModelArrayPipelineLayout(){
            VkPipelineLayoutCreateInfo layoutInfo{};
            layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            layoutInfo.setLayoutCount = 1;
            layoutInfo.pSetLayouts = &textModelArrayDescriptorSetLayout;

            LCA_VK_MUTEX(
            LCA_CHECK_VULKAN(
            vkCreatePipelineLayout
            (Core::vkDevice, 
            &layoutInfo, 
            nullptr, 
            &textModelArrayPipelineLayout),
            "createTextModelArrayPipelineLayout",
            "vkCreatePipelineLayout"
            ));
        
        }

        void destroyTextModelArrayPipelineLayout(){
            LCA_VK_MUTEX(
            vkDestroyPipelineLayout(Core::vkDevice, textModelArrayPipelineLayout, nullptr);
            );
        }
    }
} // namespace Lca::Core