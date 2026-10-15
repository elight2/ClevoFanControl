cmake_minimum_required(VERSION 3.21)
project(ClevoFanControl LANGUAGES C CXX)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(tomlplusplus REQUIRED)

# Shared library, kept so src/testec.py can exercise the EC layer directly.
add_library(clevoec SHARED src/clevoec.c)
target_include_directories(clevoec PRIVATE src)
target_compile_options(clevoec PRIVATE -Wall -Wextra)

# The daemon links the same EC source statically: no runtime .so lookup.
set(CFC_SOURCES
    src/main.cpp
    src/config.cpp
    src/control.cpp
    src/ec.cpp
    src/sensors.cpp
    src/clevoec.c)

add_executable(cfc ${CFC_SOURCES})
target_include_directories(cfc PRIVATE src)
target_link_libraries(cfc PRIVATE tomlplusplus::tomlplusplus)
target_compile_options(cfc PRIVATE -Wall -Wextra)

# Runtime config is read next to the executable, so put the template there.
add_custom_command(TARGET cfc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/src/config.toml
            $<TARGET_FILE_DIR:cfc>/config.toml
    COMMENT "Copying config.toml next to the cfc binary")
