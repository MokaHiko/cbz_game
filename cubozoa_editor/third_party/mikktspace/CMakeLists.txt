cmake_minimum_required(VERSION 3.0...3.25)
project(mikktspace VERSION 0.1.0 LANGUAGES C)

add_library(${PROJECT_NAME}
            mikktspace.c)

target_include_directories(${PROJECT_NAME} PUBLIC "./")
