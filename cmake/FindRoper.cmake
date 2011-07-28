# - Find the ROPER camera libraries and headers.
#
#  ROPER_INCLUDE_PATH - where to find needed include file
#  ROPER_LIBRARIES    - Libraries to load
#  ROPER_FOUND        - True if the required things are found.

# Look for the header files (picking any one from the directories we need).
FIND_PATH(ROPER_INCLUDE_PATH NAMES pvcam.h
		PATHS
		"C:/Program Files (x86)/CISMM/external/Roper-Photometrics/PVCam/SDK/Headers"
		/usr/local/include
)
MARK_AS_ADVANCED(ROPER_INCLUDE_PATH)

set (_libsuffixes lib Libs)

# Look for the library files (picking any one from the directories we need).
find_library(ROPER_PVCAM_LIBRARY
	NAMES
	Pvcam32
	PATH_SUFFIXES
	${_libsuffixes}
	PATHS
	"C:/Program Files (x86)/CISMM/external/Roper-Photometrics/PVCam/SDK/"
	/usr/local
)
find_library(ROPER_PV_LIBRARY
	NAMES
	pv_icl32
	PATH_SUFFIXES
	${_libsuffixes}
	PATHS
	"C:/Program Files (x86)/CISMM/external/Roper-Photometrics/PVCam/SDK/"
	/usr/local
)

# handle the QUIETLY and REQUIRED arguments and set ROPER_FOUND to TRUE if 
# all listed variables are TRUE.  The package name seems to need to be all-caps for this
# to work.
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(ROPER DEFAULT_MSG
	ROPER_INCLUDE_PATH
	ROPER_PVCAM_LIBRARY
	ROPER_PV_LIBRARY
)

IF(ROPER_FOUND)
  SET(ROPER_LIBRARIES
	${ROPER_PVCAM_LIBRARY}
	${ROPER_PV_LIBRARY}
	CACHE STRING "Libraries needed to link to for ROPER camera"
  )
  SET(ROPER_FOUND ON)
ELSE(ROPER_FOUND)
  SET(ROPER_INCLUDE_PATH "" CACHE PATH "Path to ROPER camera include files" FORCE)
  SET(ROPER_LIBRARIES "" CACHE STRING "Libraries needed to link to for ROPER camera" FORCE)
ENDIF(ROPER_FOUND)
MARK_AS_ADVANCED(ROPER_LIBRARIES)

