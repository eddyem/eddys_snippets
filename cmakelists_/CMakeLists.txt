cmake_minimum_required(VERSION 3.20)
set(PROJ ccd_capture)
set(PROJLIB ccdcapture)
set(MAJOR_VERSION "1")
set(MID_VERSION "2")
set(MINOR_VERSION "0")

set(LIBSRC ccdcapture.c)
set(SOURCES main.c cmdlnopts.c ccdfunc.c server.c client.c)
set(LIBHEADER "ccdcapture.h")

set(VERSION "${MAJOR_VERSION}.${MID_VERSION}.${MINOR_VERSION}")
project(${PROJ} VERSION ${VERSION} LANGUAGES C)
message("VER: ${VERSION}")

# list of options
option(DEBUG "Compile in debug mode" OFF)
option(IMAGEVIEW "Build with imageview module" OFF)
option(ZWO "Add support of ZWO cameras" OFF)
option(FLI "Add support of FLI cameras" OFF)
option(BASLER "Add support of BASLER cameras" OFF)
option(HIKROBOT "Add support of HIKROBOT cameras" OFF)
option(FLYCAP "Add support of Grasshopper FlyCap cameras" OFF)
option(APOGEE "Add support of Apogee cameras" OFF)
option(EXAMPLES "Some examples" OFF)

# default flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -W -Wextra -fno-builtin-strlen")

# change wrong behaviour with install prefix
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT AND CMAKE_INSTALL_PREFIX MATCHES "/usr/local")
    message("Change default install path to /usr")
    set(CMAKE_INSTALL_PREFIX "/usr")
endif()
message("Install dir prefix: ${CMAKE_INSTALL_PREFIX}")
if(NOT DEFINED LOCALEDIR)
    if(DEBUG)
        set(LOCALEDIR ${CMAKE_CURRENT_SOURCE_DIR}/locale)
    else()
        set(LOCALEDIR ${CMAKE_INSTALL_PREFIX}/share/locale)
    endif()
endif()

add_definitions(-D_XOPEN_SOURCE=1234 -D_DEFAULT_SOURCE -D_GNU_SOURCE -DLOCALEDIR=\"${LOCALEDIR}\"
        -DPACKAGE_VERSION=\"${VERSION}\" -DGETTEXT_PACKAGE=\"${PROJ}\"
        -DMINOR_VERSION=\"${MINOR_VERSION}\" -DMID_VERSION=\"${MID_VERSION}\"
        -DMAJOR_VERSION=\"${MAJOR_VESION}\")

set(CMAKE_COLOR_MAKEFILE ON)


# cmake -DDEBUG=yes -> debugging
if(DEBUG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Og -g3 -ggdb -Werror")
    add_definitions(-DEBUG)
    set(CMAKE_BUILD_TYPE DEBUG)
    set(CMAKE_VERBOSE_MAKEFILE "ON")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -march=native -fdata-sections -ffunction-sections -flto=auto")
#    add_definitions(-DEBUG)
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections -flto=auto")
    set(CMAKE_BUILD_TYPE RELEASE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")
find_package(CFITSIO REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(${PROJ} REQUIRED usefull_macros)
pkg_check_modules(${PROJLIB} REQUIRED usefull_macros)

include(FindOpenMP)
if(OPENMP_FOUND)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
    add_definitions(-DOMP_FOUND)
endif()

if(IMAGEVIEW)
    list(APPEND SOURCES events.c imageview.c)
    find_package(OpenGL REQUIRED)
    find_package(GLUT REQUIRED)
    find_package(X11 REQUIRED)
    find_package(Threads REQUIRED)
    list(APPEND ${PROJ}_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})
    add_definitions(-DIMAGEVIEW)
endif()

add_subdirectory(Dummy_cameras)

# additional modules with CCD/CMOS support
if(ZWO)
    add_subdirectory(ZWO_cameras)
endif()
if(FLI)
    add_subdirectory(FLI_cameras)
endif()
if(HIKROBOT)
    add_subdirectory(HIKROBOT_cameras)
endif()
if(BASLER)
    add_subdirectory(BASLER_cameras)
endif()
if(FLYCAP)
    add_subdirectory(GRH_cameras)
endif()
if(APOGEE)
    add_subdirectory(APOGEE_cameras)
endif()
if(EXAMPLES)
    add_subdirectory(examples)
endif()

# directory should contain dir locale/ru for gettext translations
set(LCPATH ${CMAKE_SOURCE_DIR}/locale/ru)

# gettext files
set(PO_FILE ${LCPATH}/messages.po)
set(MO_FILE ${LCPATH}/LC_MESSAGES/${PROJ}.mo)
set(RU_FILE ${LCPATH}/ru.po)

# exe & lib files
add_library(${PROJLIB} SHARED ${LIBSRC})
add_executable(${PROJ} ${SOURCES} ${PO_FILE} ${MO_FILE})
target_link_libraries(${PROJ} ${CFITSIO_LIBRARIES} ${X11_LIBRARIES} ${OPENGL_LIBRARIES} ${GLUT_LIBRARIES} ${${PROJ}_LIBRARIES} -lm ${CMAKE_DL_LIBS} ${PROJLIB})
target_link_libraries(${PROJLIB} ${CFITSIO_LIBRARIES} ${${PROJLIB}_LIBRARIES})
include_directories(${${PROJ}_INCLUDE_DIRS} .)
link_directories(${${PROJ}_LIBRARY_DIRS} )
set(PCFILE "${CMAKE_BINARY_DIR}/${PROJLIB}.pc")
configure_file("${PROJLIB}.pc.in" ${PCFILE} @ONLY)
set_target_properties(${PROJLIB} PROPERTIES VERSION ${VERSION})
set_target_properties(${PROJLIB} PROPERTIES PUBLIC_HEADER ${LIBHEADER})

# Installation of the program
install(FILES ${MO_FILE} DESTINATION "share/locale/ru/LC_MESSAGES")
install(TARGETS ${PROJ} DESTINATION "bin")
install(TARGETS ${PROJLIB} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${PCFILE} DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig)


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
    DEPENDS ${SOURCES}
)
# we need this to prewent ru.po & .mo from deleting by make clean
add_custom_command(
    OUTPUT ${MO_FILE}
    COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} ${RU_FILE} -o ${MO_FILE}
    DEPENDS ru_file_updated
)

add_custom_command(
    OUTPUT ru_file_updated
    COMMAND [ -f ${RU_FILE} ] && ${GETTEXT_MSGMERGE_EXECUTABLE} -Uis ${RU_FILE} ${PO_FILE} || cp ${PO_FILE} ${RU_FILE}
    COMMAND ${CMAKE_COMMAND} -E touch ru_file_updated
    DEPENDS ${PO_FILE}
)

add_custom_target(MO_FILE ALL DEPENDS ${MO_FILE})
