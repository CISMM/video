# - Find the Point Grey camera libraries and headers.
#
#  POINTGREY_INCLUDE_PATH - where to find needed include file
#  POINTGREY_LIBRARIES    - Libraries to load
#  POINTGREY_FOUND        - True if the required things are found.

# Look for the header files (picking any one from the directories we need).
FIND_PATH(POINTGREY_INCLUDE_PATH
		NAMES
		FlyCapture2.h
		PATHS
		"C:/Program Files (x86)/Point Grey Research/FlyCapture2/include"
		"C:/Program Files/Point Grey Research/FlyCapture2/include"
		"C:/Program Files (x86)/CISMM/external/PointGrey/include"
		/usr/local/include
		/usr/include/flycapture
)
MARK_AS_ADVANCED(POINTGREY_INCLUDE_PATH)

set (_libsuffixes lib lib64 Libs so)

# Look for the library files (picking any one from the directories we need).
find_library(POINTGREY_FlyCap_LIBRARY
	NAMES
	FlyCapture2
	flycapture
	PATH_SUFFIXES
	${_libsuffixes}
	PATHS
	"C:/Program Files (x86)/Point Grey Research/FlyCapture2"
	"C:/Program Files/Point Grey Research/FlyCapture2"
	"C:/Program Files (x86)/CISMM/external/PointGrey"
	/usr/local
	/usr/lib
)
find_library(POINTGREY_GUI_LIBRARY
	NAMES
	FlyCapture2GUI
	flycapturegui
	PATH_SUFFIXES
	${_libsuffixes}
	PATHS
	"C:/Program Files (x86)/Point Grey Research/FlyCapture2"
	"C:/Program Files/Point Grey Research/FlyCapture2"
	"C:/Program Files (x86)/CISMM/external/PointGrey"
	/usr/local
	/usr/lib
)

# handle the QUIETLY and REQUIRED arguments and set POINTGREY_FOUND to TRUE if 
# all listed variables are TRUE.  The package name seems to need to be all-caps for this
# to work.
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(POINTGREY DEFAULT_MSG
	POINTGREY_INCLUDE_PATH
	POINTGREY_FlyCap_LIBRARY
	POINTGREY_GUI_LIBRARY
)

IF(POINTGREY_FOUND)
  SET(POINTGREY_LIBRARIES
	${POINTGREY_FlyCap_LIBRARY}
	${POINTGREY_GUI_LIBRARY}
	CACHE STRING "Libraries needed to link to for Point Grey camera"
  )
  SET(POINTGREY_FOUND ON)
ELSE(POINTGREY_FOUND)
  SET(POINTGREY_INCLUDE_PATH "" CACHE PATH "Path to POINTGREY camera include files" FORCE)
  SET(POINTGREY_LIBRARIES "" CACHE STRING "Libraries needed to link to for POINTGREY camera" FORCE)
ENDIF(POINTGREY_FOUND)
MARK_AS_ADVANCED(POINTGREY_LIBRARIES)

