# RUN cmake -DSTANDALONE=1 for standalone example
# -DEBUG=1 for debugging
# -DNOGETTEXT=1 for compilation without gettext support
#
# Into cmake file in parent directory add this:
#~ add_subdirectory(image_view_module)
#~ if(IMAGEVIEW_FOUND)
	#~ message("Found OpenGL. Will use ${IMLIB} to show data")
	#~ target_link_libraries(${PROJ} ${${PROJ}_LIBRARIES} -lm ${IMLIB})
	#~ add_definitions(-DIMAGEVIEW)
#~ else()
	#~ message("not found image view module")
	#~ target_link_libraries(${PROJ} ${${PROJ}_LIBRARIES} -lm)
#~ endif()

cmake_minimum_required(VERSION 2.8)
set(IMLIB image_view_module)

if(DEFINED STANDALONE)
	set(PROJ ${IMLIB})
	project(${PROJ})
endif()

set(CFLAGS -O2 -Wextra -Wall -Werror -W -std=gnu99)
set(CMAKE_COLOR_MAKEFILE ON)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} IMSOURCES)

find_package(PkgConfig REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)

if(NOT GLUT_FOUND OR NOT OPENGL_FOUND)
	message("GLUT library not found, image view won't be available")
else()
# exe file

if(NOT DEFINED STANDALONE)
	list(REMOVE_ITEM IMSOURCES main.c)
	add_library(image_view_module ${IMSOURCES})

	set(IMAGEVIEW_FOUND TRUE PARENT_SCOPE)
	set(IMLIB ${IMLIB} PARENT_SCOPE)
	if(DEFINED DEBUG)
		set(LCPATH ${CMAKE_CURRENT_SOURCE_DIR}/locale/ru)
		set(PO_FILE ${LCPATH}/messages.po)
		set(RU_FILE ${LCPATH}/ru.po)
		set(IMPO_FILE ${RU_FILE} PARENT_SCOPE)
		add_custom_command(
			OUTPUT ${PO_FILE}
			COMMAND ${GETTEXT_XGETTEXT_EXECUTABLE} -D ${CMAKE_CURRENT_SOURCE_DIR} --from-code=koi8-r ${IMSOURCES} -c -k_ -kN_ -o ${PO_FILE}
			COMMAND sed 's/charset=UTF-8/charset=koi8-r/' ${PO_FILE} | enconv > tmp && mv -f tmp ${PO_FILE}
			COMMAND ${GETTEXT_MSGMERGE_EXECUTABLE} -Uis ${RU_FILE} ${PO_FILE}
			DEPENDS ${IMSOURCES}
		)
	endif()
else()
	# cmake -DEBUG=1 -> debugging
	if(DEFINED EBUG)
		add_definitions(-DEBUG)
		#set(CMAKE_VERBOSE_MAKEFILE ON)
	endif()

	set(MINOR_VERSION "1")
	set(MID_VERSION "0")
	set(MAJOR_VERSION "0")
	set(VERSION "${MAJOR_VERSION}.${MID_VERSION}.${MINOR_VERSION}")

	message("VER: ${VERSION}")
	# change wrong behaviour with install prefix
	if(CMAKE_INSTALL_PREFIX MATCHES "/usr/local")
		message("Change default install path to /usr")
		set(CMAKE_INSTALL_PREFIX "/usr")
	endif()
	message("Install dir prefix: ${CMAKE_INSTALL_PREFIX}")
	add_definitions(${CFLAGS} -DLOCALEDIR=\"${LOCALEDIR}\"
		-DPACKAGE_VERSION=\"${VERSION}\" -DGETTEXT_PACKAGE=\"${PROJ}\"
		-DMINOR_VERSION=\"${MINOR_VERSION}\" -DMID_VERSION=\"${MID_VERSION}\"
		-DMAJOR_VERSION=\"${MAJOR_VESION}\" -DPROJNAME=\"${PROJ}\")

	if(NOT DEFINED NOGETTEXT)
	# directory should contain dir locale/ru for gettext translations
		set(LCPATH ${CMAKE_SOURCE_DIR}/locale/ru)
		if(NOT DEFINED LOCALEDIR)
			if(DEFINED DEBUG)
				set(LOCALEDIR ${CMAKE_CURRENT_SOURCE_DIR}/locale)
			else()
				set(LOCALEDIR ${CMAKE_INSTALL_PREFIX}/share/locale)
			endif()
		endif()
		# gettext files
		set(PO_FILE ${LCPATH}/messages.po)
		set(MO_FILE ${LCPATH}/LC_MESSAGES/${PROJ}.mo)
		set(RU_FILE ${LCPATH}/ru.po)
	else()
		add_definitions(-DNOGETTEXT)
		set(PO_FILE)
		set(MO_FILE)
	endif()

	add_executable(${PROJ} ${IMSOURCES} ${PO_FILE} ${MO_FILE})

	# Installation of the program
	if(NOT DEFINED DEBUG)
		INSTALL(FILES ${MO_FILE} DESTINATION "share/locale/ru/LC_MESSAGES")
				#PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ)
		INSTALL(TARGETS ${PROJ} DESTINATION "bin")
				#PERMISSIONS OWNER_WRITE OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
		# Script to be executed at installation time (kind of post-intallation script) to
		# change the right accesses on the installed files
		#INSTALL(SCRIPT inst.cmake)
	else()
		install(CODE "MESSAGE(\"Don't install in DEBUG mode! First run cmake without -DEBUG defined.\")")
	endif(NOT DEFINED DEBUG)

	if(NOT DEFINED NOGETTEXT)
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
			COMMAND ${GETTEXT_XGETTEXT_EXECUTABLE} --from-code=utf-8 ${IMSOURCES} -c -k_ -kN_ -o ${PO_FILE}
			COMMAND sed -i 's/charset=.*\\\\n/charset=koi8-r\\\\n/' ${PO_FILE}
			COMMAND enconv ${PO_FILE}
			DEPENDS ${IMSOURCES}
		)
		# we need this to prewent ru.po from deleting by make clean
		add_custom_target(
			RU_FILE
			COMMAND [ -f ${RU_FILE} ] && ${GETTEXT_MSGMERGE_EXECUTABLE} -Uis ${RU_FILE} ${PO_FILE} || cp ${PO_FILE} ${RU_FILE}
			DEPENDS ${PO_FILE}
		)
		add_custom_command(
			OUTPUT ${MO_FILE}
			COMMAND make RU_FILE && ${GETTEXT_MSGFMT_EXECUTABLE} ${RU_FILE} -o ${MO_FILE}
			DEPENDS ${PO_FILE}
		)
	endif(NOT DEFINED NOGETTEXT)
endif(NOT DEFINED STANDALONE)
target_link_libraries(${IMLIB} ${OPENGL_LIBRARIES} ${GLUT_LIBRARIES} -lm -lpthread)
include_directories(${${IMLIB}_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIR} ${GLUT_INCLUDE_DIR})
link_directories(${${IMLIB}_LIBRARY_DIRS})
endif(NOT GLUT_FOUND OR NOT OPENGL_FOUND)


