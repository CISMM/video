# - Find LEWOSSIM
# Find the LEWOSSIM headers and libraries.
#
#  LEWOSSIM_INCLUDE_DIR  - where to find header files
#  LEWOSSIM_LIBRARIES    - List of libraries when using LEWOSSIM.
#  LEWOSSIM_FOUND        - True if LEWOSSIM found.
#
# Original Author:
# 2011 Russ Taylor <taylorr@cs.unc.edu>
#
# Distributed under the Boost Software License, Version 1.0.
#    (See accompanying file LICENSE_1_0.txt or copy at
#          http://www.boost.org/LICENSE_1_0.txt)

if(TARGET LEWOSSIM)
	# Look for the header file.
	find_path(LEWOSSIM_INCLUDE_DIR NAMES LEWOS_sim.h
			PATHS ${LEWOSSIM_SOURCE_DIR})

	set(LEWOSSIM_LIBRARY "LEWOS_sim")

	set(LEWOS_LIBRARIES
		${LEWOSSIM_LIBRARY}
		CACHE STRING "Libraries needed for LEWOSSIM")
	if(NOT WIN32)
		list(APPEND LEWOSSIM_LIBRARIES m)
	endif()

else()
	set(LEWOSSIM_ROOT_DIR
		"${LEWOSSIM_ROOT_DIR}"
		CACHE
		PATH
		"Root directory to search for LEWOSSIM")

	if("${CMAKE_SIZEOF_VOID_P}" MATCHES "8")
		set(_libsuffixes lib64 lib)
	else()
		set(_libsuffixes lib)
	endif()

	# Look for the header file.
	find_path(LEWOSSIM_INCLUDE_DIR
		NAMES
		LEWOS_sim.h
		HINTS
		"${LEWOSSIM_ROOT_DIR}"
		PATH_SUFFIXES
		include
		PATHS
		"C:/Program Files/LEWOSSIM/include"
		/usr/local/include)

	# Look for the library.
	find_library(LEWOSSIM_LIBRARY
		NAMES
		LEWOS_sim
		HINTS
		"${LEWOSSIM_ROOT_DIR}"
		PATH_SUFFIXES
		${_libsuffixes}
		PATHS
		"C:/Program Files/LEWOSSIM/lib"
		/usr/local/lib)

endif()

if (LEWOSSIM_INCLUDE_DIR AND LEWOSSIM_LIBRARY)
	SET(LEWOSSIM_FOUND TRUE)
endif (LEWOSSIM_INCLUDE_DIR AND LEWOSSIM_LIBRARY)

if(LEWOSSIM_FOUND)
	set(LEWOSSIM_LIBRARIES
		${LEWOSSIM_LIBRARY}
		CACHE STRING "Libraries needed for LEWOSSIM")
	if(NOT WIN32)
		list(APPEND LEWOSSIM_LIBRARIES m)
	endif()

	mark_as_advanced(LEWOSSIM_ROOT_DIR)
else()
	set(LEWOSSIM_LIBRARIES)
	set(LEWOSSIM_INCLUDE_DIRS)
endif()

mark_as_advanced(LEWOSSIM_LIBRARIES LEWOSSIM_INCLUDE_DIR)

