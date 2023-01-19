cmake_minimum_required(VERSION 3.16)
set (CMAKE_CXX_STANDARD 20)
set(PROJECT_NAME CKGLRasterizer)

project(${PROJECT_NAME})

set(CMAKE_MODULE_PATH
  "${CMAKE_CURRENT_LIST_DIR}/../buildsystems/cmake"
)

option(TRACY_ENABLE "" ON)
if (NOT TARGET Tracy::TracyClient)
    add_subdirectory (${CMAKE_CURRENT_LIST_DIR}/../vendor/tracy tracy.out)
endif()

set(GLEW_ROOT ${CMAKE_CURRENT_LIST_DIR}/../vendor/glew)
list(APPEND CMAKE_PREFIX_PATH ${GLEW_ROOT})
set(GLEW_USE_STATIC_LIBS TRUE)
find_package(GLEW REQUIRED)
include_directories(${GLEW_INCLUDE_DIRS})

include_directories(${VIRTOOLS_SDK_PATH}/Include)
include_directories(${CMAKE_CURRENT_LIST_DIR}/../vendor/OpenGL-Registry/api)

if (NOT TARGET CKRasterizerLib)
    set(CKRASTERIZER_LIB_DIR ${CMAKE_CURRENT_LIST_DIR}/../vendor/CKRasterizerLib)
    set(CKRASTERIZER_LIB_BUILD_DIR CKRasterizerLib.out)
    add_subdirectory(${CKRASTERIZER_LIB_DIR} ${CKRASTERIZER_LIB_BUILD_DIR})
endif()
include_directories(${CKRASTERIZER_LIB_DIR}/include)

file(GLOB CKGL_RASTERIZER_SRC
    "*.h"
    "*.cpp"
)

add_library(${PROJECT_NAME} SHARED ${CKGL_RASTERIZER_SRC} shaders.rc)
target_link_libraries(${PROJECT_NAME} PRIVATE opengl32.lib)
target_link_libraries(${PROJECT_NAME} PRIVATE GLEW::glew_s)
target_link_libraries(${PROJECT_NAME} PRIVATE CKRasterizerLib)
target_link_libraries(${PROJECT_NAME} PUBLIC Tracy::TracyClient)
target_compile_definitions(${PROJECT_NAME} PRIVATE GLEW_STATIC)
target_compile_definitions(${PROJECT_NAME} PRIVATE
            $<$<C_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
            $<$<C_COMPILER_ID:MSVC>:_CRT_NONSTDC_NO_WARNINGS>
            )
target_compile_definitions(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Debug>:GL_DEBUG>
            )
if(DEFINED BALLANCE_DIR)
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND "${CMAKE_COMMAND}" -E
		copy "$<TARGET_FILE:${PROJECT_NAME}>"
		"${BALLANCE_DIR}/RenderEngines/$<TARGET_FILE_NAME:${PROJECT_NAME}>")
endif()
