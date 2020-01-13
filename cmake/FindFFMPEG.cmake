# - Find the FFMPEG libraries and headers.
#
#  FFMPEG_INCLUDE_PATH - where to find needed include file
#  FFMPEG_LIBRARIES    - Libraries to load
#  FFMPEG_FOUND        - True if the required things are found.

# Look for the header files (picking any one from the directories we need).
FIND_PATH(FFMPEG_INCLUDE_PATH NAMES libavcodec/avcodec.h
		PATHS
		/usr/local/include
		"C:/usr/local/include"
		"C:/ffmpeg-4.2.1-win64/include"
)
MARK_AS_ADVANCED(FFMPEG_INCLUDE_PATH)

set (_libsuffixes lib)

# Look for the library files (picking any one from the directories we need).
find_library(FFMPEG_AVCODEC_LIBRARY
	NAMES
	avcodec
	PATH_SUFFIXES
	${_libsuffixes}
	PATHS
	/usr/local
	"C:/usr/local"
	"C:/ffmpeg-4.2.1-win64/lib"
)
find_library(FFMPEG_AVDEVICE_LIBRARY
	NAMES
	avdevice
	PATH_SUFFIXES
	${_libsuffixes}
	PATHS
	/usr/local
	"C:/usr/local"
	"C:/ffmpeg-4.2.1-win64/lib"
)
find_library(FFMPEG_AVFILTER_LIBRARY
	NAMES
	avfilter
	PATH_SUFFIXES
	${_libsuffixes}
	PATHS
	/usr/local
	"C:/usr/local"
	"C:/ffmpeg-4.2.1-win64/lib"
)
find_library(FFMPEG_AVFORMAT_LIBRARY
	NAMES
	avformat
	PATH_SUFFIXES
	${_libsuffixes}
	PATHS
	/usr/local
	"C:/usr/local"
	"C:/ffmpeg-4.2.1-win64/lib"
)
find_library(FFMPEG_AVUTIL_LIBRARY
	NAMES
	avutil
	PATH_SUFFIXES
	${_libsuffixes}
	PATHS
	/usr/local
	"C:/usr/local"
	"C:/ffmpeg-4.2.1-win64/lib"
)
find_library(FFMPEG_SWRESAMPLE_LIBRARY
	NAMES
	swresample
	PATH_SUFFIXES
	${_libsuffixes}
	PATHS
	/usr/local
	"C:/usr/local"
	"C:/ffmpeg-4.2.1-win64/lib"
)
find_library(FFMPEG_SWSCALE_LIBRARY
	NAMES
	swscale
	PATH_SUFFIXES
	${_libsuffixes}
	PATHS
	/usr/local
	"C:/usr/local"
	"C:/ffmpeg-4.2.1-win64/lib"
)

# handle the QUIETLY and REQUIRED arguments and set FFMPEG_FOUND to TRUE if 
# all listed variables are TRUE.  The package name seems to need to be all-caps for this
# to work.
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(FFMPEG DEFAULT_MSG
	FFMPEG_INCLUDE_PATH
	FFMPEG_AVCODEC_LIBRARY
)

IF(FFMPEG_FOUND)
  SET(FFMPEG_LIBRARIES
	${FFMPEG_AVCODEC_LIBRARY}
	${FFMPEG_AVDEVICE_LIBRARY}
	${FFMPEG_AVFILTER_LIBRARY}
	${FFMPEG_AVFORMAT_LIBRARY}
	${FFMPEG_AVUTIL_LIBRARY}
	${FFMPEG_SWRESAMPLE_LIBRARY}
	${FFMPEG_SWSCALE_LIBRARY}
	CACHE STRING "Libraries needed to link to for FFMPEG"
  )
  SET(FFMPEG_FOUND ON)
ELSE(FFMPEG_FOUND)
  SET(FFMPEG_INCLUDE_PATH "" CACHE PATH "Path to FFMPEG include files" FORCE)
  SET(FFMPEG_LIBRARIES "" CACHE STRING "Libraries needed to link to for FFMPEG" FORCE)
ENDIF(FFMPEG_FOUND)
MARK_AS_ADVANCED(FFMPEG_LIBRARIES)

