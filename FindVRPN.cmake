# - Find VRPN
# Find the VRPN headers and libraries.
#
#  VRPN_INCLUDE_DIRS - where to find the header files
#  VRPN_LIBRARIES    - List of libraries when using VRPN.
#  VRPN_FOUND        - True if VRPN found.

# Look for the header file.
FIND_PATH(VRPN_INCLUDE_DIR NAMES vrpn_Configure.h
		PATHS
		"C:/Program Files/VRPN/include"
)
MARK_AS_ADVANCED(VRPN_INCLUDE_DIR)

# Look for the library.
FIND_LIBRARY(VRPN_LIBRARY NAMES vrpn.lib libvrpn.a
		PATHS
		"C:/Program Files/VRPN/lib"
)
MARK_AS_ADVANCED(VRPN_LIBRARY)

# handle the QUIETLY and REQUIRED arguments and set VRPN_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(VRPN DEFAULT_MSG VRPN_LIBRARY VRPN_INCLUDE_DIR)

IF(VRPN_FOUND)
  SET(VRPN_LIBRARIES ${VRPN_LIBRARY})
  SET(VRPN_INCLUDE_DIRS ${VRPN_INCLUDE_DIR})
ELSE(VRPN_FOUND)
  SET(VRPN_LIBRARIES)
  SET(VRPN_INCLUDE_DIRS)
ENDIF(VRPN_FOUND)
