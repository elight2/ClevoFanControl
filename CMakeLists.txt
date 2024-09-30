cmake_minimum_required(VERSION 3.21)
project(ClevoFanControl VERSION 2.1.0 LANGUAGES CXX)

set(CMAKE_BUILD_TYPE "Release")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

include_directories("include")

IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
include_directories("/home/main/Applications/Qt/6.7.2/gcc_64/include")
set(CMAKE_PREFIX_PATH "/home/main/Applications/Qt/6.7.2/gcc_64/lib/cmake")
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Windows")
include_directories("D:\\Applications\\Qt\\6.7.2\\mingw_64\\include")
set(CMAKE_PREFIX_PATH "D:\\Applications\\Qt\\6.7.2\\mingw_64\\lib\\cmake")
set(CMAKE_C_COMPILER "D:\\Applications\\Qt\\Tools\\mingw1120_64\\bin\\gcc.exe") #using custom gcc can cause error
set(CMAKE_CXX_COMPILER "D:\\Applications\\Qt\\Tools\\mingw1120_64\\bin\\g++.exe")
if(CMAKE_BUILD_TYPE MATCHES "Release")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mwindows")
endif()
ENDIF (CMAKE_SYSTEM_NAME MATCHES "Linux")

if(CMAKE_BUILD_TYPE MATCHES "Release")
add_definitions(-DQT_NO_DEBUG_OUTPUT)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)
add_executable(ClevoFanControl main.cpp ClevoFanControl.cpp ClevoFanControl.qrc ClevoFanControl.rc config.ui monitor.ui)
target_link_libraries(ClevoFanControl PRIVATE Qt6::Core Qt6::Widgets Qt6::Gui)
