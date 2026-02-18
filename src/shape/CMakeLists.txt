cmake_minimum_required(VERSION 3.16.3)

project(LCA)

add_library(lca_shape STATIC
Shape.hpp
Shape.cpp
)

target_include_directories(lca_shape PUBLIC ../core )
target_link_libraries(lca_shape PUBLIC lca_core)