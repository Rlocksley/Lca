# LCA Example 5/CMakeLists.txt

# Set the minimum required version of CMake.
cmake_minimum_required(VERSION 3.15)

# Define the project name, version, and language.
project(LCA VERSION 1.0 LANGUAGES CXX)

# --- Build The Main Executable ---
add_executable(lca_example5 main.cpp)

# --- Link All Libraries ---
target_include_directories(lca_example5 PUBLIC ../../src/core ../../src/component ../../src/application ../../src/shape)

target_link_libraries(lca_example5 PRIVATE
    # Our internal libraries
    lca_core
    lca_component
    lca_app
    lca_shape
    # External dependencies
    Vulkan::Vulkan
    glm::glm
    glfw
    flecs::flecs
)
