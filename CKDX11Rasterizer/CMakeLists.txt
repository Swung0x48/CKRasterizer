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

# Build HLSL shaders
add_custom_target(CKDX11RasterizerShader)

file(GLOB HLSL_VSHADER_FILES
    "VShader*.hlsl"
)

file(GLOB HLSL_PSHADER_FILES
    "PShader.hlsl"
)

set(HLSL_SHADER_FILES
    ${HLSL_VSHADER_FILES}
    ${HLSL_PSHADER_FILES}
)

set_source_files_properties(${HLSL_VSHADER_FILES} PROPERTIES ShaderType "vs")
set_source_files_properties(${HLSL_PSHADER_FILES} PROPERTIES ShaderType "ps")
set_source_files_properties(${HLSL_SHADER_FILES} PROPERTIES ShaderModel "4_0")

set(COMPILED_SHADERS_PATH ${CMAKE_BINARY_DIR}/shaderblob)
file(MAKE_DIRECTORY ${COMPILED_SHADERS_PATH})

foreach(FILE ${HLSL_SHADER_FILES})
  get_filename_component(FILE_WE ${FILE} NAME_WE)
  get_source_file_property(shadertype ${FILE} ShaderType)
  get_source_file_property(shadermodel ${FILE} ShaderModel)
  message(STATUS "${shadertype}_${shadermodel}: ${FILE} ${FILE_WE}")
  add_custom_command(TARGET CKDX11RasterizerShader
                     COMMAND fxc.exe /nologo /E${FILE_WE} /T${shadertype}_${shadermodel} $<IF:$<CONFIG:DEBUG>,/Od,/O3> /Zi /Fh ${COMPILED_SHADERS_PATH}/${FILE_WE}.h /Fd ${COMPILED_SHADERS_PATH}/${FILE_WE}.pdb ${FILE}
                     MAIN_DEPENDENCY ${FILE}
                     COMMENT "Compiling HLSL: ${shadertype}_${shadermodel}: ${FILE} ${FILE_WE}\n"
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                     VERBATIM)
endforeach(FILE)

file(GLOB CKDX11_RASTERIZER_SRC
    "*.h"
    "*.cpp"
)

message(STATUS "DirectX11_INCLUDE_DIR=${DirectX11_INCLUDE_DIR}")
message(STATUS "DirectX11_LIBRARY=${DirectX11_LIBRARIES}")
include_directories(${DirectX11_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_LIST_DIR}/../vendor/DirectXMesh/Utilities/)
include_directories(${COMPILED_SHADERS_PATH})
add_library(${PROJECT_NAME} SHARED ${CKDX11_RASTERIZER_SRC})
add_dependencies(${PROJECT_NAME} CKDX11RasterizerShader)
target_link_libraries(${PROJECT_NAME} PRIVATE ${DirectX11_LIBRARY})
target_link_libraries(${PROJECT_NAME} PRIVATE CKRasterizerLib)
target_link_libraries(${PROJECT_NAME} PUBLIC Tracy::TracyClient)
if(DEFINED BALLANCE_DIR)
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND "${CMAKE_COMMAND}" -E
		copy "$<TARGET_FILE:${PROJECT_NAME}>"
		"${BALLANCE_DIR}/RenderEngines/$<TARGET_FILE_NAME:${PROJECT_NAME}>")
endif()
