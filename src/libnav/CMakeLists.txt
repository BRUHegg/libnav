FILE(GLOB LIBNAV_SRC "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
FILE(GLOB LIBNAV_HDR "${CMAKE_CURRENT_SOURCE_DIR}/libnav/*.hpp")

add_library(libnav STATIC ${LIBNAV_SRC} ${LIBNAV_HDR})
target_include_directories(libnav INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})


if(UNIX AND NOT APPLE)
    set_property(TARGET libnav PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()
#target_compile_options(libnav PUBLIC -Wall -Wextra -Werror -g)
