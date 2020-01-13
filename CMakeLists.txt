CMAKE_MINIMUM_REQUIRED(VERSION 3.0)
PROJECT(CISMM_VIDEO)

#-----------------------------------------------------------------------------
# Things you need before you can compile this on Windows:
# 	Windows SDK (download from Microsoft, includes DirectShow)
#    DirectX SDK (Download from Microsoft)
# 	NSRG Buildtime 3.2 (download from CISMM server, includes Tcl, Glut, etc)
#	Point Grey libraries: The strange thing is that you have to copy the
#		32-bit version onto a 64-bit machine because it installs the 64-bit
#		libraries, which you can't link when using a 32-bit link.  Copy them
#		into C:\Program Files (x86)\Point Grey Research\FlyCapture2

# Needed for Cmake 2.8.1 to avoid complaints
if(COMMAND cmake_policy)
      cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

#-----------------------------------------------------------------------------
# Compiler flags we got from Hans
if(WIN32) # MS-Windows, both 32 and 64 bits
  if (MINGW)
    set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} -lws2_32")
  else()
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} winmm.lib wsock32.lib /FIXED:NO")
    ADD_DEFINITIONS( -D_CRT_SECURE_NO_WARNINGS )
  endif()
elseif(APPLE) # Apple
#Enable later versions of Tcl/Tk to compile with const/non-const changes
  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -fpermissive")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive")
  INCLUDE_DIRECTORIES(
	${/usr/X11/include}
  )
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mmacosx-version-min=10.5 -arch x86_64 -arch i386")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mmacosx-version-min=10.5 -arch x86_64 -arch i386")
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -headerpad_max_install_names -mmacosx-version-min=10.5 -arch x86_64 -arch i386")

#  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Wall")
#  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
#  set(CMAKE_VERBOSE_MAKEFILE ON)
elseif(UNIX) # other than Apple UNIXes
#Enable later versions of Tcl/Tk to compile with const/non-const changes
  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -fpermissive")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive")
#  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Wall")
#  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
#  set(CMAKE_VERBOSE_MAKEFILE ON)
endif()

#-----------------------------------------------------------------------------
# Local CMake Modules
list(APPEND CMAKE_MODULE_PATH ${CISMM_VIDEO_SOURCE_DIR} ${CISMM_VIDEO_SOURCE_DIR}/cmake)

# Configuration options controlling what gets included in the build.
FIND_PACKAGE(TCL)
FIND_PACKAGE(GLUT)
if (TCL_FOUND AND GLUT_FOUND)
  OPTION(VIDEO_BUILD_APPLICATIONS "Enable to build applications" ON)
else()
  OPTION(VIDEO_BUILD_APPLICATIONS "Enable to build applications" OFF)
endif()


#-----------------------------------------------------------------------------
# Libraries we need to do our thing.
set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${VIDEO_SOURCE_DIR})
FIND_PACKAGE(quatlib REQUIRED)	# Sets QUATLIB_INCLUDE_DIR and QUATLIB_LIBRARIES
FIND_PACKAGE(VRPN REQUIRED)
FIND_PACKAGE(OpenGL REQUIRED)

IF (WIN32)
# Try to find NSRG Buildtime (NSRGBUILD) to set all we need for ImageMagic,
# TCL, and Glut.  If not, then find the individual packages.
FIND_PACKAGE(NSRGBUILD)
if (NSRGBUILD_FOUND)
	set(ImageMagick_FOUND true CACHE BOOL "NSRG found, so ImageMagick found")

	INCLUDE_DIRECTORIES(
		${GLUI_INCLUDE_PATH}
	)
else (NSRGBUILD_FOUND)
  FIND_PACKAGE(ImageMagick COMPONENTS MagickCore)	# Do we have ImageMagick?
endif (NSRGBUILD_FOUND)
ELSE (WIN32)
  FIND_PACKAGE(NSRGBUILD)
  FIND_PATH(tiff_LIBRARY_DIR
	libtiff.a	
	PATH_SUFFIXES
	${_libsuffixes}
  )
  FIND_PATH(jpeg_LIBRARY_DIR
	libjpeg.a	
	PATH_SUFFIXES
	${_libsuffixes}
  )
  FIND_PACKAGE(ImageMagick COMPONENTS MagickCore)	# Do we have ImageMagick?
ENDif (WIN32)
# See if we have OpenMP on this architecture.  If so, turn it on.
FIND_PACKAGE(OpenMP)
if (OPENMP_FOUND)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")

endif()

link_directories( ${ImageMagick_MagickCore_LIBRARY_DIR} ${tiff_LIBRARY_DIR} ${jpeg_LIBRARY_DIR})

if (WIN32)
	# To avoid warnings in case we're using Visual Studio
	ADD_DEFINITIONS( -D_CRT_SECURE_NO_WARNINGS )
	ADD_DEFINITIONS( -D_CRT_NONSTDC_NO_DEPRECATE )
	set(ImageMagick_Needed_Libs CORE_RL_bzlib_ CORE_RL_coders_ CORE_RL_filters_
		CORE_RL_jbig_ CORE_RL_jp2_ CORE_RL_jpeg_ CORE_RL_lcms_ CORE_RL_libxml_
		CORE_RL_Magick++_ CORE_RL_magick_ CORE_RL_png_ CORE_RL_tiff_
		CORE_RL_ttf_ CORE_RL_wand_ CORE_RL_wmf_ CORE_RL_xlib_ CORE_RL_zlib_
		X11 Xext
	)
elseif(APPLE)
	set (PTHREAD_LIBRARY pthread)
        # tiff shouldn't be linked separately, but ImageMagick has trouble linking static tiff delegate.
	set (ImageMagick_Needed_Libs libMagickCore-6.Q16.a libMagickWand-6.Q16.a tiff jpeg)
else(APPLE)
	set (PTHREAD_LIBRARY pthread)
        # tiff shouldn't be linked separately, but ImageMagick has trouble linking static tiff delegate.
	set (ImageMagick_Needed_Libs MagickCore tiff)
endif (WIN32)

FIND_PACKAGE(CUDA)
if (CUDA_FOUND)
	OPTION(VIDEO_USE_CUDA "Compile and link to CUDA library" ON)
	include_directories(${CUDA_TOOLKIT_INCLUDE})
	set(CUDA_NVCC_FLAGS -arch=compute_10 -code=compute_10 CACHE STRING "CUDA compiler command-line arguments" FORCE)
else()
	OPTION(VIDEO_USE_CUDA "Compile and link to CUDA library" OFF)
endif()

if (VIDEO_USE_CUDA)
	add_definitions(-DVST_USE_CUDA)
endif()

if (ImageMagick_FOUND)
	OPTION(VIDEO_USE_IMAGEMAGICK "Enable to include ImageMagick code in build" ON)
else (ImageMagick_FOUND)
	OPTION(VIDEO_USE_IMAGEMAGICK "Enable to include ImageMagick code in build" OFF)
endif (ImageMagick_FOUND)

# Override the defaults below.
FIND_PACKAGE(DirectShow)
if (DirectShow_FOUND)
  OPTION(VIDEO_USE_DIRECTSHOW "Enable to include Microsoft DirectShow code in build" ON)
  OPTION(VIDEO_USE_DIRECTSHOW_APPS "Enable to build apps using the DirectShow reader" ON)
else()
  OPTION(VIDEO_USE_DIRECTSHOW "Enable to include Microsoft DirectShow code in build" OFF)
  OPTION(VIDEO_USE_DIRECTSHOW_APPS "Enable to build apps using the DirectShow reader" OFF)
endif ()

find_package(Cooke)
if (Cooke_FOUND)
	OPTION(VIDEO_USE_COOKE "Enable to include Cooke camera code in build" ON)
else (Cooke_FOUND)
	OPTION(VIDEO_USE_COOKE "Enable to include Cooke camera code in build" OFF)
endif (Cooke_FOUND)

find_package(DiagInc)
if (DiagInc_FOUND)
	OPTION(VIDEO_USE_DIAGINC "Enable to include DiagInc camera code in build" ON)
else (DiagInc_FOUND)
	OPTION(VIDEO_USE_DIAGINC "Enable to include DiagInc camera code in build" OFF)
endif (DiagInc_FOUND)

find_package(EDT)
if (EDT_FOUND)
	OPTION(VIDEO_USE_EDT "Enable to include EDT camera code in build" ON)
else (EDT_FOUND)
	OPTION(VIDEO_USE_EDT "Enable to include EDT camera code in build" OFF)
endif (EDT_FOUND)

find_package(Roper)
if (ROPER_FOUND)
	OPTION(VIDEO_USE_ROPER "Enable to include Roper camera code in build" ON)
else (ROPER_FOUND)
	OPTION(VIDEO_USE_ROPER "Enable to include Roper camera code in build" OFF)
endif (ROPER_FOUND)

find_package(PointGrey)
if (POINTGREY_FOUND)
	OPTION(VIDEO_USE_POINTGREY "Enable to include Point Grey camera code in build" ON)
else (POINTGREY_FOUND)
	OPTION(VIDEO_USE_POINTGREY "Enable to include Point Grey camera code in build" OFF)
endif (POINTGREY_FOUND)

find_package(Qt4)
OPTION(VIDEO_USE_QT4 "Enable to include Qt video code in build" OFF)

find_package(FFMPEG)
if (FFMPEG_FOUND)
	OPTION(VIDEO_USE_FFMPEG "Enable to include FFMPEG video code in build" ON)
	INCLUDE_DIRECTORIES(
		${FFMPEG_INCLUDE_PATH}
	)
	if (APPLE)
	    link_libraries(z bz2)
	    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework CoreFoundation -framework VideoDecodeAcceleration -framework QuartzCore")
	endif (APPLE)
else (FFMPEG_FOUND)
	OPTION(VIDEO_USE_FFMPEG "Enable to include FFMPEG video code in build" OFF)
endif (FFMPEG_FOUND)

OPTION(VIDEO_USE_SEM "Enable to include SEM (from nanoManipulator) camera code in build" OFF)
OPTION(VIDEO_USE_VRPN_IMAGER "Enable to include VRPN_Imager camera code in build" ON)
OPTION(VIDEO_USE_DIRECTSHOW "Enable to include Microsoft DirectShow code in build" OFF)

#-----------------------------------------------------------------------------
# Set compile-time definitions for all projects based on what has been
# selected.

if (VIDEO_USE_IMAGEMAGICK)
	ADD_DEFINITIONS(-DVST_USE_IMAGEMAGICK)
endif (VIDEO_USE_IMAGEMAGICK)
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
if (VIDEO_USE_SEM)
	ADD_DEFINITIONS(-DVST_USE_SEM)
endif (VIDEO_USE_SEM)
if (VIDEO_USE_VRPN_IMAGER)
	ADD_DEFINITIONS(-DVST_USE_VRPN_IMAGER)
endif (VIDEO_USE_VRPN_IMAGER)
if (VIDEO_USE_POINTGREY)
	ADD_DEFINITIONS(-DVST_USE_POINTGREY)
endif (VIDEO_USE_POINTGREY)
if (VIDEO_USE_QT4)
	ADD_DEFINITIONS(-DVST_USE_QT4)
	INCLUDE(${QT_USE_FILE})
	ADD_DEFINITIONS(${QT_DEFINITIONS})
endif (VIDEO_USE_QT4)
if (VIDEO_USE_FFMPEG)
	ADD_DEFINITIONS(-DVST_USE_FFMPEG)
	ADD_DEFINITIONS(-D__STDC_CONSTANT_MACROS)
endif (VIDEO_USE_FFMPEG)

###
# On Windows 7, it does not work to install in the default location,
# which is the Program Files directory, because you have to not only have
# file permission to write there but also "run as administrator."  This
# means that "make install" from a Visual Studio project fails.  To get
# around that, we need to set CMAKE_INSTALL_PREFIX to something other
# than the default.  However, it is a cache variable that has already been
# set.  If we make a local variable, it uses this rather than the cache
# variable and never tells the poor user what happened (the GUI location
# looks standard but the files end up somewhere else).  If we make it a
# non-forced cache variable, it already has a value so does not change.
# If we make it a forced cache variable, it gets overwritten everytime
# and the user cannot change it on the GUI.  So we have a workaround here.
# We make a cache variable that records whether we have ever forced the
# install prefix.  If not, we force it.  If so, we don't force it again.
# This has the effect of setting it the first time cmake is run, showing
# the change in the GUI, and also letting the user change the value in
# the GUI if they don't like what we did.  If I knew how to do this only
# happen on Windows 7, I'd make the if(WIN32) more specific.
if (WIN32)
        if (NOT ONCE_SET_CMAKE_INSTALL_PREFIX)
                set(ONCE_SET_CMAKE_INSTALL_PREFIX true CACHE BOOL
                        "Have we set the install prefix yet?" FORCE)
                set(CMAKE_INSTALL_PREFIX c:/usr/local CACHE PATH
                    "Install path prefix, prepended onto install directories"
                    FORCE)
        endif (NOT ONCE_SET_CMAKE_INSTALL_PREFIX)
endif (WIN32)

if (VIDEO_BUILD_APPLICATIONS)
  INCLUDE_DIRECTORIES(
	${TCL_INCLUDE_PATH}
	${TK_INCLUDE_PATH}
	${TCL_X11_INCLUDE_PATH}
	${GLUT_INCLUDE_DIR}
  )
endif (VIDEO_BUILD_APPLICATIONS)

INCLUDE_DIRECTORIES(
	${Cooke_INCLUDE_PATH}
	${DiagInc_INCLUDE_PATH}
	${EDT_INCLUDE_PATH}
	${ROPER_INCLUDE_PATH}
	${POINTGREY_INCLUDE_PATH}
	stocc_random_number_generator/
	${CISMM_VIDEO_SOURCE_DIR}
)

#-----------------------------------------------------------------------------
# Camera-driver libraries
set(BCS_SOURCES base_camera_server.cpp raw_file_server.cpp)
set(BCS_PUBLIC_HEADERS base_camera_server.h raw_file_server.h controllable_video.h)
ADD_LIBRARY (base_camera_server_library
	${BCS_SOURCES} ${BCS_PUBLIC_HEADERS}
)
set_property(TARGET base_camera_server_library PROPERTY PUBLIC_HEADER ${BCS_PUBLIC_HEADERS})

if(NOT (APPLE))
install(TARGETS base_camera_server_library
	ARCHIVE DESTINATION lib
	PUBLIC_HEADER DESTINATION include)
endif (NOT (APPLE))
if (VIDEO_USE_IMAGEMAGICK)
	set(FS_SOURCES file_stack_server.cpp file_list.cpp)
	set(FS_PUBLIC_HEADERS file_stack_server.h file_list.h)
	ADD_DEFINITIONS( -D_CRT_SECURE_NO_WARNINGS )
	ADD_LIBRARY (file_stack_library
		${FS_SOURCES} ${FS_PUBLIC_HEADERS}
	)
	set_property(TARGET file_stack_library PROPERTY PUBLIC_HEADER ${FS_PUBLIC_HEADERS})
	if(NOT (APPLE))
	install(TARGETS file_stack_library
		ARCHIVE DESTINATION lib
		PUBLIC_HEADER DESTINATION include)
	endif (NOT (APPLE))
endif (VIDEO_USE_IMAGEMAGICK)

if (VIDEO_USE_COOKE)
	set(COOKE_SOURCES cooke_server.cpp)
	set(COOKE_PUBLIC_HEADERS cooke_server.h)
	ADD_LIBRARY (cooke_library
		${COOKE_SOURCES} ${COOKE_PUBLIC_HEADERS}
	)
	set_property(TARGET cooke_library PROPERTY PUBLIC_HEADER ${COOKE_PUBLIC_HEADERS})
	install(TARGETS cooke_library
		ARCHIVE DESTINATION lib
		PUBLIC_HEADER DESTINATION include)
endif (VIDEO_USE_COOKE)

if (VIDEO_USE_DIAGINC)
	set(DIAGINC_SOURCES diaginc_server.cpp)
	set(DIAGINC_PUBLIC_HEADERS diaginc_server.h)
	ADD_LIBRARY (diaginc_library
		${DIAGINC_SOURCES} ${DIAGINC_PUBLIC_HEADERS}
	)
	set_property(TARGET diaginc_library PROPERTY PUBLIC_HEADER ${DIAGINC_PUBLIC_HEADERS})
	install(TARGETS diaginc_library
		ARCHIVE DESTINATION lib
		PUBLIC_HEADER DESTINATION include)
endif (VIDEO_USE_DIAGINC)

if (VIDEO_USE_EDT)
	set(EDT_SOURCES EDT_server.cpp)
	set(EDT_PUBLIC_HEADERS EDT_server.h)
	ADD_LIBRARY (EDT_library
		${EDT_SOURCES} ${EDT_PUBLIC_HEADERS}
	)
	set_property(TARGET EDT_library PROPERTY PUBLIC_HEADER ${EDT_PUBLIC_HEADERS})
	install(TARGETS EDT_library
		ARCHIVE DESTINATION lib
		PUBLIC_HEADER DESTINATION include)
endif (VIDEO_USE_EDT)

if (VIDEO_USE_ROPER)
	set(ROPER_SOURCES roper_server.cpp)
	set(ROPER_PUBLIC_HEADERS roper_server.h)
	ADD_LIBRARY (Roper_library
		${ROPER_SOURCES} ${ROPER_PUBLIC_HEADERS}
	)
	set_property(TARGET Roper_library PROPERTY PUBLIC_HEADER ${ROPER_PUBLIC_HEADERS})
	install(TARGETS Roper_library
		ARCHIVE DESTINATION lib
		PUBLIC_HEADER DESTINATION include)
endif (VIDEO_USE_ROPER)

if (VIDEO_USE_POINTGREY)
	set(POINTGREY_SOURCES point_grey_server.cpp PGRCam.cpp)
	set(POINTGREY_PUBLIC_HEADERS point_grey_server.h PGRCam.h)
	ADD_LIBRARY (point_grey_library
		${POINTGREY_SOURCES} ${POINTGREY_PUBLIC_HEADERS}
	)
	set_property(TARGET point_grey_library PROPERTY PUBLIC_HEADER ${POINTGREY_PUBLIC_HEADERS})
	install(TARGETS point_grey_library
		ARCHIVE DESTINATION lib
		PUBLIC_HEADER DESTINATION include)
endif (VIDEO_USE_POINTGREY)

if (VIDEO_USE_VRPN_IMAGER)
	set(VRPNIMAGER_SOURCES VRPN_Imager_camera_server.cpp)
	set(VRPNIMAGER_PUBLIC_HEADERS VRPN_Imager_camera_server.h)
	ADD_LIBRARY (vrpn_imager_library
		${VRPNIMAGER_SOURCES} ${VRPNIMAGER_PUBLIC_HEADERS}
	)
	set_property(TARGET vrpn_imager_library PROPERTY PUBLIC_HEADER ${VRPNIMAGER_PUBLIC_HEADERS})
	install(TARGETS vrpn_imager_library
		ARCHIVE DESTINATION lib
		PUBLIC_HEADER DESTINATION include)
endif (VIDEO_USE_VRPN_IMAGER)

if (VIDEO_USE_QT4)
	set(QT4_SOURCES qt4_video_server.cpp)
	set(QT4_PUBLIC_HEADERS qt4_video_server.h)
	ADD_LIBRARY (qt4_video_library
		${QT4_SOURCES} ${QT4_PUBLIC_HEADERS}
	)
	set_property(TARGET qt4_video_library PROPERTY PUBLIC_HEADER ${QT4_PUBLIC_HEADERS})
	install(TARGETS qt4_video_library
		ARCHIVE DESTINATION lib
		PUBLIC_HEADER DESTINATION include)
endif (VIDEO_USE_QT4)

if (VIDEO_USE_FFMPEG)
	set(FFMPEG_SOURCES ffmpeg_video_server.cpp)
	set(FFMPEG_PUBLIC_HEADERS ffmpeg_video_server.h)
	ADD_LIBRARY (ffmpeg_video_library
		${FFMPEG_SOURCES} ${FFMPEG_PUBLIC_HEADERS}
	)
	set_property(TARGET ffmpeg_video_library PROPERTY PUBLIC_HEADER ${FFMPEG_PUBLIC_HEADERS})
	if (NOT (APPLE))
	install(TARGETS ffmpeg_video_library
		ARCHIVE DESTINATION lib
		PUBLIC_HEADER DESTINATION include)
	endif (NOT (APPLE))
endif (VIDEO_USE_FFMPEG)

# XXX NIKON

#-----------------------------------------------------------------------------
# DirectShow base-class library (if configured)
if (VIDEO_USE_DIRECTSHOW)
	FIND_PACKAGE(DirectShow REQUIRED)	# Need to have DirectShow SDK
	# Add this if you can't compile without out.
	# ADD_DEFINITIONS( -DCOINIT_DISABLE_OLE1DDE=0x4 )
	if (VIDEO_USE_DIRECTSHOW_APPS)
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
	endif (VIDEO_USE_DIRECTSHOW_APPS)

	set (DX_SOURCES directx_camera_server.cpp directx_videofile_server.cpp)
	set (DX_PUBLIC_HEADERS directx_camera_server.h directx_videofile_server.h)
	ADD_LIBRARY (directx_library ${DX_SOURCES} ${DX_PUBLIC_HEADERS})
	set_property(TARGET directx_library PROPERTY PUBLIC_HEADER ${DX_PUBLIC_HEADERS})
	install(TARGETS directx_library
		ARCHIVE DESTINATION lib
		PUBLIC_HEADER DESTINATION include)
	if (VIDEO_USE_DIRECTSHOW_APPS)
	install(TARGETS DirectShow_baseclasses
		ARCHIVE DESTINATION lib)
	set (DirectShow_LIBS DirectShow_baseclasses directx_library)

	SET(TARGET_NAME test_directx_library)
	ADD_EXECUTABLE(test_directx_library test_directx_library.cpp)
	TARGET_LINK_LIBRARIES(test_directx_library
		base_camera_server_library
		${VRPN_LIBRARIES} ${QUATLIB_LIBRARIES}
		${DirectShow_LIBS}
		${PTHREAD_LIBRARY}
		${OPENGL_gl_LIBRARY}
	)
	if (VIDEO_USE_IMAGEMAGICK)
		TARGET_LINK_LIBRARIES(test_directx_library
			file_stack_library
			${ImageMagick_Needed_Libs}
		)
	endif (VIDEO_USE_IMAGEMAGICK)
	SET_TARGET_PROPERTIES(test_directx_library PROPERTIES SOLUTION_FOLDER tests)
	endif (VIDEO_USE_DIRECTSHOW_APPS)

  if (VIDEO_BUILD_APPLICATIONS)
	if (VIDEO_USE_DIRECTSHOW_APPS)
	SET(TARGET_NAME stereo_spin)
	ADD_EXECUTABLE(stereo_spin stereo_spin.cpp)
	TARGET_LINK_LIBRARIES(stereo_spin
		directx_library
		base_camera_server_library
		Tcl_Linkvar
		${VRPN_LIBRARIES} ${QUATLIB_LIBRARIES}
		${DirectShow_LIBS}
		${PTHREAD_LIBRARY}
		${OPENGL_gl_LIBRARY}
		${TCL_LIBRARY}
		${TK_LIBRARY}
		${GLUT_LIBRARIES}
	)
	if (VIDEO_USE_IMAGEMAGICK)
		TARGET_LINK_LIBRARIES(stereo_spin
			file_stack_library
			${ImageMagick_Needed_Libs}
		)
	endif (VIDEO_USE_IMAGEMAGICK)
	if (VIDEO_USE_COOKE)
		TARGET_LINK_LIBRARIES(stereo_spin
			cooke_library
			${Cooke_LIBRARIES}
		)
	endif (VIDEO_USE_COOKE)
	if (VIDEO_USE_DIAGINC)
		TARGET_LINK_LIBRARIES(stereo_spin
			diaginc_library
			${DiagInc_LIBRARIES}
		)
	endif (VIDEO_USE_DIAGINC)
	if (VIDEO_USE_EDT)
		TARGET_LINK_LIBRARIES(stereo_spin
			EDT_library
			${EDT_LIBRARIES}
		)
	endif (VIDEO_USE_EDT)
	if (VIDEO_USE_ROPER)
		TARGET_LINK_LIBRARIES(stereo_spin
			Roper_library
			${ROPER_LIBRARIES}
		)
	endif (VIDEO_USE_ROPER)
	if (VIDEO_USE_POINTGREY)
		TARGET_LINK_LIBRARIES(stereo_spin
			point_grey_library
			${POINTGREY_LIBRARIES}
		)
	endif (VIDEO_USE_POINTGREY)
	if (VIDEO_USE_VRPN_IMAGER)
		TARGET_LINK_LIBRARIES(stereo_spin
			vrpn_imager_library
		)
	endif (VIDEO_USE_VRPN_IMAGER)
	if (VIDEO_USE_QT4)
		TARGET_LINK_LIBRARIES(stereo_spin
			qt4_video_library
			${QT_LIBRARIES}
		)
	endif (VIDEO_USE_QT4)
	if (VIDEO_USE_FFMPEG)
                SET(FFMPEG_LIBRARIES
		${FFMPEG_AVCODEC_LIBRARY}
		${FFMPEG_AVDEVICE_LIBRARY}
		${FFMPEG_AVFILTER_LIBRARY}
		${FFMPEG_AVFORMAT_LIBRARY}
		${FFMPEG_AVUTIL_LIBRARY}
		${FFMPEG_SWRESAMPLE_LIBRARY}
		${FFMPEG_SWSCALE_LIBRARY}
	        )
	        SET(FFMPEG_FOUND ON)
		TARGET_LINK_LIBRARIES(stereo_spin
			ffmpeg_video_library
			${FFMPEG_LIBRARIES}
		)
	endif (VIDEO_USE_FFMPEG)
	SET_TARGET_PROPERTIES(stereo_spin PROPERTIES SOLUTION_FOLDER servers)

	INCLUDE_DIRECTORIES(
		${DirectShow_INCLUDE_DIRS}
	)
	endif (VIDEO_USE_DIRECTSHOW_APPS)
  endif (VIDEO_BUILD_APPLICATIONS)

endif (VIDEO_USE_DIRECTSHOW)

#-----------------------------------------------------------------------------
# Spot tracker library
set(STL_SOURCES image_wrapper.cpp spot_math.cpp thread.cpp spot_tracker.cpp)
set(STL_PUBLIC_HEADERS image_wrapper.h spot_math.h thread.h spot_tracker.h)
ADD_LIBRARY (spot_tracker_library
	${STL_SOURCES} ${STL_PUBLIC_HEADERS}
)
set_property(TARGET spot_tracker_library PROPERTY PUBLIC_HEADER ${STL_PUBLIC_HEADERS})

if(NOT (APPLE))

install(TARGETS spot_tracker_library
	ARCHIVE DESTINATION lib
	PUBLIC_HEADER DESTINATION include)

endif(NOT (APPLE))
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
if(NOT (APPLE))
install(TARGETS stocc_random_number_generator_library
	ARCHIVE DESTINATION lib
	PUBLIC_HEADER DESTINATION include)
endif(NOT (APPLE))

#-----------------------------------------------------------------------------
# Tcl-Linkvar library (not a library in the initial project file)
if (VIDEO_BUILD_APPLICATIONS)
set(LV_SOURCES Tcl_Linkvar.C)
set(LV_PUBLIC_HEADERS Tcl_Linkvar.h)
ADD_LIBRARY (Tcl_Linkvar
	${LV_SOURCES} ${LV_PUBLIC_HEADERS}
)
set_property(TARGET Tcl_Linkvar PROPERTY PUBLIC_HEADER ${LV_PUBLIC_HEADERS})
if (NOT APPLE)
install(TARGETS Tcl_Linkvar
	ARCHIVE DESTINATION lib
	PUBLIC_HEADER DESTINATION include)
endif (NOT APPLE)
endif (VIDEO_BUILD_APPLICATIONS)

#-----------------------------------------------------------------------------
# Include directories needed
INCLUDE_DIRECTORIES(
	${QUATLIB_INCLUDE_DIR}
	${VRPN_INCLUDE_DIR}
)
if (VIDEO_USE_IMAGEMAGICK)
  INCLUDE_DIRECTORIES(
	${ImageMagick_MagickCore_INCLUDE_DIR}
  )
endif (VIDEO_USE_IMAGEMAGICK)
#-----------------------------------------------------------------------------
# Applications that need more than one file or nonstandard things.
if (VIDEO_BUILD_APPLICATIONS)

# Non-CUDA version of Video Spot Tracker
if (NOT VIDEO_USE_CUDA)
SET(TARGET_NAME video_spot_tracker)
IF(NOT APPLE)
	ADD_EXECUTABLE(video_spot_tracker video_spot_tracker.cpp)
ELSE(NOT APPLE)
	ADD_EXECUTABLE(video_spot_tracker MACOSX_BUNDLE video_spot_tracker.cpp)
ENDIF(NOT APPLE)
if (VIDEO_USE_IMAGEMAGICK)
	TARGET_LINK_LIBRARIES(video_spot_tracker
		file_stack_library
		${ImageMagick_Needed_Libs}
	)
endif (VIDEO_USE_IMAGEMAGICK)
if (VIDEO_USE_DIRECTSHOW)
	TARGET_LINK_LIBRARIES(video_spot_tracker
		directx_library
	)
endif (VIDEO_USE_DIRECTSHOW)  
if (VIDEO_USE_COOKE)
	TARGET_LINK_LIBRARIES(video_spot_tracker
		cooke_library
		${Cooke_LIBRARIES}
	)
endif (VIDEO_USE_COOKE)
if (VIDEO_USE_DIAGINC)
	TARGET_LINK_LIBRARIES(video_spot_tracker
		diaginc_library
		${DiagInc_LIBRARIES}
	)
endif (VIDEO_USE_DIAGINC)
if (VIDEO_USE_EDT)
	TARGET_LINK_LIBRARIES(video_spot_tracker
		EDT_library
		${EDT_LIBRARIES}
	)
endif (VIDEO_USE_EDT)
if (VIDEO_USE_ROPER)
	TARGET_LINK_LIBRARIES(video_spot_tracker
		Roper_library
		${ROPER_LIBRARIES}
	)
endif (VIDEO_USE_ROPER)
if (VIDEO_USE_POINTGREY)
	TARGET_LINK_LIBRARIES(video_spot_tracker
		point_grey_library
		${POINTGREY_LIBRARIES}
	)
endif (VIDEO_USE_POINTGREY)
if (VIDEO_USE_VRPN_IMAGER)
	TARGET_LINK_LIBRARIES(video_spot_tracker
		vrpn_imager_library
	)
endif (VIDEO_USE_VRPN_IMAGER)
if (VIDEO_USE_QT4)
	TARGET_LINK_LIBRARIES(video_spot_tracker
		qt4_video_library
		${QT_LIBRARIES}
	)
endif (VIDEO_USE_QT4)
if (VIDEO_USE_FFMPEG)
	SET(FFMPEG_LIBRARIES
	${FFMPEG_AVCODEC_LIBRARY}
	${FFMPEG_AVDEVICE_LIBRARY}
	${FFMPEG_AVFILTER_LIBRARY}
	${FFMPEG_AVFORMAT_LIBRARY}
	${FFMPEG_AVUTIL_LIBRARY}
	${FFMPEG_SWRESAMPLE_LIBRARY}
	${FFMPEG_SWSCALE_LIBRARY}
	)
	SET(FFMPEG_FOUND ON)
	TARGET_LINK_LIBRARIES(video_spot_tracker
		ffmpeg_video_library
		${FFMPEG_LIBRARIES}
	)
endif (VIDEO_USE_FFMPEG)
TARGET_LINK_LIBRARIES(video_spot_tracker
	spot_tracker_library
	base_camera_server_library
	Tcl_Linkvar
	${VRPN_SERVER_LIBRARIES} ${QUATLIB_LIBRARIES}
	${OPENGL_gl_LIBRARY}
	${TCL_LIBRARY}
	${TK_LIBRARY}
	${GLUT_LIBRARIES}
        ${FFMPEG_LIBRARIES}
)
SET_TARGET_PROPERTIES(video_spot_tracker PROPERTIES SOLUTION_FOLDER apps)

IF(NOT APPLE)
install(TARGETS video_spot_tracker
	RUNTIME DESTINATION bin)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/video_spot_tracker.tcl
		${CMAKE_CURRENT_SOURCE_DIR}/russ_widgets.tcl
	DESTINATION bin)
ELSE(NOT APPLE)
SET (CPACK_PACKAGE_FILE_NAME "Video_spot_tracker_v08.01_Intel_Mac_Universal")
FILE(COPY video_spot_tracker.tcl russ_widgets.tcl DESTINATION video_spot_tracker.app/Contents/MacOS)

# Copy all the glut libraries to the bundle
STRING(FIND ${GLUT_glut_LIBRARY} "/" DIR_END REVERSE)
STRING(SUBSTRING ${GLUT_glut_LIBRARY} 0 ${DIR_END} GLUT_LIB_DIR)
file(GLOB_RECURSE GLUT_ALL_LIBS 
     	 ${GLUT_LIB_DIR}/*${CMAKE_SHARED_LIBRARY_SUFFIX})
FILE(COPY ${GLUT_ALL_LIBS} DESTINATION video_spot_tracker.app/Contents/MacOS)

# Comment out this section if you don't want spot_tracker in the installer
#--------------------------------------------------------------------------
# configure_files makes CMAKE variables can be seen in the script
CONFIGURE_FILE(
    ${CMAKE_SOURCE_DIR}/mac_installer.cmake.in
    ${CMAKE_BINARY_DIR}/mac_installer.cmake 
    @ONLY IMMEDIATE
)
INSTALL(SCRIPT ${CMAKE_BINARY_DIR}/mac_installer.cmake)
INSTALL(TARGETS ${TARGET_NAME} BUNDLE DESTINATION .)
#--------------------------------------------------------------------------
ENDIF(NOT APPLE)
endif (NOT VIDEO_USE_CUDA)

# XXX Should move the CUDA compilation into the spot-tracker library code.
# XXX Then do we CUDA compile everything that uses spot-tracker lib?
if (VIDEO_USE_CUDA)
  SET(TARGET_NAME video_spot_tracker_CUDA)
  IF(NOT APPLE)
	CUDA_ADD_EXECUTABLE(video_spot_tracker_CUDA video_spot_tracker.cpp
spot_tracker.cu)
  ELSE(NOT APPLE)
	CUDA_ADD_EXECUTABLE(video_spot_tracker_CUDA MACOSX_BUNDLE video_spot_tracker.cpp)
  ENDIF(NOT APPLE)
  TARGET_LINK_LIBRARIES(video_spot_tracker_CUDA ${CUDA_CUDA_LIBRARY})

  if (VIDEO_USE_IMAGEMAGICK)
	TARGET_LINK_LIBRARIES(video_spot_tracker_CUDA
		file_stack_library
		${ImageMagick_Needed_Libs}
	)
  endif (VIDEO_USE_IMAGEMAGICK)
  if (VIDEO_USE_DIRECTSHOW)
	TARGET_LINK_LIBRARIES(video_spot_tracker_CUDA
		directx_library
	)
  endif (VIDEO_USE_DIRECTSHOW)  
  if (VIDEO_USE_COOKE)
	TARGET_LINK_LIBRARIES(video_spot_tracker_CUDA
		cooke_library
		${Cooke_LIBRARIES}
	)
  endif (VIDEO_USE_COOKE)
  if (VIDEO_USE_DIAGINC)
	TARGET_LINK_LIBRARIES(video_spot_tracker_CUDA
		diaginc_library
		${DiagInc_LIBRARIES}
	)
  endif (VIDEO_USE_DIAGINC)
  if (VIDEO_USE_EDT)
	TARGET_LINK_LIBRARIES(video_spot_tracker_CUDA
		EDT_library
		${EDT_LIBRARIES}
	)
  endif (VIDEO_USE_EDT)
  if (VIDEO_USE_ROPER)
	TARGET_LINK_LIBRARIES(video_spot_tracker_CUDA
		Roper_library
		${ROPER_LIBRARIES}
	)
  endif (VIDEO_USE_ROPER)
  if (VIDEO_USE_POINTGREY)
	TARGET_LINK_LIBRARIES(video_spot_tracker_CUDA
		point_grey_library
		${POINTGREY_LIBRARIES}
	)
  endif (VIDEO_USE_POINTGREY)
  if (VIDEO_USE_VRPN_IMAGER)
	TARGET_LINK_LIBRARIES(video_spot_tracker_CUDA
		vrpn_imager_library
	)
  endif (VIDEO_USE_VRPN_IMAGER)
  if (VIDEO_USE_QT4)
	TARGET_LINK_LIBRARIES(video_spot_tracker_CUDA
		qt4_video_library
		${QT_LIBRARIES}
	)
  endif (VIDEO_USE_QT4)
  if (VIDEO_USE_FFMPEG)
	SET(FFMPEG_LIBRARIES
	${FFMPEG_AVCODEC_LIBRARY}
	${FFMPEG_AVDEVICE_LIBRARY}
	${FFMPEG_AVFILTER_LIBRARY}
	${FFMPEG_AVFORMAT_LIBRARY}
	${FFMPEG_AVUTIL_LIBRARY}
	${FFMPEG_SWRESAMPLE_LIBRARY}
	${FFMPEG_SWSCALE_LIBRARY}
	)
	SET(FFMPEG_FOUND ON)
	TARGET_LINK_LIBRARIES(video_spot_tracker_CUDA
		ffmpeg_video_library
		${FFMPEG_LIBRARIES}
	)
  endif (VIDEO_USE_FFMPEG)
  TARGET_LINK_LIBRARIES(video_spot_tracker_CUDA
	spot_tracker_library
	base_camera_server_library
	Tcl_Linkvar
	${VRPN_SERVER_LIBRARIES} ${QUATLIB_LIBRARIES}
	${OPENGL_gl_LIBRARY}
	${TCL_LIBRARY}
	${TK_LIBRARY}
	${GLUT_LIBRARIES}
  )
  SET_TARGET_PROPERTIES(video_spot_tracker_CUDA PROPERTIES SOLUTION_FOLDER apps)

  IF(NOT APPLE)
    install(TARGETS video_spot_tracker_CUDA RUNTIME DESTINATION bin)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/video_spot_tracker.tcl
		${CMAKE_CURRENT_SOURCE_DIR}/russ_widgets.tcl DESTINATION bin)
  ELSE(NOT APPLE)
    FILE(COPY video_spot_tracker.tcl russ_widgets.tcl DESTINATION video_spot_tracker_CUDA.app/Contents/MacOS)
    INSTALL(TARGETS ${TARGET_NAME} BUNDLE DESTINATION .)

    # Comment out this section if you don't want spot_tracker_CUDA in the installer
    #--------------------------------------------------------------------------
    # configure_files makes CMAKE variables can be seen in the script
    #CONFIGURE_FILE(
    #  ${CMAKE_SOURCE_DIR}/mac_installer.cmake.in
    #  ${CMAKE_BINARY_DIR}/mac_installer.cmake 
    #  @ONLY IMMEDIATE
    #)
    #INSTALL(SCRIPT ${CMAKE_BINARY_DIR}/mac_installer.cmake)
    #INSTALL(TARGETS ${TARGET_NAME} BUNDLE DESTINATION .)
    #--------------------------------------------------------------------------
  ENDIF(NOT APPLE)

endif (VIDEO_USE_CUDA)

# XXX Should move the CUDA compilation into the spot-tracker library code.
# XXX Then do we CUDA compile everything that uses spot-tracker lib?
SET(TARGET_NAME video_spot_tracker_nogui)
if (VIDEO_USE_CUDA)
	CUDA_ADD_EXECUTABLE(video_spot_tracker_nogui video_spot_tracker.cpp spot_tracker.cu)
	TARGET_LINK_LIBRARIES(video_spot_tracker_nogui ${CUDA_CUDA_LIBRARY})
else (VIDEO_USE_CUDA)
	ADD_EXECUTABLE(video_spot_tracker_nogui video_spot_tracker.cpp)
endif (VIDEO_USE_CUDA)
if (VIDEO_USE_IMAGEMAGICK)
	TARGET_LINK_LIBRARIES(video_spot_tracker_nogui
		file_stack_library
		${ImageMagick_Needed_Libs}
	)
endif (VIDEO_USE_IMAGEMAGICK)
if (VIDEO_USE_DIRECTSHOW)
	TARGET_LINK_LIBRARIES(video_spot_tracker_nogui
		directx_library
	)
endif (VIDEO_USE_DIRECTSHOW)  
if (VIDEO_USE_COOKE)
	TARGET_LINK_LIBRARIES(video_spot_tracker_nogui
		cooke_library
		${Cooke_LIBRARIES}
	)
endif (VIDEO_USE_COOKE)
if (VIDEO_USE_DIAGINC)
	TARGET_LINK_LIBRARIES(video_spot_tracker_nogui
		diaginc_library
		${DiagInc_LIBRARIES}
	)
endif (VIDEO_USE_DIAGINC)
if (VIDEO_USE_EDT)
	TARGET_LINK_LIBRARIES(video_spot_tracker_nogui
		EDT_library
		${EDT_LIBRARIES}
	)
endif (VIDEO_USE_EDT)
if (VIDEO_USE_ROPER)
	TARGET_LINK_LIBRARIES(video_spot_tracker_nogui
		Roper_library
		${ROPER_LIBRARIES}
	)
endif (VIDEO_USE_ROPER)
if (VIDEO_USE_POINTGREY)
	TARGET_LINK_LIBRARIES(video_spot_tracker_nogui
		point_grey_library
		${POINTGREY_LIBRARIES}
	)
endif (VIDEO_USE_POINTGREY)
if (VIDEO_USE_VRPN_IMAGER)
	TARGET_LINK_LIBRARIES(video_spot_tracker_nogui
		vrpn_imager_library
	)
endif (VIDEO_USE_VRPN_IMAGER)
if (VIDEO_USE_QT4)
	TARGET_LINK_LIBRARIES(video_spot_tracker_nogui
		qt4_video_library
		${QT_LIBRARIES}
	)
endif (VIDEO_USE_QT4)
if (VIDEO_USE_FFMPEG)
	SET(FFMPEG_LIBRARIES
	${FFMPEG_AVCODEC_LIBRARY}
	${FFMPEG_AVDEVICE_LIBRARY}
	${FFMPEG_AVFILTER_LIBRARY}
	${FFMPEG_AVFORMAT_LIBRARY}
	${FFMPEG_AVUTIL_LIBRARY}
	${FFMPEG_SWRESAMPLE_LIBRARY}
	${FFMPEG_SWSCALE_LIBRARY}
	)
	SET(FFMPEG_FOUND ON)
	TARGET_LINK_LIBRARIES(video_spot_tracker_nogui
		ffmpeg_video_library
		${FFMPEG_LIBRARIES}
	)
endif (VIDEO_USE_FFMPEG)
TARGET_LINK_LIBRARIES(video_spot_tracker_nogui
	spot_tracker_library
	base_camera_server_library
	Tcl_Linkvar
	${VRPN_SERVER_LIBRARIES} ${QUATLIB_LIBRARIES}
	${OPENGL_gl_LIBRARY}
	${TCL_LIBRARY}
	${TK_LIBRARY}
	${GLUT_LIBRARIES}
)
set_target_properties(video_spot_tracker_nogui PROPERTIES COMPILE_FLAGS -DVST_NO_GUI)
SET_TARGET_PROPERTIES(video_spot_tracker_nogui PROPERTIES SOLUTION_FOLDER apps)

IF(NOT APPLE)
install(TARGETS video_spot_tracker_nogui
	RUNTIME DESTINATION bin)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/video_spot_tracker.tcl
		${CMAKE_CURRENT_SOURCE_DIR}/russ_widgets.tcl
	DESTINATION bin)
ELSE(NOT APPLE)
# Comment out this if you don't want spot_tracker_nogui in the installer
# ------------------------------------------------------------------
INSTALL(TARGETS ${TARGET_NAME} DESTINATION .)
# ------------------------------------------------------------------
ENDIF(NOT APPLE)

if (VIDEO_USE_CUDA)
  SET(TARGET_NAME CUDA_video_filter)
  IF(NOT APPLE)
	CUDA_ADD_EXECUTABLE(CUDA_video_filter CUDA_video_filter.cpp spot_tracker.cu)
  ELSE(NOT APPLE)
	CUDA_ADD_EXECUTABLE(CUDA_video_filter MACOSX_BUNDLE CUDA_video_filter.cpp spot_tracker.cu)
  ENDIF(NOT APPLE)
  TARGET_LINK_LIBRARIES(CUDA_video_filter ${CUDA_CUDA_LIBRARY})

  if (VIDEO_USE_IMAGEMAGICK)
	TARGET_LINK_LIBRARIES(CUDA_video_filter
		file_stack_library
		${ImageMagick_Needed_Libs}
	)
  endif (VIDEO_USE_IMAGEMAGICK)
  if (VIDEO_USE_DIRECTSHOW)
	TARGET_LINK_LIBRARIES(CUDA_video_filter
		directx_library
	)
  endif (VIDEO_USE_DIRECTSHOW)  
  if (VIDEO_USE_VRPN_IMAGER)
	TARGET_LINK_LIBRARIES(CUDA_video_filter
		vrpn_imager_library
	)
  endif (VIDEO_USE_VRPN_IMAGER)
  if (VIDEO_USE_FFMPEG)
	SET(FFMPEG_LIBRARIES
	${FFMPEG_AVCODEC_LIBRARY}
	${FFMPEG_AVDEVICE_LIBRARY}
	${FFMPEG_AVFILTER_LIBRARY}
	${FFMPEG_AVFORMAT_LIBRARY}
	${FFMPEG_AVUTIL_LIBRARY}
	${FFMPEG_SWRESAMPLE_LIBRARY}
	${FFMPEG_SWSCALE_LIBRARY}
	)
	SET(FFMPEG_FOUND ON)
	TARGET_LINK_LIBRARIES(CUDA_video_filter
		ffmpeg_video_library
		${FFMPEG_LIBRARIES}
	)
  endif (VIDEO_USE_FFMPEG)
  TARGET_LINK_LIBRARIES(CUDA_video_filter
	spot_tracker_library
	base_camera_server_library
	Tcl_Linkvar
	${VRPN_SERVER_LIBRARIES} ${QUATLIB_LIBRARIES}
	${OPENGL_gl_LIBRARY}
	${TCL_LIBRARY}
	${TK_LIBRARY}
	${GLUT_LIBRARIES}
  )
  SET_TARGET_PROPERTIES(CUDA_video_filter PROPERTIES SOLUTION_FOLDER apps)

  IF(NOT APPLE)
    install(TARGETS CUDA_video_filter RUNTIME DESTINATION bin)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/CUDA_video_filter.tcl
		${CMAKE_CURRENT_SOURCE_DIR}/russ_widgets.tcl DESTINATION bin)
  ELSE(NOT APPLE)
    FILE(COPY CUDA_video_filter.tcl russ_widgets.tcl DESTINATION CUDA_video_filter.app/Contents/MacOS)
    INSTALL(TARGETS ${TARGET_NAME} BUNDLE DESTINATION .)

    # Comment out this section if you don't want CUDA_video_filter in the installer
    #--------------------------------------------------------------------------
    # configure_files makes CMAKE variables can be seen in the script
    #CONFIGURE_FILE(
    #  ${CMAKE_SOURCE_DIR}/mac_installer.cmake.in
    #  ${CMAKE_BINARY_DIR}/mac_installer.cmake 
    #  @ONLY IMMEDIATE
    #)
    #--------------------------------------------------------------------------
    #INSTALL(SCRIPT ${CMAKE_BINARY_DIR}/mac_installer.cmake)
    #INSTALL(TARGETS ${TARGET_NAME} BUNDLE DESTINATION .)
  ENDIF(NOT APPLE)

endif (VIDEO_USE_CUDA)

SET(TARGET_NAME swap_ppm_lines)
ADD_EXECUTABLE(swap_ppm_lines swap_ppm_lines.cpp ppm.C)
TARGET_LINK_LIBRARIES(swap_ppm_lines
)
SET_TARGET_PROPERTIES(swap_ppm_lines PROPERTIES SOLUTION_FOLDER apps)

if (VIDEO_USE_EDT)
	SET(TARGET_NAME GLUItake)
	ADD_EXECUTABLE(GLUItake
		glui_take.cpp
	)
	TARGET_LINK_LIBRARIES(GLUItake
		EDT_library
		${VRPN_SERVER_LIBRARIES} ${QUATLIB_LIBRARIES}
		${EDT_LIBRARIES}
		${GLUI_LIBRARIES}
		${GLUT_LIBRARIES}
		${OPENGL_gl_LIBRARY}
	)
	SET_TARGET_PROPERTIES(GLUItake PROPERTIES SOLUTION_FOLDER apps)
endif (VIDEO_USE_EDT)

#-----------------------------------------------------------------------------
# A little utility helper macro to setup default linkages and filenames
MACRO(CAPC_APPLICATION APPLICATION_NAME SOLUTION_NAME)
  SET(TARGET_NAME ${APPLICATION_NAME})
  ADD_EXECUTABLE(${APPLICATION_NAME} ${APPLICATION_NAME}.C)
  TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
	stocc_random_number_generator_library
	spot_tracker_library
	base_camera_server_library
	Tcl_Linkvar
  )
  if (VIDEO_USE_IMAGEMAGICK)
	TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
		file_stack_library
		${ImageMagick_Needed_Libs}
	)
  endif (VIDEO_USE_IMAGEMAGICK)
  if (VIDEO_USE_DIRECTSHOW)
    TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
	directx_library
    )
  endif (VIDEO_USE_DIRECTSHOW)  
  if (VIDEO_USE_COOKE)
		TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
			cooke_library
			${Cooke_LIBRARIES}
		)
  endif (VIDEO_USE_COOKE)
  if (VIDEO_USE_DIAGINC)
		TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
			diaginc_library
			${DiagInc_LIBRARIES}
		)
  endif (VIDEO_USE_DIAGINC)
  if (VIDEO_USE_EDT)
		TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
			EDT_library
			${EDT_LIBRARIES}
		)
  endif (VIDEO_USE_EDT)
  if (VIDEO_USE_ROPER)
		TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
			Roper_library
			${ROPER_LIBRARIES}
		)
  endif (VIDEO_USE_ROPER)
  if (VIDEO_USE_POINTGREY)
		TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
			point_grey_library
			${POINTGREY_LIBRARIES}
		)
  endif (VIDEO_USE_POINTGREY)
  if (VIDEO_USE_VRPN_IMAGER)
		TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
			vrpn_imager_library
		)
  endif (VIDEO_USE_VRPN_IMAGER)
  if (VIDEO_USE_QT4)
		TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
			qt4_video_library
			${QT_LIBRARIES}
		)
  endif (VIDEO_USE_QT4)
  if (VIDEO_USE_FFMPEG)
                SET(FFMPEG_LIBRARIES
		${FFMPEG_AVCODEC_LIBRARY}
		${FFMPEG_AVDEVICE_LIBRARY}
		${FFMPEG_AVFILTER_LIBRARY}
		${FFMPEG_AVFORMAT_LIBRARY}
		${FFMPEG_AVUTIL_LIBRARY}
		${FFMPEG_SWRESAMPLE_LIBRARY}
		${FFMPEG_SWSCALE_LIBRARY}
	        )
	        SET(FFMPEG_FOUND ON)
		TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
			ffmpeg_video_library
			${FFMPEG_LIBRARIES}
		)
  endif (VIDEO_USE_FFMPEG)
  TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
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

if( ${APPLICATION_NAME} MATCHES cismm_video_optimizer )

IF(NOT APPLE)
  ADD_EXECUTABLE(${APPLICATION_NAME} ${APPLICATION_NAME}.cpp)
ELSE(NOT APPLE)
  ADD_EXECUTABLE(${APPLICATION_NAME} MACOSX_BUNDLE ${APPLICATION_NAME}.cpp)
  FILE(COPY cismm_video_optimizer.tcl russ_widgets.tcl DESTINATION cismm_video_optimizer.app/Contents/MacOS)

  # Comment out this section if you don't want video_optimizer in the installer
  #--------------------------------------------------------------------------
  # configure_files makes CMAKE variables can be seen in the script
  #CONFIGURE_FILE(
  #  ${CMAKE_SOURCE_DIR}/mac_installer.cmake.in
  #  ${CMAKE_BINARY_DIR}/mac_installer.cmake 
  #  @ONLY IMMEDIATE
  #)
  #INSTALL(SCRIPT ${CMAKE_BINARY_DIR}/mac_installer.cmake)
  #INSTALL(TARGETS ${TARGET_NAME} BUNDLE DESTINATION .)
  #--------------------------------------------------------------------------
ENDIF(NOT APPLE)

ELSE()
  ADD_EXECUTABLE(${APPLICATION_NAME} ${APPLICATION_NAME}.cpp)
ENDIF() # Match cismm_video_optimizer

  TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
	stocc_random_number_generator_library
	spot_tracker_library
	base_camera_server_library
	Tcl_Linkvar
  )
  if (VIDEO_USE_IMAGEMAGICK)
	TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
		file_stack_library
		${ImageMagick_Needed_Libs}
	)
  endif (VIDEO_USE_IMAGEMAGICK)
  if (VIDEO_USE_DIRECTSHOW)
    TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
	directx_library
    )
  endif (VIDEO_USE_DIRECTSHOW)  
  if (VIDEO_USE_COOKE)
		TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
			cooke_library
			${Cooke_LIBRARIES}
		)
  endif (VIDEO_USE_COOKE)
  if (VIDEO_USE_DIAGINC)
		TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
			diaginc_library
			${DiagInc_LIBRARIES}
		)
  endif (VIDEO_USE_DIAGINC)
  if (VIDEO_USE_EDT)
		TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
			EDT_library
			${EDT_LIBRARIES}
		)
  endif (VIDEO_USE_EDT)
  if (VIDEO_USE_ROPER)
		TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
			Roper_library
			${ROPER_LIBRARIES}
		)
  endif (VIDEO_USE_ROPER)
  if (VIDEO_USE_POINTGREY)
		TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
			point_grey_library
			${POINTGREY_LIBRARIES}
		)
  endif (VIDEO_USE_POINTGREY)
  if (VIDEO_USE_VRPN_IMAGER)
		TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
			vrpn_imager_library
		)
  endif (VIDEO_USE_VRPN_IMAGER)
  if (VIDEO_USE_QT4)
		TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
			qt4_video_library
			${QT_LIBRARIES}
		)
  endif (VIDEO_USE_QT4)
  if (VIDEO_USE_FFMPEG)
                SET(FFMPEG_LIBRARIES
		${FFMPEG_AVCODEC_LIBRARY}
		${FFMPEG_AVDEVICE_LIBRARY}
		${FFMPEG_AVFILTER_LIBRARY}
		${FFMPEG_AVFORMAT_LIBRARY}
		${FFMPEG_AVUTIL_LIBRARY}
		${FFMPEG_SWRESAMPLE_LIBRARY}
		${FFMPEG_SWSCALE_LIBRARY}
	        )
	        SET(FFMPEG_FOUND ON)
	        TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
		${FFMPEG_LIBRARIES}
		ffmpeg_video_library
	        )
  endif (VIDEO_USE_FFMPEG)
  TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
	${VRPN_SERVER_LIBRARIES}
	${QUATLIB_LIBRARIES}
	${TCL_LIBRARY}
	${TK_LIBRARY}
	${GLUT_LIBRARIES}
	${OPENGL_gl_LIBRARY}
	${PTHREAD_LIBRARY}
	${FFMPEG_LIBRARIES}
  )
  SET_TARGET_PROPERTIES(${APPLICATION_NAME} PROPERTIES SOLUTION_FOLDER ${SOLUTION_NAME})


ENDMACRO(CPP_APPLICATION)

MACRO(CPP_NOGUI_APPLICATION APPLICATION_NAME SOLUTION_NAME)
  SET(TARGET_NAME ${APPLICATION_NAME})
  ADD_EXECUTABLE(${APPLICATION_NAME} ${APPLICATION_NAME}.cpp)
  if (VIDEO_USE_IMAGEMAGICK)
	TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
		file_stack_library
		${ImageMagick_Needed_Libs}
	)
  endif (VIDEO_USE_IMAGEMAGICK)
  if (VIDEO_USE_DIRECTSHOW)
    TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
	directx_library
    )
  endif (VIDEO_USE_DIRECTSHOW)  
  if (VIDEO_USE_COOKE)
		TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
			cooke_library
			${Cooke_LIBRARIES}
		)
  endif (VIDEO_USE_COOKE)
  if (VIDEO_USE_DIAGINC)
		TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
			diaginc_library
			${DiagInc_LIBRARIES}
		)
  endif (VIDEO_USE_DIAGINC)
  if (VIDEO_USE_EDT)
		TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
			EDT_library
			${EDT_LIBRARIES}
		)
  endif (VIDEO_USE_EDT)
  if (VIDEO_USE_ROPER)
		TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
			Roper_library
			${ROPER_LIBRARIES}
		)
  endif (VIDEO_USE_ROPER)
  if (VIDEO_USE_POINTGREY)
		TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
			point_grey_library
			${POINTGREY_LIBRARIES}
		)
  endif (VIDEO_USE_POINTGREY)
  if (VIDEO_USE_VRPN_IMAGER)
		TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
			vrpn_imager_library
		)
  endif (VIDEO_USE_VRPN_IMAGER)
  if (VIDEO_USE_QT4)
		TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
			qt4_video_library
			${QT_LIBRARIES}
		)
  endif (VIDEO_USE_QT4)
  if (VIDEO_USE_FFMPEG)
                SET(FFMPEG_LIBRARIES
		${FFMPEG_AVCODEC_LIBRARY}
		${FFMPEG_AVDEVICE_LIBRARY}
		${FFMPEG_AVFILTER_LIBRARY}
		${FFMPEG_AVFORMAT_LIBRARY}
		${FFMPEG_AVUTIL_LIBRARY}
		${FFMPEG_SWRESAMPLE_LIBRARY}
		${FFMPEG_SWSCALE_LIBRARY}
	        )
	        SET(FFMPEG_FOUND ON)
		TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
			ffmpeg_video_library
			${FFMPEG_LIBRARIES}
		)
  endif (VIDEO_USE_FFMPEG)
  TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
	${VRPN_SERVER_LIBRARIES}
	${QUATLIB_LIBRARIES}
	${PTHREAD_LIBRARY}
  )
  TARGET_LINK_LIBRARIES(${APPLICATION_NAME}
	stocc_random_number_generator_library
	spot_tracker_library
	base_camera_server_library
	${OPENGL_gl_LIBRARY}
  )
  SET_TARGET_PROPERTIES(${APPLICATION_NAME} PROPERTIES SOLUTION_FOLDER ${SOLUTION_NAME})
ENDMACRO(CPP_NOGUI_APPLICATION)

#-----------------------------------------------------------------------------
# Declaration of the simple applications that don't have extra files

CPP_APPLICATION(add_noise_to_image apps)
CPP_APPLICATION(invert_image apps)
CPP_APPLICATION(airy_maker apps)
if (WIN32)
	CPP_APPLICATION(argonot apps)
endif (WIN32)
CPP_APPLICATION(average_videos apps)
if (NOT VIDEO_USE_CUDA)
CPP_APPLICATION(cismm_video_optimizer apps)
endif (NOT VIDEO_USE_CUDA)
CPP_APPLICATION(compare_tracking_files apps)
CPP_APPLICATION(check_found_spots apps)
if (VIDEO_USE_EDT)
	CPP_APPLICATION(edt_video_take apps)
	CAPC_APPLICATION(manual_capture apps)
endif (VIDEO_USE_EDT)
if (NOT VIDEO_USE_CUDA)
CPP_APPLICATION(make_compression_video apps)
CPP_APPLICATION(make_moving_spot_video apps)
CPP_APPLICATION(make_multi_spot_test apps)
CPP_APPLICATION(make_test_image apps)
endif (NOT VIDEO_USE_CUDA)
CPP_NOGUI_APPLICATION(video_imager_server apps)
CPP_APPLICATION(csv_to_xml apps)
CPP_APPLICATION(xml_tracking_compare apps)
if (VIDEO_USE_ROPER)
	CPP_APPLICATION(roper_example apps)
	CPP_APPLICATION(roper_spot_tracker apps)
endif (VIDEO_USE_ROPER)
if (NOT VIDEO_USE_CUDA)
CPP_APPLICATION(test_spot_tracker tests)
endif (NOT VIDEO_USE_CUDA)

# XXX stocc_test_poisson
# CPP_APPLICATION(stack_collector apps) XXX requires vrpn_Generic_Server_Object

endif (VIDEO_BUILD_APPLICATIONS)

IF(APPLE)
SET(CPACK_GENERATOR DragNDrop)
INCLUDE(CPACK)
ENDIF(APPLE)
