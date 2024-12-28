cmake_minimum_required(VERSION 3.21)
project(ClevoFanControl VERSION 2.3.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

set(CFC_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/main)
add_subdirectory(${CFC_SOURCE_DIR}/gui)
add_subdirectory(${CFC_SOURCE_DIR}/cmd)
