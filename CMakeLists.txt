cmake_minimum_required(VERSION 3.16)
set (CMAKE_CXX_STANDARD 20)
set(PROJECT_NAME CKRasterizer)

project(${PROJECT_NAME})

if (NOT ${MSVC})
	message(FATAL_ERROR "MSVC is required to build this project.")
endif()

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
	message(FATAL_ERROR "64-bit Virtools isn't real (yet).")
endif()

option(BUILD_DX9_RASTERIZER "Build Direct3D 9 Rasterizer" ON)
option(BUILD_GL_RASTERIZER "Build OpenGL 3.x Rasterizer" ON)

if(BUILD_DX9_RASTERIZER)
	add_subdirectory(CKDX9Rasterizer)
endif()

if(BUILD_GL_RASTERIZER)
	add_subdirectory(CKGLRasterizer)
endif()

if(NOT DEFINED BALLANCE_DIR)
	message(NOTICE "Set BALLANCE_DIR to automatically copy the rasterizer to its RenderEngines directory after build.")
endif()
