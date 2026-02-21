cmake_minimum_required(VERSION 3.16.3)

project(LCA)

add_library(lca_app STATIC
    Application.cpp
)

target_include_directories(lca_app PUBLIC ../core ../flecs ../component)
target_link_libraries(lca_app PUBLIC
    # External dependencies
    Vulkan::Vulkan  # The imported target for Vulkan
    glm::glm        # The imported target for GLM
    glfw3            # The imported target for GLFW
    flecs
#    Jolt::Jolt
    lca_core
    lca_component
)