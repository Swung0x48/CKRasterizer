cmake_minimum_required(VERSION 3.16)
set (CMAKE_CXX_STANDARD 20)
set(PROJECT_NAME CKRasterizer)

project(${PROJECT_NAME})

if (NOT ${MSVC})
	message(FATAL_ERROR "MSVC is required to build this project.")
endif()

if(NOT DEFINED CMAKE_GENERATOR_PLATFORM)
	message(FATAL_ERROR "Please specify a platform. (-A / -DCMAKE_GENERATOR_PLATFORM)")
endif()

if (NOT ${CMAKE_GENERATOR_PLATFORM} MATCHES "Win32")
	message(FATAL_ERROR "64-bit/ARM Virtools isn't real (yet).")
endif()

add_subdirectory(CKDX9Rasterizer)
add_subdirectory(CKGLRasterizer)

if(NOT DEFINED BALLANCE_DIR)
	message(NOTICE "Set BALLANCE_DIR to automatically copy the rasterizer to its RenderEngines directory after build.")
endif()
