cmake_minimum_required(VERSION 3.16)
set (CMAKE_CXX_STANDARD 20)
set(PROJECT_NAME CKGLRasterizer)

project(${PROJECT_NAME})

set(CMAKE_MODULE_PATH
  "${CMAKE_CURRENT_LIST_DIR}/../buildsystems/cmake"
)

set(GLEW_ROOT ${CMAKE_CURRENT_LIST_DIR}/../vendor/glew)
list(APPEND CMAKE_PREFIX_PATH ${GLEW_ROOT})
set(GLEW_USE_STATIC_LIBS TRUE)
find_package(GLEW REQUIRED)
include_directories(${GLEW_INCLUDE_DIRS})

include_directories(${VIRTOOLS_SDK_PATH}/Include)
include_directories(${CMAKE_CURRENT_LIST_DIR}/../vendor/OpenGL-Registry/api)

set(CKRASTERIZER_LIB_DIR ${CMAKE_CURRENT_LIST_DIR}/../vendor/CKRasterizerLib)
set(CKRASTERIZER_LIB_BUILD_DIR CKRasterizerLib.out)
add_subdirectory(${CKRASTERIZER_LIB_DIR} ${CKRASTERIZER_LIB_BUILD_DIR})
include_directories(${CKRASTERIZER_LIB_DIR}/include)

file(GLOB CKGL_RASTERIZER_SRC
    "*.h"
    "*.cpp"
)

add_library(${PROJECT_NAME} SHARED ${CKGL_RASTERIZER_SRC})
target_link_libraries(${PROJECT_NAME} PRIVATE opengl32.lib)
target_link_libraries(${PROJECT_NAME} PRIVATE GLEW::glew_s)
target_link_libraries(${PROJECT_NAME} PRIVATE CKRasterizerLib)
target_compile_definitions(${PROJECT_NAME} PRIVATE GLEW_STATIC)