cmake_minimum_required(VERSION 3.16)
set (CMAKE_CXX_STANDARD 20)
set(PROJECT_NAME CKDX9Rasterizer)

project(${PROJECT_NAME})

set(CMAKE_MODULE_PATH
  "${CMAKE_CURRENT_LIST_DIR}/../buildsystems/cmake"
)

option(TRACY_ENABLE "Enable Tracy" ON)

if (TRACY_ENABLE AND NOT TARGET Tracy::TracyClient)
    add_subdirectory (${CMAKE_CURRENT_LIST_DIR}/../vendor/tracy tracy.out)
endif()

find_package(DirectX9 REQUIRED)

include_directories(${VIRTOOLS_SDK_PATH}/Include)

if (NOT TARGET CKRasterizerLib)
    set(CKRASTERIZER_LIB_DIR ${CMAKE_CURRENT_LIST_DIR}/../vendor/CKRasterizerLib)
    set(CKRASTERIZER_LIB_BUILD_DIR CKRasterizerLib.out)
    add_subdirectory(${CKRASTERIZER_LIB_DIR} ${CKRASTERIZER_LIB_BUILD_DIR})
endif() 

include_directories(${CKRASTERIZER_LIB_DIR}/include)

file(GLOB CKDX9_RASTERIZER_SRC
    "*.h"
    "*.cpp"
)

message(STATUS "DIRECTX9_INCLUDE_DIRS=${DIRECTX9_INCLUDE_DIRS}")
message(STATUS "DIRECTX9_ROOT_DIR=${DIRECTX9_ROOT_DIR}")
include_directories(${DIRECTX9_INCLUDE_DIRS})
add_library(${PROJECT_NAME} SHARED ${CKDX9_RASTERIZER_SRC})
if (TRACY_ENABLE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE "TRACY_ENABLE")
    target_link_libraries(${PROJECT_NAME} PUBLIC Tracy::TracyClient)
endif()

if(DEFINED BALLANCE_DIR)
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND "${CMAKE_COMMAND}" -E
		copy "$<TARGET_FILE:${PROJECT_NAME}>"
		"${BALLANCE_DIR}/RenderEngines/$<TARGET_FILE_NAME:${PROJECT_NAME}>")
endif()
