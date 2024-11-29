project(ClevoFanControl-cmd)

set(CFC_SOURCE_CMD
main.cpp 
${CFC_SOURCE_DIR}/ClevoEcAccessor.cpp)
if(CMAKE_SYSTEM_NAME MATCHES "Windows")
set(CFC_SOURCE_GUI ${CFC_SOURCE_GUI} ${CFC_SOURCE_DIR}/winRing0Api.cpp)
endif()

add_executable(ClevoFanControl-cmd ${CFC_SOURCE_CMD})
