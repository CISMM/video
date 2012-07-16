# - Find Panoptes
# Find the Panoptes headers and libraries.
#
#  Panoptes_INCLUDE_DIR  - where to find header files
#  Panoptes_LIBRARIES    - List of libraries when using Panoptes.
#  Panoptes_FOUND        - True if Panoptes found.
#
# Original Author:
# 2011 Russ Taylor <taylorr@cs.unc.edu>
#
# Copyright UNC Chapel Hill.  Not available for use unless licensed.

if(TARGET Panoptes)
	# Look for the header file.
	find_path(Panoptes_INCLUDE_DIR NAMES Panoptes.h
			PATHS ${Panoptes_SOURCE_DIR})

	set(Panoptes_LIBRARY "Panoptes")

	set(Panoptes_LIBRARIES
		${Panoptes_LIBRARY}
		CACHE STRING "Libraries needed for Panoptes")
	if(NOT WIN32)
		list(APPEND Panoptes_LIBRARIES m)
	endif()

else()
	set(Panoptes_ROOT_DIR
		"${Panoptes_ROOT_DIR}"
		CACHE
		PATH
		"Root directory to search for Panoptes")

	if("${CMAKE_SIZEOF_VOID_P}" MATCHES "8")
		set(_libsuffixes lib64 lib)
	else()
		set(_libsuffixes lib)
	endif()

	# Look for the header file.
	find_path(Panoptes_INCLUDE_DIR
		NAMES
		Panoptes.h
		HINTS
		"${Panoptes_ROOT_DIR}"
		PATH_SUFFIXES
		include
		PATHS
		"C:/Program Files/Panoptes/include"
		C:/usr/local/include
		/usr/local/include)

	# Look for the library.
	find_library(Panoptes_LIBRARY
		NAMES
		Panoptes
		HINTS
		"${Panoptes_ROOT_DIR}"
		PATH_SUFFIXES
		${_libsuffixes}
		PATHS
		"C:/Program Files/Panoptes/lib"
		C:/usr/local/lib
		/usr/local/lib)

endif()

if (Panoptes_INCLUDE_DIR AND Panoptes_LIBRARY)
	SET(Panoptes_FOUND TRUE)
endif (Panoptes_INCLUDE_DIR AND Panoptes_LIBRARY)

if(Panoptes_FOUND)
	set(Panoptes_LIBRARIES
		${Panoptes_LIBRARY}
		CACHE STRING "Libraries needed for Panoptes")
	if(NOT WIN32)
		list(APPEND Panoptes_LIBRARIES m)
	endif()

	mark_as_advanced(Panoptes_ROOT_DIR)
else()
	set(Panoptes_LIBRARIES)
	set(Panoptes_INCLUDE_DIRS)
endif()

mark_as_advanced(Panoptes_LIBRARIES Panoptes_INCLUDE_DIR)

