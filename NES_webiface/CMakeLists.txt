cmake_minimum_required(VERSION 3.0)
set(PROJ NES_web)
set(MINOR_VERSION "1")
set(MID_VERSION "0")
set(MAJOR_VERSION "0")
set(VERSION "${MAJOR_VERSION}.${MID_VERSION}.${MINOR_VERSION}")

project(${PROJ} VERSION ${PROJ_VERSION} LANGUAGES C)

message("VER: ${VERSION}")

# default flags
set(CMAKE_C_FLAGS_RELEASE "")
set(CMAKE_C_FLAGS_DEBUG "")
set(CMAKE_C_FLAGS "-O2 -std=gnu99")

set(CMAKE_COLOR_MAKEFILE ON)

# here is one of two variants: all .c in directory or .c files in list
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SOURCES)

# cmake -DDEBUG=1 -> debugging
if(DEFINED EBUG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wextra -Wall -Werror -W")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra -Wall -Werror -W")
    set(CMAKE_BUILD_TYPE DEBUG)
    set(CMAKE_VERBOSE_MAKEFILE "ON")
    add_definitions(-DEBUG)
else()
    set(CMAKE_BUILD_TYPE RELEASE)
endif()

#pthreads
find_package(Threads REQUIRED)

###### pkgconfig ######
# pkg-config modules (for pkg-check-modules)
set(MODULES usefull_macros sqlite3)

# find packages:
find_package(PkgConfig REQUIRED)
pkg_check_modules(${PROJ} REQUIRED ${MODULES})

###### additional flags ######
list(APPEND ${PROJ}_LIBRARIES "-lonion -lcrypt")

# change wrong behaviour with install prefix
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT AND CMAKE_INSTALL_PREFIX MATCHES "/usr/local")
else()
    message("Change default install path to /usr/local")
    set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
message("Install dir prefix: ${CMAKE_INSTALL_PREFIX}")

# exe file
add_executable(${PROJ} ${SOURCES})
# -I
include_directories(${${PROJ}_INCLUDE_DIRS})
# -L
link_directories(${${PROJ}_LIBRARY_DIRS})
# -D
add_definitions(${CFLAGS} -DLOCALEDIR=\"${LOCALEDIR}\"
        -DPACKAGE_VERSION=\"${VERSION}\" -DGETTEXT_PACKAGE=\"${PROJ}\"
        -DMINOR_VERSION=\"${MINOR_VERSION}\" -DMID_VERSION=\"${MID_VERSION}\"
        -DMAJOR_VERSION=\"${MAJOR_VESION}\")

# -l
target_link_libraries(${PROJ} ${${PROJ}_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} -lm)

# Installation of the program
INSTALL(TARGETS ${PROJ} DESTINATION "bin")
