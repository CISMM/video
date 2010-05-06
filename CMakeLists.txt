CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(CISMM_VIDEO)

#-----------------------------------------------------------------------------
# XXX Things to make better.
# 

#-----------------------------------------------------------------------------
# Compiler flags we got from Hans
if(WIN32) # MS-Windows, both 32 and 64 bits
  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   /fp:fast")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fp:fast")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /FIXED:NO")
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
if (WIN32)
	OPTION(BUILD_DIRECTSHOW_VIDEO_SERVER "Enable to include Microsoft DirectShow code in build" ON)
endif (WIN32)

#-----------------------------------------------------------------------------
# Local CMake Modules
list(APPEND CMAKE_MODULE_PATH ${CISMM_VIDEO_SOURCE_DIR} ${CISMM_VIDEO_SOURCE_DIR}/cmake)

#-----------------------------------------------------------------------------
# Libraries we need to do our thing.
set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${VIDEO_SOURCE_DIR})
FIND_PACKAGE(quatlib REQUIRED)	# Sets QUATLIB_INCLUDE_DIR and QUATLIB_LIBRARIES
FIND_PACKAGE(VRPN REQUIRED)
FIND_PACKAGE(ImageMagick REQUIRED COMPONENTS MagickCore )	# Need to have ImageMagick
FIND_PACKAGE(OpenGL REQUIRED)
if (WIN32)
	# XXX Bad evil hard-coded path for UNC.
	link_directories( C:/NSRG/external/pc_win32/ImageMagick/lib )
else(WIN32)
	set (PTHREAD_LIBRARY pthread)
endif (WIN32)
set(ImageMagick_Needed_Libs CORE_RL_bzlib_ CORE_RL_coders_ CORE_RL_filters_
		CORE_RL_jbig_ CORE_RL_jp2_ CORE_RL_jpeg_ CORE_RL_lcms_ CORE_RL_libxml_
		CORE_RL_Magick++_ CORE_RL_magick_ CORE_RL_png_ CORE_RL_tiff_
		CORE_RL_ttf_ CORE_RL_wand_ CORE_RL_wmf_ CORE_RL_xlib_ CORE_RL_zlib_
		X11 Xext
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
ADD_LIBRARY (file_stack_library
	${FS_SOURCES} ${FS_PUBLIC_HEADERS}
)
set_property(TARGET file_stack_library PROPERTY PUBLIC_HEADER ${FS_PUBLIC_HEADERS})
install(TARGETS file_stack_library
	ARCHIVE DESTINATION lib
	PUBLIC_HEADER DESTINATION include)

#-----------------------------------------------------------------------------
# DirectShow base-class library (if configured)
if (BUILD_DIRECTSHOW_VIDEO_SERVER)
	FIND_PACKAGE(DirectShow REQUIRED)	# Need to have DirectShow SDK
	ADD_DEFINITIONS( -DCOINIT_DISABLE_OLE1DDE=0x4 )
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

	ADD_DEFINITIONS( -DDIRECTX_VIDEO_ONLY )
	SET(TARGET_NAME DIRECTX_VIDEO_ONLY)
	ADD_EXECUTABLE( video_imager_server_directx_only video_imager_server.cpp )
	TARGET_LINK_LIBRARIES(video_imager_server_directx_only
		directx_library
		base_camera_server_library
		${VRPN_LIBRARIES} ${QUATLIB_LIBRARIES}
		${DirectShow_LIBS}
		${PTHREAD_LIBRARY}
		${OPENGL_gl_LIBRARY}
		${ImageMagick_Needed_Libs}
	)
	SET_TARGET_PROPERTIES(video_imager_server_directx_only PROPERTIES SOLUTION_FOLDER servers)

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
endif (BUILD_DIRECTSHOW_VIDEO_SERVER)

#-----------------------------------------------------------------------------
# Include directories needed
INCLUDE_DIRECTORIES(
	${QUATLIB_INCLUDE_DIR}
	${VRPN_INCLUDE_DIR}
	${ImageMagick_MagickCore_INCLUDE_DIR}
	${DirectShow_BASECLASS_DIR} ${PLATFORM_SDK_INCLUDE_DIR}
		${PLATFORM_SDK_ATL_INCLUDE_DIR}
		${PLATFORM_SDK_ATL_INCLUDE_DIR} ${DIRECTX_SDK_INCLUDE_DIR}
)

#-----------------------------------------------------------------------------
# Applications that need more than one file to run.

#-----------------------------------------------------------------------------
# A little utility helper macro to setup default linkages and filenames
MACRO(CAPC_APPLICATION APPLICATION_NAME SOLUTION_NAME)
  SET(TARGET_NAME ${APPLICATION_NAME})
  ADD_EXECUTABLE(${APPLICATION_NAME} ${APPLICATION_NAME}.C)
  TARGET_LINK_LIBRARIES(${APPLICATION_NAME} ${VRPN_LIBRARIES} ${QUATLIB_LIBRARIES} ${PTHREAD_LIBRARY})
  SET_TARGET_PROPERTIES(${APPLICATION_NAME} PROPERTIES SOLUTION_FOLDER ${SOLUTION_NAME})
ENDMACRO(CAPC_APPLICATION)

MACRO(CPP_APPLICATION APPLICATION_NAME SOLUTION_NAME)
  SET(TARGET_NAME ${APPLICATION_NAME})
  ADD_EXECUTABLE(${APPLICATION_NAME} ${APPLICATION_NAME}.cpp)
  TARGET_LINK_LIBRARIES(${APPLICATION_NAME} ${VRPN_LIBRARIES} ${QUATLIB_LIBRARIES} ${PTHREAD_LIBRARY})
  SET_TARGET_PROPERTIES(${APPLICATION_NAME} PROPERTIES SOLUTION_FOLDER ${SOLUTION_NAME})
ENDMACRO(CPP_APPLICATION)

#-----------------------------------------------------------------------------
# Declaration of the simple test applications that don't have extra files

#CAPC_APPLICATION(client_and_server tests)
#CPP_APPLICATION(testimager_server tests)
