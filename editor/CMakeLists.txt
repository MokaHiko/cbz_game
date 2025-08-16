cmake_minimum_required(VERSION 3.0...3.25)
project(editor VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

FetchContent_Declare(
    fastgltf
    GIT_REPOSITORY https://github.com/spnda/fastgltf.git
    GIT_TAG        main
)

FetchContent_MakeAvailable(fastgltf)

add_subdirectory(third_party/mikktspace)

add_executable(${PROJECT_NAME}
	src/cubozoa_render_graph.cpp
	src/cubozoa_gltf.cpp
	src/cubozoa_editor.cpp
)

target_compile_definitions(${PROJECT_NAME} PRIVATE ASSET_DIR="${CMAKE_SOURCE_DIR}/assets/")
target_include_directories(${PROJECT_NAME} PRIVATE third_party)
target_include_directories(${PROJECT_NAME} PUBLIC include)

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SOL_COMPILE_VCXX)
    target_include_directories(${PROJECT_NAME} PRIVATE third_party/lua548/src)
    target_link_directories(${PROJECT_NAME} PRIVATE third_party/lua548/src/win32)
elseif(APPLE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SOL_COMPILER_CLANG)
    target_include_directories(${PROJECT_NAME} PRIVATE third_party/lua548/src)
    target_link_directories(${PROJECT_NAME} PRIVATE third_party/lua548/bin/macos-aarch64/)
elseif(UNIX)
    # TODO: Add lua support for Linux
    target_compile_definitions(${PROJECT_NAME} PUBLIC CBZ_LINUX)
endif()

# Client game
target_link_libraries(${PROJECT_NAME} PUBLIC cbz_gfx cbz_ecs glm lua fastgltf mikktspace game)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    COMPILE_WARNING_AS_ERROR ON
)

if (MSVC)
    if(CBZ_BUILD_SHARED)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:game>   # the built DLL
            $<TARGET_FILE_DIR:editor>   # editor's output folder
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:cbz>   # the built DLL
            $<TARGET_FILE_DIR:editor>   # editor's output folder
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:cbz_gfx>   # the built DLL
            $<TARGET_FILE_DIR:editor>   # editor's output folder
    )
    endif()
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic)
    target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address)
endif()

if(CBZ_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if (EMSCRIPTEN)
    set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".html")
    target_link_options(${PROJECT_NAME}  PRIVATE "--preload-file=${CMAKE_CURRENT_BINARY_DIR}/assets@/assets")
else()
    target_copy_webgpu_binaries(${PROJECT_NAME})
endif()
