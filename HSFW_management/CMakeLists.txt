cmake_minimum_required(VERSION 2.8)
set(PROJ HSFW_manage)
set(MINOR_VERSION "1")
set(MID_VERSION "0")
set(MAJOR_VERSION "0")
set(VERSION "${MAJOR_VERSION}.${MID_VERSION}.${MINOR_VERSION}")
# new policy for version control
if(POLICY CMP0048)
	cmake_policy(SET CMP0048 OLD)
endif(POLICY CMP0048)

enable_language(C)

message("VERSION: ${VERSION}")
# default flags
set(CFLAGS -O2 -Wextra -Wall -Werror -W -std=gnu99)

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
set(MODULES libudev)
# find packages:
find_package(PkgConfig REQUIRED)
pkg_check_modules(${PROJ} REQUIRED ${MODULES})
pkg_check_modules(USB libusb)
if(NOT USB_FOUND)
	pkg_check_modules(USB REQUIRED libusb-1.0)
endif()

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

# exe file
add_executable(${PROJ} ${SOURCES})
# -I
include_directories(${${PROJ}_INCLUDE_DIRS} ${USB_INCLUDE_DIRS})
# -L
link_directories(${${PROJ}_LIBRARY_DIRS} ${USB_LIBRARY_DIRS})
# -D
add_definitions(${CFLAGS} -DLOCALEDIR=\"${LOCALEDIR}\"
		-DPACKAGE_VERSION=\"${VERSION}\" -DGETTEXT_PACKAGE=\"${PROJ}\"
		-DMINOR_VERSION=\"${MINOR_VERSION}\" -DMID_VERSION=\"${MID_VERSION}\"
		-DMAJOR_VERSION=\"${MAJOR_VESION}\")
# -l
target_link_libraries(${PROJ} ${${PROJ}_LIBRARIES} ${USB_LIBRARIES})

# Installation of the program
INSTALL(FILES ${MO_FILE} DESTINATION "share/locale/ru/LC_MESSAGES")
INSTALL(TARGETS ${PROJ} DESTINATION "bin")

###### gettext ######
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

# we need this to prewent ru.po & .mo from deleting by make clean
add_custom_command(
	OUTPUT ${MO_FILE}
	COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} ${RU_FILE} -o ${MO_FILE}
	DEPENDS ${RU_FILE} ru_file_updated
)

add_custom_command(
	OUTPUT ru_file_updated
	COMMAND [ -f ${RU_FILE} ] && ${GETTEXT_MSGMERGE_EXECUTABLE} -Uis ${RU_FILE} ${PO_FILE} || cp ${PO_FILE} ${RU_FILE}
	COMMAND ${CMAKE_COMMAND} -E touch ru_file_updated
	BYPRODUCTS ${RU_FILE}
	DEPENDS ${PO_FILE}
)

add_custom_target(MO_FILE ALL DEPENDS ${MO_FILE})
