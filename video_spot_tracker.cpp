//---------------------------------------------------------------------------
// This section contains configuration settings for the Video Spot Tracker.
// It is used to make it possible to compile and link the code when one or
// more of the camera- or file- driver libraries are unavailable. First comes
// a list of definitions.  These should all be defined when building the
// program for distribution.  Following that comes a list of paths and other
// things that depend on these definitions.  They may need to be changed
// as well, depending on where the libraries were installed.

#ifdef _WIN32
#define	VST_USE_ROPER
#define	VST_USE_COOKE
#define	VST_USE_EDT
#define	VST_USE_DIAGINC
#define	VST_USE_SEM
#define	VST_USE_DIRECTX
#define VST_USE_VRPN_IMAGER
//#define USE_METAMORPH	    // Metamorph reader not completed.
#endif

// END configuration section.
//---------------------------------------------------------------------------

// This pragma tells the compiler not to tell us about truncated debugging info
// due to name expansion within the string, list, and vector classes.
#pragma warning( disable : 4786 4995 4996 )

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tcl.h>
#include <tk.h>
#ifdef _WIN32
#include "Tcl_Linkvar.h"
#else
#include "Tcl_Linkvar85.h"
#endif
#ifdef	VST_USE_ROPER
#include "roper_server.h"
#endif
#ifdef	VST_USE_COOKE
#include "cooke_server.h"
#endif
#include "directx_camera_server.h"
#include "directx_videofile_server.h"
#include "diaginc_server.h"
#include "edt_server.h"
#include "SEM_camera_server.h"
#include "VRPN_Imager_camera_server.h"
#include "file_stack_server.h"
#include "image_wrapper.h"
#include "spot_tracker.h"
#ifdef	_WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glut.h>
#include "controllable_video.h"

#include <quat.h>
#include <vrpn_Connection.h>
#include <vrpn_FileConnection.h>
#include <vrpn_Analog.h>
#include <vrpn_Tracker.h>
#include <vrpn_Imager.h>

//NMmp source files (nanoManipulator project).
#include <thread.h>

// OpenCV includes
#include <cv.h>
#include <cxcore.h>
#include <highgui.h>

#include <list>
#include <vector>
#include <algorithm>
using namespace std;

//#define	DEBUG

static void cleanup();
static void dirtyexit();

#ifndef	M_PI
#ifndef M_PI_DEFINED
const double M_PI = 2*asin(1.0);
#define M_PI_DEFINED
#endif
#endif

//--------------------------------------------------------------------------
// Version string for this program
const char *Version_string = "06.03";

//--------------------------------------------------------------------------
// Global constants

const int KERNEL_DISK = 0;	  //< These must match the values used in video_spot_tracker.tcl.
const int KERNEL_CONE = 1;
const int KERNEL_SYMMETRIC = 2;
const int KERNEL_FIONA = 3;

// How a tracker should behave when it is lost
const int LOST_STOP = 0;
const int LOST_DELETE = 1;
const int LOST_HOVER = 2;

//--------------------------------------------------------------------------
// Some classes needed for use in the rest of the program.

class Spot_Information {
public:
  Spot_Information(spot_tracker_XY *xytracker, spot_tracker_Z *ztracker, bool unofficial = false) {
    d_tracker_XY = xytracker;
    d_tracker_Z = ztracker;
    if (!unofficial) {
		d_index = d_static_index++;
    } else {
		d_index = -1;
    }
    d_velocity[0] = d_acceleration[0] = d_velocity[1] = d_acceleration[1] = 0;
    d_lost = false;
  }

  ~Spot_Information() {
	  if (d_tracker_XY != NULL)
		  delete d_tracker_XY;
	  if (d_tracker_Z != NULL)
		  delete d_tracker_Z;
  }

  spot_tracker_XY *xytracker(void) const { return d_tracker_XY; }
  spot_tracker_Z *ztracker(void) const { return d_tracker_Z; }
  unsigned index(void) const { return d_index; }

  void set_xytracker(spot_tracker_XY *tracker) { d_tracker_XY = tracker; }
  void set_ztracker(spot_tracker_Z *tracker) { d_tracker_Z = tracker; }

  void get_velocity(double velocity[2]) const { velocity[0] = d_velocity[0]; velocity[1] = d_velocity[1]; }
  void set_velocity(const double velocity[2]) { d_velocity[0] = velocity[0]; d_velocity[1] = velocity[1]; }
  void get_acceleration(double acceleration[2]) const { acceleration[0] = d_acceleration[0]; acceleration[1] = d_acceleration[1]; }
  void set_acceleration(const double acceleration[2]) { d_acceleration[0] = acceleration[0]; d_acceleration[1] = acceleration[1]; }

  bool lost(void) const { return d_lost; };
  void lost(bool l) { d_lost = l; };

  static unsigned get_static_index();

protected:
  spot_tracker_XY	*d_tracker_XY;	    //< The tracker we're keeping information for in XY
  spot_tracker_Z	*d_tracker_Z;	    //< The tracker we're keeping information for in Z
  unsigned		d_index;	    //< The index for this instance
  double		d_velocity[2];	    //< The velocity of the particle
  double		d_acceleration[2];  //< The acceleration of the particle
  bool                  d_lost;             //< Am I lost?
  static unsigned	d_static_index;     //< The index to use for the next one (never to be re-used).
};
unsigned  Spot_Information::d_static_index = 0;	  //< Start the first instance of a Spot_Information index at zero.

unsigned Spot_Information::get_static_index() { return d_static_index; };

// Used to keep track of the past traces of spots that have been tracked.
typedef struct { double x; double y; } Position_XY;
typedef vector<Position_XY> Position_Vector_XY;

//--------------------------------------------------------------------------
// Glut wants to take over the world when it starts, so we need to make
// global access to the objects we will be using.

Tcl_Interp	    *g_tk_control_interp;

Tclvar_int          g_tcl_raw_camera_numx("raw_numx");
Tclvar_int          g_tcl_raw_camera_numy("raw_numy");
Tclvar_int          g_tcl_raw_camera_bitdepth("raw_bitdepth");
Tclvar_int          g_tcl_raw_camera_channels("raw_channels");
Tclvar_int          g_tcl_raw_camera_headersize("raw_headersize");
Tclvar_int          g_tcl_raw_camera_frameheadersize("raw_frameheadersize");

char		    *g_device_name = NULL;	  //< Name of the camera/video/file device to open
base_camera_server  *g_camera;			  //< Camera used to get an image
unsigned            g_camera_bit_depth = 8;       //< Bit depth of the particular camera

image_wrapper       *g_image;                     //< Image, possibly from camera and possibly computed
image_metric	    *g_mean_image = NULL;	  //< Accumulates mean of images, if we're doing background subtract
image_wrapper	    *g_calculated_image = NULL;	  //< Image calculated from the camera image and other parameters
unsigned            g_background_count = 0;       //< Number of frames we've already averaged over
copy_of_image	    *g_last_image = NULL;	  //< Copy of the last image we had, if any

Controllable_Video  *g_video = NULL;		  //< Video controls, if we have them
Tclvar_int_with_button	g_frame_number("frame_number",NULL,-1);  //< Keeps track of video frame number
#ifdef _WIN32
Tclvar_int_with_button	g_window_offset_x("window_offset_x",NULL,0);  //< Offset windows more in some arch
Tclvar_int_with_button	g_window_offset_y("window_offset_y",NULL,0);  //< Offset windows more in some arch
#else
Tclvar_int_with_button	g_window_offset_x("window_offset_x",NULL,25);  //< Offset windows more in some arch
Tclvar_int_with_button	g_window_offset_y("window_offset_y",NULL,-10);  //< Offset windows more in some arch
#endif

// This variable is used to determine if we should consider doing maximum fits,
// by determining if the frame number has changed since last time.
int                 g_last_optimized_frame_number = -1000;

int		    g_tracking_window;		  //< Glut window displaying tracking
unsigned char	    *g_glut_image = NULL;	  //< Pointer to the storage for the image

list <Spot_Information *>g_trackers;		  //< List of active trackers
Spot_Information    *g_active_tracker = NULL;	  //< The tracker that the controls refer to
bool		    g_ready_to_display = false;	  //< Don't unless we get an image
bool		    g_already_posted = false;	  //< Posted redisplay since the last display?
int		    g_mousePressX, g_mousePressY; //< Where the mouse was when the button was pressed
int		    g_whichDragAction;		  //< What action to take for mouse drag
bool                g_tracker_is_lost = false;    //< Is there a lost tracker?

vector<Position_Vector_XY>  g_logged_traces;      //< Stores the trajectories of logged beads

vrpn_Connection	    *g_vrpn_connection = NULL;    //< Connection to send position over
vrpn_Tracker_Server *g_vrpn_tracker = NULL;	  //< Tracker server to send positions
vrpn_Analog_Server  *g_vrpn_analog = NULL;        //< Analog server to report frame number
vrpn_Imager_Server  *g_vrpn_imager = NULL;        //< VRPN Imager Server in case we're forwarding images
int                 g_server_channel = -1;        //< Server channel index to send image data on
Tclvar_int          g_video_full_frame_every("video_full_frame_every",400);  //< How often (in frames) to send a full frame of video
FILE		    *g_csv_file = NULL;		  //< File to save data in with .csv extension

unsigned char	    *g_beadseye_image = NULL;	  //< Pointer to the storage for the beads-eye image
unsigned char	    *g_landscape_image = NULL;	  //< Pointer to the storage for the fitness landscape image
float		    *g_landscape_floats = NULL;	  //< Pointer to the storage for the fitness landscape raw values
int		    g_beadseye_window;		  //< Glut window showing view from active bead
int		    g_beadseye_size = 121;	  //< Size of beads-eye-view window XXX should be dynamic
int		    g_landscape_window;		  //< Glut window showing local landscape of fitness func
int		    g_landscape_size = 25;	  //< Size of optimization landscape window
int		    g_landscape_strip_width = 101;//< Additional size of the graph showing X cross section

int		    g_kymograph_width;		  //< Width of the kymograph window (computed at creation time)
const int	    g_kymograph_height = 512;	  //< Height of the kymograph window
unsigned char	    *g_kymograph_image = NULL;	  //< Pointer to the storage for the kymograph image
float		    g_kymograph_centers[g_kymograph_height];  //< Where the cell-coordinate center of the spindle is.
int		    g_kymograph_window;		  //< Glut window showing kymograph lines stacked into an image
int		    g_kymograph_center_window;	  //< Glut window showing kymograph center-tracking
int		    g_kymograph_filled = 0;	  //< How many lines of data are in there now.

bool                g_quit_at_end_of_video = false; //< When we reach the end of the video, should we quit?

double              g_FIONA_background = 0.0;     //< Can be specified on the command line.

bool                g_use_gui = true;             //< Use 3D and 2D GUI?

//-----------------------------------------------------------------
// This section deals with providing a thread for logging, which runs
// to make sure that the network pipe doesn't get full while video data
// is being pushed into it.  Note that it should be okay to have an extra thread
// here even thought there are as many tracking threads as processors
// because it will be running in between tracking times (though it may
// still be receiving data when the next round of tracking has started).
ThreadData  g_logging_thread_data;
Thread      *g_logging_thread = NULL;
vrpn_Connection	    *g_client_connection = NULL;  //< Connection on which to perform logging
vrpn_Tracker_Remote *g_client_tracker = NULL;	  //< Client tracker object to cause ping/pong server messages
vrpn_Imager_Remote  *g_client_imager = NULL;	  //< Client imager object to cause ping/pong server messages

//-----------------------------------------------------------------
// This section deals with providing multiple threads for tracking.
unsigned            g_num_tracking_processors = 1; //< Number of processors to use for tracking
Semaphore           *g_ready_tracker_semaphore = NULL;
    //< Lets us know if there is a tracker ready to run.  This is created with the number of
    //< slots equal to the number of tracking processors.  It is decremented by each tracking
    //< thread as it enters the ready state and is incremented by the application whenever it
    //< wants a new thread to use for tracking.  This keeps the application from having to
    //< busy-wait on the list of trackers waiting for one to free up.
class ThreadUserData {
public:
  Semaphore *d_run_semaphore;   //< Thread calls P() on this when it wants to run, app calls V() to let it.
  bool      d_ready;            //< Thread is ready to run; app checks for this before calling V().
  Spot_Information  *d_tracker; //< The tracker to optimize.  App fills this in before calling V() to run thread.
};
class ThreadInfo {
public:
  Thread      *d_thread;            //< The thread to run
  ThreadData  d_thread_data;        //< The data structure to pass it including a pointer to...
  ThreadUserData  *d_thread_user_data; //< The user data to pass to the thread, protected by the semaphore in d_thread_data.
};
vector<ThreadInfo *>    g_tracking_threads;           //< The threads to use for tracking

vector<int> vertCandidates;
vector<int> horiCandidates;
bool g_gotNewFrame = false;

//--------------------------------------------------------------------------
// Tcl controls and displays
#ifdef	_WIN32
void  logfilename_changed(char *newvalue, void *);
void  device_filename_changed(char *newvalue, void *);
#else
void  logfilename_changed(const char *newvalue, void *);
void  device_filename_changed(const char *newvalue, void *);
#endif
void  rebuild_trackers(int newvalue, void *);
void  rebuild_trackers(float newvalue, void *);
void  reset_background_image(int newvalue, void *);
void  set_debug_visibility(int newvalue, void *);
void  set_kymograph_visibility(int newvalue, void *);
void  set_maximum_search_radius(int newvalue, void *);
void  handle_optimize_z_change(int newvalue, void *);
void  handle_save_state_change(int newvalue, void *);
void  handle_load_state_change(int newvalue, void *);
Tclvar_float		g_X("x");
Tclvar_float		g_Y("y");
Tclvar_float		g_Z("z");
Tclvar_float		g_Error("error");
Tclvar_float_with_scale	g_Radius("radius", ".kernel.radius", 1, 30, 5);
Tclvar_float_with_scale	*g_minX;
Tclvar_float_with_scale	*g_maxX;
Tclvar_float_with_scale	*g_minY;
Tclvar_float_with_scale	*g_maxY;
Tclvar_float_with_scale	g_exposure("exposure_millisecs", "", 1, 1000, 10);
Tclvar_float_with_scale	g_colorIndex("red_green_blue", NULL, 0, 2, 0);
Tclvar_float_with_scale	g_brighten("brighten", "", 0, 8, 0);
Tclvar_float_with_scale g_precision("precision", "", 0.001, 1.0, 0.05, rebuild_trackers);
Tclvar_float_with_scale g_sampleSpacing("sample_spacing", "", 0.1, 1.0, 1.0, rebuild_trackers);
Tclvar_float_with_scale g_lossSensitivity("kernel_lost_tracking_sensitivity", ".lost_and_found_controls.top", 0.0, 1.0, 0.0);
Tclvar_float_with_scale g_intensityLossSensitivity("intensity_lost_tracking_sensitivity", ".lost_and_found_controls.top", 0.0, 10.0, 0.0);
Tclvar_float_with_scale g_borderDeadZone("dead_zone_around_border", ".lost_and_found_controls.top", 0.0, 30.0, 0.0);
Tclvar_float_with_scale g_trackerDeadZone("dead_zone_around_trackers", ".lost_and_found_controls.top", 0.0, 3.0, 0.0);
Tclvar_float_with_scale g_findThisManyBeads("maintain_this_many_beads", ".lost_and_found_controls.bottom", 0.0, 100.0, 0.0);
Tclvar_float_with_scale g_candidateSpotThreshold("candidate_spot_threshold", ".lost_and_found_controls.bottom", 0.0, 5.0, 5.0);
Tclvar_float_with_scale g_slidingWindowRadius("sliding_window_radius", ".lost_and_found_controls.bottom", 5.0, 15.0, 10.0);
Tclvar_int_with_button  g_lostBehavior("lost_behavior",NULL,0);
Tclvar_int_with_button	g_showLostAndFound("show_lost_and_found","",0);
Tclvar_int_with_button	g_invert("dark_spot",NULL,0, rebuild_trackers);
Tclvar_int_with_button	g_interpolate("interpolate",NULL,1, rebuild_trackers);
Tclvar_int_with_button	g_parabolafit("parabolafit",NULL,0);
Tclvar_int_with_button	g_follow_jumps("follow_jumps",NULL,0, set_maximum_search_radius);
Tclvar_float		g_search_radius("search_radius",0);	  //< Search radius for doing local max in before optimizing.
Tclvar_int_with_button	g_predict("predict",NULL,0);
Tclvar_int_with_button	g_kernel_type("kerneltype", NULL, KERNEL_SYMMETRIC, rebuild_trackers);
Tclvar_int_with_button	g_rod("rod3",NULL,0, rebuild_trackers);
Tclvar_float_with_scale	g_length("length", ".rod3", 10, 50, 20);
Tclvar_float_with_scale	g_orientation("orient", ".rod3", 0, 359, 0);
Tclvar_int_with_button	g_opt("optimize",".kernel.optimize");
Tclvar_int_with_button	g_opt_z("optimize_z",".kernel.optimize", 0, handle_optimize_z_change);
Tclvar_selector		g_psf_filename("psf_filename", NULL, NULL, "");
Tclvar_selector		g_load_state_filename("load_state_filename", NULL, NULL, "");
Tclvar_selector		g_save_state_filename("save_state_filename", NULL, NULL, "");
Tclvar_int_with_button	g_round_cursor("round_cursor",NULL,1);
Tclvar_int_with_button	g_small_area("small_area",NULL);
Tclvar_int_with_button	g_full_area("full_area",NULL);
Tclvar_int_with_button	g_mark("show_tracker",NULL,1);
Tclvar_int_with_button	g_show_video("show_video","",1);
Tclvar_int_with_button	g_opengl_video("use_texture_video","",1);
Tclvar_int_with_button	g_show_debug("show_debug","",0, set_debug_visibility);
Tclvar_int_with_button	g_show_clipping("show_clipping","",0);
Tclvar_int_with_button	g_show_traces("show_logged_traces","",1);
Tclvar_int_with_button	g_background_subtract("background_subtract","",0, reset_background_image);
Tclvar_int_with_button	g_kymograph("kymograph","",0, set_kymograph_visibility);
Tclvar_int_with_button	g_quit("quit",NULL);
Tclvar_int_with_button	*g_play = NULL, *g_rewind = NULL, *g_step = NULL;
Tclvar_selector		g_logfilename("logfilename", NULL, NULL, "", logfilename_changed, NULL);
Tclvar_int		g_log_relative("logging_relative");
Tclvar_int		g_log_without_opt("logging_without_opt");
Tclvar_int		g_log_video("logging_video",0);
Tclvar_int	        g_save_state("save_state", 0, handle_save_state_change);
Tclvar_int	        g_load_state("load_state", 0, handle_load_state_change);
double			g_log_offset_x, g_log_offset_y, g_log_offset_z;
Tclvar_int              g_logging("logging"); //< Accessor for the GUI logging button so rewind can turn it off.
bool g_video_valid = false; // Do we have a valid video frame in memory?
int		        g_log_frame_number_last_logged = -1;

//--------------------------------------------------------------------------
// Return the length of the named file.  Used to help figure out what
// kind of raw file is being opened.  Returns -1 on failure.
// NOTE: Some of our files are longer than 2GB, which means that
// a 32-bit long will wrap and so be unable to determine the file
// length.
#ifndef	_WIN32
  #define __int64 long
  #define _fseeki64 fseek
  #define _ftelli64 ftell
#endif
static __int64 determine_file_length(const char *filename)
{
#ifdef	_WIN32
  FILE *f = fopen(filename, "rb");
#else
  FILE *f = fopen(filename, "r");
#endif
  if (f == NULL) {
    perror("determine_file_length(): Could not open file for reading");
    return -1;
  }
  __int64 val;
  if ( (val = _fseeki64(f, 0, SEEK_END)) != 0) {
    fprintf(stderr, "determine_file_length(): fseek() returned %ld", val);
    fclose(f);
    return -1;
  }
  __int64 length = _ftelli64(f);
  fclose(f);
  return length;
}

//--------------------------------------------------------------------------
// Helper routine to get the Y coordinate right when going between camera
// space and openGL space.
double	flip_y(double y)
{
  return g_image->get_num_rows() - 1 - y;
}

/// Create a pointer to a new tracker of the appropriate type,
// given the global settings for interpolation and inversion.
// Return NULL on failure.

spot_tracker_XY  *create_appropriate_xytracker(double x, double y, double r)
{
  spot_tracker_XY *tracker = NULL;
  // If we are using the oriented-rod kernel, we create a new one depending on the type
  // of subordinate spot tracker that is being used.
  if (g_rod) {
    // If we asked for a FIONA rod tracker, use symmetric instead.
    if (g_kernel_type == KERNEL_FIONA) {
      g_kernel_type = KERNEL_SYMMETRIC;
    }

    if (g_kernel_type == KERNEL_SYMMETRIC) {
      symmetric_spot_tracker_interp *notused = NULL;
      g_interpolate = 1;
      tracker = new rod3_spot_tracker_interp(notused, r,(g_invert != 0), g_precision, 0.1, g_sampleSpacing, g_length, 0.0);
    } else if (g_kernel_type == KERNEL_CONE) {
      cone_spot_tracker_interp *notused = NULL;
      g_interpolate = 1;
      tracker = new rod3_spot_tracker_interp(notused, r,(g_invert != 0), g_precision, 0.1, g_sampleSpacing, g_length, 0.0);
    } else if (g_interpolate) {
      disk_spot_tracker_interp *notused = NULL;
      tracker = new rod3_spot_tracker_interp(notused, r,(g_invert != 0), g_precision, 0.1, g_sampleSpacing, g_length, 0.0);
    } else {
      disk_spot_tracker *notused = NULL;
      tracker = new rod3_spot_tracker_interp(notused, r,(g_invert != 0), g_precision, 0.1, g_sampleSpacing, g_length, 0.0);
    }

  // Not building a compound kernel, so just make a simple kernel of the appropriate type.
  } else {
    if (g_kernel_type == KERNEL_FIONA) {
      // If we have an image, estimate the background value as the minimum pixel value within a square
      // that is 2*diameter (4*radius) on a side.  Estimate the total volume as the sum over this
      // same region with the background subtracted from it.
      // If we don't have an image, then set some default parameters for the kernel.
      double background = 1e50;
      double volume = 0.0;
      if (g_image) {
        unsigned count = 0;
        int i,j;
        for (i = x - 2*r; i <= x + 2*r; i++) {
          for (j = y - 2*r; j <= y + 2*r; j++) {
            double value;
	    if (g_image->read_pixel(i, j, value, g_colorIndex)) {
              volume += value;
              if (value < background) { background = value; }
              count++;
            }
          }
        }
        if (count == 0) {
          background = 0.0;
        } else {
          volume -= count*background;
        }
      } else {
        background = g_FIONA_background;
        volume = 100;
      }
      //fprintf(stderr,"XXX FIONA kernel at %lg,%lg radius %lg: background = %lg, volume = %lg\n", x, y, r, background, volume);
      tracker = new FIONA_spot_tracker(r, (g_invert != 0), g_precision, 0.1, g_sampleSpacing, background, volume);
    } else if (g_kernel_type == KERNEL_SYMMETRIC) {
      g_interpolate = 1;
      tracker = new symmetric_spot_tracker_interp(r,(g_invert != 0), g_precision, 0.1, g_sampleSpacing);
    } else if (g_kernel_type == KERNEL_CONE) {
      g_interpolate = 1;
      tracker = new cone_spot_tracker_interp(r,(g_invert != 0), g_precision, 0.1, g_sampleSpacing);
    } else if (g_interpolate) {
      tracker = new disk_spot_tracker_interp(r,(g_invert != 0), g_precision, 0.1, g_sampleSpacing);
    } else {
      tracker = new disk_spot_tracker(r,(g_invert != 0), g_precision, 0.1, g_sampleSpacing);
    }
  }

  tracker->set_location(x,y);
  tracker->set_radius(r);
  return tracker;
}

/// Create a pointer to a new tracker of the appropriate type,
// given the global settings for interpolation and inversion.
// Return NULL on failure.

spot_tracker_Z  *create_appropriate_ztracker(void)
{
  if (strlen(g_psf_filename) <= 0) {
    return NULL;
  } else {
    return new radial_average_tracker_Z(g_psf_filename, g_precision);
  }
}

//--------------------------------------------------------------------------
// Glut callback routines.

void drawStringAtXY(double x, double y, char *string)
{
  void *font = GLUT_BITMAP_TIMES_ROMAN_24;
  int len, i;

  glRasterPos2f(x, y);
  len = (int) strlen(string);
  for (i = 0; i < len; i++) {
    glutBitmapCharacter(font, string[i]);
  }
}


// This function allows for much simpler freeing up of std::lists of
//  Spot_Information pointers.
static bool deleteAll( Spot_Information * theElement ) {
	if (theElement != NULL)
		delete theElement; // we'll let our Spot_Information destructor do the work
	return true;
}


// This is called when someone kills the task by closing Glut or some
// other means we don't have control over.  If we try to delete the VRPN
// objects here, we get a seg fault for some reason.  VRPN must have already
// cleaned up our stuff for us.
static void  dirtyexit(void)
{
  static bool did_already = false;
  g_quit = 1;  // Lets all the threads know that it is time to exit.

  if (did_already) {
    return;
  } else {
    did_already = true;
  }

  // Done with the camera and other objects.
  printf("Exiting\n");

  // Make the log file name empty so that the logging thread
  // will notice and close its file.
  // Wait until the logging thread exits.
  g_logfilename = "";
  logfilename_changed("", NULL);
  while (g_logging_thread->running()) {
    //------------------------------------------------------------
    // This must be done in any Tcl app, to allow Tcl/Tk to handle
    // events.
  
    while (Tk_DoOneEvent(TK_DONT_WAIT)) {};

    //------------------------------------------------------------
    // This pushes changes in the C variables over to Tcl.

    if (Tclvar_mainloop()) {
      fprintf(stderr,"Tclvar Mainloop failed\n");
    }
  }

  // Get rid of any tracking threads and their associated data
  unsigned i;
  for (i = 0; i < g_tracking_threads.size(); i++) {
    delete g_tracking_threads[i]->d_thread;
    delete g_tracking_threads[i]->d_thread_user_data;
    delete g_tracking_threads[i]->d_thread_data.ps;
    delete g_tracking_threads[i];
  }
  g_tracking_threads.clear();

  // Get rid of any trackers.
  g_trackers.remove_if( deleteAll );
  if (g_camera) { delete g_camera; g_camera = NULL; }
  if (g_glut_image) { delete [] g_glut_image; g_glut_image = NULL; };
  if (g_beadseye_image) { delete [] g_beadseye_image; g_beadseye_image = NULL; };
  if (g_landscape_image) { delete [] g_landscape_image; g_landscape_image = NULL; };
  if (g_landscape_floats) { delete [] g_landscape_floats; g_landscape_floats = NULL; };
  if (g_play) { delete g_play; g_play = NULL; };
  if (g_rewind) { delete g_rewind; g_rewind = NULL; };
  if (g_step) { delete g_step; g_step = NULL; };
  if (g_csv_file) { fclose(g_csv_file); g_csv_file = NULL; g_csv_file = NULL; };
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

  // Do the dirty-exit stuff, then clean up VRPN stuff.
  dirtyexit();

  if (g_vrpn_tracker) { delete g_vrpn_tracker; g_vrpn_tracker = NULL; };
  if (g_vrpn_analog) { delete g_vrpn_analog; g_vrpn_analog = NULL; };
  if (g_vrpn_imager) { delete g_vrpn_imager; g_vrpn_imager = NULL; };
  if (g_vrpn_connection) { g_vrpn_connection->removeReference(); g_vrpn_connection = NULL; };
}

static	double	timediff(struct timeval t1, struct timeval t2)
{
	return (t1.tv_usec - t2.tv_usec) / 1e6 +
	       (t1.tv_sec - t2.tv_sec);
}

static bool fill_and_send_video_region(unsigned minX, unsigned minY, unsigned maxX, unsigned maxY)
{
  // Flip the Y values around the center axis; the image coordinates are upside
  // down with respect to the tracker coordinates.  This also inverts the order of
  // which is min and which is max.  There is a lot of flipping going on inside
  // this routine to get everything lined up right.
  unsigned temp = flip_y(minY);
  minY = flip_y(maxY);
  maxY = temp;

  if (g_camera_bit_depth == 8) {
    static  vrpn_uint8  *image_8bit = NULL;
    static  size_t      image_size = 0;
    size_t              new_size = g_camera->get_num_columns() * g_camera->get_num_rows();

    // Allocate a region that can hold the whole camera image if
    // the image size is different from what we had before (probebly
    // only happens the first time we're called).
    if (image_size != new_size) {
      if (image_8bit != NULL) {
        delete [] image_8bit;
      }
      if ( (image_8bit = new vrpn_uint8[new_size]) == NULL) {
        fprintf(stderr,"fill_and_send_video_region(): Out of memory\n");
        return false;
      }
      image_size = new_size;
    }

    // Fill in the pixels from the global image (based on the routines in the
    // display function, but not scaled by the brightness slider).  Also, we need
    // to flip the row we're reading from and writing to to make things in the
    // output buffer line up correctly and to read from the correct location in
    // the input buffer.
    unsigned r,c;
    vrpn_uint8  uns_pix;
    for (r = minY; r <= maxY; r++) {
      unsigned flip_r = static_cast<unsigned>(flip_y(r));
      unsigned lowc = minX, hic = maxX; //< Speeds things up compared to index arithmetic
      vrpn_uint8 *pixel_base = &image_8bit[(lowc + g_image->get_num_columns() * flip_r) ]; //< Speeds things up
      for (c = lowc; c <= hic; c++) {
	if (!g_image->read_pixel(c, flip_r, uns_pix, g_colorIndex)) {
	    fprintf(stderr, "fill_and_send_video_region(): Cannot read pixel (%d,%d) from region\n", c, flip_r);
            return false;
	}
        *(pixel_base++) = uns_pix;
      }
    }

    // Send the subregion of the image that we were asked to send.
    // We tell it to invert the rows here as well.
    unsigned send_width = maxX - minX + 1;
    unsigned nRowsPerRegion=vrpn_IMAGER_MAX_REGIONu8/send_width;
    unsigned y;
    for(y=minY; y<=maxY; y+=nRowsPerRegion) {
      g_vrpn_imager->send_region_using_base_pointer(g_server_channel,
        minX, maxX, y, min(maxY,y+nRowsPerRegion-1),
        image_8bit,
        1, g_camera->get_num_columns(),
        g_camera->get_num_rows(),true,
        0, 0, 0 /* , XXX timestamp */);
      g_vrpn_imager->mainloop();
    }
  } else {
    static  vrpn_uint16 *image_16bit = NULL;
    static  size_t      image_size = 0;
    size_t              new_size = g_camera->get_num_columns() * g_camera->get_num_rows();

    // Allocate a region that can hold the whole camera image if
    // the image size is different from what we had before (probebly
    // only happens the first time we're called).
    if (image_size != new_size) {
      if (image_16bit != NULL) {
        delete [] image_16bit;
      }
      if ( (image_16bit = new vrpn_uint16[new_size]) == NULL) {
        fprintf(stderr,"fill_and_send_video_region(): Out of memory\n");
        return false;
      }
      image_size = new_size;
    }

    // Fill in the pixels from the global image (based on the routines in the
    // display function, but not scaled by the brightness slider).  Also, we need
    // to flip the row we're reading from and writing to to make things in the
    // output buffer line up correctly and to read from the correct location in
    // the input buffer.
    unsigned r,c;
    vrpn_uint16  uns_pix;
    for (r = minY; r <= maxY; r++) {
      unsigned flip_r = static_cast<unsigned>(flip_y(r));
      unsigned lowc = minX, hic = maxX; //< Speeds things up compared to index arithmetic
      vrpn_uint16 *pixel_base = &image_16bit[(lowc + g_image->get_num_columns() * flip_r) ]; //< Speeds things up
      for (c = lowc; c <= hic; c++) {
	if (!g_image->read_pixel(c, flip_r, uns_pix, g_colorIndex)) {
	    fprintf(stderr, "fill_and_send_video_region(): Cannot read pixel (%d,%d) from region\n", c, flip_r);
            return false;
	}
        *(pixel_base++) = uns_pix;
      }
    }

    // Send the subregion of the image that we were asked to send.
    // We tell it to invert the rows here as well.
    unsigned send_width = maxX - minX + 1;
    unsigned nRowsPerRegion=vrpn_IMAGER_MAX_REGIONu16/send_width;
    unsigned y;
    for(y=minY; y<=maxY; y+=nRowsPerRegion) {
      g_vrpn_imager->send_region_using_base_pointer(g_server_channel,
        minX, maxX, y, min(maxY,y+nRowsPerRegion-1),
        image_16bit,
        1, g_camera->get_num_columns(),
        g_camera->get_num_rows(),true,
        0, 0, 0 /* , XXX timestamp */);
      g_vrpn_imager->mainloop();
    }
  }

  return true;
}

// XXX The start time needs to be reset whenever a new file is opened, probably,
// rather than once at the first logged message.

static	bool  save_log_frame(int frame_number)
{
  static struct timeval start;
  static bool first_time = true;
  struct timeval now; gettimeofday(&now, NULL);

  // If optimization is turned off, only log if the flag is set that lets us log
  // when optimization is turned off.
  if (!g_opt && !g_log_without_opt) {
    return true;
  }

  g_log_frame_number_last_logged = frame_number;

  // Record the frame number into the log file
  g_vrpn_analog->channels()[0] = frame_number;
  g_vrpn_analog->report(vrpn_CONNECTION_RELIABLE);

  // If we're logging video, send a begin-frame event.
  if (g_log_video) {
    // Send begin-frame event.
    // XXX Figure out timestamp
    g_vrpn_imager->send_begin_frame(0, g_camera->get_num_columns()-1,
      0, g_camera->get_num_rows()-1);
  }

  list<Spot_Information *>::iterator loop;
  for (loop = g_trackers.begin(); loop != g_trackers.end(); loop++) {
    // If this tracker is lost, do not log its values (this can happen
    // when we're in "Hover when lost" mode).
    if ((*loop)->lost()) { continue; }

    vrpn_float64  pos[3] = {(*loop)->xytracker()->get_x() - g_log_offset_x,
			    flip_y((*loop)->xytracker()->get_y()) - g_log_offset_y,
			    0.0};
    if ((*loop)->ztracker()) { pos[2] = (*loop)->ztracker()->get_z() - g_log_offset_z; };
    vrpn_float64  quat[4] = { 0, 0, 0, 1};
    double orient = 0.0;
    double length = 0.0;
    double background = 0.0;      // Best-fit background
    double mean_background= 0.0;  // Computed background
    double gaussiansummedvalue = 0.0, computedsummedvalue = 0.0;

    // If we are tracking rods, then we adjust the orientation to match.
    if (g_rod) {
      // Horrible hack to make this work with rod type
      orient = reinterpret_cast<rod3_spot_tracker_interp*>((*loop)->xytracker())->get_orientation();
      length = reinterpret_cast<rod3_spot_tracker_interp*>((*loop)->xytracker())->get_length();
    }

    if (g_kernel_type == KERNEL_FIONA) {
      // Horrible hack to make this export extra stuff for a FIONA kernel
      background = reinterpret_cast<FIONA_spot_tracker*>((*loop)->xytracker())->get_background();
      gaussiansummedvalue = reinterpret_cast<FIONA_spot_tracker*>((*loop)->xytracker())->get_summedvalue();

      // Calculate the background as the mean of the pixels around a 5-pixel radius
      // surrounding the tracker center.
      {
        double theta;
        unsigned count = 0;
        double r = 5;
        double start_x = (*loop)->xytracker()->get_x();
        double start_y = (*loop)->xytracker()->get_y();
        double x, y, value;
        for (theta = 0; theta < 2*M_PI; theta += r / (2 * M_PI) ) {
         x = start_x + r * cos(theta);
         y = start_y + r * sin(theta);
         if (g_image->read_pixel_bilerp(x, y, value, g_colorIndex)) {
           mean_background += value;
           count++;
         }
        }
        if (count != 0) {
         mean_background /= count;
        }
      }

      // Calculate the summed value above background in a 5-pixel-radius region around the center
      // of the Gaussian.  This is chosen to always be the same size to avoid variance based on
      // radius.  It is chosen on integer lattice to avoid interpolation artifacts.  It is round
      // to avoid getting things in the corners.  Subtract the analytic background signal from each
      // pixel's value.
      double x = (*loop)->xytracker()->get_x();
      double y = (*loop)->xytracker()->get_y();
      int ix = static_cast<unsigned>(floor(x+0.5));
      int iy = static_cast<unsigned>(floor(y+0.5));
      int loopx, loopy;
      for (loopx = -5; loopx <= 5; loopx++) {
        for (loopy = -5; loopy <= 5; loopy++) {
          if ( (loopx*loopx + loopy*loopy) <= 5*5 ) {
            double val;
            g_image->read_pixel(ix+loopx, iy+loopy, val, g_colorIndex);
            computedsummedvalue += val - mean_background;
          }
        }
      }
    }

    // If we're sending video, then send a little snippet of video
    // around each tracker every frame.
    if (g_log_video) {
      int x = (*loop)->xytracker()->get_x();
      int y = (*loop)->xytracker()->get_y();
      int halfwidth = (*loop)->xytracker()->get_radius() * 2;
      int minX = x - halfwidth;
      int minY = y - halfwidth;
      int maxX = x + halfwidth;
      int maxY = y + halfwidth;
      if (minX < *g_minX) { minX = *g_minX; }
      if (minY < *g_minY) { minY = *g_minY; }
      if (static_cast<unsigned>(maxX) > *g_maxX) { maxX = *g_maxX; }
      if (static_cast<unsigned>(maxY) > *g_maxY) { maxY = *g_maxY; }
      if (!fill_and_send_video_region(minX, minY, maxX, maxY)) {
        fprintf(stderr, "Could not fill and send video for tracker\n");
        return false;
      }
    }

    // Rotation about the Z axis, reported in radians.
    q_from_euler(quat, orient * (M_PI/180),0,0);
    if (g_vrpn_tracker->report_pose((*loop)->index(), now, pos, quat, vrpn_CONNECTION_RELIABLE) != 0) {
      fprintf(stderr,"Error: Could not log tracker number %d\n", (*loop)->index());
      return false;
    }

    // Also, write the data to the .csv file if one is open.
    if (g_csv_file) {
      if (first_time) {
	start.tv_sec = now.tv_sec; start.tv_usec = now.tv_usec;
	first_time = false;
      }
      double interval = timediff(now, start);
      fprintf(g_csv_file, "%d, %d, %lf,%lf,%lf, %lf, %lf,%lf, %lf,%lf, %lf,%lf\n",
        frame_number, (*loop)->index(),
        pos[0], pos[1], pos[2],
        (*loop)->xytracker()->get_radius(),
        orient, length,
        background, gaussiansummedvalue, mean_background,computedsummedvalue);

      // Make sure there are enough vectors to store all available trackers, then
      // store a new entry for each tracker that currently exists.
      // We do this only when the CSV file is open so we don't show un-logged locations
      while ( g_logged_traces.size() < (*loop)->index() + 1 ) {
        g_logged_traces.push_back(Position_Vector_XY());
      }
      Position_XY tracepos;
      tracepos.x = (*loop)->xytracker()->get_x();
      tracepos.y = (*loop)->xytracker()->get_y();
      g_logged_traces[(*loop)->index()].push_back(tracepos);
    }
  }

  // Forward the full region-of-interest image if our frame number is
  // oone of the lucky ones.
  if ( g_log_video && (frame_number % g_video_full_frame_every == 0) ) {
    // Send the current frame over to the client in chunks as big as possible (limited by vrpn_IMAGER_MAX_REGION).
    if (!fill_and_send_video_region(*g_minX, *g_minY, *g_maxX, *g_maxY)) {
      fprintf(stderr, "Could not fill and send region of interest\n");
      return false;
    }
  }
  
  // If we're logging video, send an end-of-frame event.
  // XXX Figure out timestamp
  if (g_log_video) {
    g_vrpn_imager->send_end_frame(0, g_camera->get_num_columns()-1,
      0, g_camera->get_num_rows()-1);
    g_vrpn_imager->mainloop();
  }

  return true;
}

void myDisplayFunc(void)
{
  unsigned  r,c;
  vrpn_uint16  uns_pix;

  if (!g_ready_to_display) { return; }

  // Clear the window and prepare to draw in the back buffer
  glDrawBuffer(GL_BACK);
  if (g_show_video || g_mark) {
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
  }

  if (g_show_video) {
    // Copy pixels into the image buffer.  Flip the image over in
    // Y so that the image coordinates display correctly in OpenGL.
#ifdef DEBUG
    printf("Filling pixels %d,%d through %d,%d\n", (int)(*g_minX),(int)(*g_minY), (int)(*g_maxX), (int)(*g_maxY));
#endif

    // Figure out how many bits we need to shift to the right.
    // This depends on how many bits the camera has above zero minus
    // the number of bits we want to shift to brighten the image.
    // If this number is negative, clamp to zero.
    int shift_due_to_camera = static_cast<int>(g_camera_bit_depth) - 8;
    int total_shift = shift_due_to_camera - g_brighten;
    if (total_shift < 0) { total_shift = 0; }

    if (g_opengl_video) {
      // If we can't write using OpenGL, turn off the feature for next frame.
      if (!g_image->write_to_opengl_quad(pow(2.0,static_cast<double>(g_brighten)))) {
        g_opengl_video = false;
      }
    } else {
      int shift = total_shift;
      for (r = *g_minY; r <= *g_maxY; r++) {
        unsigned lowc = *g_minX, hic = *g_maxX; //< Speeds things up.
        unsigned char *pixel_base = &g_glut_image[ 4*(lowc + g_image->get_num_columns() * r) ]; //< Speeds things up
        for (c = lowc; c <= hic; c++) {
	  if (!g_image->read_pixel(c, r, uns_pix, g_colorIndex)) {
	    fprintf(stderr, "Cannot read pixel from region\n");
	    cleanup();
      	    exit(-1);
	  }

	  // This assumes that the pixels are actually 8-bit values
	  // and will clip if they go above this.  It also writes pixels
	  // from the first channel into all colors of the image.  It uses
	  // RGBA so that we don't have to worry about byte-alignment problems
	  // that plagued us when using RGB pixels.

          // Storing the uns_pix >> shift into an unsigned char and then storing
          // it into all three is actually SLOWER than just storing it into all
          // three directly.  Argh!
          *(pixel_base++) = uns_pix >> shift;     // Stored in red
          *(pixel_base++) = uns_pix >> shift;     // Stored in green
          *(pixel_base++) = uns_pix >> shift;     // Stored in blue
          pixel_base++;   // Skip alpha, it doesn't matter. //*(pixel_base++) = 255;                  // Stored in alpha

#ifdef DEBUG
	  // If we're debugging, fill the border pixels with green
	  if ( (r == *g_minY) || (r == *g_maxY) || (c == *g_minX) || (c == *g_maxX) ) {
	    g_glut_image[0 + 4 * (c + g_image->get_num_columns() * r)] = 0;
	    g_glut_image[1 + 4 * (c + g_image->get_num_columns() * r)] = 255;
	    g_glut_image[2 + 4 * (c + g_image->get_num_columns() * r)] = 0;
	    g_glut_image[3 + 4 * (c + g_image->get_num_columns() * r)] = 255;
	  }
#endif
        }
      }

      // Store the pixels from the image into the frame buffer
      // so that they cover the entire image (starting from lower-left
      // corner, which is at (-1,-1)).
      glRasterPos2f(-1, -1);
#ifdef DEBUG
      printf("Drawing %dx%d pixels\n", g_image->get_num_columns(), g_image->get_num_rows());
#endif
      // At one point, I changed this to GL_LUMINANCE and then we had to do
      // only 1/4 of the pixel moving in the loop above and we have only 1/4
      // of the memory use, so I figured things would render much more rapidly.
      // In fact, the program ran more slowly (5.0 fps vs. 5.2 fps) with the
      // LUMINANCE than with the RGBA.  Dunno how this could be, but it was.
      // This happened again when I tried it with a raw file, got slightly slower
      // with GL_LUMINANCE.  It must be that the glDrawPixels routine is not
      // optimized to do that function as well as mine, perhaps because it has
      // to write to the alpha channel as well.  Sure enough -- If I switch and
      // write into the Alpha channel, it goes the same speed as when I use
      // the GL_LUMINANCE call here.  Drat!  XXX How about if we make it an
      // RGB rendering window and deal with the alignment issues and then make
      // it a LUMINANCE DrawPixels... probably won't make anything faster but
      // it may make it more confusing.
      glDrawPixels(g_image->get_num_columns(),g_image->get_num_rows(),
        GL_RGBA, GL_UNSIGNED_BYTE, g_glut_image);
    } // End of "not g_opengl_video for texture"
  }

  // Draw a green border around the selected area.  It will be beyond the
  // window border if it is set to the edges; it will just surround the
  // region being selected if it is inside the window.
  glDisable(GL_LINE_SMOOTH);
  glColor3f(0,1,0);
  glBegin(GL_LINE_STRIP);
  glVertex3f( -1 + 2*((*g_minX-1) / (g_image->get_num_columns()-1)),
	      -1 + 2*((*g_minY-1) / (g_image->get_num_rows()-1)) , 0.0 );
  glVertex3f( -1 + 2*((*g_maxX+1) / (g_image->get_num_columns()-1)),
	      -1 + 2*((*g_minY-1) / (g_image->get_num_rows()-1)) , 0.0 );
  glVertex3f( -1 + 2*((*g_maxX+1) / (g_image->get_num_columns()-1)),
	      -1 + 2*((*g_maxY+1) / (g_image->get_num_rows()-1)) , 0.0 );
  glVertex3f( -1 + 2*((*g_minX-1) / (g_image->get_num_columns()-1)),
	      -1 + 2*((*g_maxY+1) / (g_image->get_num_rows()-1)) , 0.0 );
  glVertex3f( -1 + 2*((*g_minX-1) / (g_image->get_num_columns()-1)),
	      -1 + 2*((*g_minY-1) / (g_image->get_num_rows()-1)) , 0.0 );
  glEnd();

  // If we are showing the past positions that have been logged, draw
  // them now.
  if (g_show_traces) {
    glDisable(GL_LINE_SMOOTH);
    glColor3f(1,1,0);

    unsigned trace;
    for (trace = 0; trace < g_logged_traces.size(); trace++) {
      unsigned point;

      glBegin(GL_LINE_STRIP);
      for (point = 0; point < g_logged_traces[trace].size(); point++) {
        glVertex2f( -1 + 2*(g_logged_traces[trace][point].x / (g_image->get_num_columns()-1)),
	            -1 + 2*(g_logged_traces[trace][point].y / (g_image->get_num_rows()-1)) );
      }
      glEnd();
    }
  }

  // If we have been asked to show the tracking markers, draw them.
  // The active one is drawn in red and the others are drawn in blue.
  // The markers may be either cross-hairs with a radius matching the
  // bead radius or a circle with a radius twice that of the bead; the
  // marker type depends on the g_round_cursor variable.
  if (g_mark) {
    // Use smooth lines here to avoid aliasing showing spot in wrong place
    glEnable (GL_BLEND);
    glEnable(GL_LINE_SMOOTH);
    glHint (GL_LINE_SMOOTH_HINT, GL_NICEST);
    glLineWidth(1.5);

    list <Spot_Information *>::iterator loop;
    for (loop = g_trackers.begin(); loop != g_trackers.end(); loop++) {
      // Normalize center and radius so that they match the coordinates
      // (-1..1) in X and Y.
      double  x = -1.0 + (*loop)->xytracker()->get_x() * (2.0/g_image->get_num_columns());
      double  y = -1.0 + ((*loop)->xytracker()->get_y()) * (2.0/g_image->get_num_rows());
      double  dx = (*loop)->xytracker()->get_radius() * (2.0/g_image->get_num_columns());
      double  dy = (*loop)->xytracker()->get_radius() * (2.0/g_image->get_num_rows());

      if (*loop == g_active_tracker) {
	glColor3f(1,0,0);
      } else {
	glColor3f(0,0,1);
      }
      if (g_rod) {
	// Need to draw something for the Rod3 tracker type.
	double orient = static_cast<rod3_spot_tracker_interp*>((*loop)->xytracker())->get_orientation();
	double length = static_cast<rod3_spot_tracker_interp*>((*loop)->xytracker())->get_length();
	double dxx = (cos(orient * M_PI/180)) * (2.0/g_image->get_num_columns());
	double dyx = (sin(orient * M_PI/180)) * (2.0/g_image->get_num_rows());
	double dxy = (sin(orient * M_PI/180)) * (2.0/g_image->get_num_columns());
	double dyy = -(cos(orient * M_PI/180)) * (2.0/g_image->get_num_rows());
        if (g_round_cursor) { // Draw a box around, with lines coming into edges of trackers
          double radius = static_cast<rod3_spot_tracker_interp*>((*loop)->xytracker())->get_radius();

          // Draw a box around that is two tracker radius past the ends and
          // two tracker radii away from the center on each side.  We'll be bringing
          // lines in from this by one radius next.
	  glBegin(GL_LINES);
            // +Y side
	    glVertex2f( x - dxx*(length/2 + 2*radius) + dxy*(2*radius),
                        y - dyx*(length/2 + 2*radius) + dyy*(2*radius));
	    glVertex2f( x + dxx*(length/2 + 2*radius) + dxy*(2*radius),
                        y + dyx*(length/2 + 2*radius) + dyy*(2*radius));
            // -Y side
	    glVertex2f( x - dxx*(length/2 + 2*radius) - dxy*(2*radius),
                        y - dyx*(length/2 + 2*radius) - dyy*(2*radius));
	    glVertex2f( x + dxx*(length/2 + 2*radius) - dxy*(2*radius),
                        y + dyx*(length/2 + 2*radius) - dyy*(2*radius));
            // +X side
	    glVertex2f( x + dxx*(length/2 + 2*radius) + dxy*(2*radius),
                        y + dyx*(length/2 + 2*radius) + dyy*(2*radius));
	    glVertex2f( x + dxx*(length/2 + 2*radius) - dxy*(2*radius),
                        y + dyx*(length/2 + 2*radius) - dyy*(2*radius));
            // -X side
	    glVertex2f( x - dxx*(length/2 + 2*radius) + dxy*(2*radius),
                        y - dyx*(length/2 + 2*radius) + dyy*(2*radius));
	    glVertex2f( x - dxx*(length/2 + 2*radius) - dxy*(2*radius),
                        y - dyx*(length/2 + 2*radius) - dyy*(2*radius));
	  glEnd();

          // Draw lines coming in from the ends of the box to touch the
          // outer radius of tracking.
	  glBegin(GL_LINES);
	    glVertex2f(x + dxx*(length/2 + 2*radius),y + dyx*(length/2 + 2*radius));
	    glVertex2f(x + dxx*(length/2 + radius),y + dyx*(length/2 + radius));
	    glVertex2f(x - dxx*(length/2 + 2*radius),y - dyx*(length/2 + 2*radius));
	    glVertex2f(x - dxx*(length/2 + radius),y - dyx*(length/2 + radius));
	  glEnd();

          // Draw lines coming in from the middle sides of the box to touch the
          // outer radius of tracking.
	  glBegin(GL_LINES);
	    glVertex2f( x + dxy*(2*radius), y + dyy*(2*radius));
	    glVertex2f( x + dxy*(radius),   y + dyy*(radius));
	    
            glVertex2f( x - dxy*(2*radius), y - dyy*(2*radius));
	    glVertex2f( x - dxy*(radius),   y - dyy*(radius));
	  glEnd();

        } else {  // Single-line cursor for rods if we aren't using the round cursor.
	  glBegin(GL_LINES);
	    glVertex2f(x - dxx*length/2,y - dyx*length/2);
	    glVertex2f(x + dxx*length/2,y + dyx*length/2);
	  glEnd();
        }
      } else if (g_round_cursor) {
        // First, make a ring that is twice the radius so that it does not obscure the border.
	double stepsize = M_PI / (*loop)->xytracker()->get_radius();
	double runaround;
	glBegin(GL_LINE_STRIP);
	  for (runaround = 0; runaround <= 2*M_PI; runaround += stepsize) {
	    glVertex2f(x + 2*dx*cos(runaround),y + 2*dy*sin(runaround));
	  }
	  glVertex2f(x + 2*dx, y);  // Close the circle
	glEnd();
        // Then, make four lines coming from the cirle in to the radius of the spot
        // so we can tell what radius we have set
	glBegin(GL_LINES);
	  glVertex2f(x+dx,y); glVertex2f(x+2*dx,y);
	  glVertex2f(x,y+dy); glVertex2f(x,y+2*dy);
	  glVertex2f(x-dx,y); glVertex2f(x-2*dx,y);
	  glVertex2f(x,y-dy); glVertex2f(x,y-2*dy);
	glEnd();
      } else {
	glBegin(GL_LINES);
	  glVertex2f(x-dx,y);
	  glVertex2f(x+dx,y);
	  glVertex2f(x,y-dy);
	  glVertex2f(x,y+dy);
	glEnd();
      }
      // Label the marker with its index
      // If we are doing a kymograph, then label posterior and anterior
      char numString[10];
      sprintf(numString,"%d", (*loop)->index());
      if (g_kymograph) {
	switch ((*loop)->index()) {
	case 2: sprintf(numString,"P"); break;
	case 3: sprintf(numString,"A"); break;
	default: break;
	}
      }
      drawStringAtXY(x+2*dx,y, numString);
    }
  }

  // If we are running a kymograph, draw a green line through the first two
  // trackers to show where we're collecting it from
  if (g_kymograph && (g_trackers.size() >= 2)) {
    list<Spot_Information *>::iterator  loop = g_trackers.begin();
    double x0 = -1.0 + (*loop)->xytracker()->get_x() * (2.0/g_image->get_num_columns());
    double y0 = -1.0 + ((*loop)->xytracker()->get_y()) * (2.0/g_image->get_num_rows());
    loop++;
    double x1 = -1.0 + (*loop)->xytracker()->get_x() * (2.0/g_image->get_num_columns());
    double y1 = -1.0 + ((*loop)->xytracker()->get_y()) * (2.0/g_image->get_num_rows());

    // Draw the line between them
    glColor3f(0.5,0.9,0.5);
    glBegin(GL_LINES);
      glVertex2f(x0,y0);
      glVertex2f(x1,y1);
    glEnd();

    // Find the center of the two (origin of cell coordinates) and the unit vector (dx,dy)
    // going towards the second from the first.
    double xcenter = (x0 + x1) / 2;
    double ycenter = (y0 + y1) / 2;
    double dist = sqrt( (x1-x0)*(x1-x0) + (y1-y0)*(y1-y0) );
    if (dist == 0) { dist = 1; }  //< Avoid divide-by-zero below.
    double dx = (x1 - x0) / dist;
    double dy = (y1 - y0) / dist;

    // Find the perpendicular to the line between them
    double px = dy  * g_image->get_num_rows() / g_image->get_num_columns();
    double py = -dx * g_image->get_num_columns() / g_image->get_num_rows();

    // Draw the perpendicular line
    glColor3f(0.5,0.9,0.5);
    glBegin(GL_LINES);
      glVertex2f(xcenter + 0.05 * px, ycenter + 0.05 * py);
      glVertex2f(xcenter - 0.05 * px, ycenter - 0.05 * py);
    glEnd();

    // If we have the anterior and posterior, draw the perpendicular to
    // the line between them through their center.
    if (g_trackers.size() >= 4) {
      // Find the two tracked points to use.
      loop++; // Skip to the third point.
      double cx0 = -1.0 + (*loop)->xytracker()->get_x() * (2.0/g_image->get_num_columns());
      double cy0 = -1.0 + ((*loop)->xytracker()->get_y()) * (2.0/g_image->get_num_rows());
      loop++;
      double cx1 = -1.0 + (*loop)->xytracker()->get_x() * (2.0/g_image->get_num_columns());
      double cy1 = -1.0 + ((*loop)->xytracker()->get_y()) * (2.0/g_image->get_num_rows());

      // Find the center of the two (origin of cell coordinates) and the unit vector (dx,dy)
      // going towards the second from the first.
      double cxcenter = (cx0 + cx1) / 2;
      double cycenter = (cy0 + cy1) / 2;
      double cdist = sqrt( (cx1-cx0)*(cx1-cx0) + (cy1-cy0)*(cy1-cy0) );
      if (cdist == 0) { cdist = 1; }  //< Avoid divide-by-zero below.
      double cdx = (cx1 - cx0) / cdist;
      double cdy = (cy1 - cy0) / cdist;

      // Find the perpendicular to the line between them
      double cpx = cdy  * g_image->get_num_rows() / g_image->get_num_columns();
      double cpy = -cdx * g_image->get_num_columns() / g_image->get_num_rows();

      // Draw the line
      glBegin(GL_LINES);
	glVertex2f(cxcenter + 500 * cpx, cycenter + 500 * cpy);
	glVertex2f(cxcenter - 500 * cpx, cycenter - 500 * cpy);
      glEnd();
    }
  }

  // For debug info, let's draw the horizontal and vertical lines of our
  //  candidate SMD auto-find locations.
  if (g_show_debug)
  {
	double x, y;
	glBegin(GL_LINES);
	glColor4f(1, 0, 0, 0.8);
        unsigned i;
	for (i = 0; i < vertCandidates.size(); ++i) {
		x = -1.0 + vertCandidates[i] * (2.0/g_image->get_num_columns());
		glVertex2f(x, -1);
		glVertex2f(x, 1);
	}

	glColor4f(0, 0, 1, 0.8);
	for (i = 0; i < horiCandidates.size(); ++i) {
		y = -1.0 + horiCandidates[i] * (2.0/g_image->get_num_rows());
		glVertex2f(-1, y);
		glVertex2f(1, y);
	}
	glEnd();

  }

  // Swap buffers so we can see it.
  if (g_show_video || g_mark) {
    glutSwapBuffers();
  }

  // Have no longer posted redisplay since the last display.
  g_already_posted = false;
}

// Draw the image from the point of view of the currently-active
// tracked bead.  This uses the interpolating read function on the
// image objects to provide as accurate a representation of what the
// area surrounding the tracker center looks like.
void myBeadDisplayFunc(void)
{
  // Clear the window and prepare to draw in the back buffer
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glDrawBuffer(GL_BACK);
  glClearColor(0.0, 0.0, 0.0, 0.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if (g_show_video && g_active_tracker) {
    if (g_kernel_type == KERNEL_FIONA) {
      // For FIONA, we want to draw the shape of the Gaussian kernel against
      // the shape of the underlying image, rather than just the image resampled
      // at the appropriate centering.  We do this as a pair of wire-frame images
      // so that we can see through one into the other.

      // Determine the scale needed to make the maximum Z value equal
      // to 1.
      double radius = g_active_tracker->xytracker()->get_radius();
      double xImageOffset = g_active_tracker->xytracker()->get_x();
      double yImageOffset = g_active_tracker->xytracker()->get_y();
      double max = -1e10;
      double x,y;
      double double_pix;

      // Include the maximum (center) spot in the Gaussian in the max calculation.  Then look
      // at all of the pixel values in the image near the tracker center.
      static_cast<FIONA_spot_tracker *>(g_active_tracker->xytracker())->read_pixel(0, 0, max);
      for (x = - 2*radius; x <= 2*radius; x++) {
        for (y = - 2*radius; y <= 2*radius; y++) {
	  g_image->read_pixel_bilerp(x+xImageOffset, y+yImageOffset, double_pix, g_colorIndex);
          if (double_pix > max) { max = double_pix; }
        }
      }
      double max_to_draw = (2 * radius) + 5;  // Draw a bit past the edge
      double scale = 1 / max_to_draw;
      double zScale = 1/max;

      // Set the projection matrix to let us see the whole thing.  Be sure to put
      // this back at the far end.
      glMatrixMode(GL_PROJECTION);
      glLoadIdentity();
      glFrustum(-0.5,0.5, -0.5,0.5, 7,15);
      glMatrixMode(GL_MODELVIEW);
      glLoadIdentity();

      // Set the viewpoint to something useful, so that we can see the
      // relative heights of the image and the kernel.  Set it to spinning
      // so that we can get a good idea of the relative shapes.
      glTranslated(0,-scale/2, -10);
      glRotated(-30, 1,0,0 );
      static double spin = 15.0;
      spin += 2;
      if (spin > 360) { spin -= 360; }
      glRotated( spin, 0,0,1 );

      glEnable(GL_DEPTH_TEST);

      // Draw a graph of the image data in red, along Y direction.
      // Draw it again along the X direction, to produce a complete mesh.
      // Make sure we have integral coordinates because that will center
      // the middle of the kernel correctly.
      glColor3f(1,0.2,0.2);
      for (x = floor(-max_to_draw); x <= max_to_draw; x++) {
        glBegin(GL_LINE_STRIP);
        for (y = floor(-max_to_draw); y <= max_to_draw; y++) {
	  g_image->read_pixel_bilerp(x+xImageOffset, y+yImageOffset, double_pix, g_colorIndex);
          glVertex3d(x*scale, y*scale, double_pix*zScale);
        }
        glEnd();
      }
      for (y = floor(-max_to_draw); y <= max_to_draw; y++) {
        glBegin(GL_LINE_STRIP);
        for (x = floor(-max_to_draw); x <= max_to_draw; x++) {
	  g_image->read_pixel_bilerp(x+xImageOffset, y+yImageOffset, double_pix, g_colorIndex);
          glVertex3d(x*scale, y*scale, double_pix*zScale);
        }
        glEnd();
      }

      // Draw a graph of the kernel data in green, along X direction.
      // Draw it again along the Y direction, to produce a complete mesh.
      // Note that we have to shift the drawn kernel by the inverse of the fractional
      // offset in the FIONA image, so that it will line up with the displayed image.
      int x_int = static_cast<int>(floor(xImageOffset));
      int y_int = static_cast<int>(floor(yImageOffset));
      double x_frac = xImageOffset - x_int;
      double y_frac = yImageOffset - y_int;

      glColor3f(0.0,1.0,0.6);
      for (x = floor(-max_to_draw); x <= max_to_draw; x++) {
        glBegin(GL_LINE_STRIP);
        for (y = floor(-max_to_draw); y <= max_to_draw; y++) {
	  static_cast<FIONA_spot_tracker *>(g_active_tracker->xytracker())->read_pixel(x, y, double_pix);
          glVertex3d( (x-x_frac)*scale, (y-y_frac)*scale, double_pix*zScale);
        }
        glEnd();
      }
      glColor3f(0.0,1.0,0.6);
      for (y = floor(-max_to_draw); y <= max_to_draw; y++) {
        glBegin(GL_LINE_STRIP);
        for (x = floor(-max_to_draw); x <= max_to_draw; x++) {
	  static_cast<FIONA_spot_tracker *>(g_active_tracker->xytracker())->read_pixel(x, y, double_pix);
          glVertex3d((x-x_frac)*scale, (y-y_frac)*scale, double_pix*zScale);
        }
        glEnd();
      }

      glDisable(GL_DEPTH_TEST);

    } else {
      // Copy pixels into the image buffer.
      int x,y;
      float xImageOffset = g_active_tracker->xytracker()->get_x() - (g_beadseye_size+1)/2.0;
      float yImageOffset = g_active_tracker->xytracker()->get_y() - (g_beadseye_size+1)/2.0;
      double  double_pix;

      // If we are outside 2 radii, then leave it blank to avoid having a
      // moving border that will make us think the spot is moving when it
      // is not.
      for (x = 0; x < g_beadseye_size; x++) {
        for (y = 0; y < g_beadseye_size; y++) {
	  g_beadseye_image[0 + 4 * (x + g_beadseye_size * (y))] = 0;
	  g_beadseye_image[1 + 4 * (x + g_beadseye_size * (y))] = 0;
	  g_beadseye_image[2 + 4 * (x + g_beadseye_size * (y))] = 0;
	  g_beadseye_image[3 + 4 * (x + g_beadseye_size * (y))] = 255;
        }
      }
      double radius = g_active_tracker->xytracker()->get_radius();
      if (g_rod) {
        // Horrible hack to make this work with rod type
        radius = static_cast<rod3_spot_tracker_interp*>(g_active_tracker->xytracker())->get_length() / 2;
      }
      int min_x = (g_beadseye_size+1)/2 - 2 * radius;
      int max_x = (g_beadseye_size+1)/2 + 2 * radius;
      int min_y = (g_beadseye_size+1)/2 - 2 * radius;
      int max_y = (g_beadseye_size+1)/2 + 2 * radius;

      // Make sure we don't try to draw outside the allocated buffer
      if (min_x < 0) { min_x = 0; }
      if (min_y < 0) { min_y = 0; }
      if (max_x > g_beadseye_size-1) { max_x = g_beadseye_size-1; }
      if (max_y > g_beadseye_size-1) { max_y = g_beadseye_size-1; }

      // Figure out how many bits we need to shift to the right.
      // This depends on how many bits the camera has above zero minus
      // the number of bits we want to shift to brighten the image.
      // If this number is negative, clamp to zero.
      int shift_due_to_camera = g_camera_bit_depth - 8;
      int total_shift = shift_due_to_camera - g_brighten;
      if (total_shift < 0) { total_shift = 0; }
      int shift = total_shift;
      for (x = min_x; x < max_x; x++) {
        for (y = min_y; y < max_y; y++) {
	  if (!g_image->read_pixel_bilerp(x+xImageOffset, y+yImageOffset, double_pix, g_colorIndex)) {
	    g_beadseye_image[0 + 4 * (x + g_beadseye_size * (y))] = 0;
	    g_beadseye_image[1 + 4 * (x + g_beadseye_size * (y))] = 0;
	    g_beadseye_image[2 + 4 * (x + g_beadseye_size * (y))] = 0;
	    g_beadseye_image[3 + 4 * (x + g_beadseye_size * (y))] = 255;
	  } else {
	    g_beadseye_image[0 + 4 * (x + g_beadseye_size * (y))] = ((vrpn_uint16)(double_pix)) >> shift;
	    g_beadseye_image[1 + 4 * (x + g_beadseye_size * (y))] = ((vrpn_uint16)(double_pix)) >> shift;
	    g_beadseye_image[2 + 4 * (x + g_beadseye_size * (y))] = ((vrpn_uint16)(double_pix)) >> shift;
	    g_beadseye_image[3 + 4 * (x + g_beadseye_size * (y))] = 255;
	  }
        }
      }

      // Store the pixels from the image into the frame buffer
      // so that they cover the entire image (starting from lower-left
      // corner, which is at (-1,-1)).
      glRasterPos2f(-1, -1);
      glDrawPixels(g_beadseye_size, g_beadseye_size,
        GL_RGBA, GL_UNSIGNED_BYTE, g_beadseye_image);
    }
  }

  // Swap buffers so we can see it.
  glutSwapBuffers();
}

// Draw the image from the point of view of the currently-active
// tracked bead.  This window shows the optimization value landscape in
// the region around the current point.
void myLandscapeDisplayFunc(void)
{
  // Clear the window and prepare to draw in the back buffer
  glDrawBuffer(GL_BACK);
  glClearColor(0.0, 0.0, 0.0, 0.0);
  glClear(GL_COLOR_BUFFER_BIT);

  if (g_show_video && g_active_tracker) {
    int x,y;

    // Find the raw fitness function values for a range of locations
    // at 1-pixel steps surrounding the current location.  Store these
    // into a raw floating-point buffer, and then set the optimizer back
    // where it started.
    // Solve for the max and min values in the floating-point buffer, then
    // a scale and offset to map them to the range 0-255.
    double min_val = 1e100, max_val = -1e100;
    double start_x = g_active_tracker->xytracker()->get_x();
    double start_y = g_active_tracker->xytracker()->get_y();
    float xImageOffset = start_x - (g_landscape_size+1)/2.0;
    float yImageOffset = start_y - (g_landscape_size+1)/2.0;
    double this_val;
    for (x = 0; x < g_landscape_size; x++) {
      for (y = 0; y < g_landscape_size; y++) {
	g_active_tracker->xytracker()->set_location(x + xImageOffset, y + yImageOffset);
	this_val = g_active_tracker->xytracker()->check_fitness(*g_image, g_colorIndex);
	g_landscape_floats[x + g_landscape_size * y] = this_val;
	if (this_val < min_val) { min_val = this_val; }
	if (this_val > max_val) { max_val = this_val; }
      }
    }

    // Copy pixels into the image buffer.
    // Scale and offset them to fit in the range 0-255.
    double scale = 255 / (max_val - min_val);
    double offset = min_val * scale;
    int	    int_val;
    for (x = 0; x < g_landscape_size; x++) {
      for (y = 0; y < g_landscape_size; y++) {
	int_val = (int)( g_landscape_floats[x + g_landscape_size * y] * scale - offset );
	g_landscape_image[0 + 4 * (x + g_landscape_size * (y))] = int_val;
	g_landscape_image[1 + 4 * (x + g_landscape_size * (y))] = int_val;
	g_landscape_image[2 + 4 * (x + g_landscape_size * (y))] = int_val;
	g_landscape_image[3 + 4 * (x + g_landscape_size * (y))] = 255;
      }
    }

    // Store the pixels from the image into the frame buffer
    // so that they cover the entire image (starting from lower-left
    // corner, which is at (-1,-1)).
    glRasterPos2f(-1, -1);
    glDrawPixels(g_landscape_size, g_landscape_size,
      GL_RGBA, GL_UNSIGNED_BYTE, g_landscape_image);

    // Draw a line graph showing the values along the X axis
    double strip_start_x = -1 + 2 * ((double)(g_landscape_size)) / (g_landscape_size + g_landscape_strip_width);
    double strip_step_x = (1 - strip_start_x) / g_landscape_strip_width;
    xImageOffset = start_x - (g_landscape_strip_width+1)/2.0;
    scale = 2 / (max_val - min_val);
    offset = min_val*scale + 1; // Want to get -1 when subtract this and multiply by the scale.
    glColor3f(1,1,1);
    glBegin(GL_LINE_STRIP);
    for (x = 0; x < g_landscape_strip_width; x++) {
      // If we are using the rod tracker, we translate the kernel along its orientation
      // axis.  Otherwise, we translate in X.
      if (g_rod) {
	// Horrible hack to make this work with rod type
	double orient = static_cast<rod3_spot_tracker_interp*>(g_active_tracker->xytracker())->get_orientation();
	double length = static_cast<rod3_spot_tracker_interp*>(g_active_tracker->xytracker())->get_length();
	double dx =   cos(orient * M_PI/180);
	double dy = - sin(orient * M_PI/180);
	double delta = x - (g_landscape_strip_width+1)/2.0;
	g_active_tracker->xytracker()->set_location(start_x + dx*delta, start_y + dy*delta);
      } else {
	g_active_tracker->xytracker()->set_location(x + xImageOffset, start_y);
      }
      this_val = g_active_tracker->xytracker()->check_fitness(*g_image, g_colorIndex);
      glVertex3f( strip_start_x + x * strip_step_x, this_val * scale - offset,0);
    }
    glEnd();

    // Put the tracker back where it started.
    g_active_tracker->xytracker()->set_location(start_x, start_y);
  }

  // Swap buffers so we can see it.
  glutSwapBuffers();
}

// This draws the graph of how the spindle center moves with
// respect to the cell anterior-posterior axis.  This axis is
// defined by trackers 2 and 3 (the third and fourth trackers),
// which do not get optimized when the kymograph is run but
// rather stay put in the image.
void mykymographCenterDisplayFunc(void)
{
  // Clear the window and prepare to draw in the back buffer
  glDrawBuffer(GL_BACK);
  glClearColor(0.0, 0.0, 0.0, 0.0);
  glClear(GL_COLOR_BUFFER_BIT);

  if (g_show_video) {
    int i;

    // Draw the centerline for the image and indicators for grids off-center.
    glColor3f(0.5,0.5,0.5);
    glBegin(GL_LINES);
      glVertex2f(0, 1);
      glVertex2f(0,-1);
    glEnd();

    // Draw a dim mark every ten pixels
    glColor3f(0.1,0.1,0.1);
    glBegin(GL_LINES);
    for (i = 10; i < 500; i += 10) {
	glVertex2f(i * (2.0/g_kymograph_width), 1);
	glVertex2f(i * (2.0/g_kymograph_width),-1);

	glVertex2f(-i * (2.0/g_kymograph_width), 1);
	glVertex2f(-i * (2.0/g_kymograph_width),-1);
    }
    glEnd();

    // Draw a brighter mark every hundred pixels
    glColor3f(0.2,0.2,0.2);
    glBegin(GL_LINES);
    for (i = 100; i < 500; i += 100) {
	glVertex2f(i * (2.0/g_kymograph_width), 1);
	glVertex2f(i * (2.0/g_kymograph_width),-1);

	glVertex2f(-i * (2.0/g_kymograph_width), 1);
	glVertex2f(-i * (2.0/g_kymograph_width),-1);
    }
    glEnd();

    // Draw each center entry into the image.
    glColor3f(1,1,0);
    glBegin(GL_LINE_STRIP);
      for (i = 0; i < g_kymograph_filled; i++) {
	  double  x = g_kymograph_centers[i] * (2.0/g_kymograph_width);
	  double  y = 1.0 - i * (2.0/g_kymograph_height);
	  glVertex2f(x,y);
      }
    glEnd();
  }

  // Swap buffers so we can see it.
  glutSwapBuffers();
}

// Draw the kymograph image.
// This uses the interpolating read function on the
// image objects to provide as accurate a representation of what the
// line between the first two tracked spots looks like.
void mykymographDisplayFunc(void)
{
  // Clear the window and prepare to draw in the back buffer
  glDrawBuffer(GL_BACK);
  glClearColor(0.0, 0.0, 0.0, 0.0);
  glClear(GL_COLOR_BUFFER_BIT);

  if (g_show_video) {

    // Store the pixels from the image into the frame buffer
    // so that they cover the entire image (starting from lower-left
    // corner, which is at (-1,-1)).
    glRasterPos2f(-1, -1);
    glDrawPixels(g_kymograph_width, g_kymograph_height,
      GL_RGBA, GL_UNSIGNED_BYTE, g_kymograph_image);

    // Draw a tiny red dot at the center position just below the
    // last line of pixels.

  }

  // Swap buffers so we can see it.
  glutSwapBuffers();
}

bool  delete_active_xytracker(void)
{
  if (!g_active_tracker) {
    return false;
  } else {
    // Delete the entry in the list that corresponds to the active tracker.
    list <Spot_Information *>::iterator loop;
    list <Spot_Information *>::iterator next;
    for (loop = g_trackers.begin(); loop != g_trackers.end(); loop++) {
		if (*loop == g_active_tracker) {
			next = loop; next++;
			delete *loop;
			g_trackers.erase(loop);
			break;
		}
    }

    // Set the active tracker to the next one in the list if there is
    // a next one.  Otherwise, try to set it to the first one on the list,
    // if there is one.  Otherwise, set it to NULL.
    if (next != g_trackers.end()) {
      g_active_tracker = *next;
    } else {
      if (g_trackers.size() > 0) {
	g_active_tracker = *g_trackers.begin();
      } else {
	g_active_tracker = NULL;
      }
    }

    // Set the radius slider to the value of the new active tracker to avoid
    // having it change to the current radius value.
    if (g_active_tracker) {
      g_Radius = g_active_tracker->xytracker()->get_radius();
    }

    // Play the "deleted tracker" sound.
#ifdef	_WIN32
    if (!PlaySound("deleted_tracker.wav", NULL, SND_FILENAME | SND_ASYNC)) {
      fprintf(stderr,"Cannot play sound %s\n", "deleted_tracker.wav");
    }
#endif

    return true;
  }
}


// Optimize to find the best fit starting from last position for a tracker.
// Invert the Y values on the way in and out.
// Don't let it adjust the radius here (otherwise it gets too jumpy).
static void optimize_tracker(Spot_Information *tracker)
{
  double  x, y;
  double last_pos[2];
  last_pos[0] = tracker->xytracker()->get_x();
  last_pos[1] = tracker->xytracker()->get_y();
  double last_vel[2];
  tracker->get_velocity(last_vel);

  // If we are doing prediction, apply the estimated last velocity to
  // move the estimated position to a new location
  if ( g_predict && (g_last_optimized_frame_number != g_frame_number) ) {
    double new_pos[2];
    new_pos[0] = last_pos[0] + last_vel[0];
    new_pos[1] = last_pos[1] + last_vel[1];
    tracker->xytracker()->set_location(new_pos[0], new_pos[1]);
  }

  // If the frame number has changed, and we are doing global searches
  // within a radius, then create an image-based tracker at the last
  // location for the current tracker on the last frame; scan all locations
  // within radius and find the maximum fit on this frame; move the tracker
  // location to that maximum before doing the optimization.  We use the
  // image-based tracker for this because other trackers may have maximum
  // fits outside the region where the bead is -- the symmetric tracker often
  // has a local maximum at best fit and global maxima elsewhere.
  if ( g_last_image && ((double)(g_search_radius) > 0) && (g_last_optimized_frame_number != g_frame_number) ) {

    double x_base = tracker->xytracker()->get_x();
    double y_base = tracker->xytracker()->get_y();

    // If we are doing prediction, reduce the search radius to 3 because we rely
    // on the prediction to get us close to the global maximum.  If we make it 2.5,
    // it starts to lose track with an accuracy of 1 pixel for a bead on cilia in
    // pulnix video (acquired at 120 frames/second).
    double used_search_radius = g_search_radius;
    if ( g_predict && (used_search_radius > 3) ) {
      used_search_radius = 3;
    }

    // Create an image spot tracker and initize it at the location where the current
    // tracker started this frame (before prediction), but in the last image.  Grab enough
    // of the image that we will be able to check over the used_search_radius for a match.
    // Use the faster twolines version of the image-based tracker.
    twolines_image_spot_tracker_interp max_find(tracker->xytracker()->get_radius(), (g_invert != 0), g_precision,
      0.1, g_sampleSpacing);
    max_find.set_location(last_pos[0], last_pos[1]);
    max_find.set_image(*g_last_image, g_colorIndex, last_pos[0], last_pos[1], tracker->xytracker()->get_radius() + used_search_radius);

    // Loop over the pixels within used_search_radius of the initial location and find the
    // location with the best match over all of these points.  Do this in the current image,
    // at the (possibly-predicted) starting location and find the offset from the (possibly
    // predicted) current location to get to the right place.
    double radsq = used_search_radius * used_search_radius;
    int x_offset, y_offset;
    int best_x_offset = 0;
    int best_y_offset = 0;
    double best_value = max_find.check_fitness(*g_image, g_colorIndex);
    for (x_offset = -floor(used_search_radius); x_offset <= floor(used_search_radius); x_offset++) {
      for (y_offset = -floor(used_search_radius); y_offset <= floor(used_search_radius); y_offset++) {
	if ( (x_offset * x_offset) + (y_offset * y_offset) <= radsq) {
	  max_find.set_location(x_base + x_offset, y_base + y_offset);
	  double val = max_find.check_fitness(*g_image, g_colorIndex);
	  if (val > best_value) {
	    best_x_offset = x_offset;
	    best_y_offset = y_offset;
	    best_value = val;
	  }
	}
      }
    }

    // Put the tracker at the location of the maximum, so that it will find the
    // total maximum when it finds the local maximum.
    tracker->xytracker()->set_location(x_base + best_x_offset, y_base + best_y_offset);
  }

  // Here's where the tracker is optimized to its new location.
  // FIONA trackers always try to optimize radius along with XY.
  if (g_parabolafit) {
    tracker->xytracker()->optimize_xy_parabolafit(*g_image, g_colorIndex, x, y, tracker->xytracker()->get_x(), tracker->xytracker()->get_y() );
  } else {
    if (g_kernel_type == KERNEL_FIONA) {
      tracker->xytracker()->optimize(*g_image, g_colorIndex, x, y, tracker->xytracker()->get_x(), tracker->xytracker()->get_y() );
    } else {
      tracker->xytracker()->optimize_xy(*g_image, g_colorIndex, x, y, tracker->xytracker()->get_x(), tracker->xytracker()->get_y() );
    }
  }

  // If we are doing prediction, update the estimated velocity based on the
  // step taken.
  if ( g_predict && (g_last_optimized_frame_number != g_frame_number) ) {
    const double vel_frac_to_use = 0.9; //< 0.85-0.95 seems optimal for cilia in pulnix; 1 is too much, 0.83 is too little
    const double acc_frac_to_use = 0.0; //< Due to the noise, acceleration estimation makes things worse

    // Compute the new velocity estimate as the subtraction of the
    // last position from the current position.
    double new_vel[2];
    new_vel[0] = (tracker->xytracker()->get_x() - last_pos[0]) * vel_frac_to_use;
    new_vel[1] = (tracker->xytracker()->get_y() - last_pos[1]) * vel_frac_to_use;

    // Compute the new acceleration estimate as the subtraction of the new
    // estimate from the old.
    double new_acc[2];
    new_acc[0] = (new_vel[0] - last_vel[0]) * acc_frac_to_use;
    new_acc[1] = (new_vel[1] - last_vel[1]) * acc_frac_to_use;

    // Re-estimate the new velocity by taking into account the acceleration.
    new_vel[0] += new_acc[0];
    new_vel[1] += new_acc[1];

    // Store the quantities for use next time around.
    tracker->set_velocity(new_vel);
    tracker->set_acceleration(new_acc);
  }

  // If we have a non-zero kernel-based threshold for determining if we are lost,
  // check and see if we are.  This is done by finding the value of
  // the fitness function at the actual tracker location and comparing
  // it to the maximum values located on concentric rings around the
  // tracker.  We look for the minimum of these maximum values (the
  // deepest moat around the peak in the center) and determine if we're
  // lost based on the type of kernel we are using.
  // Some ideas for determining goodness of tracking for a bead...
  //   (It looks like different metrics are needed for symmetric and cone and disk.)
  // For symmetric:
  //    Value compared to initial value when tracking that bead: When in a flat area, it can be better.
  //    Minimum over area vs. center value: get low-valued lobes in certain directions, but other directions bad.
  //    Maximum of all minima as you travel in different directions from center: dunno.
  //    Maximum at radius from center: How do you know what radius to select?
  //      Max at radius of the minimum value from center?
  //      Minimum of the maxima over a number of radii? -- Yep, we're trying this one.
  // FIONA kernel operates differently; it looks to see if the value at the
  //    center is more than the specified fraction of twice the standard deviation away
  //    from the mean of the surrounding values.  For an inverted tracker, this
  //    must be below; for a non-inverted tracker it is above.
  tracker->lost(false); // Not lost yet...
  if (g_lossSensitivity > 0.0) {
   if (g_kernel_type == KERNEL_FIONA) {
     // Compute the mean of the values two radii out from the
     // center of the tracker.
     double mean = 0.0, value;
     double theta;
     unsigned count = 0;
     double r = 2 * tracker->xytracker()->get_radius();
     double start_x = tracker->xytracker()->get_x();
     double start_y = tracker->xytracker()->get_y();
     double x, y;
     for (theta = 0; theta < 2*M_PI; theta += r / (2 * M_PI) ) {
       x = start_x + r * cos(theta);
       y = start_y + r * sin(theta);
       if (g_image->read_pixel_bilerp(x, y, value, g_colorIndex)) {
         mean += value;
         count++;
       }
     }
     if (count != 0) {
       mean /= count;
     }

     // Compute the standard deviation of the values
     double std_dev = 0.0;
     count = 0;
     for (theta = 0; theta < 2*M_PI; theta += r / (2 * M_PI) ) {
       x = start_x + r * cos(theta);
       y = start_y + r * sin(theta);
       if (g_image->read_pixel_bilerp(x, y, value, g_colorIndex)) {
         std_dev += (mean - value) * (mean - value);
         count++;
       }
     }
     if (count > 0) {
       std_dev /= (count-1);
     }
     std_dev = sqrt(std_dev);

     // Check to see if we're lost based on how far we are above/below the
     // mean.
     double threshold = g_lossSensitivity * (2 * std_dev);
     if (g_image->read_pixel_bilerp(start_x, start_y, value, g_colorIndex)) {
       double diff = value - mean;
       if (g_invert) { diff *= -1; }
       if (diff < threshold) {
         tracker->lost(true);
       } else {
         tracker->lost(false);
       }
     }

   } else {
    double min_val = 1e100; //< Min over all circles
    double start_x = tracker->xytracker()->get_x();
    double start_y = tracker->xytracker()->get_y();
    double this_val;
    double r, theta;
    double x, y;
    // Only look every two-pixel radius from three out to the radius of the tracker.
    for (r = 3; r <= tracker->xytracker()->get_radius(); r += 2) {
      double max_val = -1e100;  //< Max over this particular circle
      // Look at every pixel around the circle.
      for (theta = 0; theta < 2*M_PI; theta += r / (2 * M_PI) ) {
        x = r * cos(theta);
        y = r * sin(theta);
	tracker->xytracker()->set_location(x + start_x, y + start_y);
	this_val = tracker->xytracker()->check_fitness(*g_image, g_colorIndex);
	if (this_val > max_val) { max_val = this_val; }
      }
      if (max_val < min_val) { min_val = max_val; }
    }
    //Put the tracker back where it started.
    tracker->xytracker()->set_location(start_x, start_y);

    // See if we are lost.  The way we tell if we are lost or not depends
    // on the type of kernel we are using.  It also depends on the setting of
    // the "lost sensitivity" parameter, which varies from 0 (not sensitive at
    // all) to 1 (most sensitive).
    double starting_value = tracker->xytracker()->check_fitness(*g_image, g_colorIndex);
    //printf("XXX Center = %lg, min of maxes = %lg, scale = %lg\n", starting_value, min_val, min_val/starting_value);
    if (g_kernel_type == KERNEL_SYMMETRIC) {
      // The symmetric kernel reports values that are strictly non-positive for
      // its fitness function.  Some observation of values reveals that the key factor
      // seems to be how many more times negative the "moat" is than the central peak.
      // Based on a few observations of different bead tracks, it looks like a factor
      // of 2 is almost always too small, and a factor of 4-10 is almost always fine.
      // So, we'll set the "scale factor" to be between 1 (when sensitivity is just
      // above zero) and 10 (when the scale factor is just at one).  If a tracker gets
      // lost, set it to be the active tracker so the user can tell which one is
      // causing trouble.
      double scale_factor = 1 + 9*g_lossSensitivity;
      if (starting_value * scale_factor < min_val) {
        tracker->lost(true);
      } else {
        tracker->lost(false);
      }
    } else if (g_kernel_type == KERNEL_CONE) {
      // Differences (not factors) of 0-5 seem more appropriate for a quick test of the cone kernel.
      double difference = 5*g_lossSensitivity;
      if (starting_value - min_val < difference) {
        tracker->lost(true);
      } else {
        tracker->lost(false);
      }
    } else {  // Must be a disk kernel
      // Differences (not factors) of 0-5 seem more appropriate for a quick test of the disk kernel.
      double difference = 5*g_lossSensitivity;
      if (starting_value - min_val < difference) {
        tracker->lost(true);
      } else {
        tracker->lost(false);
      }
    }
   }
  }

  // If the intensity-based lost threshold setting is nonzero, check to see
  // whether the pixel at the center of the tracker is significantly different
  // from the pixels around the outside of the tracker.  We do this by looking at
  // the mean and variance of the pixels along a box with sides that are
  // 1.5 times the diameter of the tracker (3x the radius) and seeing if the
  // squared difference between the center pixel and mean is greater than the
  // variance scaled by the sensitivity.  If it is, there is a well-defined
  // bead and so we're not lost.  (This routine does not check the corner
  // pixels of the square surrounding, to avoid double-weighting them based
  // on our loop strategy.)
  if (g_intensityLossSensitivity > 0.0) {
     int halfwidth = static_cast<int>(1.5 * tracker->xytracker()->get_radius());
     int x = static_cast<int>(tracker->xytracker()->get_x());
     int y = static_cast<int>(tracker->xytracker()->get_y());
     int pixels = 0;
     double val = 0;
     double mean = 0;
     double variance = 0;
     int offset;
     for (offset = -halfwidth+1; offset < halfwidth; offset++) {
       if (g_image->read_pixel(x-offset, y-halfwidth, val, g_colorIndex)) {
         mean += val;
         variance += val*val;
         pixels++;
       }
       if (g_image->read_pixel(x-offset, y+halfwidth, val, g_colorIndex)) {
         mean += val;
         variance += val*val;
         pixels++;
       }
       if (g_image->read_pixel(x-halfwidth, y+offset, val, g_colorIndex)) {
         mean += val;
         variance += val*val;
         pixels++;
       }
       if (g_image->read_pixel(x+halfwidth, y+offset, val, g_colorIndex)) {
         mean += val;
         variance += val*val;
         pixels++;
       }
     }

     // Compute the variance of all the points using the shortcut
     // formula: var = (sum of squares of measures) - (sum of measurements)^2 / n.
     // Then compute the mean.
     if (pixels) {
       variance = variance - mean*mean/pixels;
       mean /= pixels;
     }

     // Compare to see if we're lost.  Check <= rather than < because in
     // regions where the pixels clamp to 0 or max there is no difference on
     // either side of the equation and we should be lost.
     g_image->read_pixel(x, y, val, g_colorIndex);
     if ((val - mean)*(val-mean) <= variance * g_intensityLossSensitivity) {
       tracker->lost(true);
     }
  }

  // If we have a non-zero threshold to check against the boundary of the
  // image, check to see if we've wandered too close to it.
  if (g_borderDeadZone > 0) {   
      double x = tracker->xytracker()->get_x();
      double y = tracker->xytracker()->get_y();
      double zone = g_borderDeadZone;

      // Check against the border.
      if ( (x < (*g_minX) + zone) || (x > (*g_maxX) - zone) ||
           (y < (*g_minY) + zone) || (y > (*g_maxY) - zone) ) {
        tracker->lost(true);
      }
  }

  // If we're set to hover when lost and we are lost, then go back to where we
  // started.
  if (tracker->lost() && (g_lostBehavior == LOST_HOVER)) {
    tracker->xytracker()->set_location(last_pos[0], last_pos[1]);
  }

  // If we are optimizing in Z, then do it here.
  if (g_opt_z) {
    double  z = 0;
    tracker->ztracker()->locate_close_fit_in_depth(*g_image, g_colorIndex, tracker->xytracker()->get_x(), tracker->xytracker()->get_y(), z);
    tracker->ztracker()->optimize(*g_image, g_colorIndex, tracker->xytracker()->get_x(), tracker->xytracker()->get_y(), z);
  }
}

// The logging on the VRPN connection is done by a separate thread so that
// the network buffer won't get filled up while writing video data, causing
// the porgram to hang while trying to pack more.  The CSV file writing is
// handled in the main thread, so don't be confused by that.
//   This thread watches the logging file name and starts up a new logging
// connection when it has a new, non-empty value.

void logging_thread_function(void *)
{
  printf("Starting VRPN logging thread\n");
  while (!g_quit) {
    //------------------------------------------------------------
    // Watch the log file name and see if it becomes non-empty.
    // Open a new connection and ask it to log, then open a new
    // tracker and connect it to this connection.
    const char *newvalue;
    do {
      vrpn_SleepMsecs(10);
      newvalue = g_logfilename;
    } while ((strlen(newvalue) == 0) && !g_quit);
    if (g_quit) { break; }

    // Make sure that the file does not exist by deleting it if it does.
    // The Tcl code had a dialog box that asked the user if they wanted
    // to overwrite, so this is "safe."
    FILE *in_the_way;
    if ( (in_the_way = fopen(newvalue, "r")) != NULL) {
      fclose(in_the_way);
      int err;
      if ( (err=remove(newvalue)) != 0) {
	fprintf(stderr,"Error: could not delete existing logfile %s\n", newvalue);
	perror("   Reason");
	cleanup();
	exit(-1);
      }
    }

    g_client_connection = vrpn_get_connection_by_name("Spot@localhost", newvalue);
    g_client_tracker = new vrpn_Tracker_Remote("Spot@localhost");
    g_client_imager = new vrpn_Imager_Remote("TestImage@localhost");

    //------------------------------------------------------------
    // Let the logging connection do its thing, sleeping only briefly
    // to avoid eating the processor.  If the logging file name changes,
    // then we close the existing log file and go back to waiting for a
    // non-empty name in the next iteration.
    char  *oldvalue = new char[strlen(newvalue) + 1];
    if (oldvalue == NULL) {
      fprintf(stderr,"logging_thread_function(): Out of memory\n");
      break;
    }
    strncpy(oldvalue, newvalue, strlen(newvalue));
    do {
      g_client_tracker->mainloop();
      g_client_imager->mainloop();
      g_client_connection->mainloop();
      g_client_connection->save_log_so_far();
      vrpn_SleepMsecs(1);
      newvalue = g_logfilename;
    } while (strcmp(newvalue, oldvalue) == 0);

    //------------------------------------------------------------
    // Delete and make NULL the client tracker and connection object.
    // Then free up the space from the oldvalue.  After that, go back
    // and watch for the name to become non-empty so we open a new
    // log file.
    if (g_client_tracker) { delete g_client_tracker; g_client_tracker = NULL; };
    if (g_client_imager) { delete g_client_imager; g_client_imager = NULL; };
    if (g_client_connection) { g_client_connection->removeReference(); g_client_connection = NULL; };
    delete [] oldvalue;
  }
  printf("Exiting VRPN logging thread\n");
}

// The tracking is now done by separate threads from the main program.  This function is
// what is called for each of these threads.  Semaphores are used to communicate with
// the main program thread and determine when each sub-thread will run.  This is basically
// a big wrapper for the optimize_tracker() function, which is called by each thread on
// an appropriate tracker on the current frame as needed.
void tracking_thread_function(void *pvThreadData)
{
  struct ThreadData  *td = (struct ThreadData *)pvThreadData;
  ThreadUserData *tud = static_cast<ThreadUserData *>(td->pvUD);

  while (1) {
    // Grab the semaphore for the thread data and then set the "ready" flag.
    // This lets the application know that we're ready to do a tracking
    // optimization.
    td->ps->p();
    tud->d_ready = true;
    td->ps->v();

    // Report that there is another free tracker using the global "number of active trackers"
    // semaphore, which is used by the application to keep from having to
    // busy-wait on the tracking threads to see when one is ready for a new task.
    g_ready_tracker_semaphore->v();
    
    // Grab the run semaphore from the user data (which is accessed without
    // grabbing the lock on the user data) and then run the associated tracking
    // job.
    tud->d_run_semaphore->p();
    if (tud->d_tracker == NULL) {
      fprintf(stderr,"tracking_thread_function(): Called with NULL tracker pointer!\n");
      dirtyexit();
    }
    optimize_tracker(tud->d_tracker);
  }
}


// Helper function for find_more_trackers.
// Computes a local SMD measure (cross) at the location (x,y) with
//  the specified radius.
double localSMD(int x, int y, int radius)
{
	double Ia, Ib;
	double xSMD = 0;
	int n = 0;
	for (int lx = x - radius; lx < x + radius; ++lx)
	{
		g_image->read_pixel(lx, y, Ia, g_colorIndex);
		g_image->read_pixel(lx + 1, y, Ib, g_colorIndex);
		xSMD += fabs(Ia - Ib);
		++n;
	}
	xSMD = xSMD / (double)n;

	double ySMD = 0;
	n = 0;
	for (int ly = y - radius; ly < y + radius; ++ly)
	{
		g_image->read_pixel(x, ly, Ia, g_colorIndex);
		g_image->read_pixel(x, ly + 1, Ib, g_colorIndex);
		ySMD += fabs(Ia - Ib);
		++n;
	}
	ySMD = ySMD / (double)n;

	return (xSMD + ySMD);
}


// Helper function for the next routine; it fills in the region of the
// image around the specified tracer with the specified value.  It fills
// the image in to twice the specified radius, to keep spots from overlapping.
void fill_around_tracker_with_value(double_image &im, spot_tracker_XY *t, double v)
{
  int i,j;
  int r = 2 * t->get_radius();
  int x = t->get_x();
  int y = t->get_y();
  for (i = -r; i <= r; i++) {
    for (j = -r; j <= r; j++) {
      if (i*i+j*j <= r*r) {
        im.write_pixel_nocheck(x+i, y+j, v);
      }
    }
  }
}

// Find the specified number of additional trackers, which should be placed
// at the highest-response locations in the image that is not within one
// tracker radius of an existing tracker or within one tracker radius of the
// border.  Create new trackers at those locations.  Returns true if it was
// able to find them, false if not (or error).
bool find_more_trackers(unsigned how_many_more)
{
	// make sure we only try to auto-find once per new frame of video
        if (!g_gotNewFrame) {
		return true;
        } else {
		g_gotNewFrame = false;
        }

	// empty out our candidate vectors...
	vertCandidates.clear();
	horiCandidates.clear();

  int i, radius;
  int minx, maxx, miny, maxy;
  g_image->read_range(minx, maxx, miny, maxy);

  int x, y;

  // We'll do a simple gaussian filter on our image before looking for beads.
  // For this we'll use OpenCV.

	CvSize imgSize;
	imgSize.width = maxx - minx + 1;
	imgSize.height = maxy - miny + 1;
	IplImage* src = cvCreateImage(imgSize, 8, 1);

	// first, find the max pixel value so we can scale our double intensities
	//  effectively into our 0-255 single byte values
	double maxi = 0;
	double curi;
	for (y = miny; y <= maxy; ++y) {
		for (x = minx; x <= maxx; ++x) {
			curi = g_image->read_pixel_nocheck(x, y);
                        if (curi > maxi) {
				maxi = curi;
                        }
		}
	}

	// Write the current image values into our temporary IplImage
	for (y = miny; y <= maxy; ++y) {
		for (x = minx; x <= maxx; ++x) {
			curi = g_image->read_pixel_nocheck(x, y);
			((uchar*)(src->imageData + src->widthStep*(y-miny)))[x - minx] = (curi / maxi) * 255;
		}
	}

	IplImage* img = cvCreateImage(cvGetSize(src), src->depth, src->nChannels);

	cvSmooth(src, img, CV_GAUSSIAN, 9, 9, 5);

	/* this was to check if we were even doing anything right!
	cvNamedWindow("Image:", 1);
	cvShowImage("Image:", img);
	cvWaitKey();
	cvDestroyWindow("Image:");
	//*/

	// first, we calculate horizontal and vertical SMDs on the global image
	double SMD = 0;
	double Ia, Ib;

	double sum = 0, max = -1, min = -1;

	// vertical SMDs
	double* vertSMDs = new double[maxx - minx + 1];
	for (x = minx; x <= maxx; ++x) {
		SMD = 0;
		// calcualte one SMD
		for (y = miny + 1; y <= maxy; ++y)
		{
			Ia = ((uchar*)(img->imageData + img->widthStep*y))[x];
			Ib = ((uchar*)(img->imageData + img->widthStep*(y-1)))[x];
			SMD += fabs(Ia - Ib);
		}
		// normalize by dividing by the number of pairwise computations
		SMD = SMD / (float)(maxy - miny);
		vertSMDs[x] = SMD;

		if (max == -1)
			max = SMD;
		if (min == -1)
			min = SMD;
		
		if (SMD > max)
			max = SMD;
		if (SMD < min)
			min = SMD;		

		sum += SMD;
	}

	double vertAvg = sum / ((float)maxx - minx + 1);
	//printf("min = %f, max = %f, vertAvg = %f\n", min, max, vertAvg);
	double vertThresh = 0;
	//printf("using a vertThresh = %f\n", vertThresh);

	sum = 0;
	max = -1;
	min = -1;

	// horizontal SMDs
	double* horiSMDs = new double[maxy - miny + 1];
	for (y = miny; y <= maxy; ++y)
	{
		SMD = 0;
		// calcualte one SMD
		for (x = minx + 1; x <= maxx; ++x)
		{
			Ia = ((uchar*)(img->imageData + img->widthStep*y))[x];
			Ib = ((uchar*)(img->imageData + img->widthStep*y))[x-1];
			SMD += fabs(Ia - Ib);
		}
		// normalize by dividing by the number of pairwise computations
		SMD = SMD / (float)(maxx - minx);
		horiSMDs[y] = SMD;

		if (max == -1)
			max = SMD;
		if (min == -1)
			min = SMD;
		
		if (SMD > max)
			max = SMD;
		if (SMD < min)
			min = SMD;		

		sum += SMD;
	}

	double horiAvg = sum / ((float)maxx - minx + 1);
	//printf("min = %f, max = %f, horiAvg = %f\n", min, max, horiAvg);
	double horiThresh = 0;
	//printf("using a horiThresh = %f\n", horiThresh);

	// now we need to pick out the local maxes in our SMDs as candidate bead positions

	int windowRadius = g_slidingWindowRadius;
	double curMax = 0;

	// pick out local maxes on our vertical SMDs
	for (x = minx + windowRadius; x <= maxx - windowRadius; ++x) {
		curMax = 0;
		// check for the max SMD value within our window size that's > thresh
		for (i = x - windowRadius; i < x + windowRadius; ++i) {
			if (vertSMDs[i] > curMax && vertSMDs[i] > vertThresh) {
				curMax = vertSMDs[i];
			}
		}
		// if we were the max SMD value in our window, then we're a candidate!
		if (curMax == vertSMDs[x] && curMax != 0) {
			vertCandidates.push_back(x);
			x = x + (windowRadius - 1);
		}
	}

	// pick out local maxes on our horizontal SMDs
	for (y = miny + windowRadius; y <= maxy - windowRadius; ++y) {
		curMax = 0;
		// check for the max SMD value within our window size that's > thresh
		for (i = y - windowRadius; i < y + windowRadius; ++i) {
			if (horiSMDs[i] > curMax && horiSMDs[i] > horiThresh) {
				curMax = horiSMDs[i];
			}
		}
		// if we were the max SMD value in our window, then we're a candidate!
		if (curMax == horiSMDs[y] && curMax != 0) {
			horiCandidates.push_back(y);
			y = y + (windowRadius - 1);
		}
	}

	// Calculate all candidate spot locations (intersections of horizontal and
	//  vertical candidate scan lines).
	vector<double> candidateSpotsX;
	vector<double> candidateSpotsY;
	vector<double> candidateSpotsSMD;

	double tooClose = 5;
	double curX, curY;
	bool safe = false;
	//Spot_Information* si;
	list <Spot_Information *>::iterator loop;
	int cx, cy;
	spot_tracker_XY* curTracker;
	for (y = 0; y < static_cast<int>(horiCandidates.size()); ++y) {
		cy = horiCandidates[y];
		for (x = 0; x < static_cast<int>(vertCandidates.size()); ++x) {
			cx = vertCandidates[x];
			safe = true;

			// check to make sure we don't already have a tracker too close
			for (loop = g_trackers.begin(); loop != g_trackers.end(); loop++)  {
			//for each (Spot_Information* si in g_trackers)
			//{
				//si = (*loop);
				curTracker = (*loop)->xytracker();
				curX = curTracker->get_x();
				curY = curTracker->get_y();
				if (cx >= curX - tooClose && cx <= curX + tooClose &&
					cy >= curY - tooClose && cy <= curY + tooClose ) {
				  safe = false;
				}
			}
			if (safe) {
				candidateSpotsX.push_back(cx);
				candidateSpotsY.push_back(cy);
			}
		}
	}

	//printf("%i valid candidateSpots to check\n", candidateSpotsX.size());

	// now do local SMDs at candidate spots to determine if they're actually spots.
	double maxSMD = 0;
	double minSMD = 1e50;
	double avgSMD = 0;
	SMD = 0;
	radius = g_Radius;
	for (i = 0; i < static_cast<int>(candidateSpotsX.size()); ++i) {
		x = candidateSpotsX[i];
		y = candidateSpotsY[i];
		SMD = localSMD(x, y, radius);
		avgSMD += SMD;
                if (SMD > maxSMD) {
			maxSMD = SMD;
                }
                if (SMD < minSMD) {
			minSMD = SMD;
                }
		candidateSpotsSMD.push_back(SMD);
	}
	avgSMD /= candidateSpotsX.size();

	//printf("minSMD = %f, maxSMD = %f, avgSMD = %f\n", minSMD, maxSMD, avgSMD);
	double SMDthresh = avgSMD * g_candidateSpotThreshold;

	list<Spot_Information*> potentialTrackers;

	int newTrackers = 0;
	for (i = 0; i < static_cast<int>(candidateSpotsSMD.size()); ++i) {
		if (candidateSpotsSMD[i] > SMDthresh) {
			// add a new potential tracker!
			++newTrackers;

			// last argument = true tells Spot_Information that this isn't an official, logged tracker
			potentialTrackers.push_back(new Spot_Information(create_appropriate_xytracker(candidateSpotsX[i],candidateSpotsY[i],g_Radius),create_appropriate_ztracker(), true));

		}
	}
	//printf("%i potential new trackers added!\n", newTrackers);

/*
	// let's take a look at the distribution of our local SMDs
	std::sort(candidateSpotsSMD.begin(), candidateSpotsSMD.end());

	for each (double localSMDval in candidateSpotsSMD)
	{
		printf("%f\n", localSMDval);
	}

	printf("candidateSpotsSMD.size() = %i\n", candidateSpotsSMD.size());

	CvSize smdImgSize;
	smdImgSize.width = 600;
	smdImgSize.height = 100;
	IplImage* plot = cvCreateImage(smdImgSize, 8, 1);

	for (y = 0; y <= 100; ++y)
	{
		for (x = 0; x <= 600; ++x)
		{
			((uchar*)(plot->imageData + plot->widthStep*(y)))[x] = 0;
		}
	}

	// generate our crude plot in the image we just made
	int plotX = 0;
	double plotYscale = 100.0f / maxSMD;
	int plotY;
	for each (double localSMDval in candidateSpotsSMD)
	{
		plotY = plotYscale * localSMDval;
		((uchar*)(plot->imageData + plot->widthStep*plotY))[plotX] = 255;
		plotX = plotX + 1;
	}

	cvNamedWindow("SMD_dist:", 1);
	cvShowImage("SMD_dist:", plot);
	cvWaitKey();
	cvDestroyWindow("SMD_dist:");

	cvReleaseImage( &plot );
*/

	int numlost = 0;
	int numnotlost = 0;

	// check to see which candidate spots aren't immediately lost
	for (loop = potentialTrackers.begin(); loop != potentialTrackers.end(); loop++) {
		// if our candidate tracker isn't lost, then we add it to our list of real trackers
		optimize_tracker((*loop));
		if (!(*loop)->lost() && (*loop)->xytracker()->get_fitness() < 0) {
			++numnotlost;
                        g_trackers.push_back(new Spot_Information(create_appropriate_xytracker((*loop)->xytracker()->get_x(),(*loop)->xytracker()->get_y(),g_Radius),create_appropriate_ztracker()));
                        g_active_tracker = g_trackers.back();
                        if (g_active_tracker->ztracker()) { g_active_tracker->ztracker()->set_depth_accuracy(0.25); }
                } else {
			++numlost;
                }
	}

	//printf("%i candidates were lost after optimizing\n", numlost);
	//printf("%i candidates were not lost.\n", numnotlost);
	
	// clean up candidate spots memory
	potentialTrackers.remove_if( deleteAll );

	// clear up our SMD memory in reverse order from allocation to make it
        // easier for the memory manager
	delete [] horiSMDs;
	delete [] vertSMDs;

	// clean up our image filtering memory in reverse order to make it
        // easier for the memory manager
	cvReleaseImage( &img );
	cvReleaseImage( &src );

  return true;
}


void myIdleFunc(void)
{
  list<Spot_Information *>::iterator loop;

  //------------------------------------------------------------
  // This must be done in any Tcl app, to allow Tcl/Tk to handle
  // events.  This must happen at the beginning of the idle function
  // so that the camera-capture and video-display routines are
  // using the same values for the global parameters.

  while (Tk_DoOneEvent(TK_DONT_WAIT)) {};

  //------------------------------------------------------------
  // This is called once every time through the main loop.  It
  // pushes changes in the C variables over to Tcl.

  if (Tclvar_mainloop()) {
    fprintf(stderr,"Tclvar Mainloop failed\n");
  }

  //------------------------------------------------------------
  // If we have a video object, let the video controls operate
  // on it.
  if (g_video) {
    static  int	last_play = 0;

    // If the user has pressed step, then run the video for a
    // single step and pause it.
    if (*g_step) {
      g_video->single_step();
      *g_play = 0;
      *g_step = 0;
    }

    // If the user has pressed play, start the video playing
    if (!last_play && *g_play) {
      g_video->play();
      *g_rewind = 0;
    }

    // If the user has cleared play, then pause the video
    if (last_play && !(*g_play)) {
      g_video->pause();
    }
    last_play = *g_play;

    // If the user has pressed rewind, go the the beginning of
    // the stream and then pause (by clearing play).  This has
    // to come after the checking for stop play above so that the
    // video doesn't get paused.  Also, reset the frame count
    // when we rewind.  Also, turn off logging when we rewind
    // (if it was on).  Also clear any logged traces.
    if (*g_rewind) {
      *g_play = 0;
      *g_rewind = 0;
      g_logging = 0;
      g_logfilename = "";
      logfilename_changed("", NULL);
      g_logged_traces.clear();

      // Do the rewind and setting after clearing the logging, so that
      // the last logged frame has the correct index.
      g_video->rewind();
      g_frame_number = -1;
    }
  }

  // If they just asked for the full area, reset to that and
  // then turn off the check-box.  Also turn off small-area.
  if (g_full_area) {
    g_small_area = 0;
    *g_minX = 0;
    *g_minY = 0;
    *g_maxX = g_image->get_num_columns() - 1;
    *g_maxY = g_image->get_num_rows() - 1;
    g_full_area = 0;
  }

  // If we are asking for a small region to be used around the tracked dot,
  // set the borders to be around the set of trackers.
  // This will be a min/max over all of the
  // bounding boxes.  Do not do this if the tracker is lost, mainly
  // because it causes the program to exit when a wanted pixel is not
  // found but also because we don't want to follow the lost tracker.
  if (g_opt && g_small_area && g_active_tracker && !g_tracker_is_lost) {
    // Initialize it with values from the active tracker, which are stored
    // in the global variables.
    (*g_minX) = g_X - 4*g_Radius;
    (*g_minY) = flip_y(g_Y) - 4*g_Radius;
    (*g_maxX) = g_X + 4*g_Radius;
    (*g_maxY) = flip_y(g_Y) + 4*g_Radius;

    // Check them against all of the open trackers and push them to bound all
    // of them.

    list<Spot_Information *>::iterator  loop;
    for (loop = g_trackers.begin(); loop != g_trackers.end(); loop++) {
      double x = (*loop)->xytracker()->get_x();
      double y = ((*loop)->xytracker()->get_y());
      double fourRad = 4 * (*loop)->xytracker()->get_radius();
      if (g_rod) {
	// Horrible hack to make this work with rod type
	fourRad = 2 * static_cast<rod3_spot_tracker_interp*>(g_active_tracker->xytracker())->get_length();
      }
      if (*g_minX > x - fourRad) { *g_minX = x - fourRad; }
      if (*g_maxX < x + fourRad) { *g_maxX = x + fourRad; }
      if (*g_minY > y - fourRad) { *g_minY = y - fourRad; }
      if (*g_maxY < y + fourRad) { *g_maxY = y + fourRad; }
    }

    // Make sure not to push them off the screen
    if ((*g_minX) < 0) { (*g_minX) = 0; }
    if ((*g_minY) < 0) { (*g_minY) = 0; }
    if ((*g_maxX) >= g_image->get_num_columns()) { (*g_maxX) = g_image->get_num_columns() - 1; }
    if ((*g_maxY) >= g_image->get_num_rows()) { (*g_maxY) = g_image->get_num_rows() - 1; }
  }

  // If we're doing a search for local maximum during optimization, then make a
  // copy of the previous image before reading a new one.

  if ((double)(g_search_radius) > 0) {
    if (g_last_image == NULL) {
      g_last_image = new copy_of_image(*g_image);
    } else {
      *g_last_image = *g_image;
    }
  }

  // Read an image from the camera into memory, within a structure that
  // is wrapped by an image_wrapper object that the tracker can use.
  // Tell Glut that it can display an image.
  // We ignore the error return if we're doing a video file because
  // this can happen due to timeouts when we're paused or at the
  // end of a file.

  // If we have a lost tracker, then do not attempt to read more frames
  // from the camera.  This is intended to prevent us from going
  // past the frame before the problem is corrected, either by
  // moving the tracker back on or by adjusting the sensitivity.
  // EXCEPTION: If we are using "Hover when lost", then the trackers
  // should continue to move from frame to frame but just not save
  // their position data to the file while they are lost.

  if (g_tracker_is_lost && (g_lostBehavior != LOST_HOVER)) {
    g_video_valid = false;
  } else {
    if (!g_camera->read_image_to_memory((int)(*g_minX),(int)(*g_maxX), (int)(*g_minY),(int)(*g_maxY), g_exposure)) {
      if (!g_video) {
	fprintf(stderr, "Can't read image to memory!\n");
	cleanup();
	exit(-1);
      } else {
	// We timed out; either paused or at the end.  Don't log in this case.
	g_video_valid = false;

	// If we are playing, then say that we've finished the run and
	// stop playing.
	if (((int)(*g_play)) != 0) {
	  // If we are supposed to quit at the end of the video, do so.
	  if (g_quit_at_end_of_video) {
	    printf("Exiting at the end of the video\n");
	    g_quit = 1;
	  } else {
#ifdef	_WIN32
	    if (!PlaySound("end_of_video.wav", NULL, SND_FILENAME | SND_ASYNC)) {
	      fprintf(stderr,"Cannot play sound %s\n", "end_of_video.wav");
	    }
#endif
	  }
	  *g_play = 0;
	}
      }
    } else {
      // Got a valid video frame; can log it.  Add to the frame number.
      g_video_valid = true;
	  g_gotNewFrame = true;
      g_frame_number++;
    }
  }
  // Point the image to use at the camera's image.
  g_image = g_camera;

  // XXX Make a double-precision copy of the camera's image and point
  // our image at that.  This should remove a lot of conversion math that
  // has to happen for the tracking.

  //XXX_why_does_clipping_cause_badness_and_shifting
  //XXX It also causes crashing when used with the Cooke camera driver!
  //XXX It will also remove the ability to do OpenGL-accelerated rendering.
/*XXX
  printf("XXX copying image\n");
  static copy_of_image double_image(*g_camera);
  double_image = *g_camera;
  g_image = &double_image;
*/
  // If we're doing background subtraction, then we set things up to accumulate
  // an average image and do a subtraction between the current image and the
  // background image, and then we point the g_image at this computed image.
  if (g_background_subtract) {

    // Create the mean image if it does not exist.  When we create it,
    // we include the current image no matter what.
    if (!g_mean_image) {
      g_mean_image = new mean_image(*g_image);
      (*g_mean_image) += *g_image;
      g_background_count = 0;
    } else {
      // Otherwise, only add in a new image when we get a new frame.
      if (g_video_valid) {

        // Actually, only average over at most 50 frames and then just re-use the image
        if (g_background_count < 50) {
          (*g_mean_image) += *g_image;
          g_background_count++;
        }
      }
    }

    // Get rid of the old calculated image, if we have one
    if (g_calculated_image) { delete g_calculated_image; g_calculated_image = NULL; }
    // Calculate the new image and then point the image to display at it.
    // The offset is determined by the maximum value that can be displayed in an
    // image with half as many values as the present image.
    double offset = (1 << (((unsigned)(g_camera_bit_depth))-1) ) - 1;
    g_calculated_image = new subtracted_image(*g_image, *g_mean_image, offset);
    if (g_calculated_image == NULL) {
      fprintf(stderr, "Out of memory when calculating image\n");
      cleanup();
      exit(-1);
    }

    g_image = g_calculated_image;
  }

  // We're ready to display an image.
  g_ready_to_display = true;

  if (g_active_tracker) { 
    g_active_tracker->xytracker()->set_radius(g_Radius);
    if (g_rod) {
      // Horrible hack to enable adjustment of a parameter that only exists on
      // the rod tracker
      static_cast<rod3_spot_tracker_interp*>(g_active_tracker->xytracker())->set_length(g_length);
      static_cast<rod3_spot_tracker_interp*>(g_active_tracker->xytracker())->set_orientation(g_orientation);
    }
  }

  // Update the VRPN tracker position for each tracker and report it
  // using the same time value for each.  Don't do the update if we
  // don't currently have a valid video frame.  Sensors are indexed
  // by their position in the list.  Putting this here before the
  // optimize loop means that we're reporting the PREVIOUS values of
  // the optimizer (the ones for the frame just ending) rather than
  // the CURRENT values.  This means we'll not write a value for the
  // last frame in a video.  Before, we weren't writing a value for the first
  // frame.  The way it is now, the user can single-step through a
  // file and move the trackers around to line up on each frame before
  // their values are saved.  To fix this, we put in a log save at
  // program exit and logfile name change.
  // Remember to invert the Y value so that it logs based on the
  // upper-left corner of the image.
  // We log even if we aren't optimizing, because the user may be moving
  // the dots around by hand.
  // Don't log if we just stepped to the zeroeth frame (this can happen
  // if we start logging on the command line).
  if (g_vrpn_tracker && g_video_valid && (g_frame_number > 0)) {
    if (!save_log_frame(g_frame_number-1)) {
	fprintf(stderr,"Could not save data to log file\n");
	cleanup();
	exit(-1);
    }
  }

  // If we are trying to maintain a minimum number of beads, and we don't have enough,
  // then call the auto-find routine to add more.  We do this before the code that
  // checks for lost beads because it may be the case that the new "beads" we find
  // are not high-enough quality to even survive one frame; we don't want to log
  // them if they are not real.  Also do this before optimization step, so that we
  // can leave the trackers not completely optimized when creating them (just
  // finding the closest pixel value).
  if (g_findThisManyBeads > g_trackers.size()) {
    find_more_trackers(g_findThisManyBeads - g_trackers.size());
  }

  // Nobody is known to be lost yet...
  g_tracker_is_lost = false;

  // Optimize all trackers and see if they are lost, if we have trackers.
  if (g_opt && g_active_tracker) {

    int kymocount = 0;
    for (loop = g_trackers.begin(); loop != g_trackers.end(); loop++) {

      // If we're single-threaded, then call the tracker optimization loop
      // directly.  If we're multi-threaded, then wait for a ready tracking
      // thread and use it to do the tracking.
      if (g_tracking_threads.size() == 0) {
        optimize_tracker(*loop);
      } else {
        // Wait until at least one tracker has entered the ready state
        // and indicated this using the active tracking semaphore.
        g_ready_tracker_semaphore->p();

        // Find a ready tracker (there may be more than one, just pick the
        // first).
        unsigned i;
        for (i = 0; i < g_tracking_threads.size(); i++) {
          if (g_tracking_threads[i]->d_thread_user_data->d_ready) {
            break;
          }
        }
        if (i >= g_tracking_threads.size()) {
          fprintf(stderr,"The active tracking semaphore indicated a free tracker but none found in list\n");
          exit(-1);
        }

        // Fill in the tracker pointer in the user data and then use the run
        // semaphore to cause the tracking thread to run.  Tell ourselves that
        // the tracker is busy, so we don't try to re-use it until it is done.
        g_tracking_threads[i]->d_thread_user_data->d_tracker = *loop;
        g_tracking_threads[i]->d_thread_user_data->d_ready = false;
        g_tracking_threads[i]->d_thread_user_data->d_run_semaphore->v();
      }

      // If we are running a kymograph, then we don't optimize any trackers
      // past the first two because they are only there as markers in the
      // image for cell boundaries.
      if (g_kymograph) {
	if (++kymocount == 2) {
	  break;
	}
      }
    }

    // If we're using multiple tracking threads, then we need to wait until all
    // threads have finished tracking before going on, to avoid getting ahead of
    // ourselves while one or more of them are finishing.  To avoid busy-waiting,
    // we do this by grabbing as many extra entries in the active-tracking semaphore
    // as there are threads and then freeing them up once we have them all.
    // If we're not using threads, then there are no entries in the g_tracking_threads vector.
    unsigned i;
    for (i = 0; i < g_tracking_threads.size(); i++) {
      g_ready_tracker_semaphore->p();
    }
    for (i = 0; i < g_tracking_threads.size(); i++) {
      g_ready_tracker_semaphore->v();
    }

    // Now that we are back in a single thread, run through the list of trackers
    // and see if any of them are lost.  If so, what happens depends on whether
    // the auto-deletion of lost trackers is enabled.  If so, then delete each
    // lost tracker.  If not, then point at one of the lost trackers and
    // set the global lost-tracker flag;

    // We do additional lost-tracker checking here, marking trackers that are too near
    // other trackers.  We do this here to avoid race conditions that would happen in
    // the threads as each tracker moved itself.
    if (g_trackerDeadZone > 0) {   
      for (loop = g_trackers.begin(); loop != g_trackers.end(); loop++) {
        double x = (*loop)->xytracker()->get_x();
        double y = (*loop)->xytracker()->get_y();

        list<Spot_Information *>::iterator loop2;
        double zone2 = g_trackerDeadZone * g_trackerDeadZone;
        for (loop2 = g_trackers.begin(); loop2 != loop; loop2++) {
          double x2 = (*loop2)->xytracker()->get_x();
          double y2 = (*loop2)->xytracker()->get_y();
          double dist2 = ( (x-x2)*(x-x2) + (y-y2)*(y-y2) );
          if (dist2 < zone2) {
            (*loop)->lost(true);
          }
        }
      }
    }

    // If this tracker is lost, and we have the "autodelete lost tracker" feature
    // turned on, then delete this tracker and say that we are no longer lost.
    loop = g_trackers.begin();
    while (loop != g_trackers.end()) {
      if ((*loop)->lost()) {
        // Set me to be the active tracker.
        g_active_tracker = *loop;
        if (g_lostBehavior == LOST_DELETE) {
          // Delete this tracker from the list (it was made active above)
          delete_active_xytracker();

          // Set the loop back to the beginning of the list of trackers
          // so we don't miss a lost one by incrementing the loop counter.
          loop = g_trackers.begin();

        } else {
          // Make me the active tracker (above) and set the global "lost tracker"
          // flag.
          // Then go to the next tracker in the list.
          g_tracker_is_lost = true;
          loop++;
        }
      } else {
        // Not lost, go to the next tracker in the list.
        loop++;
      }
    }

    // Pause and say that we are lost unless we're in "Hover when lost" mode
    if (g_tracker_is_lost && (g_lostBehavior != LOST_HOVER)) {
      // If we have a playable video and the video is not paused, then say we're lost and pause it.
      if (g_play != NULL) {
        if (*g_play) {
          fprintf(stderr, "Lost in frame %d\n", (int)(float)(g_frame_number));
          if (g_video) { g_video->pause(); }
          *g_play = 0;
#ifdef	_WIN32
	  if (!PlaySound("lost.wav", NULL, SND_FILENAME | SND_ASYNC)) {
	    fprintf(stderr,"Cannot play sound %s\n", "lost.wav");
	  }
#endif
        }
      }

      // XXX Do not attempt to pause live video, but do tell that we're lost -- only tell once.
    }

    g_last_optimized_frame_number = g_frame_number;
  }

  // Update the kymograph if it is active and if we have a new video frame.
  // This must be done AFTER the optimization because we need to find the right
  // trace through the NEW image.  If we lost tracking, too bad...
  if (g_kymograph && g_video_valid) {
    // Make sure we have at least two trackers, which will define the
    // frame of reference for the kymograph.  Also make sure that we are
    // not out of display room in the kymograph.
    if ( (g_trackers.size() >= 2) && (g_kymograph_filled+1 < g_kymograph_height) ) {

      // Find the two tracked points to use.
      list<Spot_Information *>::iterator  loop = g_trackers.begin();
      double x0 = (*loop)->xytracker()->get_x();
      double y0 = (*loop)->xytracker()->get_y();
      loop++;
      double x1 = (*loop)->xytracker()->get_x();
      double y1 = (*loop)->xytracker()->get_y();

      // Find the center of the two (origin of kymograph coordinates) and the unit vector (dx,dy)
      // going towards the second from the first.
      double xcenter = (x0 + x1) / 2;
      double ycenter = (y0 + y1) / 2;
      double dist = sqrt( (x1-x0)*(x1-x0) + (y1-y0)*(y1-y0) );
      if (dist == 0) { dist = 1; }  //< Avoid divide-by-zero below.
      double dx = (x1 - x0) / dist;
      double dy = (y1 - y0) / dist;

      // March along in the image from negative half-width to half-width
      // and fill in the samples image values at these locations.
      double step;
      double double_pix;
      // Figure out how many bits we need to shift to the right.
      // This depends on how many bits the camera has above zero minus
      // the number of bits we want to shift to brighten the image.
      // If this number is negative, clamp to zero.
      int shift_due_to_camera = g_camera_bit_depth - 8;
      int total_shift = shift_due_to_camera - g_brighten;
      if (total_shift < 0) { total_shift = 0; }
      int shift = total_shift;
      for (step = -g_kymograph_width/2.0; step <= g_kymograph_width / 2.0; step++) {

	// Figure out where to look in the image
	double x = xcenter + step * dx;
	double y = ycenter + step * dy;

	// Figure out where to put this in the kymograph
	int kx = (int)(step + g_kymograph_width/2.0);
	int ky = g_kymograph_filled;

	// Look up the pixel in the image and put it into the kymograph if we get a reading.
	if (!g_image->read_pixel_bilerp(x, y, double_pix, g_colorIndex)) {
	  g_kymograph_image[0 + 4 * (kx + g_kymograph_width * (g_kymograph_height - 1 - ky))] = 0;
	  g_kymograph_image[1 + 4 * (kx + g_kymograph_width * (g_kymograph_height - 1 - ky))] = 0;
	  g_kymograph_image[2 + 4 * (kx + g_kymograph_width * (g_kymograph_height - 1 - ky))] = 0;
	  g_kymograph_image[3 + 4 * (kx + g_kymograph_width * (g_kymograph_height - 1 - ky))] = 255;
	} else {
	  // This assumes that the pixels are actually 8-bit values
	  // and will clip if they go above this.  It also writes pixels
	  // from the first channel into all colors of the image.  It uses
	  // RGBA so that we don't have to worry about byte-alignment problems
	  // that plagued us when using RGB pixels.
	  g_kymograph_image[0 + 4 * (kx + g_kymograph_width * (g_kymograph_height - 1 - ky))] = (vrpn_uint16)(double_pix) >> shift;
	  g_kymograph_image[1 + 4 * (kx + g_kymograph_width * (g_kymograph_height - 1 - ky))] = (vrpn_uint16)(double_pix) >> shift;
	  g_kymograph_image[2 + 4 * (kx + g_kymograph_width * (g_kymograph_height - 1 - ky))] = (vrpn_uint16)(double_pix) >> shift;
	  g_kymograph_image[3 + 4 * (kx + g_kymograph_width * (g_kymograph_height - 1 - ky))] = 255;
	}
      }

      // Figure out the coordinates of the kymograph center in the coordinate system
      // of the cell, where the third tracker (#2) is the posterior end and the fourth
      // is the anterior end.
      if (g_trackers.size() >= 4) {
	// Find the two tracked points to use.
	loop++; // Skip to the third point.
	double cx0 = (*loop)->xytracker()->get_x();
	double cy0 = (*loop)->xytracker()->get_y();
	loop++;
	double cx1 = (*loop)->xytracker()->get_x();
	double cy1 = (*loop)->xytracker()->get_y();

	// Find the center of the two (origin of cell coordinates) and the unit vector (dx,dy)
	// going towards the second from the first.
	double cxcenter = (cx0 + cx1) / 2;
	double cycenter = (cy0 + cy1) / 2;
	double cdist = sqrt( (cx1-cx0)*(cx1-cx0) + (cy1-cy0)*(cy1-cy0) );
	if (cdist == 0) { cdist = 1; }  //< Avoid divide-by-zero below.
	double cdx = (cx1 - cx0) / cdist;
	double cdy = (cy1 - cy0) / cdist;

	// Find the dot product between the spindle center and the cell center,
	// which is the value of the cell-centered ordinate.
	g_kymograph_centers[g_kymograph_filled] = 
	  (xcenter - cxcenter) * cdx + (ycenter - cycenter) * cdy;
      }

      // We've added another line to the kymograph!
      g_kymograph_filled++;
    }
  } 

  //------------------------------------------------------------
  // Make the GUI track the result for the active tracker
  if (g_active_tracker) {
    g_X = (float)g_active_tracker->xytracker()->get_x();
    g_Y = (float)flip_y(g_active_tracker->xytracker()->get_y());
    if (g_active_tracker->ztracker()) {
      g_Z = g_active_tracker->ztracker()->get_z();
    }
    g_Radius = (float)g_active_tracker->xytracker()->get_radius();
    g_Error = (float)g_active_tracker->xytracker()->get_fitness();
    if (g_rod) {
      // Horrible hack to make this work with rod type
      g_orientation = static_cast<rod3_spot_tracker_interp*>(g_active_tracker->xytracker())->get_orientation();
      g_length = static_cast<rod3_spot_tracker_interp*>(g_active_tracker->xytracker())->get_length();
    }
  }

  //------------------------------------------------------------
  // Post a redisplay so that Glut will draw the new image
  if (g_use_gui && !g_already_posted) {
    glutSetWindow(g_tracking_window);
    glutPostRedisplay();
    glutSetWindow(g_beadseye_window);
    glutPostRedisplay();
    glutSetWindow(g_landscape_window);
    glutPostRedisplay();
    glutSetWindow(g_kymograph_window);
    glutPostRedisplay();
    glutSetWindow(g_kymograph_center_window);
    glutPostRedisplay();
    g_already_posted = true;
  }

  //------------------------------------------------------------
  // Let the VRPN objects send their reports.  Log the analog first
  // because it stores the frame numbers for the following tracker
  // reports.
  if (g_vrpn_analog) { g_vrpn_analog->mainloop(); }
  if (g_vrpn_tracker) { g_vrpn_tracker->mainloop(); }
  if (g_vrpn_imager) { g_vrpn_imager->mainloop(); }
  if (g_vrpn_connection) { g_vrpn_connection->mainloop(); }

  //------------------------------------------------------------
  // Capture timing information and print out how many frames per second
  // are being tracked.

  { static struct timeval last_print_time;
    struct timeval now;
    static bool first_time = true;
    static int last_frame_number = g_frame_number;

    if (first_time) {
      gettimeofday(&last_print_time, NULL);
      first_time = false;
    } else {
      gettimeofday(&now, NULL);
      double timesecs = 0.001 * vrpn_TimevalMsecs(vrpn_TimevalDiff(now, last_print_time));
      if (timesecs >= 5) {
	double frames_per_sec = (g_frame_number - last_frame_number) / timesecs;
	last_frame_number = g_frame_number;
	printf("Tracking %lg frames per second\n", frames_per_sec);
	last_print_time = now;
      }
    }
  }

//------------------------------------------------------------
  // Time to quit?
  if (g_quit) {
    // If we have been logging, then see if we have saved the
    // current frame's image.  If not, go ahead and do it now.
    if (g_vrpn_tracker && (g_log_frame_number_last_logged != g_frame_number)) {
      if (!save_log_frame(g_frame_number)) {
	fprintf(stderr, "logfile_changed: Could not save log frame\n");
	cleanup();
	exit(-1);
      }
    }

    cleanup();
    exit(0);
  }
  
  // Sleep a little while so that we don't eat the whole CPU.
#ifdef	_WIN32
  vrpn_SleepMsecs(0);
#else
  vrpn_SleepMsecs(1);
#endif
}

// This routine finds the tracker whose coordinates are
// the nearest to those specified, makes it the active
// tracker, and moved it to the specified location.
void  activate_and_drag_nearest_tracker_to(double x, double y)
{
  // Looks for the minimum squared distance and the tracker that is
  // there.
  double  minDist2 = 1e100;
  Spot_Information *minTracker = NULL;
  double  dist2;
  list<Spot_Information *>::iterator loop;

  for (loop = g_trackers.begin(); loop != g_trackers.end(); loop++) {
    dist2 = (x - (*loop)->xytracker()->get_x())*(x - (*loop)->xytracker()->get_x()) +
      (y - (*loop)->xytracker()->get_y())*(y - (*loop)->xytracker()->get_y());
    if (dist2 < minDist2) {
      minDist2 = dist2;
      minTracker = *loop;
    }
  }
  if (minTracker == NULL) {
    fprintf(stderr, "No tracker to pick out of %d\n", g_trackers.size());
  } else {
    g_active_tracker = minTracker;
    g_active_tracker->xytracker()->set_location(x, y);
    g_X = g_active_tracker->xytracker()->get_x();
    g_Y = flip_y(g_active_tracker->xytracker()->get_y());
    if (g_active_tracker->ztracker()) {
      g_Z = g_active_tracker->ztracker()->get_z();
    }
    // Set the velocity and acceleration estimates to zero
    double zeroes[2] = { 0, 0 };
    minTracker->set_velocity(zeroes);
    minTracker->set_acceleration(zeroes);

    g_Radius = g_active_tracker->xytracker()->get_radius();
    if (g_rod) {
      // Horrible hack to make this work with rod type
      g_orientation = static_cast<rod3_spot_tracker_interp*>(g_active_tracker->xytracker())->get_orientation();
      g_length = static_cast<rod3_spot_tracker_interp*>(g_active_tracker->xytracker())->get_length();
    }
  }
}


void keyboardCallbackForGLUT(unsigned char key, int x, int y)
{
  switch (key) {
  case 'q':
  case 'Q':
    g_quit = 1;
    break;

  case 8:   // Backspace
  case 127: // Delete on Windows
    delete_active_xytracker();
  }
}

void mouseCallbackForGLUT(int button, int state, int raw_x, int raw_y)
{
  // Record where the button was pressed for use in the motion
  // callback, flipping the Y axis to make the coordinates match
  // image coordinates.
  g_mousePressX = raw_x;
  g_mousePressY = raw_y;

  // Convert from raw device coordinates to image coordinates.  This
  // means scaling by the ratio of window size to image size.  If the
  // user has not resized the window, and if the image was not too small
  // then this will be 1; otherwise it will be different in general.
  double xScale = (g_camera->get_num_columns() - 1.0) / (glutGet(GLUT_WINDOW_WIDTH) - 1.0);
  double yScale = (g_camera->get_num_rows() - 1.0) / (glutGet(GLUT_WINDOW_HEIGHT) - 1.0);
  int x = xScale * raw_x;
  int y = flip_y( yScale * raw_y);

  switch(button) {
    // The left button will create a new tracker and let the
    // user specify its radius if they move far enough away
    // from the pick point (it starts with a default of the same
    // as the current active tracker).
    // The new tracker becomes the active tracker.
    case GLUT_LEFT_BUTTON:
      if (state == GLUT_DOWN) {

	g_whichDragAction = 1;
	g_trackers.push_back(new Spot_Information(create_appropriate_xytracker(x,y,g_Radius),create_appropriate_ztracker()));
	g_active_tracker = g_trackers.back();
	if (g_active_tracker->ztracker()) { g_active_tracker->ztracker()->set_depth_accuracy(0.25); }

	// Move the pointer to where the user clicked.
	// Invert Y to match the coordinate systems.
	g_X = x;
	g_Y = flip_y(y);
	if (g_active_tracker->ztracker()) {
	  g_Z = g_active_tracker->ztracker()->get_z();
	}
      } else {
	// Nothing to do at release.
      }
      break;

    case GLUT_MIDDLE_BUTTON:
      if (state == GLUT_DOWN) {
	g_whichDragAction = 0;
      }
      break;

    // The right button will pull the closest existing tracker
    // to the location where the mouse button was pressed, and
    // then let the user pull it around the screen
    case GLUT_RIGHT_BUTTON:
      if (state == GLUT_DOWN) {
	g_whichDragAction = 2;
	activate_and_drag_nearest_tracker_to(x,y);
      }
      break;
  }
}

void motionCallbackForGLUT(int raw_x, int raw_y)
{
  // Convert from raw device coordinates to image coordinates.  This
  // means scaling by the ratio of window size to image size.  If the
  // user has not resized the window, and if the image was not too small
  // then this will be 1; otherwise it will be different in general.
  double xScale = (g_camera->get_num_columns() - 1.0) / (glutGet(GLUT_WINDOW_WIDTH) - 1.0);
  double yScale = (g_camera->get_num_rows() - 1.0) / (glutGet(GLUT_WINDOW_HEIGHT) - 1.0);
  int x = xScale * raw_x;
  int y = flip_y( yScale * raw_y);
  int pressX = xScale * g_mousePressX;
  int pressY = flip_y( yScale * g_mousePressY );

  switch (g_whichDragAction) {

  case 0: //< Do nothing on drag.
    break;

  // Set the radius of the active spot if we've moved far enough
  // from where we pressed the button.
  case 1:
    {
      // Clip the motion to stay within the window boundaries.
      if (x < 0) { x = 0; };
      if (y < 0) { y = 0; };
      if (x >= (int)g_image->get_num_columns()) { x = g_image->get_num_columns() - 1; }
      if (y >= (int)g_image->get_num_rows()) { y = g_image->get_num_rows() - 1; };

      // Set the radius based on how far the user has moved from click
      double radius = sqrt( static_cast<double>( (x - pressX) * (x - pressX) + (y - pressY) * (y - pressY) ) );
      if (radius >= 3) {
	g_active_tracker->xytracker()->set_radius(radius);
	g_Radius = radius;
      }
    }
    break;

  // Pull the closest existing tracker
  // to the location where the mouse button was pressed, and
  // keep pulling it around if the mouse is moved while this
  // button is held down.
  case 2:
    activate_and_drag_nearest_tracker_to(x,y);
    break;

  default:
    fprintf(stderr,"Internal Error: Unknown drag action (%d)\n", g_whichDragAction);
  }
  return;
}


//--------------------------------------------------------------------------
// Tcl callback routines.

// If the logfilename becomes non-empty, then open a logging VRPN
// connection to the server, so that we can track where the spot is
// going.  If the file name becomes empty again, then delete the
// connection so that it will store the logged data.  If the name
// changes, and was not empty before, then close the existing
// connection and start a new one.
// We use the "newvalue" here rather than the file name because the
// file name gets truncated to the maximum TCLVAR string length.
// Also do the same for a comma-separated values file, replacing the
// .vrpn extension with .csv

#ifdef _WIN32
void  logfilename_changed(char *newvalue, void *)
#else
void  logfilename_changed(const char *newvalue, void *)
#endif
{
  // If we have been logging, then see if we have saved the
  // current frame's image.  If not, go ahead and do it now.
  if (g_vrpn_tracker && (g_log_frame_number_last_logged != g_frame_number)) {
    if (!save_log_frame(g_frame_number)) {
      fprintf(stderr, "logfile_changed: Could not save log frame\n");
      cleanup();
      exit(-1);
    }
  }

  // The logging thread will take care of closing the client tracker
  // and connection as needed when the file name changes.

  // Close the old CSV log file, if there was one.
  if (g_csv_file != NULL) {
    fclose(g_csv_file);
    g_csv_file = NULL;
  }

  // Open a new connection and .csv file, if we have a non-empty name.
  if (strlen(newvalue) > 0) {

    // Open the CSV file and put the titles on.
    // Make sure that the file does not exist by deleting it if it does.
    // The Tcl code had a dialog box that asked the user if they wanted
    // to overwrite, so this is "safe."
    char *csvname = new char[strlen(newvalue)+1];	// Remember the closing '\0'
    if (csvname == NULL) {
      fprintf(stderr, "Out of memory when allocating CSV file name\n");
      cleanup();
      exit(-1);
    }
    strcpy(csvname, newvalue);
    strcpy(&csvname[strlen(csvname)-5], ".csv");
    FILE *in_the_way;
    if ( (in_the_way = fopen(csvname, "r")) != NULL) {
      fclose(in_the_way);
      int err;
      if ( (err=remove(csvname)) != 0) {
	fprintf(stderr,"Error: could not delete existing logfile %s\n", (char*)(csvname));
	perror("   Reason");
	cleanup();
	exit(-1);
      }
    }
    if ( NULL == (g_csv_file = fopen(csvname, "w")) ) {
      fprintf(stderr,"Cannot open CSV file for writing: %s\n", csvname);
    } else {
      fprintf(g_csv_file, "FrameNumber,Spot ID,X,Y,Z,Radius,Orientation (if meaningful),Length (if meaningful), Fit Background (for FIONA), Gaussian Summed Value (for FIONA), Mean Background (FIONA), Summed Value (for FIONA)\n");
    }
    delete [] csvname;
  }

  // Set the offsets to use when logging.  If we are not doing relative logging,
  // then these offsets are 0,0.  If we are, then they are the current position of
  // the active tracker (if it exists).
  g_log_offset_x = g_log_offset_y = g_log_offset_z = 0;
  if (g_log_relative && g_active_tracker) {
    g_log_offset_x = g_active_tracker->xytracker()->get_x();
    g_log_offset_y = flip_y(g_active_tracker->xytracker()->get_y());
    if (g_active_tracker->ztracker()) {
      g_log_offset_z = g_active_tracker->ztracker()->get_z();
    }
  }
}

// If the device filename becomes non-empty, then set the global
// device name to match what it is set to.

#ifdef _WIN32
void  device_filename_changed(char *newvalue, void *)
#else
void  device_filename_changed(const char *newvalue, void *)
#endif
{
  // Set the global name, if we have a non-empty name.
  if (strlen(newvalue) > 0) {
    g_device_name = new char[strlen(newvalue)+1];
    if (g_device_name == NULL) {
	fprintf(stderr,"device_filename_changed(): Out of memory\n");
	dirtyexit();
	exit(-1);
    }
    strcpy(g_device_name, newvalue);
  }
}

void  reset_background_image(int newvalue, void *)
{
  if (g_mean_image) {
    delete g_mean_image;
    g_mean_image = NULL;
  }
}

// If the value of the interpolate box changes, then create a new spot
// tracker of the appropriate type in the same location and with the
// same radius as the one that was used before.

void  rebuild_trackers(int newvalue, void *)
{
  // Delete all trackers and replace with the correct types.
  // Make sure to put them back where they came from.
  // Re-point the active tracker at its corresponding new
  // tracker.
  list<Spot_Information *>::iterator  loop;

  for (loop = g_trackers.begin(); loop != g_trackers.end(); loop++) {
    double x = (*loop)->xytracker()->get_x();
    double y = (*loop)->xytracker()->get_y();
    double r = (*loop)->xytracker()->get_radius();

    // Be sure to delete only the trackers and not the indices when
    // rebuilding the trackers.
    if (g_active_tracker == *loop) {
      delete (*loop)->xytracker();
      if ( (*loop)->ztracker() ) { delete (*loop)->ztracker(); }
      (*loop)->set_xytracker(create_appropriate_xytracker(x,y,r));
      (*loop)->set_ztracker(create_appropriate_ztracker());
      g_active_tracker = *loop;
    } else {
      delete (*loop)->xytracker();
      if ( (*loop)->ztracker() ) { delete (*loop)->ztracker(); }
      (*loop)->set_xytracker(create_appropriate_xytracker(x,y,r));
      (*loop)->set_ztracker(create_appropriate_ztracker());
    }
    if ((*loop)->ztracker()) { (*loop)->ztracker()->set_depth_accuracy(g_precision); }
  }
}

// Hide or show all of the debugging windows
void  set_debug_visibility(int newvalue, void *)
{
  if (g_ready_to_display) {
    if (newvalue) {
      glutSetWindow(g_beadseye_window);
      glutShowWindow();
      glutSetWindow(g_landscape_window);
      glutShowWindow();
    } else {
      glutSetWindow(g_beadseye_window);
      glutHideWindow();
      glutSetWindow(g_landscape_window);
      glutHideWindow();
    }
  }
}

void  initializekymograph(void) {
  // Mark the kymograph as not filled.
  g_kymograph_filled = 0;

  // Fill the image buffer with black
  int x,y;
  for (x = 0; x < g_kymograph_width; x++) {
    for (y = 0; y < g_kymograph_height; y++) {
      g_kymograph_image[0 + 4 * (x + g_kymograph_width * (g_kymograph_height - 1 - y))] = 0;
      g_kymograph_image[1 + 4 * (x + g_kymograph_width * (g_kymograph_height - 1 - y))] = 0;
      g_kymograph_image[2 + 4 * (x + g_kymograph_width * (g_kymograph_height - 1 - y))] = 0;
      g_kymograph_image[3 + 4 * (x + g_kymograph_width * (g_kymograph_height - 1 - y))] = 255;
    }
  }
}


// Hide or show the kymograph windows
void  set_kymograph_visibility(int newvalue, void *)
{
  if (g_ready_to_display) {
    if (newvalue) {
      initializekymograph();
      glutSetWindow(g_kymograph_window);
      glutShowWindow();
      glutSetWindow(g_kymograph_center_window);
      glutShowWindow();
    } else {
      glutSetWindow(g_kymograph_center_window);
      glutHideWindow();
      glutSetWindow(g_kymograph_window);
      glutHideWindow();
    }
  }
}

// This version is for float sliders
void  rebuild_trackers(float newvalue, void *) {
  rebuild_trackers((int)newvalue, NULL);
}

// Sets the radius as the check-box is turned on (1) and off (0);
// it will be set to 4x the current bead radius
void  set_maximum_search_radius(int newvalue, void *)
{
  g_search_radius = 4 * g_Radius * newvalue;
}

// When the button for "Save State" is pressed, this
// routine is called.  Ask the user for a file to save the state to.
void  handle_save_state_change(int newvalue, void *)
{
  //------------------------------------------------------------
  // Create a dialog box that will ask the user
  // to either fill it in or cancel (if the file is "NONE").
  // Wait until they have made a choice.
  g_save_state_filename = "";
  if (Tcl_Eval(g_tk_control_interp, "ask_user_for_save_state_filename") != TCL_OK) {
    fprintf(stderr, "Tcl_Eval(ask_user_for_save_state_filename) failed: %s\n", g_tk_control_interp->result);
    cleanup();
    exit(-1);
  }
  do {
    //------------------------------------------------------------
    // This pushes changes in the C variables over to Tcl.

    while (Tk_DoOneEvent(TK_DONT_WAIT)) {};
    if (Tclvar_mainloop()) {
	fprintf(stderr,"Tclvar Mainloop failed\n");
    }
  } while (strcmp(g_save_state_filename,"") == 0);

  //------------------------------------------------------------
  // Open the file and write a Tcl-interpretable line for each
  // state value that we want to set.  These will then be
  // interpreted when the file is loaded again.  Do not store
  // the state of the optimize flag or the locations of any of
  // the trackers, but do set all of the default values.
  const char *outname = g_save_state_filename;
  FILE *f = fopen(outname, "w");
  if (f == NULL) {
    perror("Cannot open state file for writing");
    fprintf(stderr, "  (%s)\n", outname);
    return;
  }
  fprintf(f, "set radius %lg\n", (double)(g_Radius));
  fprintf(f, "set exposure_millisecs %lg\n", (double)(g_exposure));
  fprintf(f, "set red_green_blue %lg\n", (double)(g_colorIndex));
  fprintf(f, "set brighten %lg\n", (double)(g_brighten));
  fprintf(f, "set precision %lg\n", (double)(g_precision));
  fprintf(f, "set sample_spacing %lg\n", (double)(g_sampleSpacing));
  fprintf(f, "set kernel_lost_tracking_sensitivity %lg\n", (double)(g_lossSensitivity));
  fprintf(f, "set intensity_lost_tracking_sensitivity %lg\n", (double)(g_intensityLossSensitivity));
  fprintf(f, "set dead_zone_around_border %lg\n", (double)(g_borderDeadZone));
  fprintf(f, "set dead_zone_around_trackers %lg\n", (double)(g_trackerDeadZone));
  fprintf(f, "set maintain_this_many_beads %lg\n", (double)(g_findThisManyBeads));
  fprintf(f, "set candidate_spot_threshold %lg\n", (double)(g_candidateSpotThreshold));
  fprintf(f, "set sliding_window_radius %lg\n", (double)(g_slidingWindowRadius));
  fprintf(f, "set lost_behavior %d\n", (int)(g_lostBehavior));
  fprintf(f, "set kerneltype %d\n", (int)(g_kernel_type));
  fprintf(f, "set dark_spot %d\n", (int)(g_invert));
  fprintf(f, "set interpolate %d\n", (int)(g_interpolate));
  fprintf(f, "set parabolafit %d\n", (int)(g_parabolafit));
  fprintf(f, "set predict %d\n", (int)(g_predict));
  fprintf(f, "set follow_jumps %d\n", (int)(g_follow_jumps));
  fprintf(f, "set search_radius %lg\n", (double)(g_search_radius));
  fprintf(f, "set predict %d\n", (int)(g_predict));
  fprintf(f, "set rod3 %d\n", (int)(g_rod));
  fprintf(f, "set length %lg\n", (double)(g_length));
  fprintf(f, "set orientation %lg\n", (double)(g_orientation));
  fprintf(f, "set round_cursor %d\n", (int)(g_round_cursor));
  fprintf(f, "set show_tracker %d\n", (int)(g_mark));
  fprintf(f, "set show_video %d\n", (int)(g_show_video));
  fprintf(f, "set background_subtract %d\n", (int)(g_background_subtract));
  fprintf(f, "set logging_video %d\n", (int)(g_log_video));
  fprintf(f, "set video_full_frame_every %lg\n", (double)(g_video_full_frame_every));

  fclose(f);
}

bool load_state_from_file(const char *inname)
{
  //------------------------------------------------------------
  // Open the file and read each of its lines, passing it to the Tcl
  // parser for evaluation.  This lets people put whatever set commands
  // they want into one of these files.
  FILE *f = fopen(inname, "r");
  if (f == NULL) {
    perror("Cannot open state file for reading");
    fprintf(stderr, "  (%s)\n", inname);
    return false;
  }
  char  line[4096];
  while (fgets(line, sizeof(line)-1, f)) {
    if (Tcl_Eval(g_tk_control_interp, line) != TCL_OK) {
      fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", line, g_tk_control_interp->result);
      cleanup();
      exit(-1);
    }
  }
  fclose(f);

  return true;
}

// When the button for "Load State" is pressed, this
// routine is called.  Ask the user for a file to load the state from.
void  handle_load_state_change(int newvalue, void *)
{
  //------------------------------------------------------------
  // Create a dialog box that will ask the user
  // to either fill it in or cancel (if the file is "NONE").
  // Wait until they have made a choice.
  g_load_state_filename = "";
  if (Tcl_Eval(g_tk_control_interp, "ask_user_for_load_state_filename") != TCL_OK) {
    fprintf(stderr, "Tcl_Eval(ask_user_for_load_state_filename) failed: %s\n", g_tk_control_interp->result);
    cleanup();
    exit(-1);
  }
  do {
    //------------------------------------------------------------
    // This pushes changes in the C variables over to Tcl.

    while (Tk_DoOneEvent(TK_DONT_WAIT)) {};
    if (Tclvar_mainloop()) {
	fprintf(stderr,"Tclvar Mainloop failed\n");
    }
  } while (strcmp(g_load_state_filename,"") == 0);

  load_state_from_file(g_load_state_filename);
}

// When the check-box for "optimize Z" is changed, this
// routine is called.  When it is turning on, we make the
// user select a PSF file to use for Z tracking and create
// the Z tracker.  When it is turning off, we delete the
// Z tracker and set its pointer to NULL.
void  handle_optimize_z_change(int newvalue, void *)
{
  list<Spot_Information *>::iterator  loop;

  // User is trying to start Z tracking, so ask them for
  // a file to load.  If we load one, then set the global
  // Z tracker to use it.  If we don't set g_opt_z to
  // zero again.
  if (newvalue == 1) {
    g_psf_filename = "";

    //------------------------------------------------------------
    // Create a dialog box that will ask the user
    // to either fill it in or cancel (if the file is "NONE").
    if (Tcl_Eval(g_tk_control_interp, "ask_user_for_psf_filename") != TCL_OK) {
      fprintf(stderr, "Tcl_Eval(ask_user_for_psf_filename) failed: %s\n", g_tk_control_interp->result);
      cleanup();
      exit(-1);
    }

    do {
      //------------------------------------------------------------
      // This pushes changes in the C variables over to Tcl.

      while (Tk_DoOneEvent(TK_DONT_WAIT)) {};
      if (Tclvar_mainloop()) {
	fprintf(stderr,"Tclvar Mainloop failed\n");
      }
    } while (strcmp(g_psf_filename,"") == 0);

    // Set the Z tracker in all of the existing trackers to
    // be the proper one (whether NULL or not).
    if (strcmp(g_psf_filename, "NONE") != 0) {
      for (loop = g_trackers.begin(); loop != g_trackers.end(); loop++) {
	(*loop)->set_ztracker(create_appropriate_ztracker());
      }
    } else {
      for (loop = g_trackers.begin(); loop != g_trackers.end(); loop++) {
	if ( (*loop)->ztracker() ) {
	  delete (*loop)->ztracker();
	  (*loop)->set_ztracker(NULL);
	}
      }
      g_psf_filename = "";
      g_opt_z = 0;
    }

  // User is turning off Z tracking, so destroy any existing
  // Z tracker and set the pointer to NULL.
  } else {
    for (loop = g_trackers.begin(); loop != g_trackers.end(); loop++) {
      if ( (*loop)->ztracker() ) {
	delete (*loop)->ztracker();
	(*loop)->set_ztracker(NULL);
      }
    }
    g_psf_filename = "";
  }
}


//--------------------------------------------------------------------------

void Usage(const char *progname)
{
    fprintf(stderr, "Usage: %s [-nogui] [-kernel disc|cone|symmetric|FIONA]\n", progname);
    fprintf(stderr, "           [-dark_spot] [-follow_jumps] [-rod3 LENGTH ORIENT] [-outfile NAME]\n");
    fprintf(stderr, "           [-precision P] [-sample_spacing S] [-show_lost_and_found]\n");
    fprintf(stderr, "           [-lost_behavior B] [-lost_tracking_sensitivity L]\n");
    fprintf(stderr, "           [-intensity_lost_sensitivity IL] [-dead_zone_around_border DB]\n");
    fprintf(stderr, "           [-maintain_this_many_beads M] [-dead_zone_around_trackers DT]\n");
    fprintf(stderr, "           [-candidate_spot_threshold T] [-sliding_window_radius SR]\n");
    fprintf(stderr, "           [-radius R] [-tracker X Y R] [-tracker X Y R] ...\n");
    fprintf(stderr, "           [-FIONA_background BG]\n");
    fprintf(stderr, "           [-raw_camera_params sizex sizey bitdepth channels headersize frameheadersize]\n");
    fprintf(stderr, "           [-load_state FILE] [-log_video N]\n");
    fprintf(stderr, "           [roper|cooke|edt|diaginc|directx|directx640x480|filename]\n");
    fprintf(stderr, "       -nogui: Run without the video display window (no Glut/OpenGL)\n");
    fprintf(stderr, "       -kernel: Use kernels of the specified type (default symmetric)\n");
    fprintf(stderr, "       -rod3: Make a rod3 kernel of specified LENGTH(pixels) & ORIENT(degrees)\n");
    fprintf(stderr, "       -dark_spot: Track a dark spot (default is bright spot)\n");
    fprintf(stderr, "       -follow_jumps: Set the follow_jumps flag\n");
    fprintf(stderr, "       -outfile: Save the track to the file 'name' (.vrpn will be appended)\n");
    fprintf(stderr, "       -precision: Set the precision for created trackers to P (default 0.05)\n");
    fprintf(stderr, "       -sample_spacing: Set the sample spacing for trackers to S (default 1)\n");
    fprintf(stderr, "       -show_lost_and_found: Show the lost_and_found window on startup\n");
    fprintf(stderr, "       -lost_behavior: Set lost tracker behavior: 0:stop; 1:delete; 2:hover\n");
    fprintf(stderr, "       -lost_tracking_sensitivity: Set lost_tracking_sensitivity to L\n");
    fprintf(stderr, "       -intensity_lost_sensitivity:Set intensity_lost_tracking_sensitivity to IL\n");
    fprintf(stderr, "       -dead_zone_around_border: Set a dead zone around the region of interest\n");
    fprintf(stderr, "                 edge within which new trackers will not be found\n");
    fprintf(stderr, "       -dead_zone_around_trackers: Set a dead zone around all current trackers\n");
    fprintf(stderr, "                 within which new trackers will not be found\n");
    fprintf(stderr, "       -maintain_this_many_beads: Try to autofind up to M beads at every frame\n");
    fprintf(stderr, "       -candidate_spot_threshold: Set the threshold for possible spots when\n");
    fprintf(stderr, "                 autofinding.  Setting this lower will not miss as many spots,\n");
    fprintf(stderr, "                 but will also find garbage (default 5)\n");
    fprintf(stderr, "       -sliding_window_radius: Set the radius of the global SMD sliding window\n");
    fprintf(stderr, "                 neighborhood.  Higher values of SR may cause some spots to\n");
    fprintf(stderr, "                 take longer before they are detected, but will greatly\n");
    fprintf(stderr, "                 increase the running speed (default 9)\n");
    fprintf(stderr, "       -radius: Set the radius to use for new trackers to R (default 5)\n");
    fprintf(stderr, "       -tracker: Create a tracker with radius R at pixel X,Y and initiate\n");
    fprintf(stderr, "                 optimization.  Multiple trackers can be created\n");
    fprintf(stderr, "       -FIONA_background: Set the default background for FIONA trackers to BG\n");
    fprintf(stderr, "                 (default 0)\n");
    fprintf(stderr, "       -raw_camera_params: Set parameters in case we're opening a raw file\n");
    fprintf(stderr, "                 (default throws a dialog box to ask you for them)\n");
    fprintf(stderr, "       -load_state: Load program state from FILE\n");
    fprintf(stderr, "       -log_video: Log every Nth frame of video (in addition to every tracker every frame)\n");
    fprintf(stderr, "       source: The source file for tracking can be specified here (default is\n");
    fprintf(stderr, "                 a dialog box)\n");
    exit(-1);
}

int main(int argc, char *argv[])
{
  // Set up exit handler to make sure we clean things up no matter
  // how we are quit.  We hope that we exit in a good way and so
  // cleanup() gets called, but if not then we do a dirty exit.
  atexit(dirtyexit);

  //------------------------------------------------------------------
  // Set up the threads used for tracking, if we have more than one
  // processor to use for tracking.  Try to use all of the available
  // processors for tracking.
  g_num_tracking_processors = Thread::number_of_processors();
  if (g_num_tracking_processors > 1) {
    // Create the "Number of active tracking thread" semaphore that is used
    // to determine when a tracker has finished and is ready to go.
    g_ready_tracker_semaphore = new Semaphore(g_num_tracking_processors);
    if (g_ready_tracker_semaphore == NULL) {
      fprintf(stderr,"Could not create semaphore for number of active tracking threads\n");
      exit(-1);
    }

    // Create the tracking threads.
    printf("Creating %d tracking threads\n", g_num_tracking_processors);
    unsigned i;
    for (i = 0; i < g_num_tracking_processors; i++) {

      // Create the semaphore that will be used to control access to the UserData object
      Semaphore *tds = new Semaphore();

      // Build the thread UserData object that will be passed to the thread function
      ThreadUserData *tud = new ThreadUserData();
      tud->d_ready = false;
      tud->d_tracker = NULL;
      tud->d_run_semaphore = new Semaphore();

      // Fill in the ThreadInfo structure.  First the user data, then the thread
      // data (which includes a pointer to the user data) and finally the thread
      // itself.
      ThreadInfo *ti = new ThreadInfo;
      ti->d_thread_user_data = tud;
      ti->d_thread_data.pvUD = tud;
      ti->d_thread_data.ps = tds;
      ti->d_thread = new Thread(tracking_thread_function, ti->d_thread_data);

      // Add this to the thread list, and increment the semaphore so we
      // won't try to run one of the threads until at least one has become
      // ready.
      g_tracking_threads.push_back(ti);
      g_ready_tracker_semaphore->p();

      // Call P() on the run semaphore so that the thread will not be able to run
      // until we call V() on it later.  Then run the thread itself to get it into
      // the ready state.
      tud->d_run_semaphore->p();
      if (ti->d_thread->go() != 0) {
        fprintf(stderr,"Could not run tracking thread number %d\n", i+1);
        exit(-1);
      }
    }
  }

  //------------------------------------------------------------------
  // Create the tracking thread that will create and fill the
  // VRPN log file.
  g_logging_thread_data.pvUD = NULL;
  g_logging_thread_data.ps = new Semaphore();
  g_logging_thread = new Thread(logging_thread_function, g_logging_thread_data);
  if (g_logging_thread == NULL) {
    fprintf(stderr, "Could not create logging thread; exiting\n");
    exit(-1);
  }
  g_logging_thread->go();

  //------------------------------------------------------------------
  // VRPN state setting so that we don't try to preload a video file
  // when it is opened, which wastes time.  Also tell it not to
  // accumulate messages, which can cause us to run out of memory.
  vrpn_FILE_CONNECTIONS_SHOULD_PRELOAD = false;
  vrpn_FILE_CONNECTIONS_SHOULD_ACCUMULATE = false;

  //------------------------------------------------------------------
  // Generic Tcl startup.  Getting and interpreter and mainwindow.

  char		command[256];
  Tk_Window       tk_control_window;
  g_tk_control_interp = Tcl_CreateInterp();

  /* Start a Tcl interpreter */
  if (Tcl_Init(g_tk_control_interp) == TCL_ERROR) {
          fprintf(stderr,
                  "Tcl_Init failed: %s\n",g_tk_control_interp->result);
          return(-1);
  }

  /* Start a Tk mainwindow to hold the widgets */
  if (Tk_Init(g_tk_control_interp) == TCL_ERROR) {
	  fprintf(stderr,
	  "Tk_Init failed: %s\n",g_tk_control_interp->result);
	  return(-1);
  }
  tk_control_window = Tk_MainWindow(g_tk_control_interp);
  if (tk_control_window == NULL) {
          fprintf(stderr,"%s\n", g_tk_control_interp->result);
          return(-1);
  }

  //------------------------------------------------------------------
  // Loading the particular definition files we need.  russ_widgets is
  // required by the Tclvar_float_with_scale class.  simple_magnet_drive
  // is application-specific and sets up the controls for the integer
  // and float variables.

  /* Load the Tcl scripts that handle widget definition and
   * variable controls */
  sprintf(command, "source russ_widgets.tcl");
  if (Tcl_Eval(g_tk_control_interp, command) != TCL_OK) {
          fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
                  g_tk_control_interp->result);
          return(-1);
  }

  //------------------------------------------------------------------
  // Put the version number into the main window.
  sprintf(command, "label .versionlabel -text Video_spot_tracker_v:%s", Version_string);
  if (Tcl_Eval(g_tk_control_interp, command) != TCL_OK) {
          fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
                  g_tk_control_interp->result);
          return(-1);
  }
  sprintf(command, "pack .versionlabel");
  if (Tcl_Eval(g_tk_control_interp, command) != TCL_OK) {
          fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
                  g_tk_control_interp->result);
          return(-1);
  }

  //------------------------------------------------------------------
  // Put the "Thank-you Ware" button into the main window.
  sprintf(command, "package require http");
  if (Tcl_Eval(g_tk_control_interp, command) != TCL_OK) {
          fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
                  g_tk_control_interp->result);
          return(-1);
  }
  sprintf(command, "set thanks_text \"Say Thank You!\"");
  if (Tcl_Eval(g_tk_control_interp, command) != TCL_OK) {
          fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
                  g_tk_control_interp->result);
          return(-1);
  }
  sprintf(command, "button .thankyouware -textvariable thanks_text -command { ::http::geturl \"http://www.cs.unc.edu/Research/nano/cismm/thankyou/yourewelcome.htm?program=video_spot_tracker&Version=%s\" ; set thanks_text \"Paid for by NIH/NIBIB\" }", Version_string);
  if (Tcl_Eval(g_tk_control_interp, command) != TCL_OK) {
          fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
                  g_tk_control_interp->result);
          return(-1);
  }
  sprintf(command, "pack .thankyouware");
  if (Tcl_Eval(g_tk_control_interp, command) != TCL_OK) {
          fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
                  g_tk_control_interp->result);
          return(-1);
  }

  //------------------------------------------------------------------
  // Load the specialized Tcl code needed by this program.  This must
  // be loaded before the Tclvar_init() routine is called because it
  // puts together some of the windows needed by the variables.
  sprintf(command, "source video_spot_tracker.tcl");
  if (Tcl_Eval(g_tk_control_interp, command) != TCL_OK) {
          fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
                  g_tk_control_interp->result);
          return(-1);
  }

  //------------------------------------------------------------------
  // This routine must be called in order to initialize all of the
  // variables that came into scope before the interpreter was set
  // up, and to tell the variables which interpreter to use.  It is
  // called once, after the interpreter exists.

  // Initialize the variables using the interpreter
  if (Tclvar_init(g_tk_control_interp)) {
	  fprintf(stderr,"Can't do init!\n");
	  return -1;
  }
  sprintf(command, "wm geometry . +10+10");
  if (Tcl_Eval(g_tk_control_interp, command) != TCL_OK) {
          fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
                  g_tk_control_interp->result);
          return(-1);
  }

  //------------------------------------------------------------------
  // Parse the command line.  This has to be done after a bunch of the
  // set-up, so that we can create trackers and output files and such.
  // It has to be before the imager set-up because we don't know what
  // source to use until after we parse this.
  int	i, realparams;		  // How many non-flag command-line arguments
  realparams = 0;
  // These defaults are set for a Pulnix camera
  unsigned  raw_camera_params_valid = false;  //< Have the following been set intentionally?
  unsigned  raw_camera_numx = 648;
  unsigned  raw_camera_numy = 484;
  unsigned  raw_camera_bitdepth = 8;        //< Number of bits per channel in raw file camera
  unsigned  raw_camera_channels = 1;        //< Number of channels in raw file camera
  unsigned  raw_camera_headersize = 0;      //< Number of header bytes in raw file camera
  unsigned  raw_camera_frameheadersize = 0;      //< Number of header bytes in raw file camera

  for (i = 1; i < argc; i++) {
    if (!strncmp(argv[i], "-kernel", strlen("-kernel"))) {
      if (++i > argc) { Usage(argv[0]); }
      if (!strncmp(argv[i], "disc", strlen("disc"))) {
        g_kernel_type = KERNEL_DISK;
      } else if (!strncmp(argv[i], "cone", strlen("cone"))) {
        g_kernel_type = KERNEL_CONE;
      } else if (!strncmp(argv[i], "symmetric", strlen("symmetric"))) {
        g_kernel_type = KERNEL_SYMMETRIC;
      } else if (!strncmp(argv[i], "FIONA", strlen("FIONA"))) {
        g_kernel_type = KERNEL_FIONA;
      } else {
        Usage(argv[0]);
        exit(-1);
      }
    } else if (!strncmp(argv[i], "-nogui", strlen("-nogui"))) {
      g_use_gui = false;
    } else if (!strncmp(argv[i], "-dark_spot", strlen("-dark_spot"))) {
      g_invert = true;
    } else if (!strncmp(argv[i], "-follow_jumps", strlen("-follow_jumps"))) {
      g_follow_jumps = 1;
    } else if (!strncmp(argv[i], "-raw_camera_params", strlen("-raw_camera_params"))) {
      if (++i > argc) { Usage(argv[0]); }
      raw_camera_numx = atoi(argv[i]);
      if (++i > argc) { Usage(argv[0]); }
      raw_camera_numy = atoi(argv[i]);
      if (++i > argc) { Usage(argv[0]); }
      raw_camera_bitdepth = atoi(argv[i]);
      if (++i > argc) { Usage(argv[0]); }
      raw_camera_channels = atoi(argv[i]);
      if (++i > argc) { Usage(argv[0]); }
      raw_camera_headersize = atoi(argv[i]);
      if (++i > argc) { Usage(argv[0]); }
      raw_camera_frameheadersize = atoi(argv[i]);
      raw_camera_params_valid = true;
    } else if (!strncmp(argv[i], "-outfile", strlen("-outfile"))) {
      if (++i > argc) { Usage(argv[0]); }
      char *name = new char[strlen(argv[i])+6];
      sprintf(name, "%s.vrpn", argv[i]);
      logfilename_changed(name, NULL);
    } else if (!strncmp(argv[i], "-rod3", strlen("-rod3"))) {
      if (++i > argc) { Usage(argv[0]); }
      g_length = atof(argv[i]);
      if (++i > argc) { Usage(argv[0]); }
      g_orientation = atof(argv[i]);
      g_rod = 1;
    } else if (!strncmp(argv[i], "-precision", strlen("-precision"))) {
      if (++i > argc) { Usage(argv[0]); }
      g_precision = atof(argv[i]);
    } else if (!strncmp(argv[i], "-sample_spacing", strlen("-sample_spacing"))) {
      if (++i > argc) { Usage(argv[0]); }
      g_sampleSpacing = atof(argv[i]);
    } else if (!strncmp(argv[i], "-FIONA_background", strlen("-FIONA_background"))) {
      if (++i > argc) { Usage(argv[0]); }
      g_FIONA_background = atof(argv[i]);
    } else if (!strncmp(argv[i], "-tracker", strlen("-tracker"))) {
      if (++i > argc) { Usage(argv[0]); }
      g_X = atof(argv[i]);
      if (++i > argc) { Usage(argv[0]); }
      g_Y = atof(argv[i]);
      if (++i > argc) { Usage(argv[0]); }
      g_Radius = atof(argv[i]);
      g_trackers.push_back(new Spot_Information(create_appropriate_xytracker(g_X,g_Y,g_Radius),create_appropriate_ztracker()));
      g_active_tracker = g_trackers.back();
      if (g_active_tracker->ztracker()) { g_active_tracker->ztracker()->set_depth_accuracy(0.25); }
	} else if (!strncmp(argv[i], "-lost_tracking_sensitivity", strlen("-lost_tracking_sensitivity"))) {
		if (++i > argc) { Usage(argv[0]); }
		g_lossSensitivity = atof(argv[i]);
	} else if (!strncmp(argv[i], "-intensity_lost_sensitivity", strlen("-intensity_lost_sensitivity"))) {
		if (++i > argc) { Usage(argv[0]); }
		g_intensityLossSensitivity = atof(argv[i]);	
	} else if (!strncmp(argv[i], "-dead_zone_around_border", strlen("-dead_zone_around_border"))) {
		if (++i > argc) { Usage(argv[0]); }
		g_borderDeadZone = atof(argv[i]);
	} else if (!strncmp(argv[i], "-dead_zone_around_trackers", strlen("-dead_zone_around_trackers"))) {
		if (++i > argc) { Usage(argv[0]); }
		g_trackerDeadZone = atof(argv[i]);
	} else if (!strncmp(argv[i], "-maintain_this_many_beads", strlen("-maintain_this_many_beads"))) {
		if (++i > argc) { Usage(argv[0]); }
		g_findThisManyBeads = atof(argv[i]);
	} else if (!strncmp(argv[i], "-candidate_spot_threshold", strlen("-candidate_spot_threshold"))) {
		if (++i > argc) { Usage(argv[0]); }
		g_candidateSpotThreshold = atof(argv[i]);
	} else if (!strncmp(argv[i], "-lost_behavior", strlen("-lost_behavior"))) {
		if (++i > argc) { Usage(argv[0]); }
		g_lostBehavior = atof(argv[i]);
	} else if (!strncmp(argv[i], "-sliding_window_radius", strlen("-sliding_window_radius"))) {
		if (++i > argc) { Usage(argv[0]); }
		g_slidingWindowRadius = atof(argv[i]);
	} else if (!strncmp(argv[i], "-radius", strlen("-radius"))) {
		if (++i > argc) { Usage(argv[0]); }
		g_Radius = atof(argv[i]);
	} else if (!strncmp(argv[i], "-show_lost_and_found", strlen("-show_lost_and_found"))) {
		g_showLostAndFound = true;
    } else if (!strncmp(argv[i], "-load_state", strlen("-load_state"))) {
      if (++i > argc) { Usage(argv[0]); }
      if (!load_state_from_file(argv[i])) {
        fprintf(stderr,"Could not load state file from %s\n", argv[i]);
        exit(-1);
      }
    } else if (!strncmp(argv[i], "-log_video", strlen("-log_video"))) {
      if (++i > argc) { Usage(argv[0]); }
      g_log_video = 1;
      g_video_full_frame_every = atoi(argv[i]);
    } else if (argv[i][0] == '-') {
      Usage(argv[0]);
    } else {
      switch (++realparams) {
      case 1:
        // Filename argument: open the file specified.
        g_device_name = argv[i];
        break;

      default:
        Usage(argv[0]);
      }
    }
  }

  //------------------------------------------------------------
  // This pushes changes in the C variables over to Tcl and then
  // calls any resulting callbacks (handles the commands set during
  // the command-line parsing).

  while (Tk_DoOneEvent(TK_DONT_WAIT)) {};
  if (Tclvar_mainloop()) {
    fprintf(stderr,"Tclvar Mainloop failed\n");
    return -1;
  }
  while (Tk_DoOneEvent(TK_DONT_WAIT)) {};

  //------------------------------------------------------------------
  // If we don't have a device name, then throw a Tcl dialog asking
  // the user for the name of a file to use and wait until they respond.
  if (g_device_name == NULL) {

    //------------------------------------------------------------
    // Create a callback for a variable that will hold the device
    // name and then create a dialog box that will ask the user
    // to either fill it in or quit.
    Tclvar_selector filename("device_filename", NULL, NULL, "", device_filename_changed, NULL);
    if (Tcl_Eval(g_tk_control_interp, "ask_user_for_filename") != TCL_OK) {
      fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command, g_tk_control_interp->result);
      cleanup();
      exit(-1);
    }

    do {
      //------------------------------------------------------------
      // This pushes changes in the C variables over to Tcl.

      while (Tk_DoOneEvent(TK_DONT_WAIT)) {};
      if (Tclvar_mainloop()) {
	fprintf(stderr,"Tclvar Mainloop failed\n");
      }
    } while ( (g_device_name == NULL) && !g_quit);
    if (g_quit) {
      cleanup();
      exit(0);
    }
  }

  //------------------------------------------------------------------
  // If we're being asked to open a raw file and we don't have a set of
  // raw-file parameters, then see if we can figure them out based on the
  // file size or else throw a dialog box asking for them.
  if (  (strcmp(".raw", &g_device_name[strlen(g_device_name)-4]) == 0) ||
        (strcmp(".RAW", &g_device_name[strlen(g_device_name)-4]) == 0) ) {

    if (!raw_camera_params_valid) {
      //------------------------------------------------------------
      // See if we can figure out the right settings for these parameters
      // based on the file size (if it is an even multiple of the particular
      // file sizes for the odd-sized Pulnix camera or the Point Grey camera that
      // we're using at UNC.  Standard sizes are going to be harder to
      // determine, because if there are 100 frames it will mask the difference
      // between power-of-2 changes on each axis (640x480 vs. 1280x960, for example).
      __int64 file_length = determine_file_length(g_device_name);
      if (file_length > 0) {   // Zero length means we can't tell, <0 means failure.
        double frame_size, num_frames;

        // Check for original Pulnix cameras used at UNC
        frame_size = 648 * 484;
        num_frames = file_length / frame_size;
        if ( num_frames == floor(num_frames) ) {
          printf("Assuming EDT/Pulnix file format (648x484)\n");
          raw_camera_numx = 648;
          raw_camera_numy = 484;
          raw_camera_bitdepth = 8;
          raw_camera_channels = 1;
          raw_camera_headersize = 0;
          raw_camera_frameheadersize = 0;
          raw_camera_params_valid = true;
        } else {
          // Check for Point Grey camera being used at UNC
          frame_size = 1024 * 768 + 112;
          num_frames = file_length / frame_size;
          if ( num_frames == floor(num_frames) ) {
            printf("Assuming Point Grey file format (1024x768, 112-byte frame headers)\n");
            raw_camera_numx = 1024;
            raw_camera_numy = 768;
            raw_camera_bitdepth = 8;
            raw_camera_channels = 1;
            raw_camera_headersize = 0;
            raw_camera_frameheadersize = 112;
            raw_camera_params_valid = true;
          }
        }
      }
    }

    if (!raw_camera_params_valid) {
      //------------------------------------------------------------
      // Create a callback for a variable that will tell when the
      // params are filled and then create a dialog box that will ask the user
      // to either fill it in or quit.
      Tclvar_int  raw_params_set("raw_params_set", 0);
      if (Tcl_Eval(g_tk_control_interp, "ask_user_for_raw_file_params") != TCL_OK) {
        fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command, g_tk_control_interp->result);
        cleanup();
        exit(-1);
      }

      do {
        //------------------------------------------------------------
        // This pushes changes in the C variables over to Tcl.

        while (Tk_DoOneEvent(TK_DONT_WAIT)) {};
        if (Tclvar_mainloop()) {
	  fprintf(stderr,"Tclvar Mainloop failed\n");
        }
      } while ( (raw_params_set == 0) && !g_quit);
      if (g_quit) {
        cleanup();
        exit(0);
      }

      // XXX Read the Tcl variables associated with this into our params
      // and then say that the params have been filled in.
      raw_camera_numx = g_tcl_raw_camera_numx;
      raw_camera_numy = g_tcl_raw_camera_numy;
      raw_camera_bitdepth = g_tcl_raw_camera_bitdepth;
      raw_camera_channels = g_tcl_raw_camera_channels;
      raw_camera_headersize = g_tcl_raw_camera_headersize;
      raw_camera_frameheadersize = g_tcl_raw_camera_frameheadersize;
    }
  }

  //------------------------------------------------------------------
  // Open the camera.  If we have a video file, then
  // set up the Tcl controls to run it.  Also, report the frame number.
  float exposure = g_exposure;
  g_camera_bit_depth = raw_camera_bitdepth;
  if (!get_camera(g_device_name, &g_camera_bit_depth, &exposure,
                  &g_camera, &g_video, raw_camera_numx, raw_camera_numy,
                  raw_camera_channels, raw_camera_headersize, raw_camera_frameheadersize)) {
    fprintf(stderr,"Cannot open camera\n");
    if (g_camera) { delete g_camera; g_camera = NULL; }
    cleanup();
    exit(-1);
  }
  g_exposure = exposure;

  if (g_video) {  // Put these in a separate control panel?
    // Start out paused at the beginning of the file.
    g_play = new Tclvar_int_with_button("play_video","",0);
    g_rewind = new Tclvar_int_with_button("rewind_video","",1);
    g_step = new Tclvar_int_with_button("single_step_video","");
    sprintf(command, "frame .frame");
    if (Tcl_Eval(g_tk_control_interp, command) != TCL_OK) {
	    fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
		    g_tk_control_interp->result);
	    return(-1);
    }
    sprintf(command, "pack .frame");
    if (Tcl_Eval(g_tk_control_interp, command) != TCL_OK) {
	    fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
		    g_tk_control_interp->result);
	    return(-1);
    }
    sprintf(command, "label .frame.frametitle -text FrameNum");
    if (Tcl_Eval(g_tk_control_interp, command) != TCL_OK) {
	    fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
		    g_tk_control_interp->result);
	    return(-1);
    }
    sprintf(command, "pack .frame.frametitle -side left");
    if (Tcl_Eval(g_tk_control_interp, command) != TCL_OK) {
	    fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
		    g_tk_control_interp->result);
	    return(-1);
    }
    sprintf(command, "label .frame.framevalue -textvariable frame_number");
    if (Tcl_Eval(g_tk_control_interp, command) != TCL_OK) {
	    fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
		    g_tk_control_interp->result);
	    return(-1);
    }
    sprintf(command, "pack .frame.framevalue -side right");
    if (Tcl_Eval(g_tk_control_interp, command) != TCL_OK) {
	    fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
		    g_tk_control_interp->result);
	    return(-1);
    }
  }

  // Verify that the camera is working.
  if (!g_camera->working()) {
    fprintf(stderr,"Could not establish connection to camera\n");
    if (g_camera) { delete g_camera; g_camera = NULL; }
    cleanup();
    exit(-1);
  }

  //------------------------------------------------------------------
  // Initialize the controls for the clipping based on the size of
  // the image we got.
  g_minX = new Tclvar_float_with_scale("minX", ".clipping", 0, g_camera->get_num_columns()-1, 0);
  g_maxX = new Tclvar_float_with_scale("maxX", ".clipping", 0, g_camera->get_num_columns()-1, g_camera->get_num_columns()-1);
  g_minY = new Tclvar_float_with_scale("minY", ".clipping", 0, g_camera->get_num_rows()-1, 0);
  g_maxY = new Tclvar_float_with_scale("maxY", ".clipping", 0, g_camera->get_num_rows()-1, g_camera->get_num_rows()-1);

  //------------------------------------------------------------------
  // Set up the VRPN server connection and the tracker object that will
  // report the position when tracking is turned on.  Reserve 30,000
  // sensor locations (this should be an overestimate).
  g_vrpn_connection = vrpn_create_server_connection();
  g_vrpn_tracker = new vrpn_Tracker_Server("Spot", g_vrpn_connection, 30000);
  g_vrpn_analog = new vrpn_Analog_Server("FrameNumber", g_vrpn_connection, 1);
  g_vrpn_imager = new vrpn_Imager_Server("TestImage", g_vrpn_connection,
    g_camera->get_num_columns(), g_camera->get_num_rows());
  if ( (g_server_channel = g_vrpn_imager->add_channel("tracked", "unknown", 0, pow(2.0, (double)g_camera_bit_depth)-1 )) == -1) {
    fprintf(stderr, "Could not add channel to server image\n");
    cleanup();
    exit(-1);
  }
  if (g_vrpn_imager == NULL) {
    fprintf(stderr, "Could not create imager server\n");
    cleanup();
    exit(-1);
  }

  //------------------------------------------------------------------
  // Horrible hack to deal with the swapping of the Y axis in all of this
  // code.  Any trackers that have been created need to have their Y value
  // flipped.  We can't have done this until we've created the image and
  // we know how large it is in y, so we do it here.
  g_image = g_camera;
  list<Spot_Information *>::iterator  loop;
  for (loop = g_trackers.begin(); loop != g_trackers.end(); loop++) {
    double x = (*loop)->xytracker()->get_x();
    double y = flip_y((*loop)->xytracker()->get_y());
    (*loop)->xytracker()->set_location( x, y );
  }

  //------------------------------------------------------------------
  // If we created a tracker during the command-line parsing, then we
  // want to turn on optimization and also play the video.  Also set things
  // so that we will exit when we get to the end of the video.  We also do
  // this if we're running without a GUI.  We also do this if we're trying
  // to autofind beads.  If we don't have a video in one
  // of these circumstances, then we go ahead and quit.
  if (g_trackers.size() || !g_use_gui || (g_findThisManyBeads > 0.0)) {
    g_opt = 1;
    g_quit_at_end_of_video = true;
    if (g_video) {
      (*g_play) = 1;
    } else {
      fprintf(stderr,"No GUI or tracker created but no video file opened\n");
      cleanup();
      exit(100);
    }
  }

  //------------------------------------------------------------------
  // Initialize GLUT and create the window that will display the
  // video -- name the window after the device that has been
  // opened in VRPN.  Also set mouse callbacks.
  if (g_use_gui) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowPosition(175 + g_window_offset_x, 210 + g_window_offset_y);
    glutInitWindowSize(g_camera->get_num_columns(), g_camera->get_num_rows());
  #ifdef DEBUG
    printf("initializing window to %dx%d\n", g_camera->get_num_columns(), g_camera->get_num_rows());
  #endif
    g_tracking_window = glutCreateWindow(g_device_name);
    glutMotionFunc(motionCallbackForGLUT);
    glutMouseFunc(mouseCallbackForGLUT);
    glutKeyboardFunc(keyboardCallbackForGLUT);
  }

  // Create the buffer that Glut will use to send to the tracking window.  This is allocating an
  // RGBA buffer.  It needs to be 4-byte aligned, so we allocated it as a group of
  // words and then cast it to the right type.  We're using RGBA rather than just RGB
  // because it also solves the 4-byte alignment problem caused by funky sizes of image
  // that are RGB images.
  if ( (g_glut_image = (unsigned char *)(void*)new vrpn_uint32
      [g_camera->get_num_columns() * g_camera->get_num_rows()]) == NULL) {
    fprintf(stderr,"Out of memory when allocating image!\n");
    fprintf(stderr,"  (Image is %u by %u)\n", g_camera->get_num_columns(), g_camera->get_num_rows());
    if (g_camera) { delete g_camera; g_camera = NULL; }
    cleanup();
    exit(-1);
  }

  //------------------------------------------------------------------
  // Create the window that will be used for the "Bead's-eye view"
  if (g_use_gui) {
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowPosition(375 + g_window_offset_x, 140 + g_window_offset_y);
    glutInitWindowSize(g_beadseye_size, g_beadseye_size);
    g_beadseye_window = glutCreateWindow("Tracked");
  }

  // Create the buffer that Glut will use to send to the beads-eye window.  This is allocating an
  // RGBA buffer.  It needs to be 4-byte aligned, so we allocated it as a group of
  // words and then cast it to the right type.  We're using RGBA rather than just RGB
  // because it also solves the 4-byte alignment problem caused by funky sizes of image
  // that are RGB images.
  if ( (g_beadseye_image = (unsigned char *)(void*)new vrpn_uint32
      [g_beadseye_size * g_beadseye_size]) == NULL) {
    fprintf(stderr,"Out of memory when allocating beads-eye image!\n");
    fprintf(stderr,"  (Image is %u by %u)\n", g_beadseye_size, g_beadseye_size);
    if (g_camera) { delete g_camera; g_camera = NULL; }
    cleanup();
    exit(-1);
  }

  //------------------------------------------------------------------
  // Create the window that will be used for the "Landscape view"
  if (g_use_gui) {
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowPosition(375 + 10 + g_beadseye_size + g_window_offset_x, 140 + g_window_offset_y);
    glutInitWindowSize(g_landscape_size + g_landscape_strip_width, g_landscape_size);
    g_landscape_window = glutCreateWindow("Landscape");
  }

  // Create the floating-point buffer that will hold the fitness values before
  // they are scaled to match the displayable range.
  if ( (g_landscape_floats = new vrpn_float32
      [g_landscape_size * g_landscape_size]) == NULL) {
    fprintf(stderr,"Out of memory when allocating landscape float buffer!\n");
    fprintf(stderr,"  (Image is %u by %u)\n", g_landscape_size, g_landscape_size);
    if (g_camera) { delete g_camera; g_camera = NULL; }
    cleanup();
    exit(-1);
  }

  // Create the buffer that Glut will use to send to the landscape window.  This is allocating an
  // RGBA buffer.  It needs to be 4-byte aligned, so we allocated it as a group of
  // words and then cast it to the right type.  We're using RGBA rather than just RGB
  // because it also solves the 4-byte alignment problem caused by funky sizes of image
  // that are RGB images.
  if ( (g_landscape_image = (unsigned char *)(void*)new vrpn_uint32
      [g_landscape_size * g_landscape_size]) == NULL) {
    fprintf(stderr,"Out of memory when allocating landscape image!\n");
    fprintf(stderr,"  (Image is %u by %u)\n", g_landscape_size, g_landscape_size);
    if (g_camera) { delete g_camera; g_camera = NULL; }
    cleanup();
    exit(-1);
  }

  //------------------------------------------------------------------
  // Create the window that will be used for the kymograph
  if (g_use_gui) {
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowPosition(190 + g_camera->get_num_columns() + g_window_offset_x, 140 + g_window_offset_y);
    initializekymograph();
  }

  // Figure out how wide the kymograph needs to be based on the window size.
  g_kymograph_width = g_camera->get_num_columns();

  // Create the buffer that Glut will use to send to the kymograph window.  This is allocating an
  // RGBA buffer.  It needs to be 4-byte aligned, so we allocated it as a group of
  // words and then cast it to the right type.  We're using RGBA rather than just RGB
  // because it also solves the 4-byte alignment problem caused by funky sizes of image
  // that are RGB images.
  if ( (g_kymograph_image = (unsigned char *)(void*)new vrpn_uint32
      [g_kymograph_width * g_kymograph_height]) == NULL) {
    fprintf(stderr,"Out of memory when allocating kymograph image!\n");
    fprintf(stderr,"  (Image is %u by %u)\n", g_kymograph_width, g_kymograph_height);
    cleanup();
    exit(-1);
  }
  if (g_use_gui) {
    glutInitWindowSize(g_kymograph_width, g_kymograph_height);
    g_kymograph_window = glutCreateWindow("kymograph");
    glutInitWindowPosition(190 + g_camera->get_num_columns() + g_window_offset_x, 440 + g_window_offset_y);
    g_kymograph_center_window = glutCreateWindow("kymograph_center");
  }

  // Set the display functions for each window and idle function for GLUT (they
  // will do all the work) and then give control over to GLUT.
  if (g_use_gui) {
    glutSetWindow(g_tracking_window);
    glutDisplayFunc(myDisplayFunc);
    glutShowWindow();
    glutSetWindow(g_beadseye_window);
    glutDisplayFunc(myBeadDisplayFunc);
    glutHideWindow();
    glutSetWindow(g_landscape_window);
    glutDisplayFunc(myLandscapeDisplayFunc);
    glutHideWindow();
    glutSetWindow(g_kymograph_window);
    glutDisplayFunc(mykymographDisplayFunc);
    glutHideWindow();
    glutSetWindow(g_kymograph_center_window);
    glutDisplayFunc(mykymographCenterDisplayFunc);
    glutHideWindow();
    glutIdleFunc(myIdleFunc);
  }

  if (g_use_gui) {
    glutMainLoop();
  } else {
    while (true) { myIdleFunc(); }
  }
  // glutMainLoop() NEVER returns.  Wouldn't it be nice if it did when Glut was done?
  // Nope: Glut calls exit(0);

  return 0;
}
