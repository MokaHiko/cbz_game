cmake_minimum_required(VERSION 3.0...3.25)
project(cubozoa_game VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# TODO: (jacob): Probably only want this for shared builds
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Engine 
add_subdirectory(cbz)
add_subdirectory(cbz_gfx)
add_subdirectory(cbz_ecs)

# Game
add_subdirectory(game)
add_subdirectory(editor)

# Track shaders
file(GLOB_RECURSE ASSET_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/*"
)

# Set your shader source and output directories
set(SHADER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders)

# TODO: Check if editor or game
set(SHADER_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/editor/assets/shaders)

# Find all .slang files recursively
file(GLOB_RECURSE SLANG_SHADERS RELATIVE ${SHADER_SRC_DIR} ${SHADER_SRC_DIR}/*.slang)
list(REMOVE_ITEM SLANG_SHADERS "cbz.slang")
list(REMOVE_ITEM SLANG_SHADERS "cbz_graphics.slang")
list(REMOVE_ITEM SLANG_SHADERS "cbz_graphics_pbr_forward.slang")

# Collect output targets to build as dependencies
set(SLANG_OUTPUTS)

foreach(SHADER_FILE ${SLANG_SHADERS})
    # Full path to the input shader
    set(INPUT_PATH ${SHADER_SRC_DIR}/${SHADER_FILE})

    # Replace extension with .wgsl and .json
    string(REPLACE ".slang" ".wgsl" SPIRV_FILE ${SHADER_FILE})
    string(REPLACE ".slang" ".json"  JSON_FILE  ${SHADER_FILE})

    # Output paths in the working directory
    set(SPIRV_OUTPUT ${SHADER_OUT_DIR}/${SPIRV_FILE})
    set(JSON_OUTPUT  ${SHADER_OUT_DIR}/${JSON_FILE})

    # Make sure the output directories exist
    get_filename_component(SPIRV_DIR ${SPIRV_OUTPUT} DIRECTORY)
    file(MAKE_DIRECTORY ${SPIRV_DIR})

    add_custom_command(
        OUTPUT ${SPIRV_OUTPUT} ${JSON_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SPIRV_DIR}
        COMMAND slangc ${INPUT_PATH}
                -target wgsl
                -o ${SPIRV_OUTPUT}
                -reflection-json ${JSON_OUTPUT}
        DEPENDS ${INPUT_PATH}
        COMMENT "Compiling shader ${SHADER_FILE}"
        VERBATIM
    )

    list(APPEND SLANG_OUTPUTS ${SPIRV_OUTPUT} ${JSON_OUTPUT})
endforeach()

# Add a target that builds all shaders
add_custom_target(compile_shaders ALL DEPENDS ${SLANG_OUTPUTS})
