cmake_minimum_required(VERSION 3.16)
set (CMAKE_CXX_STANDARD 20)
set(PROJECT_NAME CKVkRasterizer)

project(${PROJECT_NAME})

set(CMAKE_MODULE_PATH
  "${CMAKE_CURRENT_LIST_DIR}/../buildsystems/cmake"
)

#option(TRACY_ENABLE "" ON)
#if (NOT TARGET Tracy::TracyClient)
#    add_subdirectory (${CMAKE_CURRENT_LIST_DIR}/../vendor/tracy tracy.out)
#endif()

include_directories(${VIRTOOLS_SDK_PATH}/Include)

if (NOT TARGET CKRasterizerLib)
    set(CKRASTERIZER_LIB_DIR ${CMAKE_CURRENT_LIST_DIR}/../vendor/CKRasterizerLib)
    set(CKRASTERIZER_LIB_BUILD_DIR CKRasterizerLib.out)
    add_subdirectory(${CKRASTERIZER_LIB_DIR} ${CKRASTERIZER_LIB_BUILD_DIR})
endif()
include_directories(${CKRASTERIZER_LIB_DIR}/include)

if (DEFINED ENV{VULKAN_SDK})
set(VULKAN_SDK_PATH "$ENV{VULKAN_SDK}" CACHE PATH "Path to Vulkan SDK")
else()
set(VULKAN_SDK_PATH "${VULKAN_SDK_PATH}" CACHE PATH "Path to Vulkan SDK")
endif()

if ((NOT IS_ABSOLUTE "${VULKAN_SDK_PATH}") OR (NOT IS_DIRECTORY "${VULKAN_SDK_PATH}"))
message(FATAL_ERROR "Vulkan SDK not found. Please add -DVULKAN_SDK_PATH= to the cmake command")
endif()

set(shader_i_list "")

function(compile_glsl GLSL_SRC)
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${GLSL_SRC}.i
  COMMAND ${VULKAN_SDK_PATH}/Bin/glslangValidator.exe -V100 -o
  ${CMAKE_CURRENT_BINARY_DIR}/${GLSL_SRC}.i
  ${CMAKE_CURRENT_SOURCE_DIR}/${GLSL_SRC}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${GLSL_SRC}
  COMMENT "GLSL -> SPIR-V: ${GLSL_SRC}"
  VERBATIM
)
list(APPEND shader_i_list ${CMAKE_CURRENT_BINARY_DIR}/${GLSL_SRC}.i)
set(shader_i_list ${shader_i_list} PARENT_SCOPE)
endfunction()

include_directories(${VULKAN_SDK_PATH}/Include)
link_directories(${VULKAN_SDK_PATH}/Lib32)

file(GLOB CKVK_RASTERIZER_SRC
    "*.h"
    "*.cpp"
)
configure_file(shaders.rc.in shaders.rc)
list(APPEND CKVK_RASTERIZER_SRC ${CMAKE_CURRENT_BINARY_DIR}/shaders.rc)

file(GLOB CKVK_RASTERIZER_SHADERS "*.vert" "*.frag")
foreach(shdr IN LISTS CKVK_RASTERIZER_SHADERS)
  get_filename_component(shader_src ${shdr} NAME)
  compile_glsl(${shader_src})
endforeach()

set_property(SOURCE ${CMAKE_CURRENT_BINARY_DIR}/shaders.rc APPEND
  PROPERTY OBJECT_DEPENDS ${shader_i_list})

add_library(${PROJECT_NAME} SHARED ${CKVK_RASTERIZER_SRC})
target_link_libraries(${PROJECT_NAME} PRIVATE CKRasterizerLib)
#target_link_libraries(${PROJECT_NAME} PUBLIC Tracy::TracyClient)
target_link_libraries(${PROJECT_NAME} PRIVATE vulkan-1)
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
