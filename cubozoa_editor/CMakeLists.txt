cmake_minimum_required(VERSION 3.0...3.25)
project(cubozoa_editor VERSION 0.1.0 LANGUAGES CXX C)

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

target_include_directories(${PROJECT_NAME} PRIVATE third_party)
target_include_directories(${PROJECT_NAME} PUBLIC include)

# Macos && Clang
target_compile_definitions(${PROJECT_NAME} PRIVATE SOL_COMPILER_CLANG)
target_include_directories(${PROJECT_NAME} PRIVATE third_party/lua548/src)
target_link_directories(${PROJECT_NAME} PRIVATE third_party/lua548/src/macos)

# Client game
target_link_libraries(${PROJECT_NAME} PUBLIC cbz_gfx cbz_ecs glm lua fastgltf mikktspace game_rts)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    COMPILE_WARNING_AS_ERROR ON
)

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic)
    target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
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
