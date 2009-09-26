# this is a gmake file

##########################
# common definitions. For non-UNC sites, uncomment one of the lines
# that defines hw_os for the machine you are on in the section just
# below. Then, the code should compile in your environment.
#
#HW_OS := sgi_irix
#HW_OS := sparc_solaris
#HW_OS := sparc_solaris_64
#HW_OS := hp700_hpux10
#HW_OS := pc_linux
#HW_OS := pc_linux64
#HW_OS := pc_linux_ia64
#HW_OS := pc_cygwin
#HW_OS := pc_FreeBSD
#HW_OS := powerpc_aix
#HW_OS := powerpc_macosx
##########################

IMAGEMAGIC_INCLUDE := ../External/pc_linux64/include/ImageMagick
IMAGEMAGIC_LIB := ../External/pc_linux64/lib

OPENCV_INCLUDE := /usr/include/opencv

##########################
# Directories for installation

INSTALL_DIR := /usr/local
BIN_DIR := $(INSTALL_DIR)/bin
INCLUDE_DIR := $(INSTALL_DIR)/include
LIB_DIR := $(INSTALL_DIR)/lib

ifndef	HW_OS
# hw_os does not exist on FreeBSD at UNC
UNAME := $(shell uname -s)
ifeq ($(UNAME), FreeBSD)
  HW_OS := pc_FreeBSD
else
  # pc_cygwin doesn't have HW_OS
  ifeq ($(UNAME), CYGWIN_NT-4.0)
    HW_OS := pc_cygwin
    # On cygwin make is gmake (and gmake doesn't exist)
    MAKE  := make -f $(MAKEFILE)
  else
    ifeq ($(UNAME), CYGWIN_98-4.10)
		HW_OS := pc_cygwin
		MAKE := make -f $(MAKEFILE)
	else
	  HW_OS := $(shell hw_os)
	endif
  endif
endif
endif

CC := g++
AR := ar ruv
RANLIB := touch
GL := -lGL

ifeq ($(HW_OS),pc_cygwin)
	SYSLIBS := -lcygwin -luser32 -lgdi32 -lcomdlg32 -lwsock32
endif
ifeq ($(HW_OS),sparc_solaris)
	CC := /opt/SUNWspro/bin/CC
	SYSLIBS := -lsocket -lnsl
	AR := /opt/SUNWspro/bin/CC -xar -o
endif
ifeq ($(HW_OS),sparc_solaris_64)
	CC := /opt/SUNWspro/bin/CC -xarch=v9a
	SYSLIBS := -lsocket -lnsl
	AR := /opt/SUNWspro/bin/CC -xarch=v9a -xar -o
endif
ifeq ($(HW_OS),powerpc_aix)
	CC := /usr/ibmcxx/bin/xlC_r -g -qarch=pwr3 -w
	RANLIB := ranlib
endif

ifeq ($(HW_OS),pc_linux_ia64)
	CC := g++
	SYSLIBS := -lgpm
endif
ifeq ($(HW_OS),pc_linux64)
	CC := g++ -m64
	SYSLIBS := -lgpm
	RANLIB := ranlib
endif
ifeq ($(HW_OS),pc_linux)
	SYSLIBS := -lgpm
	RANLIB := ranlib
endif

ifeq ($(HW_OS),sgi_irix)
   ifndef SGI_ABI
      SGI_ABI := n32
   endif
   ifndef SGI_ARCH
      SGI_ARCH := mips3
   endif
   OBJECT_DIR_SUFFIX := .$(SGI_ABI).$(SGI_ARCH)
   CC := CC -$(SGI_ABI) -$(SGI_ARCH)
endif

ifeq ($(HW_OS),hp700_hpux10)
	CC := CC +a1
endif
ifeq ($(HW_OS),sparc_sunos)
	CC := /usr/local/lib/CenterLine/bin/CC
endif

ifeq ($(HW_OS), hp_flow_aCC)
	CC := /opt/aCC/bin/aCC
endif

OBJ_DIR := $(HW_OS)$(OBJECT_DIR_SUFFIX)
LIB_DIR := ../$(OBJ_DIR)

CFLAGS = -g -I./ -Istocc_random_number_generator -I../quat -I../vrpn \
	-I../nano/src/lib/nmMP -I/usr/local/include -I$(IMAGEMAGIC_INCLUDE) -I$(OPENCV_INCLUDE)

# The -rpath command specifies that the system should look for shared objects in
# the specified location.  In our case, this is an attempt to get ImageMagick
# shared objects to load.
LFLAGS = -L$(LIB_DIR) -L../quat/$(HW_OS)$(OBJECT_DIR_SUFFIX) -L../vrpn/$(HW_OS)$(OBJECT_DIR_SUFFIX) -L$(HW_OS)$(OBJECT_DIR_SUFFIX) -L../nano/obj/$(HW_OS)/lib/nmMP -L$(IMAGEMAGIC_LIB) -W1,-rpath,/usr/local/lib

TCLLIBS = -ltk8.5 -ltcl8.5 -lXss

MAGICLIBS = -lMagickCore -ltiff -lfreetype -ljpeg -lpng -lfontconfig -lXext -lXt -lSM -lICE -lX11 -lbz2 -lrsvg-2 -lgdk_pixbuf-2.0 -lgobject-2.0 -lgmodule-2.0 -ldl -lglib-2.0 -lxml2 -lz -lpthread -lpthread 

VRPNLIBS = -lvrpn -lquat

#OPENCVLIBS = -lcvaux -lcvservice -lcv
OPENCVLIBS = -lcv

.SUFFIXES: .cpp

.C.o:
	$(CC) $(CFLAGS) -c $*.C

.c.o:
	$(CC) $(CFLAGS) -c $*.c

.cpp.o:
	$(CC) $(CFLAGS) -c $*.cpp

$(OBJ_DIR)/%.o:	%.c
	@[ -d $(OBJ_DIR) ] || mkdir $(OBJ_DIR)
	$(CC) $(CFLAGS) -o $@ -c $<

$(OBJ_DIR)/%.o:	%.C
	@[ -d $(OBJ_DIR) ] || mkdir $(OBJ_DIR)
	$(CC) $(CFLAGS) -o $@ -c $<

$(OBJ_DIR)/%.o:	%.cpp
	@[ -d $(OBJ_DIR) ] || mkdir $(OBJ_DIR)
	$(CC) $(CFLAGS) -o $@ -c $<

STOCC_LIB_FILES = stocc_random_number_generator/mersenne.cpp stocc_random_number_generator/stoc1.cpp stocc_random_number_generator/userintf.cpp
STOCC_LIB_OBJECTS = $(patsubst %,%,$(STOCC_LIB_FILES:.cpp=.o))

SPOT_TRACKER_LIB_FILES = spot_math.cpp spot_tracker.cpp image_wrapper.cpp base_camera_server.cpp file_stack_server.cpp file_list.cpp VRPN_Imager_camera_server.cpp
SPOT_TRACKER_LIB_OBJECTS = $(patsubst %,$(OBJ_DIR)/%,$(SPOT_TRACKER_LIB_FILES:.cpp=.o))

TCL_LINKVAR_LIB_FILES = Tcl_Linkvar85.C
TCL_LINKVAR_LIB_OBJECTS = $(patsubst %,$(OBJ_DIR)/%,$(TCL_LINKVAR_LIB_FILES:.C=.o))

EDT_LIB_FILES = edt_server.cpp
EDT_LIB_OBJECTS = $(patsubst %,$(OBJ_DIR)/%,$(EDT_LIB_FILES:.cpp=.o))

INSTALL_APPS := video_spot_tracker
APPS := test_spot_tracker add_noise_to_image $(INSTALL_APPS)
LIBS := libspottracker.a

all: $(LIBS) $(APPS)

.PHONY:	libspottracker.a
libspottracker.a:	$(OBJ_DIR)/libspottracker.a

.PHONY:	add_noise_to_image
add_noise_to_image:	$(OBJ_DIR)/add_noise_to_image

.PHONY:	average_videos
average_videos:	$(OBJ_DIR)/average_videos

.PHONY:	libedt.a
libedt.a:	$(OBJ_DIR)/libedt.a

.PHONY:	libtcllinkvar85.a
libtcllinkvar85.a:	$(OBJ_DIR)/libtcllinkvar85.a

.PHONY:	test_spot_tracker
test_spot_tracker:	$(OBJ_DIR)/test_spot_tracker

.PHONY:	video_spot_tracker
video_spot_tracker:	$(OBJ_DIR)/video_spot_tracker

$(OBJ_DIR)/libspottracker.a : $(MAKEFILE) $(SPOT_TRACKER_LIB_OBJECTS)
	$(AR) $(OBJ_DIR)/libspottracker.a $(SPOT_TRACKER_LIB_OBJECTS)
	-$(RANLIB) $(OBJ_DIR)/libspottracker.a

$(OBJ_DIR)/libtcllinkvar85.a : $(MAKEFILE) $(TCL_LINKVAR_LIB_OBJECTS)
	$(AR) $(OBJ_DIR)/libtcllinkvar85.a $(TCL_LINKVAR_LIB_OBJECTS)
	-$(RANLIB) $(OBJ_DIR)/libtcllinkvar85.a

$(OBJ_DIR)/libedt.a : $(MAKEFILE) $(EDT_LIB_OBJECTS)
	$(AR) $(OBJ_DIR)/libedt.a $(EDT_LIB_OBJECTS)
	-$(RANLIB) $(OBJ_DIR)/libedt.a

$(OBJ_DIR)/libstocc.a : $(MAKEFILE) $(STOCC_LIB_OBJECTS)
	-mv *.o stocc_random_number_generator
	$(AR) $(OBJ_DIR)/libstocc.a $(STOCC_LIB_OBJECTS)
	-$(RANLIB) $(OBJ_DIR)/libstocc.a

$(OBJ_DIR)/test_spot_tracker: $(OBJ_DIR)/test_spot_tracker.o $(OBJ_DIR)/libspottracker.a
	$(CC) $(LFLAGS) -o $(OBJ_DIR)/test_spot_tracker \
		$(OBJ_DIR)/test_spot_tracker.o \
		-lspottracker $(MAGICLIBS) $(GL) $(VRPNLIBS) $(OPENCVLIBS) $(SYSLIBS) -lm

$(OBJ_DIR)/video_spot_tracker: $(OBJ_DIR)/video_spot_tracker.o $(OBJ_DIR)/libspottracker.a\
				$(OBJ_DIR)/libtcllinkvar85.a libedt.a
	$(CC) $(LFLAGS) -o $(OBJ_DIR)/video_spot_tracker \
		$(OBJ_DIR)/video_spot_tracker.o \
		-lnmMP -lspottracker -ledt -ltcllinkvar85 $(TCLLIBS) \
		$(MAGICLIBS) -lglut $(GL) $(VRPNLIBS) $(OPENCVLIBS) $(SYSLIBS) -lm

$(OBJ_DIR)/add_noise_to_image: $(OBJ_DIR)/add_noise_to_image.o $(OBJ_DIR)/libstocc.a
	$(CC) $(LFLAGS) -o $(OBJ_DIR)/add_noise_to_image \
		$(OBJ_DIR)/add_noise_to_image.o \
		-lstocc $(MAGICLIBS) $(GL) $(VRPNLIBS) $(OPENCVLIBS) $(SYSLIBS) -lm

$(OBJ_DIR)/average_videos: $(OBJ_DIR)/average_videos.o
	$(CC) $(LFLAGS) -o $(OBJ_DIR)/average_videos \
		$(OBJ_DIR)/average_videos.o \
		$(MAGICLIBS) $(GL) $(VRPNLIBS) $(OPENCVLIBS) $(SYSLIBS) -lm

install: all
	-mkdir $(BIN_DIR)
	( cd $(BIN_DIR) ; rm -f $(INSTALL_APPS) )
	( cd $(OBJ_DIR) ; cp $(INSTALL_APPS) $(BIN_DIR) )
	( cd $(BIN_DIR) ; strip $(INSTALL_APPS) )

uninstall:
	( cd $(BIN_DIR) ; rm -f $(INSTALL_APPS) )

clean:
	rm -f $(OBJ_DIR)/*

