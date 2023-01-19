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

set(FVF_HEADER_PATH ${CMAKE_CURRENT_LIST_DIR}/../vendor/DirectXMesh/Utilities/FlexibleVertexFormat.h)
if (NOT EXISTS ${FVF_HEADER_PATH})
    file(DOWNLOAD
        https://raw.githubusercontent.com/microsoft/DirectXMesh/main/Utilities/FlexibleVertexFormat.h
        ${FVF_HEADER_PATH}
        SHOW_PROGRESS LOG FVF_HEADER_LOG STATUS DOWNLOAD_STATUS)

    message(STATUS "Download FlexibleVertexFormat.h from DirectXMesh: ${FVF_HEADER_LOG}")

    list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
    list(GET DOWNLOAD_STATUS 1 ERROR_MESSAGE)

    if(${STATUS_CODE} EQUAL 0)
      message(STATUS "FlexibleVertexFormat.h from DirectXMesh downloaded.")
    else()
      file(REMOVE ${FVF_HEADER_PATH})
      message(FATAL_ERROR "Error occurred during downloading FlexibleVertexFormat.h from DirectXMesh: ${ERROR_MESSAGE}\nYou may download it manually and place it under ${FVF_HEADER_PATH}")
    endif()
else()
    
endif()


file(GLOB CKDX11_RASTERIZER_SRC
    "*.h"
    "*.cpp"
)

message(STATUS "DirectX11_INCLUDE_DIR=${DirectX11_INCLUDE_DIR}")
message(STATUS "DirectX11_LIBRARY=${DirectX11_LIBRARIES}")
include_directories(${DirectX11_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_LIST_DIR}/../vendor/DirectXMesh/Utilities/)
add_library(${PROJECT_NAME} SHARED ${CKDX11_RASTERIZER_SRC})
target_link_libraries(${PROJECT_NAME} PRIVATE ${DirectX11_LIBRARY})
target_link_libraries(${PROJECT_NAME} PRIVATE CKRasterizerLib)
target_link_libraries(${PROJECT_NAME} PUBLIC Tracy::TracyClient)
if(DEFINED BALLANCE_DIR)
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND "${CMAKE_COMMAND}" -E
		copy "$<TARGET_FILE:${PROJECT_NAME}>"
		"${BALLANCE_DIR}/RenderEngines/$<TARGET_FILE_NAME:${PROJECT_NAME}>")
endif()
