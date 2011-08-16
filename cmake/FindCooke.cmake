# - Find the Cooke camera libraries and headers.
#
#  Cooke_INCLUDE_PATH - where to find needed include file
#  Cooke_LIBRARIES    - Libraries to load
#  Cooke_FOUND        - True if the required things are found.

# Look for the header files (picking any one from the directories we need).
FIND_PATH(Cooke_INCLUDE_PATH NAMES sc2_defs.h
		PATHS
		"C:/Program Files (x86)/CISMM/external/Cooke-PCO/pco.camera.sdk/include"
		/usr/local/include
)
MARK_AS_ADVANCED(Cooke_INCLUDE_PATH)

set (_libsuffixes lib)

# Look for the library files (picking any one from the directories we need).
find_library(Cooke_SC2_Cam_LIBRARY
	NAMES
	SC2_Cam
	PATH_SUFFIXES
	${_libsuffixes}
	PATHS
	"C:/Program Files (x86)/CISMM/external/Cooke-PCO/pco.camera.sdk"
	/usr/local
)
find_library(Cooke_SC2_DLG_LIBRARY
	NAMES
	SC2_DLG
	PATH_SUFFIXES
	${_libsuffixes}
	PATHS
	"C:/Program Files (x86)/CISMM/external/Cooke-PCO/pco.camera.sdk" /usr/local)
find_library(Cooke_Pcocnv_LIBRARY
	NAMES
	Pcocnv
	PATH_SUFFIXES
	${_libsuffixes}
	PATHS
	"C:/Program Files (x86)/CISMM/external/Cooke-PCO/pco.camera.sdk" /usr/local)
find_library(Cooke_PCOLtDlg_LIBRARY
	NAMES
	PCOLtDlg
	PATH_SUFFIXES
	${_libsuffixes}
	PATHS
	"C:/Program Files (x86)/CISMM/external/Cooke-PCO/pco.camera.sdk" /usr/local)

# handle the QUIETLY and REQUIRED arguments and set OpenCV_FOUND to TRUE if 
# all listed variables are TRUE.  The package name seems to need to be all-caps for this
# to work.
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(COOKE DEFAULT_MSG
	Cooke_INCLUDE_PATH
	Cooke_SC2_Cam_LIBRARY
	Cooke_SC2_DLG_LIBRARY
	Cooke_Pcocnv_LIBRARY
	Cooke_PCOLtDlg_LIBRARY
)

IF(COOKE_FOUND)
  SET(Cooke_LIBRARIES
	${Cooke_SC2_Cam_LIBRARY}
	${Cooke_SC2_DLG_LIBRARY}
	${Cooke_Pcocnv_LIBRARY}
	${Cooke_PCOLtDlg_LIBRARY}
  )
  SET(Cooke_FOUND ON)
ELSE(COOKE_FOUND)
  SET(Cooke_INCLUDE_PATH "" CACHE PATH "Path to Cooke camera include files" FORCE)
  SET(Cooke_LIBRARIES "" CACHE STRING "Libraries needed to link to for Cooke camera" FORCE)
ENDIF(COOKE_FOUND)
MARK_AS_ADVANCED(Cooke_LIBRARIES)

