cmake_minimum_required(VERSION 3.9)
set(PROJ usefull_macros)
set(MINOR_VERSION "1")
set(MID_VERSION "0")
set(MAJOR_VERSION "0")
set(PROJ_VERSION "${MAJOR_VERSION}.${MID_VERSION}.${MINOR_VERSION}")

project(${PROJ} VERSION ${PROJ_VERSION} LANGUAGES C)

# default flags
set(CMAKE_C_FLAGS "${CFLAGS} -O2")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS}")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS} -Wextra -Wall -Werror -W")
set(CMAKE_COLOR_MAKEFILE ON)

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

# cmake -DDEBUG=1 -> debugging
if(DEFINED DEBUG AND DEBUG EQUAL 1)
    set(CMAKE_BUILD_TYPE "Debug")
else()
    set(CMAKE_BUILD_TYPE "Release")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_definitions(-DEBUG)
	set(CMAKE_VERBOSE_MAKEFILE true)
	if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
		message("install to ${CMAKE_CURRENT_SOURCE_DIR}/install ")
		set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/install)
	endif()
	set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS_DEBUG})
else()
	set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS_RELEASE})
endif()

message("Build type: ${CMAKE_BUILD_TYPE}")

# here is one of two variants: all .c in directory or .c files in list
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SOURCES)

# directory should contain dir locale/ru for gettext translations
set(LCPATH ${CMAKE_SOURCE_DIR}/locale/ru)
if(NOT DEFINED LOCALEDIR)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(LOCALEDIR ${CMAKE_CURRENT_SOURCE_DIR}/locale)
    else()
        set(LOCALEDIR ${CMAKE_INSTALL_PREFIX}/share/locale)
    endif()
endif()

###### pkgconfig ######
# pkg-config modules (for pkg-check-modules)
#set(MODULES cfitsio fftw3)
# find packages:
#find_package(PkgConfig REQUIRED)
#pkg_check_modules(${PROJ} REQUIRED ${MODULES})

# external modules like OpenMP:
include(FindOpenMP)
if(OPENMP_FOUND)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
    add_definitions(-DOMP_FOUND)
endif()
###### additional flags ######
#list(APPEND ${PROJ}_LIBRARIES "-lfftw3_threads")

# gettext files
set(PO_FILE ${LCPATH}/messages.po)
set(MO_FILE ${LCPATH}/LC_MESSAGES/${PROJ}.mo)
set(RU_FILE ${LCPATH}/ru.po)

# exe file
#add_executable(${PROJ} ${SOURCES})
# library
add_library(${PROJ} SHARED ${SOURCES})
# library header files
set(LIBHEADER "usefull_macros.h")
# -I
include_directories(${${PROJ}_INCLUDE_DIRS})
# -L
link_directories(${${PROJ}_LIBRARY_DIRS})
# -D
add_definitions(-DLOCALEDIR=\"${LOCALEDIR}\"
        -DPACKAGE_VERSION=\"${PROJ_VERSION}\" -DGETTEXT_PACKAGE=\"${PROJ}\"
        -DMINOR_VERSION=\"${MINOR_VERSION}\" -DMID_VERSION=\"${MID_VERSION}\"
        -DMAJOR_VERSION=\"${MAJOR_VESION}\")

# -l
target_link_libraries(${PROJ} ${${PROJ}_LIBRARIES})

set(PCFILE "${CMAKE_BINARY_DIR}/${PROJ}.pc")
configure_file("${PROJ}.pc.in" ${PCFILE} @ONLY)

set_target_properties(${PROJ} PROPERTIES VERSION ${PROJ_VERSION})
set_target_properties(${PROJ} PROPERTIES PUBLIC_HEADER ${LIBHEADER})

# Installation of the program
include(GNUInstallDirs)
#install(TARGETS ${PROJ} DESTINATION "bin")
install(TARGETS ${PROJ} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${PCFILE} DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig)

# EXAMPLES
add_subdirectory(examples)

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
            DEPENDS ${RU_FILE}
        )
        add_dependencies(${PROJ} MO_FILE)
    else() # install .mo file
        install(FILES ${MO_FILE} DESTINATION "${LOCALEDIR}/ru/LC_MESSAGES")
    endif()
endif(NOT DEFINED NOGETTEXT)
