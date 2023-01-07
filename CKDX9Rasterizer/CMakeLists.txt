cmake_minimum_required(VERSION 3.16)
set (CMAKE_CXX_STANDARD 20)
set(PROJECT_NAME CKDX9Rasterizer)

project(${PROJECT_NAME})

if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "CMAKE_BUILD_TYPE empty, using Debug")
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_MODULE_PATH
  "${CMAKE_CURRENT_LIST_DIR}/../buildsystems/cmake"
)

set(CMAKE_INSTALL_PREFIX Install)

find_package(DirectX9)

if(${DIRECTX9_FOUND})
    include_directories(${VIRTOOLS_SDK_PATH}/Include)

    set(CKRASTERIZER_LIB_DIR ${CMAKE_CURRENT_LIST_DIR}/../vendor/CKRasterizerLib)
    set(CKRASTERIZER_LIB_BUILD_DIR CKRasterizerLib.out)
    add_subdirectory(${CKRASTERIZER_LIB_DIR} ${CKRASTERIZER_LIB_BUILD_DIR})
    include_directories(${CKRASTERIZER_LIB_DIR}/include)
    set(CKRASTERIZER_LIB_LIBRARIES ${CKRASTERIZER_LIB_BUILD_DIR}/src/${CMAKE_BUILD_TYPE}/CKRasterizerLib)

    file(GLOB CKDX9_RASTERIZER_SRC
        "*.h"
        "*.cpp"
    )

    message(STATUS "DIRECTX9_INCLUDE_DIRS=${DIRECTX9_INCLUDE_DIRS}")
    message(STATUS "DIRECTX9_ROOT_DIR=${DIRECTX9_ROOT_DIR}")
    include_directories(${DIRECTX9_INCLUDE_DIRS})
    add_library(${PROJECT_NAME} SHARED ${CKDX9_RASTERIZER_SRC})
    target_link_libraries(${PROJECT_NAME} ${DIRECTX9_LIBRARIES} ${CKRASTERIZER_LIB_LIBRARIES})
    target_link_libraries(${PROJECT_NAME} ${VIRTOOLS_SDK_PATH}/Lib/CK2.lib ${VIRTOOLS_SDK_PATH}/Lib/VxMath.lib)
    # target_link_libraries(${PROJECT_NAME} ${CKRASTERIZER_LIB_LIBRARIES})
else()
    message(SEND_ERROR "DirectX 9 SDK not found, skipping generation for CKDX9Rasterizer.")
endif()
