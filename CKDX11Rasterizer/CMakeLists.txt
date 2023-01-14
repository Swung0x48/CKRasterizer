cmake_minimum_required(VERSION 3.16)
set (CMAKE_CXX_STANDARD 20)
set(PROJECT_NAME CKDX11Rasterizer)

project(${PROJECT_NAME})

set(CMAKE_MODULE_PATH
  "${CMAKE_CURRENT_LIST_DIR}/../buildsystems/cmake"
)

option(TRACY_ENABLE "" ON)
if (NOT TARGET Tracy::TracyClient)
    add_subdirectory (${CMAKE_CURRENT_LIST_DIR}/../vendor/tracy tracy.out)
endif()

find_package(DirectX11 REQUIRED)

include_directories(${VIRTOOLS_SDK_PATH}/Include)

if (NOT TARGET CKRasterizerLib)
    set(CKRASTERIZER_LIB_DIR ${CMAKE_CURRENT_LIST_DIR}/../vendor/CKRasterizerLib)
    set(CKRASTERIZER_LIB_BUILD_DIR CKRasterizerLib.out)
    add_subdirectory(${CKRASTERIZER_LIB_DIR} ${CKRASTERIZER_LIB_BUILD_DIR})
endif() 

include_directories(${CKRASTERIZER_LIB_DIR}/include)

file(GLOB CKDX11_RASTERIZER_SRC
    "*.h"
    "*.cpp"
)

message(STATUS "DirectX11_INCLUDE_DIR=${DirectX11_INCLUDE_DIR}")
message(STATUS "DirectX11_LIBRARY=${DirectX11_LIBRARIES}")
include_directories(${DirectX11_INCLUDE_DIRS})
add_library(${PROJECT_NAME} SHARED ${CKDX11_RASTERIZER_SRC})
target_link_libraries(${PROJECT_NAME} PRIVATE ${DirectX11_LIBRARY})
target_link_libraries(${PROJECT_NAME} PRIVATE CKRasterizerLib)
target_link_libraries(${PROJECT_NAME} PUBLIC Tracy::TracyClient)
if(DEFINED BALLANCE_DIR)
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND "${CMAKE_COMMAND}" -E
		copy "$<TARGET_FILE:${PROJECT_NAME}>"
		"${BALLANCE_DIR}/RenderEngines/$<TARGET_FILE_NAME:${PROJECT_NAME}>")
endif()
