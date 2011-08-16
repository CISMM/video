# - Find the DiagInc camera libraries and headers.
#
#  DiagInc_INCLUDE_PATH - where to find needed include file
#  DiagInc_LIBRARIES    - Libraries to load
#  DiagInc_FOUND        - True if the required things are found.

# Look for the header files (picking any one from the directories we need).
FIND_PATH(DiagInc_INCLUDE_PATH NAMES SpotCam.h
		PATHS
		"C:/Program Files (x86)/CISMM/external/DiagInc"
		/usr/local/include
)
MARK_AS_ADVANCED(DiagInc_INCLUDE_PATH)

set (_libsuffixes lib)

# Look for the library files (picking any one from the directories we need).
find_library(DiagInc_LIBRARY
	NAMES
	SPotCamVC SpotCam
	PATH_SUFFIXES
	${_libsuffixes}
	PATHS
	"C:/Program Files (x86)/CISMM/external/DiagInc"
	/usr/local
)

# handle the QUIETLY and REQUIRED arguments and set DiagInc_FOUND to TRUE if 
# all listed variables are TRUE.  The package name seems to need to be all-caps for this
# to work.
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(DIAGINC DEFAULT_MSG
	DiagInc_INCLUDE_PATH
	DiagInc_LIBRARY
)

IF(DIAGINC_FOUND)
  SET(DiagInc_LIBRARIES
	${DiagInc_LIBRARY}
	CACHE STRING "Libraries needed to link to for DiagInc camera"
  )
  SET(DiagInc_FOUND ON)
ELSE(DIAGINC_FOUND)
  SET(DiagInc_INCLUDE_PATH "" CACHE PATH "Path to DiagInc camera include files" FORCE)
  SET(DiagInc_LIBRARIES "" CACHE STRING "Libraries needed to link to for DiagInc camera" FORCE)
ENDIF(DIAGINC_FOUND)
MARK_AS_ADVANCED(DiagInc_LIBRARIES)

