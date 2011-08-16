# - Find the EDT camera libraries and headers.
#
#  EDT_INCLUDE_PATH - where to find needed include file
#  EDT_LIBRARIES    - Libraries to load
#  EDT_FOUND        - True if the required things are found.

# Look for the header files (picking any one from the directories we need).
FIND_PATH(EDT_INCLUDE_PATH NAMES libedt.h
		PATHS
		"C:/Program Files (x86)/CISMM/external/EDT/pdv"
		/usr/local/include
)
MARK_AS_ADVANCED(EDT_INCLUDE_PATH)

set (_libsuffixes lib)

# Look for the library files (picking any one from the directories we need).
find_library(PDV_LIBRARY
	NAMES
	pdvlib
	PATH_SUFFIXES
	${_libsuffixes}
	PATHS
	"C:/Program Files (x86)/CISMM/external/EDT/pdv"
	/usr/local
)
find_library(EDTMMX_LIBRARY
	NAMES
	edt_mmx
	PATH_SUFFIXES
	${_libsuffixes}
	PATHS
	"C:/Program Files (x86)/CISMM/external/EDT/pdv"
	/usr/local
)

# handle the QUIETLY and REQUIRED arguments and set EDT_FOUND to TRUE if 
# all listed variables are TRUE.  The package name seems to need to be all-caps for this
# to work.
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(EDT DEFAULT_MSG
	EDT_INCLUDE_PATH
	PDV_LIBRARY
	EDTMMX_LIBRARY
)

IF(EDT_FOUND)
  SET(EDT_LIBRARIES
	${PDV_LIBRARY}
	${EDTMMX_LIBRARY}
	CACHE STRING "Libraries needed to link to for EDT camera"
  )
ELSE(EDT_FOUND)
  SET(EDT_INCLUDE_PATH "" CACHE PATH "Path to EDT camera include files" FORCE)
  SET(EDT_LIBRARIES "" CACHE STRING "Libraries needed to link to for EDT camera" FORCE)
ENDIF(EDT_FOUND)
MARK_AS_ADVANCED(EDT_LIBRARIES)

