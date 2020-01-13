# - Find the NSRG BuildTime, which has a collection of libraries and
#   include files needed to build the Video projects.  Made at UNC, this
#   lets us build on Windows without having to go and install each of
#   them separately, and keeps the versions the same until we are ready
#   to try and upgrade.
#
# - Usage: Look for NSRGBUILD first, but don't require it.  If it is not
#   found, then look for the various included packages.
#
#  TCL_INCLUDE_PATH - where to find needed include files
#  TCL_LIBRARY - the library to link to
#  TK_INCLUDE_PATH - where to find needed include files
#  TK_LIBRARY - the library to link to
#  TCL_X11_INCLUDE_PATH - where to find needed include files
#  ImageMagick_MagickCore_INCLUDE_DIR
#  ImageMagick_MagickCore_LIBRARY
#  ImageMagick_MagickCore_LIBRARY_DIR
#  GLUT_INCLUDE_DIR
#  GLUT_glut_LIBRARY
#  GLUT_LIBRARIES
#  GLUI_INCLUDE_PATH
#  GLUI_LIBRARIES
#  
#  NSRGBUILD_FOUND        - True if we found what we were looking for

# Look for the header files (picking any one from the directories we need).
FIND_PATH(TCL_INCLUDE_PATH NAMES tcl.h
		PATHS
		"C:/Program Files (x86)/CISMM/external/tcl/include"
		"C:/Program Files/CISMM/external/tcl/include"
		/usr/local/include
)
MARK_AS_ADVANCED(TCL_INCLUDE_PATH)

set (_libsuffixes lib)

# Look for the library files (picking any one from the directories we need).
find_library(TCL_LIBRARY
	NAMES
	tcl83
	PATH_SUFFIXES
	${_libsuffixes}
	PATHS
	"C:/Program Files (x86)/CISMM/external/tcl"
	"C:/Program Files/CISMM/external/tcl"
	/usr/local
)
MARK_AS_ADVANCED(TCL_LIBRARY)

# Look for the header files (picking any one from the directories we need).
FIND_PATH(TK_INCLUDE_PATH NAMES tk.h
		PATHS
		"C:/Program Files (x86)/CISMM/external/tcl/include"
		"C:/Program Files/CISMM/external/tcl/include"
		/usr/local/include
)
MARK_AS_ADVANCED(TK_INCLUDE_PATH)

# Look for the library files (picking any one from the directories we need).
find_library(TK_LIBRARY
	NAMES
	tk83
	PATH_SUFFIXES
	${_libsuffixes}
	PATHS
	"C:/Program Files (x86)/CISMM/external/tcl"
	"C:/Program Files/CISMM/external/tcl"
	/usr/local
)
MARK_AS_ADVANCED(TCL_LIBRARY)

# Look for the header files (picking any one from the directories we need).
FIND_PATH(ImageMagick_MagickCore_INCLUDE_DIR NAMES magick/magick.h
		PATHS
		"C:/Program Files (x86)/CISMM/external/ImageMagick/include"
		"C:/Program Files/CISMM/external/ImageMagick/include"
		/usr/local/include
)
MARK_AS_ADVANCED(ImageMagick_MagickCore_INCLUDE_DIR)

# Look for the library files (picking any one from the directories we need).
find_library(ImageMagick_MagickCore_LIBRARY
	NAMES
	CORE_RL_magick_
	PATH_SUFFIXES
	${_libsuffixes}
	PATHS
	"C:/Program Files (x86)/CISMM/external/ImageMagick"
	"C:/Program Files/CISMM/external/ImageMagick"
	/usr/local
)
MARK_AS_ADVANCED(ImageMagick_MagickCore_LIBRARY)

# Look for the library directory
FIND_PATH(ImageMagick_MagickCore_LIBRARY_DIR
		NAMES CORE_RL_magick_.lib
		PATH_SUFFIXES
		${_libsuffixes}
		PATHS
		"C:/Program Files (x86)/CISMM/external/ImageMagick"
		"C:/Program Files/CISMM/external/ImageMagick"
		/usr/local
)
MARK_AS_ADVANCED(ImageMagick_MagickCore_LIBRARY_DIR)

# Look for the header files (picking any one from the directories we need).
FIND_PATH(GLUT_INCLUDE_DIR NAMES GL/glut.h
		PATH_SUFFIXES
		${_libsuffixes}
		PATHS
		"C:/Program Files (x86)/CISMM/external/GL/include"
		"C:/Program Files/CISMM/external/GL/include"
		/usr/local/include
)
MARK_AS_ADVANCED(GLUT_INCLUDE_DIR)

# Look for the library files (picking any one from the directories we need).
find_library(GLUT_glut_LIBRARY
	NAMES
	glut32
	PATH_SUFFIXES
	${_libsuffixes}
	PATHS
	"C:/Program Files (x86)/CISMM/external/GL"
	"C:/Program Files/CISMM/external/GL"
	/usr/local
)
MARK_AS_ADVANCED(GLUT_glut_LIBRARY)

# Look for the header files (picking any one from the directories we need).
FIND_PATH(GLUI_INCLUDE_PATH NAMES glui.h
		PATHS
		"C:/Program Files (x86)/CISMM/external/glui"
		"C:/Program Files/CISMM/external/glui"
		/usr/local/include
)
MARK_AS_ADVANCED(GLUI_INCLUDE_DIR)

# Look for the library files (picking any one from the directories we need).
find_library(GLUI_LIBRARY
	NAMES
	glui32
	PATH_SUFFIXES
	${_libsuffixes}
	PATHS
	"C:/Program Files (x86)/CISMM/external/glui"
	"C:/Program Files/CISMM/external/glui"
	/usr/local
)
MARK_AS_ADVANCED(GLUI_glut_LIBRARY)

# Normally in Tcl, but NSRG buildtime has it elsewhere...
SET(TCL_X11_INCLUDE_PATH ${TCL_INCLUDE_PATH}/../../X11/include
	CACHE STRING "Tcl include directory")

# handle the QUIETLY and REQUIRED arguments and set OpenCV_FOUND to TRUE if 
# all listed variables are TRUE.  The package name seems to need to be all-caps for this
# to work.
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(NSRGBUILD DEFAULT_MSG
	GLUT_INCLUDE_DIR
	GLUT_glut_LIBRARY
	GLUI_INCLUDE_PATH
	GLUI_LIBRARY
	ImageMagick_MagickCore_INCLUDE_DIR
	ImageMagick_MagickCore_LIBRARY
	ImageMagick_MagickCore_LIBRARY_DIR
	TCL_INCLUDE_PATH
	TCL_LIBRARY
	TK_INCLUDE_PATH
	TK_LIBRARY
)

SET(GLUT_LIBRARIES ${GLUT_glut_LIBRARY}
	CACHE STRING "GLUT libraries to link to")
SET(GLUI_LIBRARIES ${GLUI_LIBRARY}
	CACHE STRING "GLUI libraries to link to")

