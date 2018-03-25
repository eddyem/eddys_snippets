cmake_minimum_required(VERSION 2.8)
set(PROJ fitsread)
set(MINOR_VERSION "1")
set(MID_VERSION "0")
set(MAJOR_VERSION "0")
set(VERSION "${MAJOR_VERSION}.${MID_VERSION}.${MINOR_VERSION}")

enable_language(C)

message("VER: ${VERSION}")
# threads number definition
if(NOT DEFINED PROCESSOR_COUNT)
  set(PROCESSOR_COUNT 2) # by default 2 cores
  set(cpuinfo_file "/proc/cpuinfo")
  if(EXISTS "${cpuinfo_file}")
    file(STRINGS "${cpuinfo_file}" procs REGEX "^processor.: [0-9]+$")
    list(LENGTH procs PROCESSOR_COUNT)
  endif()
endif()
add_definitions(-DTHREAD_NUMBER=${PROCESSOR_COUNT})
message("In multithreaded operations will use ${PROCESSOR_COUNT} threads")

# default flags
set(CFLAGS -O2 -Wextra -Wall -Werror -W -Wno-trampolines -std=gnu99)

set(CMAKE_COLOR_MAKEFILE ON)

# here is one of two variants: all .c in directory or .c files in list
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SOURCES)

# cmake -DDEBUG=1 -> debugging
if(DEFINED DEBUG)
    add_definitions(-DEBUG)
endif()

# directory should contain dir locale/ru for gettext translations
set(LCPATH ${CMAKE_SOURCE_DIR}/locale/ru)

if(NOT DEFINED LOCALEDIR)
    if(DEFINED DEBUG)
        set(LOCALEDIR ${CMAKE_CURRENT_SOURCE_DIR}/locale)
    else()
        set(LOCALEDIR ${CMAKE_INSTALL_PREFIX}/share/locale)
    endif()
endif()

###### pkgconfig ######
# pkg-config modules (for pkg-check-modules)
set(MODULES cfitsio fftw3)

# find packages:
find_package(PkgConfig REQUIRED)

pkg_check_modules(${PROJ} REQUIRED ${MODULES})

# external modules like OpenMP:
include(FindOpenMP)
if(OPENMP_FOUND)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
    add_definitions(-DOMP_FOUND)
endif()

###### additional flags ######
list(APPEND ${PROJ}_LIBRARIES "-lfftw3_threads")

project(${PROJ})
# change wrong behaviour with install prefix
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT AND CMAKE_INSTALL_PREFIX MATCHES "/usr/local")
    message("Change default install path to /usr")
    set(CMAKE_INSTALL_PREFIX "/usr")
endif()
message("Install dir prefix: ${CMAKE_INSTALL_PREFIX}")

# gettext files
set(PO_FILE ${LCPATH}/messages.po)
set(MO_FILE ${LCPATH}/LC_MESSAGES/${PROJ}.mo)
set(RU_FILE ${LCPATH}/ru.po)
set(CTAGS_FILE ${CMAKE_SOURCE_DIR}/${PROJ}.c.tags)

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
target_link_libraries(${PROJ} ${${PROJ}_LIBRARIES} -lm)

# Installation of the program
INSTALL(FILES ${MO_FILE} DESTINATION "share/locale/ru/LC_MESSAGES")
        #PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ)
INSTALL(TARGETS ${PROJ} DESTINATION "bin")

###### gettext & ctags ######
if(DEFINED EBUG)
    message("Generate locale & tags files")
    find_package(Gettext REQUIRED)
    find_program(GETTEXT_XGETTEXT_EXECUTABLE xgettext)
    if(NOT GETTEXT_XGETTEXT_EXECUTABLE OR NOT GETTEXT_MSGFMT_EXECUTABLE)
        message(FATAL_ERROR "xgettext not found")
    endif()
    file(MAKE_DIRECTORY ${LCPATH})
    file(MAKE_DIRECTORY ${LCPATH}/LC_MESSAGES)

    add_custom_command(
        OUTPUT ${PO_FILE}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMAND ${GETTEXT_XGETTEXT_EXECUTABLE} --from-code=utf-8 ${SOURCES} -c -k_ -kN_ -o ${PO_FILE}
        COMMAND sed -i 's/charset=.*\\\\n/charset=koi8-r\\\\n/' ${PO_FILE}
        COMMAND enconv ${PO_FILE}
        DEPENDS ${SOURCES}
    )
    # we need this to prevent ru.po & .mo from deleting by make clean
    add_custom_target(
        RU_FILE
        COMMAND [ -f ${RU_FILE} ] && ${GETTEXT_MSGMERGE_EXECUTABLE} -Uis ${RU_FILE} ${PO_FILE} || cp ${PO_FILE} ${RU_FILE}
        DEPENDS ${PO_FILE} ${SOURCES}
    )
    add_custom_target(
        MO_FILE
        COMMAND make RU_FILE && ${GETTEXT_MSGFMT_EXECUTABLE} ${RU_FILE} -o ${MO_FILE}
        DEPENDS ${SOURCES}
    )
    add_dependencies(${PROJ} MO_FILE)

    set(CFLAGS_A CFLAGS=${CFLAGS})
    add_custom_target(ctags
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMAND CFLAGS=-DEBUG geany -g ${CTAGS_FILE} ${SOURCES}
        DEPENDS ${SOURCES}
    )
    add_dependencies(${PROJ} ctags)
endif()
