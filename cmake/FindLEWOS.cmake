# - Find LEWOS
# Find the LEWOS headers and libraries.
#
#  LEWOS_INCLUDE_DIR  - where to find header files
#  LEWOS_LIBRARIES    - List of libraries when using LEWOS.
#  LEWOS_FOUND        - True if LEWOS found.
#
# Original Author:
# 2011 Russ Taylor <taylorr@cs.unc.edu>
#
# Distributed under the Boost Software License, Version 1.0.
#    (See accompanying file LICENSE_1_0.txt or copy at
#          http://www.boost.org/LICENSE_1_0.txt)

if(TARGET LEWOS)
	# Look for the header file.
	find_path(LEWOS_INCLUDE_DIR NAMES LEWOS_api.h
			PATHS ${LEWOS_SOURCE_DIR})

	set(LEWOS_xlate_LIBRARY "LEWOS_xlate")
	set(LEWOS_xmit_LIBRARY "LEWOS_xmit")
	set(LEWOS_api_LIBRARY "LEWOS_api")

	set(LEWOS_LIBRARIES
		${LEWOS_api_LIBRARY}
		${LEWOS_xmit_LIBRARY}
		${LEWOS_xlate_LIBRARY}
		CACHE STRING "Libraries needed for LEWOS")
	if(NOT WIN32)
		list(APPEND LEWOS_LIBRARIES m)
	endif()

else()
	set(LEWOS_ROOT_DIR
		"${LEWOS_ROOT_DIR}"
		CACHE
		PATH
		"Root directory to search for LEWOS")

	if("${CMAKE_SIZEOF_VOID_P}" MATCHES "8")
		set(_libsuffixes lib64 lib)
	else()
		set(_libsuffixes lib)
	endif()

	# Look for the header file.
	find_path(LEWOS_INCLUDE_DIR
		NAMES
		LEWOS_xlate.h
		HINTS
		"${LEWOS_ROOT_DIR}"
		PATH_SUFFIXES
		include
		PATHS
		"C:/Program Files/LEWOS/include"
		/usr/local/include)

	# Look for the xlate library.
	find_library(LEWOS_xlate_LIBRARY
		NAMES
		LEWOS_xlate
		HINTS
		"${LEWOS_ROOT_DIR}"
		PATH_SUFFIXES
		${_libsuffixes}
		PATHS
		"C:/Program Files/LEWOS/lib"
		/usr/local/lib)

	# Look for the xmit library.
	find_library(LEWOS_xmit_LIBRARY
		NAMES
		LEWOS_xmit
		HINTS
		"${LEWOS_ROOT_DIR}"
		PATH_SUFFIXES
		${_libsuffixes}
		PATHS
		"C:/Program Files/LEWOS/lib"
		/usr/local/lib)

	# Look for the api library.
	find_library(LEWOS_api_LIBRARY
		NAMES
		LEWOS_api
		HINTS
		"${LEWOS_ROOT_DIR}"
		PATH_SUFFIXES
		${_libsuffixes}
		PATHS
		"C:/Program Files/LEWOS/lib"
		/usr/local/lib)

endif()

if (LEWOS_INCLUDE_DIR AND LEWOS_xlate_LIBRARY)
	SET(LEWOS_FOUND TRUE)
endif (LEWOS_INCLUDE_DIR AND LEWOS_xlate_LIBRARY)

if(LEWOS_FOUND)
	set(LEWOS_LIBRARIES
		${LEWOS_api_LIBRARY}
		${LEWOS_xmit_LIBRARY}
		${LEWOS_xlate_LIBRARY}
		CACHE STRING "Libraries needed for LEWOS")
	if(NOT WIN32)
		list(APPEND LEWOS_LIBRARIES m)
	endif()

	mark_as_advanced(LEWOS_ROOT_DIR)
else()
	set(LEWOS_LIBRARIES)
	set(LEWOS_INCLUDE_DIRS)
endif()

mark_as_advanced(LEWOS_LIBRARIES LEWOS_INCLUDE_DIR)

