// XXX Why is there a failed endpoint message on quit?
// XXX Why is there a red line along the right border of the averaged image?
//     This only seems to happen in the Cilia video, not when doing AVIs...

#define	VST_USE_ROPER
#define	VST_USE_COOKE
#define	VST_USE_EDT
#define	VST_USE_DIAGINC
#define	VST_USE_SEM
#define	VST_USE_DIRECTX
#define VST_USE_VRPN_IMAGER

//#pragma comment(lib,"C:\\Program Files\\Roper Scientific\\PVCAM\\pvcam32.lib")

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
#include "controllable_video.h"

//--------------------------------------------------------------------------
// Version string for this program
const char *Version_string = "02.01";

typedef enum { MEAN_IMAGE, MAX_IMAGE, MIN_IMAGE } g_ACCUMULATION_TYPE;
static g_ACCUMULATION_TYPE  g_accum_type = MEAN_IMAGE;

//--------------------------------------------------------------------------
// Global constants

//--------------------------------------------------------------------------
static void  dirtyexit(void);
static void  cleanup(void);

char  *g_device_name = NULL;			  //< Name of the device to open
base_camera_server  *g_camera = NULL;		  //< Camera used to get an image
image_metric	    *g_composite_image = NULL;	  //< Accumulates composite image
Controllable_Video  *g_video = NULL;		  //< Video controls, if we have them
unsigned            g_bitdepth = 8;               //< Bit depth of the input image
float               g_exposure = 0;               //< Exposure for live camera

bool g_video_valid = false; // Do we have a valid video frame in memory?
unsigned            g_num_frames = 0;             //< How many frames have we read?
int                 g_max_frames = 0;             //< How many frames to average over at most?

// This is called in dirtyexit() whenever the program quits, so does
// not have to be called otherwise.
static	bool  store_summed_image(void)
{
  if (!g_composite_image) {
    fprintf(stderr, "store_summed_image(): No composite image available\n");
    return false;
  }

  char  *filename = new char[strlen(g_device_name) + 15];
  if (filename == NULL) {
    fprintf(stderr, "Out of memory!\n");
    return false;
  }

  // If the device name included "file://" then we should only use
  // the portion after that because this was a VRPN object inside
  // a file.  Same thing for "file:"
  const char *temp = strstr(g_device_name, "file://");
  if (temp) {
    sprintf(filename, "%s.avg.tif", temp + strlen("file://"));
  } else {
    temp = strstr(g_device_name, "file:");
    if (temp) {
      sprintf(filename, "%s.composite.tif", temp + strlen("file:"));
    } else {
      sprintf(filename, "%s.composite.tif", g_device_name);
    }
  }
  printf("Saving composite image to %s\n", filename);

  // Figure out whether the image will be sixteen bits, and also
  // the gain to apply (256 if 8-bit, 1 if 16-bit).  If it is 8-bit
  // output, also apply a shift related to the global bit-shift,
  // so that what ends up in the file is the same as what ends up
  // on the screen.
  bool do_sixteen = (g_bitdepth != 8);
  int bitshift_gain = 1;
  if (!do_sixteen) {
    bitshift_gain = 256;
    bitshift_gain /= pow(2.0,static_cast<double>(g_bitdepth - 8));
  }

  if (!g_composite_image->write_to_tiff_file(filename, bitshift_gain, 0, do_sixteen)) {
    delete [] filename;
    return false;
  }

  delete [] filename;
  return true;
}

// This is called when someone kills the task by ^C or some
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
  if (g_composite_image) {
    if (!store_summed_image()) {
      fprintf(stderr,"dirtyexit(): Could not save composite image\n");
    }
  }

  // Done with the camera and other objects.
  printf("Exiting\n");
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

// Returns true if there are more frames to do, false if not.
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
      return false;
    } else {
      return false;
    }
  }

  if (!g_composite_image) {
    switch (g_accum_type) {
      case MEAN_IMAGE:
        g_composite_image = new mean_image(*g_camera);
        break;
      case MIN_IMAGE:
        g_composite_image = new minimum_image(*g_camera);
        break;
      case MAX_IMAGE:
        g_composite_image = new maximum_image(*g_camera);
        break;
      default:
        fprintf(stderr,"Unknown composition type (%d)\n", g_accum_type);
    }
  }
  (*g_composite_image) += *g_camera;

  // If we've done enough frames, we're done.
  g_num_frames++;
  if ( (g_max_frames) && (g_num_frames >= static_cast<unsigned>(g_max_frames)) ) {
    return false;
  }

  return true;
}

void Usage(const char *s)
{
    fprintf(stderr, "Usage: %s [-f num] [-min] [-max] [-mean] [roper|diaginc|directx|directx640x480|VRPN imager name|filename]\n", s);
    fprintf(stderr, "       -f: Number of frames to average over (0 means all, and is the default)\n");
    fprintf(stderr, "     -min: Accumulate minimum image (default mean)\n");
    fprintf(stderr, "     -max: Accumulate maximum image (default mean)\n");
    fprintf(stderr, "    -mean: Accumulate mean image (default)\n");
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
    } else if (!strncmp(argv[i], "-min", strlen("-min"))) {
       g_accum_type = MIN_IMAGE;
    } else if (!strncmp(argv[i], "-max", strlen("-max"))) {
       g_accum_type = MAX_IMAGE;
    } else if (!strncmp(argv[i], "-mean", strlen("-mean"))) {
       g_accum_type = MEAN_IMAGE;
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
        if (g_composite_image) { delete g_composite_image; g_composite_image = NULL; }
        g_num_frames = 0;

        //------------------------------------------------------------------
        // Open the camera and image wrapper.  If we have a video file, then
        // press play.
        if (!get_camera(g_device_name, &g_bitdepth, &g_exposure, &g_camera, &g_video,
                        648,484,1,0,0)) {
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

        // Read and process all of the frames
        while (do_next_frame()) {}

        // Store the composite image and then delete it.
        if (!store_summed_image()) {
          fprintf(stderr,"dirtyexit(): Could not save composite image\n");
        }
        if (g_composite_image) { delete g_composite_image; g_composite_image = NULL; }

        break;
      }
    }
  }
  if (realparams == 0) {
    Usage(argv[0]);
  }

  if (g_camera) { delete g_camera; g_camera = NULL; }
  return 0;
}
