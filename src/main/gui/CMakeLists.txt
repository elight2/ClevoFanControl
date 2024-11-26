project(ClevoFanControl-gui)

include_directories("../../../include")

# qt
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)
if(CMAKE_BUILD_TYPE MATCHES "Release")
add_definitions(-DQT_NO_DEBUG_OUTPUT)
endif()

# windows no window
IF (CMAKE_SYSTEM_NAME MATCHES "Windows")
if(CMAKE_BUILD_TYPE MATCHES "Release")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mwindows")
endif()
ENDIF ()
# source
set(CFC_SOURCE_GUI
ClevoFanControl.cpp 
ConfigManager.cpp 
FanController.cpp 
CFCconfig.cpp 
CFCmonitor.cpp 
main.cpp 
../ClevoEcAccessor.cpp)
if(CMAKE_SYSTEM_NAME MATCHES "Windows")
set(CFC_SOURCE_GUI ${CFC_SOURCE_GUI} ../winRing0Api.cpp)
endif()

# ui
set(CFC_UI_DIR ../../ui)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_AUTOUIC_SEARCH_PATHS} ${CFC_UI_DIR})
set(CFC_UI ${CFC_UI_DIR}/config.ui ${CFC_UI_DIR}/monitor.ui)

# rc
set(CFC_RES_DIR ../../resources)
set(CFC_RES ${CFC_RES_DIR}/ClevoFanControl.qrc ${CFC_RES_DIR}/ClevoFanControl.rc)

add_executable(ClevoFanControl-gui ${CFC_SOURCE_GUI} ${CFC_UI} ${CFC_RES})
target_link_libraries(ClevoFanControl-gui PRIVATE Qt6::Core Qt6::Widgets Qt6::Gui)
