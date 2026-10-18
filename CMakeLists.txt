cmake_minimum_required(VERSION 3.21)
project(ClevoFanControl LANGUAGES C CXX)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(tomlplusplus REQUIRED)
# log.hpp includes <systemd/sd-daemon.h> for the SD_ERR/SD_INFO/SD_DEBUG
# prefixes. No library is linked, only the header macros are used, so this is a
# header dependency rather than a link dependency and is not declared to CMake.
# A build fails at compile time if systemd headers are missing.

# Note: no CMAKE_BUILD_TYPE is set, so a plain `cmake -B build` builds without
# optimisation (-O0 with the Ninja generator's default flags). Changing that is a
# policy choice for the packager, not done here.

# Shared library, kept so src/testec.py can exercise the EC layer directly.
add_library(clevoec SHARED src/libclevoec.c)
target_include_directories(clevoec PRIVATE src)
target_compile_options(clevoec PRIVATE -Wall -Wextra)

# The daemon links the same EC source statically: no runtime .so lookup.
set(CFC_SOURCES
    src/main.cpp
    src/config.cpp
    src/control.cpp
    src/ec.cpp
    src/sensors.cpp
    src/libclevoec.c)

add_executable(cfc ${CFC_SOURCES})
target_include_directories(cfc PRIVATE src)
target_link_libraries(cfc PRIVATE tomlplusplus::tomlplusplus)
target_compile_options(cfc PRIVATE -Wall -Wextra)

# No config.toml is installed next to the binary: src/config.cpp embeds the
# default through #embed and writes it on first run, when the file is missing.

# Staging directory for manual testing: every artifact flat in one place, so a
# test run needs no path juggling. `cmake --install build` fills it after a
# build. The default prefix lives inside the source tree (and is git-ignored);
# a real system install still needs an explicit --prefix such as /usr/local.
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/out" CACHE PATH
        "Install prefix" FORCE)
endif()

install(TARGETS cfc
    RUNTIME DESTINATION .)
# src/testec.py looks for the library next to itself, so it must be installed
# flat rather than into a lib/ subdirectory.
install(TARGETS clevoec
    LIBRARY DESTINATION .)
install(PROGRAMS src/testec.py
    DESTINATION .)
# The default config is copied so the staged directory is self-contained. Note:
# this overwrites out/config.toml on every install, so edits made while testing
# (max_speed, temps, ...) are lost on the next install. Copy the file elsewhere,
# or pass --prefix to a directory holding the config you want to keep.
install(FILES src/config.toml
    DESTINATION .)
# The user-facing description, copied for the same reason: the staged
# directory is self-contained, including the notes on the control law.
install(FILES README.md
    DESTINATION .)
