cmake_minimum_required(VERSION 3.21)
project(ClevoFanControl LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(clevoec SHARED src/clevoec.c)
target_include_directories(clevoec PRIVATE src)
target_compile_options(clevoec PRIVATE -Wall -Wextra)
