cmake_minimum_required(VERSION 3.16)
set (CMAKE_CXX_STANDARD 20)
set(PROJECT_NAME CKRasterizer)

project(${PROJECT_NAME})

add_subdirectory(CKDX9Rasterizer)
add_subdirectory(CKGLRasterizer)
