cmake_minimum_required(VERSION 3.0...3.25)
project(game VERSION 0.1.0 LANGUAGES CXX C)

add_library(${PROJECT_NAME} SHARED
	src/rts.cpp)
target_compile_definitions(${PROJECT_NAME} PRIVATE CBZ_EXPORTS)

target_include_directories(${PROJECT_NAME} PRIVATE third_party)
target_include_directories(${PROJECT_NAME} PUBLIC include)

# Client game
target_link_libraries(${PROJECT_NAME} PRIVATE cbz cbz_ecs)
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    COMPILE_WARNING_AS_ERROR ON
)

if (MSVC)
    # target_compile_options(${PROJECT_NAME} PRIVATE)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic)
    target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()
