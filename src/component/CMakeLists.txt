cmake_minimum_required(VERSION 3.16.3)

project(LCA)

add_library(lca_component STATIC
Transform.hpp
Transform.cpp
TransformID.hpp
TransformID.cpp
MeshTransform.hpp
Mesh.hpp
Mesh.cpp
SkeletonMesh.hpp
FlyingCamera.hpp
FlyingCamera.cpp
Light.hpp
AnimationStateMachine.hpp
AnimationStateMachine.cpp
BoxCollider.hpp
CharacterCapsule.hpp
PhysicsModule.hpp
RigidBody.hpp
Sensor.hpp
)

target_include_directories(lca_component PUBLIC ../core ../flecs)
target_link_libraries(lca_component PUBLIC
    # External dependencies
    Vulkan::Vulkan  # The imported target for Vulkan
    glm::glm        # The imported target for GLM
    glfw            # The imported target for GLFW
    lca_core
)