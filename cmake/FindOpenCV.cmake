# - Find OpenCV library and headers.
#
#  OpenCV_INCLUDE_DIRS - where to find needed include file
#  OpenCV_LIBRARIES    - Libraries to load
#  OpenCV_FOUND        - True if OpenCV found.

# Look for the header files (picking any one from the directories we need).
FIND_PATH(OpenCV_INCLUDE_DIR NAMES cv.h
		PATHS
		"C:/Program Files (x86)/CISMM/external/OpenCV/cv/include"
		/usr/local/include
)
MARK_AS_ADVANCED(OpenCV_INCLUDE_DIR)

# Look for the header files (picking any one from the directories we need).
FIND_PATH(CxCore_INCLUDE_DIR NAMES cxcore.h
		PATHS
		"C:/Program Files (x86)/CISMM/external/OpenCV/cxcore/include"
		/usr/local/include
)
MARK_AS_ADVANCED(CxCore_INCLUDE_DIR)

# Look for the header files (picking any one from the directories we need).
FIND_PATH(HighGUI_INCLUDE_DIR NAMES highgui.h
		PATHS
		"C:/Program Files (x86)/CISMM/external/OpenCV/otherlibs/highgui"
		/usr/local/include
)
MARK_AS_ADVANCED(HighGUI_INCLUDE_DIR)

# Look for the library files (picking any one from the directories we need).
set (_libsuffixes lib)
find_library(OpenCV_LIBRARY
	NAMES
	cv
	PATH_SUFFIXES
	${_libsuffixes}
	PATHS
	"C:/Program Files (x86)/CISMM/external/OpenCV" /usr/local)
find_library(HighGUI_LIBRARY
	NAMES
	highgui
	PATH_SUFFIXES
	${_libsuffixes}
	PATHS
	"C:/Program Files (x86)/CISMM/external/OpenCV" /usr/local)
find_library(CxCore_LIBRARY
	NAMES
	cxcore
	PATH_SUFFIXES
	${_libsuffixes}
	PATHS
	"C:/Program Files (x86)/CISMM/external/OpenCV" /usr/local)

# handle the QUIETLY and REQUIRED arguments and set OpenCV_FOUND to TRUE if 
# all listed variables are TRUE.  The package name seems to need to be all-caps for this
# to work.
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(OPENCV DEFAULT_MSG OpenCV_INCLUDE_DIR CxCore_INCLUDE_DIR HighGUI_INCLUDE_DIR OpenCV_LIBRARY HighGUI_LIBRARY CxCore_LIBRARY)

IF(OPENCV_FOUND)
  SET(OpenCV_INCLUDE_DIRS
	${OpenCV_INCLUDE_DIR}
	${CxCore_INCLUDE_DIR}
	${HighGUI_INCLUDE_DIR}
  )
  SET(OpenCV_LIBRARIES
	${OpenCV_LIBRARY}
	${HighGUI_LIBRARY}
	${CxCore_LIBRARY}
  )
  SET(OpenCV_FOUND ON)
ELSE(OPENCV_FOUND)
  SET(OpenCV_INCLUDE_DIRS)
ENDIF(OPENCV_FOUND)
MARK_AS_ADVANCED(OpenCV_INCLUDE_DIRS)
