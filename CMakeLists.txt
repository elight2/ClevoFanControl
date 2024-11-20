cmake_minimum_required(VERSION 3.21)
project(ClevoFanControl VERSION 2.3.0 LANGUAGES CXX)

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
include_directories("/usr/include/qt6/")
set(CMAKE_PREFIX_PATH "/usr/lib/cmake/")
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

set(CFC_SOURCE_DIR src/main)
set(CFC_SOURCE
${CFC_SOURCE_DIR}/main.cpp
${CFC_SOURCE_DIR}/ClevoFanControl.cpp
${CFC_SOURCE_DIR}/ConfigManager.cpp
${CFC_SOURCE_DIR}/FanController.cpp
${CFC_SOURCE_DIR}/CFCconfig.cpp
${CFC_SOURCE_DIR}/CFCmonitor.cpp
)
set(CFC_UI_DIR src/ui)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_AUTOUIC_SEARCH_PATHS} ${CFC_UI_DIR})
set(CFC_UI ${CFC_UI_DIR}/config.ui ${CFC_UI_DIR}/monitor.ui)
set(CFC_RES_DIR src/resources)
set(CFC_RES ${CFC_RES_DIR}/ClevoFanControl.qrc ${CFC_RES_DIR}/ClevoFanControl.rc)
add_executable(ClevoFanControl ${CFC_SOURCE} ${CFC_UI} ${CFC_RES})

target_link_libraries(ClevoFanControl PRIVATE Qt6::Core Qt6::Widgets Qt6::Gui)
