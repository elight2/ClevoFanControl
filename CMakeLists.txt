cmake_minimum_required(VERSION 3.21)
project(ClevoFanControl)

set(CMAKE_BUILD_TYPE "Release")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC on)
set(CMAKE_AUTOUIC on)
set(CMAKE_AUTORCC on)
set(CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
include_directories("/home/main/User/Program/Qt/5.15.2/gcc_64/include")
set(CMAKE_PREFIX_PATH "/home/main/User/Program/Qt/5.15.2/gcc_64/lib/cmake")
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Windows")
include_directories("D:\\Applications\\Qt\\5.15.2\\mingw81_64\\include")
set(CMAKE_PREFIX_PATH "D:\\Applications\\Qt\\5.15.2\\mingw81_64\\lib\\cmake")
if(CMAKE_BUILD_TYPE MATCHES "Release")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mwindows")
endif()
ENDIF (CMAKE_SYSTEM_NAME MATCHES "Linux")

if(CMAKE_BUILD_TYPE MATCHES "Release")
add_definitions(-DQT_NO_DEBUG_OUTPUT)
endif()

find_package(Qt5 REQUIRED COMPONENTS Widgets Core Gui)

qt5_add_resources(QRC_FILES ClevoFanControl.qrc)
qt5_wrap_ui(ui_FILES config.ui monitor.ui)

add_executable(ClevoFanControl main.cpp ClevoFanControl.cpp ${QRC_FILES} ClevoFanControl.rc ${ui_FILES})

target_link_libraries(ClevoFanControl Qt5::Core Qt5::Gui Qt5::Widgets)
