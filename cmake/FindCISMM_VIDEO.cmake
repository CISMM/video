# - Find CISMM_VIDEO
# Find the CISMM Video headers and libraries.
#
#  CISMM_VIDEO_INCLUDE_DIRS - where to find the header files
#  CISMM_VIDEO_LIBRARIES    - List of libraries when using CISMM VIDEO.
#  CISMM_VIDEO_FOUND        - True if the package was found.

# Look for the header file.
find_path(CISMM_VIDEO_INCLUDE_DIR NAMES base_camera_server.h
		PATHS
		"C:/Program Files/CISMM_VIDEO/include"
		"../video"
)
mark_as_advanced(CISMM_VIDEO_INCLUDE_DIR)

# Look for the libraries.
find_library(BASE_CAMERA_LIBRARY NAMES base_camera_server_library.lib libbase_camera_server_library.a
		PATHS
		"C:/Program Files/CISMM_VIDEO/lib"
		"../build_video"
		"../build_video/release"
)
mark_as_advanced(FILE_STACK_LIBRARY)
find_library(FILE_STACK_LIBRARY NAMES file_stack_library.lib libfile_stack_library.a
		PATHS
		"C:/Program Files/CISMM_VIDEO/lib"
		"../build_video"
		"../build_video/release"
)
mark_as_advanced(FILE_STACK_LIBRARY)

# handle the QUIETLY and REQUIRED arguments and set CISMM_VIDEO_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CISMM_VIDEO DEFAULT_MSG BASE_CAMERA_LIBRARY CISMM_VIDEO_INCLUDE_DIR)

if(CISMM_VIDEO_FOUND)
  set(CISMM_VIDEO_LIBRARIES ${BASE_CAMERA_LIBRARY} ${FILE_STACK_LIBRARY})
  if(NOT WIN32)
  	list(APPEND CISMM_VIDEO_LIBRARIES m)
  endif()
  set(CISMM_VIDEO_INCLUDE_DIRS ${CISMM_VIDEO_INCLUDE_DIR})
else()
  set(CISMM_VIDEO_LIBRARIES)
  set(CISMM_VIDEO_INCLUDE_DIRS)
endif()
