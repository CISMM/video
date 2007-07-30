// XXX Why is there a red line along the right border of the averaged image?
//     This only seems to happen in the Cilia video, not when doing AVIs...

#pragma comment(lib,"C:\\Program Files\\Roper Scientific\\PVCAM\\pvcam32.lib")

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "roper_server.h"
#include "directx_camera_server.h"
#include "directx_videofile_server.h"
#include "diaginc_server.h"
#include "edt_server.h"
#include "SEM_camera_server.h"
#include "file_stack_server.h"
#include "image_wrapper.h"
#ifdef	_WIN32
#include <windows.h>
#endif
#include <quat.h>
#include <vrpn_Types.h>
// This pragma tells the compiler not to tell us about truncated debugging info
// due to name expansion within the string, list, and vector classes.
#pragma warning( disable : 4786 4995 )
#include <list>
#include <vector>
using namespace std;

//#define	DEBUG

//--------------------------------------------------------------------------
// Version string for this program
const char *Version_string = "01.00";

//--------------------------------------------------------------------------
// Global constants

//--------------------------------------------------------------------------
// Some classes needed for use in the rest of the program.

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

class Directx_Controllable_Video : public Controllable_Video , public directx_videofile_server {
public:
  Directx_Controllable_Video(const char *filename) : directx_videofile_server(filename) {};
  virtual ~Directx_Controllable_Video() {};
  void play(void) { directx_videofile_server::play(); }
  void pause(void) { directx_videofile_server::pause(); }
  void rewind(void) { pause(); directx_videofile_server::rewind(); }
  void single_step(void) { directx_videofile_server::single_step(); }
};

class Pulnix_Controllable_Video : public Controllable_Video, public edt_pulnix_raw_file_server {
public:
  Pulnix_Controllable_Video(const char *filename) : edt_pulnix_raw_file_server(filename) {};
  virtual ~Pulnix_Controllable_Video() {};
  void play(void) { edt_pulnix_raw_file_server::play(); }
  void pause(void) { edt_pulnix_raw_file_server::pause(); }
  void rewind(void) { pause(); edt_pulnix_raw_file_server::rewind(); }
  void single_step(void) { edt_pulnix_raw_file_server::single_step(); }
};

class FileStack_Controllable_Video : public Controllable_Video, public file_stack_server {
public:
  FileStack_Controllable_Video(const char *filename) : file_stack_server(filename, "C:/nsrg/external/pc_win32/bin/ImageMagick-5.5.7-Q16/MAGIC_DIR_PATH") {};
  virtual ~FileStack_Controllable_Video() {};
  void play(void) { file_stack_server::play(); }
  void pause(void) { file_stack_server::pause(); }
  void rewind(void) { pause(); file_stack_server::rewind(); }
  void single_step(void) { file_stack_server::single_step(); }
};

class SEM_Controllable_Video : public Controllable_Video, public SEM_camera_server {
public:
  SEM_Controllable_Video(const char *filename) : SEM_camera_server(filename) {};
  virtual ~SEM_Controllable_Video() {};
  void play(void) { SEM_camera_server::play(); }
  void pause(void) { SEM_camera_server::pause(); }
  void rewind(void) { pause(); SEM_camera_server::rewind(); }
  void single_step(void) { SEM_camera_server::single_step(); }
};

//--------------------------------------------------------------------------
static void  dirtyexit(void);
static void  cleanup(void);

char  *g_device_name = NULL;			  //< Name of the device to open
base_camera_server  *g_camera = NULL;		  //< Camera used to get an image
image_metric	    *g_mean_image = NULL;	  //< Accumulates mean of images
Controllable_Video  *g_video = NULL;		  //< Video controls, if we have them
int                 g_bitdepth = 8;               //< Bit depth of the input image

int                 g_exposure = 0;               //< Exposure for live camera

bool g_video_valid = false; // Do we have a valid video frame in memory?
unsigned            g_num_frames = 0;             //< How many frames have we read?
int                 g_max_frames = 0;             //< How many frames to average over at most?

/// Open the wrapped camera we want to use depending on the name of the
//  camera we're trying to open.
bool  get_camera(const char *type, base_camera_server **camera,
			    Controllable_Video **video)
{
  if (!strcmp(type, "roper")) {
    // XXX Starts with binning of 2 to get the image size down so that
    // it fits on the screen.
    roper_server *r = new roper_server(2);
    *camera = r;
  } else
  if (!strcmp(type, "diaginc")) {
    // XXX Starts with binning of 2 to get the image size down so that
    // it fits on the screen.
    diaginc_server *r = new diaginc_server(2);
    *camera = r;
    g_exposure = 80;	// Seems to be the minimum exposure for the one we have
  } else if (!strcmp(type, "directx")) {
    // Passing width and height as zero leaves it open to whatever the camera has
    directx_camera_server *d = new directx_camera_server(1,0,0);	// Use camera #1 (first one found)
    *camera = d;
  } else if (!strcmp(type, "directx640x480")) {
    directx_camera_server *d = new directx_camera_server(1,640,480);	// Use camera #1 (first one found)
    *camera = d;

  // If this is a VRPN URL for an SEM device, then open the file and set up
  // to read from that device.
  } else if (!strncmp(type, "SEM@", 4)) {
    SEM_Controllable_Video  *s = new SEM_Controllable_Video (type);
    *camera = s;
    *video = s;

  // Unknown type, so we presume that it is a file.  Now we figure out what
  // kind of file based on the extension and open the appropriate type of
  // imager.
  } else {
    fprintf(stderr,"get_camera(): Assuming filename (%s)\n", type);

    // If the extension is ".raw" then we assume it is a Pulnix file and open
    // it that way.
    if (strcmp(".raw", &type[strlen(type)-4]) == 0) {
      Pulnix_Controllable_Video *f = new Pulnix_Controllable_Video(type);
      *camera = f;
      *video = f;

    // If the extension is ".sem" then we assume it is a VRPN-format file
    // with an SEM device in it, so we form the name of the device and open
    // a VRPN Remote object to handle it.
    } else if (strcmp(".sem", &type[strlen(type)-4]) == 0) {
      char *name;
      if ( NULL == (name = new char[strlen(type) + 20]) ) {
	fprintf(stderr,"Out of memory when allocating file name\n");
	cleanup();
        exit(-1);
      }
      sprintf(name, "SEM@file:%s", type);
      SEM_Controllable_Video *s = new SEM_Controllable_Video(name);
      *camera = s;
      *video = s;
      delete [] name;

    // If the extension is ".tif" or ".tiff" or ".bmp" then we assume it is
    // a file or stack of files to be opened by ImageMagick.
    } else if (   (strcmp(".tif", &type[strlen(type)-4]) == 0) ||
		  (strcmp(".TIF", &type[strlen(type)-4]) == 0) ||
		  (strcmp(".bmp", &type[strlen(type)-4]) == 0) ||
		  (strcmp(".BMP", &type[strlen(type)-4]) == 0) ||
		  (strcmp(".jpg", &type[strlen(type)-4]) == 0) ||
		  (strcmp(".JPG", &type[strlen(type)-4]) == 0) ||
		  (strcmp(".tiff", &type[strlen(type)-5]) == 0) || 
		  (strcmp(".TIFF", &type[strlen(type)-5]) == 0) ) {
      FileStack_Controllable_Video *s = new FileStack_Controllable_Video(type);
      *camera = s;
      *video = s;

    } else {
      Directx_Controllable_Video *f = new Directx_Controllable_Video(type);
      *camera = f;
      *video = f;
    }
  }
  return true;
}

static	bool  store_summed_image(void)
{
  if (!g_mean_image) {
    fprintf(stderr, "store_summed_image(): No mean image available\n");
    return false;
  }

  char  *filename = new char[strlen(g_device_name) + 15];
  if (filename == NULL) {
    fprintf(stderr, "Out of memory!\n");
    return false;
  }
  sprintf(filename, "%s.avg.tif", g_device_name);

  // Figure out whether the image will be sixteen bits, and also
  // the gain to apply (256 if 8-bit, 1 if 16-bit).  If it is 8-bit
  // output, also apply a shift related to the global bit-shift,
  // so that what ends up in the file is the same as what ends up
  // on the screen.
  bool do_sixteen = (g_bitdepth != 8);
  int bitshift_gain = 1;
  if (!do_sixteen) {
    bitshift_gain = 256;
    bitshift_gain /= pow(2.0,g_bitdepth - 8);
  }

  if (!g_mean_image->write_to_tiff_file(filename, bitshift_gain, 0, do_sixteen)) {
    delete [] filename;
    return false;
  }

  delete [] filename;
  return true;
}

// This is called when someone kills the task by closing Glut or some
// other means we don't have control over.
static void  dirtyexit(void)
{
  static bool did_already = false;

  if (did_already) {
    return;
  } else {
    did_already = true;
  }

  // Save the output file
  printf("Saving mean image...\n");
  if (!store_summed_image()) {
    fprintf(stderr,"dirtyexit(): Could not save mean image\n");
  }

  // Done with the camera and other objects.
  printf("Exiting\n");

  if (g_camera) { delete g_camera; g_camera = NULL; }
  if (g_mean_image) { delete g_mean_image; g_mean_image = NULL; }
}

static void  cleanup(void)
{
  static bool cleaned_up_already = false;

  if (cleaned_up_already) {
    return;
  } else {
    cleaned_up_already = true;
  }

  // Done with the camera and other objects.
  printf("Cleanly ");

  dirtyexit();
}

// Returns true if there are more frames to do, false (and saves
// the mean image) if not.
bool do_next_frame(void)
{
  // Read an image from the camera into memory, within a structure that
  // is wrapped by an image_wrapper object that the tracker can use.
  // add it into the sum image.

  // If the read fails, we're probably at the end of the video so go
  // ahead and store it, then return that we're done with this video.
  if (!g_camera->read_image_to_memory(1,0, 1,0, g_exposure)) {
    if (!g_video) {
      fprintf(stderr, "Can't read image to memory!\n");
      store_summed_image();
      return false;
    } else {
      store_summed_image();
      return false;
    }
  }

  if (!g_mean_image) {
    g_mean_image = new mean_image(*g_camera);
  }
  (*g_mean_image) += *g_camera;

  // If we've done enough frames, we're done.
  g_num_frames++;
  if ( (g_max_frames) && (g_num_frames >= static_cast<unsigned>(g_max_frames)) ) {
    store_summed_image();
    return false;
  }

  return true;
}

void Usage(const char *s)
{
    fprintf(stderr, "Usage: %s [-f num] [roper|diaginc|directx|directx640x480|filename]\n", s);
    fprintf(stderr, "       -f: Number of frames to average over (0 means all, and is the default)\n");
    exit(-1);
}

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
  //---------------------------------------------------------------
  // Set up exit handler to make sure we clean things up no matter
  // how we are quit.  We hope that we exit in a good way and so
  // cleanup() gets called, but if not then we do a dirty exit.
  atexit(dirtyexit);

  //------------------------------------------------------------------
  // VRPN state setting so that we don't try to preload a video file
  // when it is opened, which wastes time.  Also tell it not to
  // accumulate messages, which can cause us to run out of memory.
  vrpn_FILE_CONNECTIONS_SHOULD_PRELOAD = false;
  vrpn_FILE_CONNECTIONS_SHOULD_ACCUMULATE = false;

  //---------------------------------------------------------------
  // Parse the command line, handling video files as we go.
  int	i, realparams;		  // How many non-flag command-line arguments
  realparams = 0;
  for (i = 1; i < argc; i++) {
    if (!strncmp(argv[i], "-f", strlen("-f"))) {
      if (++i > argc) { Usage(argv[0]); }
      g_max_frames = atoi(argv[i]);
      if (g_max_frames < 0) {
	fprintf(stderr,"Invalid maximum frames (0+ allowed, %d entered)\n", g_max_frames);
	exit(-1);
      }
      if (g_max_frames) {
        printf("Using at most %d frames per file\n", g_max_frames);
      } else {
        printf("Using all frames in each file\n");
      }
    } else if (argv[i][0] == '-') {
      Usage(argv[0]);
    } else {
      switch (++realparams) {
      case 1: // In case we later care about how many
      default:
        //-----------------------------------------------------------------
        // Get the name of the next file to handle
        g_device_name = argv[i];

        //------------------------------------------------------------------
        // Clean up our state for the beginning of a new file.
        if (g_camera) { delete g_camera; g_camera = NULL; }
        if (g_mean_image) { delete g_mean_image; g_mean_image = NULL; }
        g_num_frames = 0;

        //------------------------------------------------------------------
        // Open the camera and image wrapper.  If we have a video file, then
        // press play.
        if (!get_camera(g_device_name, &g_camera, &g_video)) {
          fprintf(stderr,"Cannot open camera/imager\n");
          if (g_camera) { delete g_camera; g_camera = NULL; }
          exit(-1);
        }
        // Verify that the camera is working.
        if (!g_camera->working()) {
          fprintf(stderr,"Could not establish connection to camera\n");
          if (g_camera) { delete g_camera; g_camera = NULL; }
          exit(-1);
        }

        if (g_video) {
          g_video->play();
        }

        while (do_next_frame()) {}

        break;
      }
    }
  }
  
  return 0;
}
