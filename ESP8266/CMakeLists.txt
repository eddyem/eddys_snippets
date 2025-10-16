cmake_minimum_required(VERSION 3.16)
set(PROJ esp8266)

project(${PROJ} LANGUAGES C)

set(CMAKE_COLOR_MAKEFILE ON)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SOURCES)

option(DEBUG "Compile in debug mode" OFF)

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS} -Og -g3 -ggdb -Werror")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS} -O3 -march=native -fdata-sections -ffunction-sections -flto=auto")

if(DEBUG)
    add_definitions(-DEBUG)
    set(CMAKE_BUILD_TYPE DEBUG)
    set(CMAKE_VERBOSE_MAKEFILE "ON")
else()
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections -flto=auto")
    set(CMAKE_BUILD_TYPE RELEASE)
endif()


find_package(PkgConfig REQUIRED)
pkg_check_modules(${PROJ} REQUIRED usefull_macros)
add_executable(esp8266 ${SOURCES})
target_link_libraries(${PROJ} ${${PROJ}_LIBRARIES} -lm)

include(GNUInstallDirs)
install(TARGETS ${PROJ}
#    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
