# LCA Example 1/CMakeLists.txt

# Set the minimum required version of CMake.
cmake_minimum_required(VERSION 3.15)

# Define the project name, version, and language. This will be the name of the .sln file.
project(LCA VERSION 1.0 LANGUAGES CXX)

# --- Build The Main Executable ---
# Define the main executable target, "LCA Example 1".
add_executable(lca_example1 main.cpp)

# --- Link All Libraries ---
# Link LCA Example 1 against all the libraries it needs. Modern CMake uses imported targets
# (like Vulkan::Vulkan, glm::glm, glfw) which also handle include directories automatically.

target_include_directories(lca_example1 PUBLIC ../../src/core ../../src/component ../../src/application ../../src/shape)

target_link_libraries(lca_example1 PRIVATE
    # Our internal libraries
    lca_core
    lca_component
    #lca_app
    lca_shape
    # External dependencies
    Vulkan::Vulkan  # The imported target for Vulkan
    glm::glm        # The imported target for GLM
    glfw            # The imported target for GLFW
    flecs::flecs
)



