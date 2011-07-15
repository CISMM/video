CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(CISMM_VIDEO)

#-----------------------------------------------------------------------------
# XXX Things to make better.
# 

#-----------------------------------------------------------------------------
# Things you need before you can compile this on Windows:
# 	Windows SDK (download from Microsoft, includes DirectShow)
#    DirectX SDK (Download from Microsoft)
# 	NSRG Buildtime 3.2 (download from CISMM server, includes Tcl, Glut, etc)
#		You have to manually point at a bunch of stuff in here

# Needed for Cmake 2.8.1 to avoid complaints
if(COMMAND cmake_policy)
      cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

#-----------------------------------------------------------------------------
# Compiler flags we got from Hans
if(WIN32) # MS-Windows, both 32 and 64 bits
  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} wsock32.lib /FIXED:NO")
elseif(APPLE) # Apple
#  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Wall")
#  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
#  set(CMAKE_VERBOSE_MAKEFILE ON)
elseif(UNIX) # other than Apple UNIXes
#  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Wall")
#  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
#  set(CMAKE_VERBOSE_MAKEFILE ON)
endif()

# Configuration options controlling what gets included in the build.
OPTION(VIDEO_USE_ROPER "Enable to include Roper camera code in build" OFF)
OPTION(VIDEO_USE_COOKE "Enable to include Cooke camera code in build" OFF)
OPTION(VIDEO_USE_EDT "Enable to include EDT camera code in build" OFF)
OPTION(VIDEO_USE_DIAGINC "Enable to include Diag Inc camera code in build" OFF)
OPTION(VIDEO_USE_SEM "Enable to include SEM (from nanoManipulator) camera code in build" OFF)
OPTION(VIDEO_USE_VRPN_IMAGER "Enable to include VRPN_Imager camera code in build" ON)
OPTION(VIDEO_USE_DIRECTSHOW "Enable to include Microsoft DirectShow code in build" OFF)
OPTION(VIDEO_USE_POINTGREY "Enable to include Point Grey camera code in build" OFF)

if (WIN32)
	set(VIDEO_USE_ROPER ON CACHE TYPE BOOLEAN)
	set(VIDEO_USE_COOKE ON CACHE TYPE BOOLEAN)
	set(VIDEO_USE_EDT ON CACHE TYPE BOOLEAN)
	set(VIDEO_USE_DIAGINC ON CACHE TYPE BOOLEAN)
	set(VIDEO_USE_DIRECTSHOW ON CACHE TYPE BOOLEAN)
	set(VIDEO_USE_POINTGREY ON CACHE TYPE BOOLEAN)
endif (WIN32)

#-----------------------------------------------------------------------------
# Set compile-time definitions for all projects based on what has been
# selected.

if (VIDEO_USE_ROPER)
	ADD_DEFINITIONS(-DVST_USE_ROPER)
endif (VIDEO_USE_ROPER)
if (VIDEO_USE_COOKE)
	ADD_DEFINITIONS(-DVST_USE_COOKE)
endif (VIDEO_USE_COOKE)
if (VIDEO_USE_EDT)
	ADD_DEFINITIONS(-DVST_USE_EDT)
endif (VIDEO_USE_EDT)
if (VIDEO_USE_DIAGINC)
	ADD_DEFINITIONS(-DVST_USE_DIAGINC)
endif (VIDEO_USE_DIAGINC)
if (VIDEO_USE_DIRECTSHOW)
	ADD_DEFINITIONS(-DVST_USE_DIRECTX)
endif (VIDEO_USE_DIRECTSHOW)
if (VIDEO_USE_POINTGREY)
	ADD_DEFINITIONS(-DVST_USE_POINTGREY)
endif (VIDEO_USE_POINTGREY)

#-----------------------------------------------------------------------------
# Local CMake Modules
list(APPEND CMAKE_MODULE_PATH ${CISMM_VIDEO_SOURCE_DIR} ${CISMM_VIDEO_SOURCE_DIR}/cmake)

#-----------------------------------------------------------------------------
# Put this in /usr/local unless overridden.  The default on Windows is
# in Program Files, but that breaks on Windows 7 because you have to be
# admin to write there.

set(CMAKE_INSTALL_PREFIX /usr/local/ CACHE TYPE STRING FORCE)


#-----------------------------------------------------------------------------
# Libraries we need to do our thing.
set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${VIDEO_SOURCE_DIR})
FIND_PACKAGE(quatlib REQUIRED)	# Sets QUATLIB_INCLUDE_DIR and QUATLIB_LIBRARIES
FIND_PACKAGE(VRPN REQUIRED)
FIND_PACKAGE(ImageMagick REQUIRED COMPONENTS MagickCore)	# Need to have ImageMagick
FIND_PACKAGE(OpenGL REQUIRED)
if (WIN32)
	# XXX Bad evil hard-coded path for UNC.
	link_directories( C:/NSRG/external/pc_win32/ImageMagick/lib )
	link_directories( "C:/Program Files (x86)/CISMM/external/ImageMagick/lib" )
else(WIN32)
	set (PTHREAD_LIBRARY pthread)
endif (WIN32)
set(ImageMagick_Needed_Libs CORE_RL_bzlib_ CORE_RL_coders_ CORE_RL_filters_
		CORE_RL_jbig_ CORE_RL_jp2_ CORE_RL_jpeg_ CORE_RL_lcms_ CORE_RL_libxml_
		CORE_RL_Magick++_ CORE_RL_magick_ CORE_RL_png_ CORE_RL_tiff_
		CORE_RL_ttf_ CORE_RL_wand_ CORE_RL_wmf_ CORE_RL_xlib_ CORE_RL_zlib_
		X11 Xext
)
FIND_PACKAGE(TCL REQUIRED)
FIND_PACKAGE(GLUT REQUIRED)
SET(X11_INCLUDE_DIR ${TCL_INCLUDE_PATH}/../../X11/include)	# Normally in Tcl, but NSRG buildtime has it elsewhere...
INCLUDE_DIRECTORIES(
	${TCL_INCLUDE_PATH}
	${TK_INCLUDE_PATH}
	${X11_INCLUDE_DIR}
	${GLUT_INCLUDE_DIR}
	stocc_random_number_generator/
)

#-----------------------------------------------------------------------------
# Camera-driver libraries
set(BCS_SOURCES base_camera_server.cpp raw_file_server.cpp)
set(BCS_PUBLIC_HEADERS base_camera_server.h raw_file_server.h controllable_video.h)
ADD_LIBRARY (base_camera_server_library
	${BCS_SOURCES} ${BCS_PUBLIC_HEADERS}
)
set_property(TARGET base_camera_server_library PROPERTY PUBLIC_HEADER ${BCS_PUBLIC_HEADERS})
install(TARGETS base_camera_server_library
	ARCHIVE DESTINATION lib
	PUBLIC_HEADER DESTINATION include)

set(FS_SOURCES file_stack_server.cpp file_list.cpp)
set(FS_PUBLIC_HEADERS file_stack_server.h)
ADD_DEFINITIONS( -D_CRT_SECURE_NO_WARNINGS )
ADD_LIBRARY (file_stack_library
	${FS_SOURCES} ${FS_PUBLIC_HEADERS}
)
set_property(TARGET file_stack_library PROPERTY PUBLIC_HEADER ${FS_PUBLIC_HEADERS})
install(TARGETS file_stack_library
	ARCHIVE DESTINATION lib
	PUBLIC_HEADER DESTINATION include)

# XXX Need to describe how to build (and then link) the other camera libraries.
# XXX COOKE
# XXX DIAGINC
# XXX EDT
# XXX NIKON
# XXX ROPER
# XXX Point Grey

#-----------------------------------------------------------------------------
# DirectShow base-class library (if configured)
if (VIDEO_USE_DIRECTSHOW)
	FIND_PACKAGE(DirectShow REQUIRED)	# Need to have DirectShow SDK
	# Add this if you can't compile without out.
	# ADD_DEFINITIONS( -DCOINIT_DISABLE_OLE1DDE=0x4 )
	ADD_LIBRARY (DirectShow_baseclasses
		${DirectShow_BASECLASS_DIR}/amextra.cpp
		${DirectShow_BASECLASS_DIR}/amfilter.cpp
		${DirectShow_BASECLASS_DIR}/amvideo.cpp
		${DirectShow_BASECLASS_DIR}/combase.cpp
		${DirectShow_BASECLASS_DIR}/cprop.cpp
		${DirectShow_BASECLASS_DIR}/ctlutil.cpp
		${DirectShow_BASECLASS_DIR}/ddmm.cpp
		${DirectShow_BASECLASS_DIR}/dllentry.cpp
		${DirectShow_BASECLASS_DIR}/dllsetup.cpp
		${DirectShow_BASECLASS_DIR}/mtype.cpp
		${DirectShow_BASECLASS_DIR}/outputq.cpp
		${DirectShow_BASECLASS_DIR}/pstream.cpp
		${DirectShow_BASECLASS_DIR}/pullpin.cpp
		${DirectShow_BASECLASS_DIR}/refclock.cpp
		${DirectShow_BASECLASS_DIR}/renbase.cpp
		${DirectShow_BASECLASS_DIR}/schedule.cpp
		${DirectShow_BASECLASS_DIR}/seekpt.cpp
		${DirectShow_BASECLASS_DIR}/source.cpp
		${DirectShow_BASECLASS_DIR}/strmctl.cpp
		${DirectShow_BASECLASS_DIR}/sysclock.cpp
		${DirectShow_BASECLASS_DIR}/transfrm.cpp
		${DirectShow_BASECLASS_DIR}/transip.cpp
		${DirectShow_BASECLASS_DIR}/videoctl.cpp
		${DirectShow_BASECLASS_DIR}/vtrans.cpp
		${DirectShow_BASECLASS_DIR}/winctrl.cpp
		${DirectShow_BASECLASS_DIR}/winutil.cpp
		${DirectShow_BASECLASS_DIR}/wxdebug.cpp
		${DirectShow_BASECLASS_DIR}/wxlist.cpp
		${DirectShow_BASECLASS_DIR}/wxutil.cpp
	)

	set (DX_SOURCES directx_camera_server.cpp directx_videofile_server.cpp)
	set (DX_PUBLIC_HEADERS directx_camera_server.h directx_videofile_server.h)
	ADD_LIBRARY (directx_library ${DX_SOURCES} ${DX_PUBLIC_HEADERS})
	set_property(TARGET directx_library PROPERTY PUBLIC_HEADER ${DX_PUBLIC_HEADERS})
	install(TARGETS DirectShow_baseclasses
		ARCHIVE DESTINATION lib)
	install(TARGETS directx_library
		ARCHIVE DESTINATION lib
		PUBLIC_HEADER DESTINATION include)
	set (DirectShow_LIBS DirectShow_baseclasses directx_library)

	SET(TARGET_NAME test_directx_library)
	ADD_EXECUTABLE(test_directx_library test_directx_library.cpp)
	TARGET_LINK_LIBRARIES(test_directx_library
		base_camera_server_library
		${VRPN_LIBRARIES} ${QUATLIB_LIBRARIES}
		${DirectShow_LIBS}
		${PTHREAD_LIBRARY}
		${OPENGL_gl_LIBRARY}
		${ImageMagick_Needed_Libs}
	)
	SET_TARGET_PROPERTIES(test_directx_library PROPERTIES SOLUTION_FOLDER tests)

	SET(TARGET_NAME stereo_spin)
	ADD_EXECUTABLE(stereo_spin stereo_spin.cpp)
	TARGET_LINK_LIBRARIES(stereo_spin
		directx_library
		base_camera_server_library
		file_stack_library
		Tcl_Linkvar
		${VRPN_LIBRARIES} ${QUATLIB_LIBRARIES}
		${DirectShow_LIBS}
		${PTHREAD_LIBRARY}
		${OPENGL_gl_LIBRARY}
		${ImageMagick_Needed_Libs}
		${TCL_LIBRARY}
		${TK_LIBRARY}
		${GLUT_LIBRARIES}
	)
	SET_TARGET_PROPERTIES(stereo_spin PROPERTIES SOLUTION_FOLDER servers)

	INCLUDE_DIRECTORIES(
		${DirectShow_INCLUDE_DIRS}
	)

endif (VIDEO_USE_DIRECTSHOW)

#-----------------------------------------------------------------------------
# Spot tracker library
set(STL_SOURCES image_wrapper.cpp spot_math.cpp spot_tracker.cpp)
set(STL_PUBLIC_HEADERS image_wrapper.h spot_math.h spot_tracker.h)
ADD_LIBRARY (spot_tracker_library
	${STL_SOURCES} ${STL_PUBLIC_HEADERS}
)
set_property(TARGET spot_tracker_library PROPERTY PUBLIC_HEADER ${STL_PUBLIC_HEADERS})
install(TARGETS spot_tracker_library
	ARCHIVE DESTINATION lib
	PUBLIC_HEADER DESTINATION include)

#-----------------------------------------------------------------------------
# STOCC random-number library
set(STC_SOURCES stocc_random_number_generator/mersenne.cpp
			stocc_random_number_generator/stoc1.cpp
			stocc_random_number_generator/userintf.cpp)
set(STC_PUBLIC_HEADERS stocc_random_number_generator/stocc.h)
ADD_LIBRARY (stocc_random_number_generator_library
	${STC_SOURCES} ${STC_PUBLIC_HEADERS}
)
set_property(TARGET stocc_random_number_generator_library
			PROPERTY PUBLIC_HEADER ${STC_PUBLIC_HEADERS})
install(TARGETS stocc_random_number_generator_library
	ARCHIVE DESTINATION lib
	PUBLIC_HEADER DESTINATION include)

#-----------------------------------------------------------------------------
# Tcl-Linkvar library (not a library in the initial project file)
set(LV_SOURCES Tcl_Linkvar.C)
set(LV_PUBLIC_HEADERS Tcl_Linkvar.h)
ADD_LIBRARY (Tcl_Linkvar
	${LV_SOURCES} ${LV_PUBLIC_HEADERS}
)
set_property(TARGET Tcl_Linkvar PROPERTY PUBLIC_HEADER ${LV_PUBLIC_HEADERS})
install(TARGETS Tcl_Linkvar
	ARCHIVE DESTINATION lib
	PUBLIC_HEADER DESTINATION include)

#-----------------------------------------------------------------------------
# Include directories needed
INCLUDE_DIRECTORIES(
	${QUATLIB_INCLUDE_DIR}
	${VRPN_INCLUDE_DIR}
	${ImageMagick_MagickCore_INCLUDE_DIR}
)

#-----------------------------------------------------------------------------
# Applications that need more than one file or nonstandard things.

FIND_PACKAGE(OpenCV)
if (OpenCV_FOUND)
	INCLUDE_DIRECTORIES( ${OpenCV_INCLUDE_DIRS} )

	SET(TARGET_NAME video_spot_tracker)
	ADD_EXECUTABLE(video_spot_tracker video_spot_tracker.cpp thread.cpp)
	TARGET_LINK_LIBRARIES(video_spot_tracker
		base_camera_server_library
		file_stack_library
		spot_tracker_library
		Tcl_Linkvar
		directx_library
		${ImageMagick_Needed_Libs}
		${OpenCV_LIBRARIES}
		${VRPN_SERVER_LIBRARIES} ${QUATLIB_LIBRARIES}
		${OPENGL_gl_LIBRARY}
		${TCL_LIBRARY}
		${TK_LIBRARY}
		${GLUT_LIBRARIES}
	)
	SET_TARGET_PROPERTIES(video_spot_tracker PROPERTIES SOLUTION_FOLDER apps)
endif (OpenCV_FOUND)

SET(TARGET_NAME swap_ppm_lines)
ADD_EXECUTABLE(swap_ppm_lines swap_ppm_lines.cpp ppm.C)
TARGET_LINK_LIBRARIES(swap_ppm_lines
)
SET_TARGET_PROPERTIES(swap_ppm_lines PROPERTIES SOLUTION_FOLDER apps)

#-----------------------------------------------------------------------------
# A little utility helper macro to setup default linkages and filenames
MACRO(CAPC_APPLICATION APPLICATION_NAME SOLUTION_NAME)
  SET(TARGET_NAME ${APPLICATION_NAME})
  ADD_EXECUTABLE(${APPLICATION_NAME} ${APPLICATION_NAME}.C)
  TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
	stocc_random_number_generator_library
	base_camera_server_library
	file_stack_library
	spot_tracker_library
	Tcl_Linkvar
	directx_library
	${ImageMagick_Needed_Libs}
	${VRPN_SERVER_LIBRARIES}
	${QUATLIB_LIBRARIES}
	${TCL_LIBRARY}
	${TK_LIBRARY}
	${GLUT_LIBRARIES}
	${OPENGL_gl_LIBRARY}
	${PTHREAD_LIBRARY}
  )
  SET_TARGET_PROPERTIES(${APPLICATION_NAME}
	PROPERTIES SOLUTION_FOLDER ${SOLUTION_NAME})
ENDMACRO(CAPC_APPLICATION)

MACRO(CPP_APPLICATION APPLICATION_NAME SOLUTION_NAME)
  SET(TARGET_NAME ${APPLICATION_NAME})
  ADD_EXECUTABLE(${APPLICATION_NAME} ${APPLICATION_NAME}.cpp)
  TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
	stocc_random_number_generator_library
	base_camera_server_library
	file_stack_library
	spot_tracker_library
	Tcl_Linkvar
	directx_library
	${ImageMagick_Needed_Libs}
	${VRPN_SERVER_LIBRARIES}
	${QUATLIB_LIBRARIES}
	${TCL_LIBRARY}
	${TK_LIBRARY}
	${GLUT_LIBRARIES}
	${OPENGL_gl_LIBRARY}
	${PTHREAD_LIBRARY}
  )
  SET_TARGET_PROPERTIES(${APPLICATION_NAME} PROPERTIES SOLUTION_FOLDER ${SOLUTION_NAME})
ENDMACRO(CPP_APPLICATION)

#-----------------------------------------------------------------------------
# Declaration of the simple test applications that don't have extra files

CPP_APPLICATION(add_noise_to_image apps)
CPP_APPLICATION(airy_maker apps)
CPP_APPLICATION(argonot apps)
CPP_APPLICATION(average_videos apps)
CPP_APPLICATION(cismm_video_optimizer apps)
CPP_APPLICATION(compare_tracking_files apps)
if (VIDEO_USE_EDT)
	CPP_APPLICATION(edt_video_take apps)
endif (VIDEO_USE_EDT)

# XXX GLUItake
# XXX roper_example
# XXX roper_spot_tracker
# XXX GLUItake

CPP_APPLICATION(make_moving_spot_video apps)
CPP_APPLICATION(make_test_image apps)
CAPC_APPLICATION(manual_capture apps)
# CPP_APPLICATION(stack_collector apps) XXX requires vrpn_Generic_Server_Object
CPP_APPLICATION(test_spot_tracker tests)
CPP_APPLICATION(video_imager_server apps)

# XXX Panoptes_Stage_Control
# XXX PanopticNerve
# XXX stocc_test_poisson
