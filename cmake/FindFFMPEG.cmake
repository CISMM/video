# - Find the FFPMEG libraries and headers.
#
#  FFPMEG_INCLUDE_PATH - where to find needed include file
#  FFPMEG_LIBRARIES    - Libraries to load
#  FFPMEG_FOUND        - True if the required things are found.

# Look for the header files (picking any one from the directories we need).
FIND_PATH(FFPMEG_INCLUDE_PATH NAMES libavcodec/avcodec.h
		PATHS
		"C:/usr/local/include"
		/usr/local/include
)
MARK_AS_ADVANCED(FFPMEG_INCLUDE_PATH)

set (_libsuffixes lib)

# Look for the library files (picking any one from the directories we need).
find_library(FFPMEG_LIBRARY
	NAMES
	avcodec
	PATH_SUFFIXES
	${_libsuffixes}
	PATHS
	"C:/usr/local"
	/usr/local
)

# handle the QUIETLY and REQUIRED arguments and set FFPMEG_FOUND to TRUE if 
# all listed variables are TRUE.  The package name seems to need to be all-caps for this
# to work.
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(FFMPEG DEFAULT_MSG
	FFPMEG_INCLUDE_PATH
	FFPMEG_LIBRARY
)

IF(FFMPEG_FOUND)
  SET(FFPMEG_LIBRARIES
	avcodec avdevice avfilter avformat avutil postproc swresample swscale
	CACHE STRING "Libraries needed to link to for FFPMEG"
  )
  SET(FFMPEG_FOUND ON)
ELSE(FFMPEG_FOUND)
  SET(FFMPEG_INCLUDE_PATH "" CACHE PATH "Path to FFMPEG include files" FORCE)
  SET(FFMPEG_LIBRARIES "" CACHE STRING "Libraries needed to link to for FFMPEG" FORCE)
ENDIF(FFMPEG_FOUND)
MARK_AS_ADVANCED(FFMPEG_LIBRARIES)

