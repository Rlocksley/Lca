cmake_minimum_required(VERSION 3.16.3)

project(LCA)

add_library(lca_core STATIC
Window.hpp
Window.cpp
Time.hpp
Time.cpp
Input.hpp
Input.cpp
Instance.hpp
Instance.cpp
Surface.hpp
Surface.cpp
PhysicalDevice.hpp
PhysicalDevice.cpp
Device.hpp
Device.cpp
Queue.hpp
Queue.cpp
Command.hpp
Command.cpp
Allocator.hpp
Allocator.cpp
DescriptorPool.hpp
DescriptorPool.cpp
Semaphore.hpp
Semaphore.cpp
Fence.hpp
Fence.cpp
Image.hpp
Image.cpp
Buffer.hpp
Buffer.cpp
Eye.hpp
Eye.cpp
Swapchain.hpp
Swapchain.cpp
RenderPass.hpp
RenderPass.cpp
Framebuffers.hpp
Framebuffers.cpp
Shader.hpp
Shader.cpp
DescriptorSetLayouts.hpp
DescriptorSetLayouts.cpp
PipelineLayouts.hpp
PipelineLayouts.cpp
Light.hpp
Light.cpp
Physics.hpp
Core.hpp
Core.cpp
Pipeline.hpp
Pipeline.cpp
GraphicsPipeline.hpp
GraphicsPipeline.cpp
MeshPipeline.hpp
MeshPipeline.cpp
ComputePipeline.hpp
ComputePipeline.cpp
UberCullPipeline.hpp
UberCullPipeline.cpp
AssetManager.hpp
AssetManager.cpp
Renderer.hpp
Renderer.cpp
Pipeline.hpp
Pipeline.cpp
GraphicsPipeline.hpp
GraphicsPipeline.cpp
)


target_link_libraries(lca_core PUBLIC
    # External dependencies
    Vulkan::Vulkan  # The imported target for Vulkan
    glm::glm        # The imported target for GLM
    glfw            # The imported target for GLFW (note: glfw, not glfw3)
)