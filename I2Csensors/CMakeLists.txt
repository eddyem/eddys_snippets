cmake_minimum_required(VERSION 3.9)
set(PROJ i2csensorsPTH)
set(MINOR_VERSION "1")
set(MID_VERSION "1")
set(MAJOR_VERSION "0")
set(VERSION "${MAJOR_VERSION}.${MID_VERSION}.${MINOR_VERSION}")

project(${PROJ} VERSION ${VERSION} LANGUAGES C)

# default flags
set(CMAKE_C_FLAGS "${CFLAGS} -O2 -Wno-address-of-packed-member")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS}")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS} -Wextra -Wall -Werror -W")
set(CMAKE_COLOR_MAKEFILE ON)

option(DEBUG "Compile in debug mode" OFF)
option(EXAMPLES "Build examples" ON)

if(DEBUG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Og -g3 -ggdb -Werror")
    add_definitions(-DEBUG)
    set(CMAKE_BUILD_TYPE DEBUG)
    set(CMAKE_VERBOSE_MAKEFILE "ON")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -march=native -fdata-sections -ffunction-sections -flto=auto")
#    add_definitions(-DEBUG)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections -flto=auto")
    set(CMAKE_BUILD_TYPE RELEASE)
endif()
message("Build type: ${CMAKE_BUILD_TYPE}")

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SOURCES)

find_package(PkgConfig REQUIRED)
pkg_check_modules(${PROJ} REQUIRED usefull_macros)

# library
add_library(${PROJ} SHARED ${SOURCES})
# library header files
set(LIBHEADER "i2csensorsPTH.h")
# -I
include_directories(${${PROJ}_INCLUDE_DIRS})
# -L
link_directories(${${PROJ}_LIBRARY_DIRS})
# -D
add_definitions(-DPACKAGE_VERSION=\"${VERSION}\" -DMINOR_VERSION=\"${MINOR_VERSION}\"
        -DMID_VERSION=\"${MID_VERSION}\" -DMAJOR_VERSION=\"${MAJOR_VESION}\")
# -l
target_link_libraries(${PROJ} ${${PROJ}_LIBRARIES})

set(PCFILE "${CMAKE_BINARY_DIR}/${PROJ}.pc")
configure_file("${PROJ}.pc.in" ${PCFILE} @ONLY)

set_target_properties(${PROJ} PROPERTIES VERSION ${VERSION})
set_target_properties(${PROJ} PROPERTIES PUBLIC_HEADER ${LIBHEADER})

# Installation of the program
include(GNUInstallDirs)
install(TARGETS ${PROJ} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${PCFILE} DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig)

# EXAMPLES
if(EXAMPLES)
    add_subdirectory(examples)
endif()

###### gettext ######
if(NOT DEFINED NOGETTEXT)
    add_definitions(-DGETTEXT)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        message("Generate locale files @ make")
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
            COMMAND ${GETTEXT_XGETTEXT_EXECUTABLE} --from-code=koi8-r ${SOURCES} -c -k_ -kN_ -o ${PO_FILE}
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
            DEPENDS ${RU_FILE}
        )
        add_dependencies(${PROJ} MO_FILE)
    else() # install .mo file
        install(FILES ${MO_FILE} DESTINATION "${LOCALEDIR}/ru/LC_MESSAGES")
    endif()
endif(NOT DEFINED NOGETTEXT)
