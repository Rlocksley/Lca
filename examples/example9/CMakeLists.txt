# LCA Example 9 / CMakeLists.txt

cmake_minimum_required(VERSION 3.15)
project(LCA VERSION 1.0 LANGUAGES CXX)

add_executable(lca_example9 main.cpp)

target_include_directories(lca_example9 PUBLIC
    ../../src/core
    ../../src/component
    ../../src/application
    ../../src/shape
)

target_link_libraries(lca_example9 PRIVATE
    lca_core
    lca_component
    lca_app
    lca_shape
    Vulkan::Vulkan
    glm::glm
    glfw
    flecs::flecs
)
