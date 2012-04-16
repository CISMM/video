#ifndef	CONTROLLABLE_VIDEO_H
#define	CONTROLLABLE_VIDEO_H

#include <stdio.h>
#include <math.h>

#include "base_camera_server.h"
#include "raw_file_server.h"

#ifdef	VST_USE_ROPER
#include "roper_server.h"
#endif
#ifdef	VST_USE_COOKE
#include "cooke_server.h"
#endif
#ifdef	VST_USE_DIRECTX
#include "directx_camera_server.h"
#include "directx_videofile_server.h"
#endif
#ifdef  VST_USE_DIAGINC
#include "diaginc_server.h"
#endif
#ifdef  VST_USE_EDT
#include "edt_server.h"
#endif
#ifdef  VST_USE_POINTGREY
#include "point_grey_server.h"
#endif
#ifdef  VST_USE_SEM
#include "SEM_camera_server.h"
#endif
#ifdef  VST_USE_VRPN_IMAGER
#include "VRPN_Imager_camera_server.h"
#endif
#ifdef	VST_USE_IMAGEMAGICK
#include "file_stack_server.h"
#endif
#ifdef	VST_USE_QT4
#include "qt4_video_server.h"
#endif
#ifdef	VST_USE_FFMPEG
#include "ffmpeg_video_server.h"
#endif

class Controllable_Video {
public:
  /// Start the stored video playing.
  virtual void play(void) = 0;

  /// Pause the stored video
  virtual void pause(void) = 0;

  /// Rewind the stored video to the beginning (also pauses).
  virtual void rewind(void) = 0;

  /// Single-step the stored video for one frame.
  virtual void single_step() = 0;
};

#ifdef	VST_USE_DIRECTX
class Directx_Controllable_Video : public Controllable_Video , public directx_videofile_server {
public:
  Directx_Controllable_Video(const char *filename) : directx_videofile_server(filename) {};
  virtual ~Directx_Controllable_Video() {};
  void play(void) { directx_videofile_server::play(); }
  void pause(void) { directx_videofile_server::pause(); }
  void rewind(void) { pause(); directx_videofile_server::rewind(); }
  void single_step(void) { directx_videofile_server::single_step(); }
};
#endif

#ifdef	VST_USE_ROPER
class SPE_Controllable_Video : public Controllable_Video, public spe_file_server {
public:
  SPE_Controllable_Video(const char *filename) : spe_file_server(filename) {};
  virtual ~SPE_Controllable_Video() {};
  void play(void) { spe_file_server::play(); }
  void pause(void) { spe_file_server::pause(); }
  void rewind(void) { pause(); spe_file_server::rewind(); }
  void single_step(void) { spe_file_server::single_step(); }
};
#endif

#ifdef	VST_USE_SEM
class SEM_Controllable_Video : public Controllable_Video, public SEM_camera_server {
public:
  SEM_Controllable_Video(const char *filename) : SEM_camera_server(filename) {};
  virtual ~SEM_Controllable_Video() {};
  void play(void) { SEM_camera_server::play(); }
  void pause(void) { SEM_camera_server::pause(); }
  void rewind(void) { pause(); SEM_camera_server::rewind(); }
  void single_step(void) { SEM_camera_server::single_step(); }
};
#endif

#ifdef  VST_USE_VRPN_IMAGER
class VRPN_Imager_Controllable_Video : public Controllable_Video, public VRPN_Imager_camera_server {
public:
  VRPN_Imager_Controllable_Video(const char *filename) : VRPN_Imager_camera_server(filename) {};
  virtual ~VRPN_Imager_Controllable_Video() {};
  void play(void) { VRPN_Imager_camera_server::play(); }
  void pause(void) { VRPN_Imager_camera_server::pause(); }
  void rewind(void) { pause(); VRPN_Imager_camera_server::rewind(); }
  void single_step(void) { VRPN_Imager_camera_server::single_step(); }
};
#endif

class Raw_Controllable_Video : public Controllable_Video, public raw_file_server {
public:
  Raw_Controllable_Video(const char *filename, unsigned numX, unsigned numY,
    unsigned bitdepth, unsigned channels, unsigned headersize, unsigned frameheadersize) :
      raw_file_server(filename, numX, numY, bitdepth, channels, headersize, frameheadersize) {};
  virtual ~Raw_Controllable_Video() {};
  void play(void) { raw_file_server::play(); }
  void pause(void) { raw_file_server::pause(); }
  void rewind(void) { pause(); raw_file_server::rewind(); }
  void single_step(void) { raw_file_server::single_step(); }
};

#ifdef VST_USE_IMAGEMAGICK
class FileStack_Controllable_Video : public Controllable_Video, public file_stack_server {
public:
  FileStack_Controllable_Video(const char *filename) : file_stack_server(filename) {};
  virtual ~FileStack_Controllable_Video() {};
  void play(void) { file_stack_server::play(); }
  void pause(void) { file_stack_server::pause(); }
  void rewind(void) { pause(); file_stack_server::rewind(); }
  void single_step(void) { file_stack_server::single_step(); }
};
#endif

#ifdef	VST_USE_QT4
class QT_Controllable_Video : public Controllable_Video , public qt4_video_server {
public:
  QT_Controllable_Video(const char *filename) : qt4_video_server(filename) {};
  virtual ~QT_Controllable_Video() {};
  void play(void) { qt4_video_server::play(); }
  void pause(void) { qt4_video_server::pause(); }
  void rewind(void) { pause(); qt4_video_server::rewind(); }
  void single_step(void) { qt4_video_server::single_step(); }
};
#endif

#ifdef	VST_USE_FFMPEG
class FFMPEG_Controllable_Video : public Controllable_Video , public ffmpeg_video_server {
public:
  FFMPEG_Controllable_Video(const char *filename) : ffmpeg_video_server(filename) {};
  virtual ~FFMPEG_Controllable_Video() {};
  void play(void) { ffmpeg_video_server::play(); }
  void pause(void) { ffmpeg_video_server::pause(); }
  void rewind(void) { pause(); ffmpeg_video_server::rewind(); }
  void single_step(void) { ffmpeg_video_server::single_step(); }
};
#endif

#ifdef	USE_METAMORPH
class MetamorphStack_Controllable_Video : public Controllable_Video, public Metamorph_stack_server {
public:
  MetamorphStack_Controllable_Video(const char *filename) : Metamorph_stack_server(filename) {};
  virtual ~MetamorphStack_Controllable_Video() {};
  void play(void) { Metamorph_stack_server::play(); }
  void pause(void) { Metamorph_stack_server::pause(); }
  void rewind(void) { pause(); Metamorph_stack_server::rewind(); }
  void single_step(void) { Metamorph_stack_server::single_step(); }
};
#endif

/// Open the wrapped camera we want to use depending on the name of the
//  camera we're trying to open.  If it is a raw camera, then the params
//  needed to open it are specified (one in bit_depth, the others in
//  special params at the end).
bool  get_camera(const char *name,
                 unsigned *bit_depth,
                 float *exposure,
                 base_camera_server **camera, Controllable_Video **video,
                 unsigned raw_camera_numx, unsigned raw_camera_numy,
                 unsigned raw_camera_channels, unsigned raw_camera_headersize,
                 unsigned raw_camera_frameheadersize)
{
#ifdef VST_USE_ROPER
  if (!strcmp(name, "roper")) {
    roper_server *r = new roper_server(1);
    *camera = r;
    *bit_depth = 16;
  } else
#endif  
#ifdef VST_USE_COOKE
  if (!strcmp(name, "cooke")) {
    // XXX Starts with binning of 2 to get the image size down so that
    // it fits on the screen.
    cooke_server *r = new cooke_server(2);
    *camera = r;
    *bit_depth = 16;
  } else
#endif  
#ifdef	VST_USE_DIAGINC
  if (!strcmp(name, "diaginc")) {
    // XXX Starts with binning of 2 to get the image size down so that
    // it fits on the screen.
    diaginc_server *r = new diaginc_server(2);
    *camera = r;
    *exposure = 80;	// Seems to be the minimum exposure for the one we have
    *bit_depth = 12;
  } else
#endif  
#ifdef	VST_USE_EDT
  if (!strcmp(name, "edt")) {
    edt_server *r = new edt_server();
    *camera = r;
  } else
#endif  
#ifdef	VST_USE_POINTGREY
  if (!strcmp(name, "point_grey")) {
    point_grey_server *r = new point_grey_server();
    *camera = r;
  } else
#endif  
#ifdef	VST_USE_DIRECTX
  if (!strcmp(name, "directx")) {
    // Passing width and height as zero leaves it open to whatever the camera has
    directx_camera_server *d = new directx_camera_server(1,0,0);	// Use camera #1 (first one found)
    *camera = d;

    //-------------------------------------------------------------------
    // Read frames until we get a good one or until we get ten bad ones
    // in a row (ten timeouts).  We do this here because the read_one_frame()
    // function is timing out several times after the device is initially
    // opened on one particular laptop (Sager, Windows 7 64-bit).
    bool got_good_frame = false;
    unsigned i;
    for (i = 0; i < 10; i++) {
      if (d->read_image_to_memory()) {
        got_good_frame = true;
        break;
      }
    }
    if (!got_good_frame) {
      fprintf(stderr,"get_camera(): Could not read frame from directx camera\n");
    }

  } else if (!strcmp(name, "directx640x480")) {
    directx_camera_server *d = new directx_camera_server(1,640,480);	// Use camera #1 (first one found)
    *camera = d;

    //-------------------------------------------------------------------
    // Read frames until we get a good one or until we get ten bad ones
    // in a row (ten timeouts).  We do this here because the read_one_frame()
    // function is timing out several times after the device is initially
    // opened on one particular laptop (Sager, Windows 7 64-bit).
    bool got_good_frame = false;
    unsigned i;
    for (i = 0; i < 10; i++) {
      if (d->read_image_to_memory()) {
        got_good_frame = true;
        break;
      }
    }
    if (!got_good_frame) {
      fprintf(stderr,"get_camera(): Could not read frame from directx camera\n");
    }

  // If this is a VRPN URL for an SEM device, then open the file and set up
  // to read from that device.
  } else
#endif  
#ifdef	VST_USE_SEM
  if (!strncmp(name, "SEM@", 4)) {
    SEM_Controllable_Video  *s = new SEM_Controllable_Video (name);
    *camera = s;
    *video = s;
    *bit_depth = 16;
  } else
#endif  
  // If this is a VRPN URL for a VRPN Imager device, then open the file and set up
  // to read from that device.
#ifdef  VST_USE_VRPN_IMAGER
  if (strchr(name, '@')) {
    VRPN_Imager_Controllable_Video  *s = new VRPN_Imager_Controllable_Video (name);
    *camera = s;
    *video = s;
    // We're going to put out 16-bit video no matter what the input is.
    *bit_depth = 16;

  // Unknown type, so we presume that it is a file.  Now we figure out what
  // kind of file based on the extension and open the appropriate type of
  // imager.
  } else
#endif
  {
    fprintf(stderr,"get_camera(): Assuming filename (%s)\n", name);

    // If the extension is ".raw" then we assume it is a raw video file and open
    // it that way.
    if (  (strcmp(".raw", &name[strlen(name)-4]) == 0) ||
	  (strcmp(".RAW", &name[strlen(name)-4]) == 0) ) {
      Raw_Controllable_Video *f = new Raw_Controllable_Video(name,
        raw_camera_numx, raw_camera_numy, *bit_depth, raw_camera_channels,
        raw_camera_headersize, raw_camera_frameheadersize);
      *camera = f;
      *video = f;

    // If the extension is ".spe" then we assume it is a Roper file and open
    // it that way.
#ifdef	VST_USE_ROPER
    } else if ( (strcmp(".spe", &name[strlen(name)-4]) == 0) ||
		(strcmp(".SPE", &name[strlen(name)-4]) == 0) ) {
      SPE_Controllable_Video *f = new SPE_Controllable_Video(name);
      *camera = f;
      *video = f;
      *bit_depth = 12;

    // If the extension is ".sem" then we assume it is a VRPN-format file
    // with an SEM device in it, so we form the name of the device and open
    // a VRPN Remote object to handle it.
#endif
#ifdef	VST_USE_SEM
    } else if (strcmp(".sem", &name[strlen(name)-4]) == 0) {
      char *semname;
      if ( NULL == (semname = new char[strlen(name) + 20]) ) {
	fprintf(stderr,"Out of memory when allocating file name\n");
        return false;
      }
      sprintf(semname, "SEM@file://%s", name);
      SEM_Controllable_Video *s = new SEM_Controllable_Video(semname);
      *camera = s;
      *video = s;
      delete [] semname;
#endif
#ifdef  VST_USE_VRPN_IMAGER
    // If the extension is ".vrpn" then we assume it is a VRPN-format file
    // with an imager device in it, so we form the name of the device and open
    // a VRPN Remote object to handle it.  We have to assume some name for the
    // object, so we call it "TestImage" and hope for the best (this is what the
    // video_imager_server emits, so is a good bet).  If you know that
    // you are connecting to a different name in a file, you can use the full
    // DEVICENAME@file:// VRPN name and it will have been parsed above.
    } else if (strcmp(".vrpn", &name[strlen(name)-5]) == 0) {
      char *vrpnname;
      if ( NULL == (vrpnname = new char[strlen(name) + 20]) ) {
	fprintf(stderr,"Out of memory when allocating file name\n");
        return false;
      }
      sprintf(vrpnname, "TestImage@file://%s", name);
      VRPN_Imager_Controllable_Video *s = new VRPN_Imager_Controllable_Video(vrpnname);
      *camera = s;
      *video = s;
      *bit_depth = 16;
      delete [] vrpnname;

#endif

    // If the extension is ".tif" or ".tiff" or ".bmp" or ".png" then we assume it is
    // a file or stack of files to be opened by ImageMagick.
#ifdef VST_USE_IMAGEMAGICK
    } else if (   (strcmp(".tif", &name[strlen(name)-4]) == 0) ||
		  (strcmp(".TIF", &name[strlen(name)-4]) == 0) ||
		  (strcmp(".bmp", &name[strlen(name)-4]) == 0) ||
		  (strcmp(".BMP", &name[strlen(name)-4]) == 0) ||
		  (strcmp(".jpg", &name[strlen(name)-4]) == 0) ||
		  (strcmp(".JPG", &name[strlen(name)-4]) == 0) ||
		  (strcmp(".ppm", &name[strlen(name)-4]) == 0) ||
		  (strcmp(".PPM", &name[strlen(name)-4]) == 0) ||
		  (strcmp(".pgm", &name[strlen(name)-4]) == 0) ||
		  (strcmp(".PGM", &name[strlen(name)-4]) == 0) ||
		  (strcmp(".png", &name[strlen(name)-4]) == 0) ||
		  (strcmp(".PNG", &name[strlen(name)-4]) == 0) ||
		  (strcmp(".tiff", &name[strlen(name)-5]) == 0) || 
		  (strcmp(".TIFF", &name[strlen(name)-5]) == 0) ) {
      FileStack_Controllable_Video *s = new FileStack_Controllable_Video(name);
      *camera = s;
      *video = s;
      // We're going to put out 16-bit video no matter what the input is.
      *bit_depth = 16;
#endif

    // If the extension is ".stk"  then we assume it is a Metamorph file
    // to be opened by the Metamorph reader.
#ifdef	USE_METAMORPH
    } else if (strcmp(".stk", &name[strlen(name)-4]) == 0) {
      MetamorphStack_Controllable_Video *s = new MetamorphStack_Controllable_Video(name);
      *camera = s;
      *video = s;

    // File of unknown type.  We assume that it is something that DirectX knows
    // how to open.
#endif
    } else {
#ifdef	VST_USE_DIRECTX
      Directx_Controllable_Video *f = new Directx_Controllable_Video(name);
      *camera = f;
      *video = f;
#elif defined VST_USE_QT4
      QT_Controllable_Video *f = new QT_Controllable_Video(name);
      *camera = f;
      *video = f;
#else
	fprintf(stderr,"Unknown camera or file type\n");
	return false;
#endif
    }
  }
  return true;
}

#endif
