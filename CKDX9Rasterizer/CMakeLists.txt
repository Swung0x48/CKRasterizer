cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 20)
set(PROJECT_NAME CKDX9Rasterizer)

project(${PROJECT_NAME})

list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_LIST_DIR}/../buildsystems/cmake")

option(USE_D3D9EX "Use DirectX 9Ex" ON)
option(ENABLE_TRIPLE_BUFFER "Enable triple buffering" ON)
option(ENABLE_TRACY "Enable Tracy" ON)

if (ENABLE_TRACY AND NOT TARGET Tracy::TracyClient)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../vendor/tracy tracy.out)
endif ()

if (NOT TARGET CKRasterizerLib)
    set(CKRASTERIZER_LIB_DIR ${CMAKE_CURRENT_LIST_DIR}/../vendor/CKRasterizerLib)
    set(CKRASTERIZER_LIB_BUILD_DIR CKRasterizerLib.out)
    add_subdirectory(${CKRASTERIZER_LIB_DIR} ${CKRASTERIZER_LIB_BUILD_DIR})
endif ()

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} CKDX9_RASTERIZER_SOURCES)

add_library(${PROJECT_NAME} SHARED ${CKDX9_RASTERIZER_SOURCES})
target_link_libraries(${PROJECT_NAME} PRIVATE CKRasterizerLib d3d9)

if (USE_D3D9EX)
    target_compile_definitions(${PROJECT_NAME} PRIVATE "USE_D3D9EX")
endif ()

if (ENABLE_TRIPLE_BUFFER)
    target_compile_definitions(${PROJECT_NAME} PRIVATE "ENABLE_TRIPLE_BUFFER")
endif ()

if (ENABLE_TRACY)
    target_compile_definitions(${PROJECT_NAME} PRIVATE "ENABLE_TRACY")
    target_link_libraries(${PROJECT_NAME} PUBLIC Tracy::TracyClient)
endif ()

if (DEFINED BALLANCE_DIR)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND "${CMAKE_COMMAND}" -E
            copy "$<TARGET_FILE:${PROJECT_NAME}>"
            "${BALLANCE_DIR}/RenderEngines/$<TARGET_FILE_NAME:${PROJECT_NAME}>")
endif ()
