//---------------------------------------------------------------------------
// This section contains configuration settings for the Video Spot Tracker.
// It is used to make it possible to compile and link the code when one or
// more of the camera- or file- driver libraries are unavailable. First comes
// a list of definitions.  These should all be defined when building the
// program for distribution.  Following that comes a list of paths and other
// things that depend on these definitions.  They may need to be changed
// as well, depending on where the libraries were installed.

#define	VST_USE_ROPER
//#define USE_METAMORPH	    // Metamorph reader not completed.

#ifdef	VST_USE_ROPER
#pragma comment(lib,"D:\\Program Files\\Roper Scientific\\PVCAM\\pvcam32.lib")
#endif

// END configuration section.
//---------------------------------------------------------------------------

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tcl.h>
#include <tk.h>
#include "Tcl_Linkvar.h"
#ifdef	VST_USE_ROPER
#include "roper_server.h"
#endif
#include "directx_camera_server.h"
#include "directx_videofile_server.h"
#include "diaginc_server.h"
#include "edt_server.h"
#include "SEM_camera_server.h"
#include "file_stack_server.h"
#include "image_wrapper.h"
#include "spot_tracker.h"
#ifdef	_WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glut.h>
#include <quat.h>
#include <vrpn_Connection.h>
#include <vrpn_Analog.h>
#include <vrpn_Tracker.h>
// This pragma tells the compiler not to tell us about truncated debugging info
// due to name expansion within the string, list, and vector classes.
#pragma warning( disable : 4786 )
#include <list>
using namespace std;

//#define	DEBUG

static void cleanup();
static void dirtyexit();

const int MAX_TRACKERS = 100; // How many trackers can exist (for VRPN's tracker object)
#ifndef	M_PI
const double M_PI = 2*asin(1.0);
#endif

//--------------------------------------------------------------------------
// Version string for this program
const char *Version_string = "04.01";

//--------------------------------------------------------------------------
// Global constants

const int KERNEL_DISK = 0;	  //< These must match the values used in video_spot_tracker.tcl.
const int KERNEL_CONE = 1;
const int KERNEL_SYMMETRIC = 2;

//--------------------------------------------------------------------------
// Some classes needed for use in the rest of the program.

class Spot_Information {
public:
  Spot_Information(spot_tracker_XY *xytracker, spot_tracker_Z *ztracker) {
    d_tracker_XY = xytracker;
    d_tracker_Z = ztracker;
    d_index = d_static_index++;
    d_velocity[0] = d_acceleration[0] = d_velocity[1] = d_acceleration[1] = 0;
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

protected:
  spot_tracker_XY	*d_tracker_XY;	    //< The tracker we're keeping information for in XY
  spot_tracker_Z	*d_tracker_Z;	    //< The tracker we're keeping information for in Z
  unsigned		d_index;	    //< The index for this instance
  double		d_velocity[2];	    //< The velocity of the particle
  double		d_acceleration[2];  //< The acceleration of the particle
  static unsigned	d_static_index;     //< The index to use for the next one (never to be re-used).
};
unsigned  Spot_Information::d_static_index = 0;	  //< Start the first instance of a Spot_Information index at zero.

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

class SPE_Controllable_Video : public Controllable_Video, public spe_file_server {
public:
  SPE_Controllable_Video(const char *filename) : spe_file_server(filename) {};
  virtual ~SPE_Controllable_Video() {};
  void play(void) { spe_file_server::play(); }
  void pause(void) { spe_file_server::pause(); }
  void rewind(void) { pause(); spe_file_server::rewind(); }
  void single_step(void) { spe_file_server::single_step(); }
};

class FileStack_Controllable_Video : public Controllable_Video, public file_stack_server {
public:
  FileStack_Controllable_Video(const char *filename) : file_stack_server(filename) {};
  virtual ~FileStack_Controllable_Video() {};
  void play(void) { file_stack_server::play(); }
  void pause(void) { file_stack_server::pause(); }
  void rewind(void) { pause(); file_stack_server::rewind(); }
  void single_step(void) { file_stack_server::single_step(); }
};

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
// Glut wants to take over the world when it starts, so we need to make
// global access to the objects we will be using.

Tcl_Interp	    *g_tk_control_interp;

char		    *g_device_name = NULL;	  //< Name of the camera/video/file device to open
base_camera_server  *g_camera;			  //< Camera used to get an image
copy_of_image	    *g_last_image = NULL;	  //< Copy of the last image we had, if any
float		    g_search_radius = 0;	  //< Search radius for doing local max in before optimizing.
Controllable_Video  *g_video = NULL;		  //< Video controls, if we have them
Tclvar_int_with_button	g_frame_number("frame_number",NULL,-1);  //< Keeps track of video frame number

int		    g_tracking_window;		  //< Glut window displaying tracking
unsigned char	    *g_glut_image = NULL;	  //< Pointer to the storage for the image

list <Spot_Information *>g_trackers;		  //< List of active trackers
Spot_Information    *g_active_tracker = NULL;	  //< The tracker that the controls refer to
bool		    g_ready_to_display = false;	  //< Don't unless we get an image
bool		    g_already_posted = false;	  //< Posted redisplay since the last display?
int		    g_mousePressX, g_mousePressY; //< Where the mouse was when the button was pressed
int		    g_whichDragAction;		  //< What action to take for mouse drag
bool                g_tracker_is_lost = false;    //< Is there a lost tracker?

vrpn_Connection	    *g_vrpn_connection = NULL;    //< Connection to send position over
vrpn_Tracker_Server *g_vrpn_tracker = NULL;	  //< Tracker server to send positions
vrpn_Analog_Server  *g_vrpn_analog = NULL;        //< Analog server to report frame number
vrpn_Connection	    *g_client_connection = NULL;  //< Connection on which to perform logging
vrpn_Tracker_Remote *g_client_tracker = NULL;	  //< Client tracker object to case ping/pong server messages
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

//--------------------------------------------------------------------------
// Tcl controls and displays
void  logfilename_changed(char *newvalue, void *);
void  device_filename_changed(char *newvalue, void *);
void  rebuild_trackers(int newvalue, void *);
void  rebuild_trackers(float newvalue, void *);
void  set_debug_visibility(int newvalue, void *);
void  set_kymograph_visibility(int newvalue, void *);
void  set_maximum_search_radius(int newvalue, void *);
void  handle_optimize_z_change(int newvalue, void *);
Tclvar_float		g_X("x");
Tclvar_float		g_Y("y");
Tclvar_float		g_Z("z");
Tclvar_float_with_scale	g_Radius("radius", ".kernel.radius", 1, 30, 5);
Tclvar_float_with_scale	*g_minX;
Tclvar_float_with_scale	*g_maxX;
Tclvar_float_with_scale	*g_minY;
Tclvar_float_with_scale	*g_maxY;
Tclvar_float_with_scale	g_exposure("exposure_millisecs", "", 1, 1000, 10);
Tclvar_float_with_scale	g_colorIndex("red_green_blue", "", 0, 2, 0);
Tclvar_float_with_scale	g_bitdepth("bit_depth", "", 8, 12, 8);
Tclvar_float_with_scale g_precision("precision", "", 0.001, 1.0, 0.05, rebuild_trackers);
Tclvar_float_with_scale g_sampleSpacing("sample_spacing", "", 0.1, 1.0, 1.0, rebuild_trackers);
Tclvar_float_with_scale g_lossSensitivity("lost_tracking_sensitivity", "", 0.0, 1.0, 0.0);
Tclvar_int_with_button	g_invert("dark_spot",NULL,1, rebuild_trackers);
Tclvar_int_with_button	g_interpolate("interpolate",NULL,1, rebuild_trackers);
Tclvar_int_with_button	g_parabolafit("parabolafit",NULL,0);
Tclvar_int_with_button	g_areamax("areamax",NULL,0, set_maximum_search_radius);
Tclvar_int_with_button	g_predict("predict",NULL,0);
Tclvar_int_with_button	g_kernel_type("kerneltype", NULL, KERNEL_SYMMETRIC, rebuild_trackers);
Tclvar_int_with_button	g_rod("rod3",NULL,0, rebuild_trackers);
Tclvar_float_with_scale	g_length("length", ".rod3", 10, 50, 20);
Tclvar_float_with_scale	g_orientation("orient", ".rod3", 0, 359, 0);
Tclvar_int_with_button	g_opt("optimize",".kernel.optimize");
Tclvar_int_with_button	g_opt_z("optimize_z",".kernel.optimize", 0, handle_optimize_z_change);
Tclvar_selector		g_psf_filename("psf_filename", NULL, NULL, "");
Tclvar_int_with_button	g_round_cursor("round_cursor","");
Tclvar_int_with_button	g_small_area("small_area","");
Tclvar_int_with_button	g_full_area("full_area","");
Tclvar_int_with_button	g_mark("show_tracker","",1);
Tclvar_int_with_button	g_show_video("show_video","",1);
Tclvar_int_with_button	g_show_debug("show_debug","",0, set_debug_visibility);
Tclvar_int_with_button	g_show_clipping("show_clipping","",0);
Tclvar_int_with_button	g_kymograph("kymograph","",0, set_kymograph_visibility);
Tclvar_int_with_button	g_quit("quit",NULL);
Tclvar_int_with_button	*g_play = NULL, *g_rewind = NULL, *g_step = NULL;
Tclvar_selector		g_logfilename("logfilename", NULL, NULL, "", logfilename_changed, NULL);
Tclvar_int		g_log_relative("logging_relative");
double			g_log_offset_x, g_log_offset_y, g_log_offset_z;
bool g_video_valid = false; // Do we have a valid video frame in memory?
int		        g_log_frame_number_last_logged = -1;

//--------------------------------------------------------------------------
// Helper routine to get the Y coordinate right when going between camera
// space and openGL space.
double	flip_y(double y)
{
  return g_camera->get_num_rows() - 1 - y;
}


/// Open the wrapped camera we want to use depending on the name of the
//  camera we're trying to open.
bool  get_camera(const char *type, base_camera_server **camera, Controllable_Video **video)
{
#ifdef VST_USE_ROPER
  if (!strcmp(type, "roper")) {
    // XXX Starts with binning of 2 to get the image size down so that
    // it fits on the screen.
    roper_server *r = new roper_server(2);
    *camera = r;
    g_bitdepth = 12;
  } else
#endif  
  if (!strcmp(type, "diaginc")) {
    // XXX Starts with binning of 2 to get the image size down so that
    // it fits on the screen.
    diaginc_server *r = new diaginc_server(2);
    *camera = r;
    g_exposure = 80;	// Seems to be the minimum exposure for the one we have
    g_bitdepth = 12;
  } else if (!strcmp(type, "edt")) {
    edt_server *r = new edt_server();
    *camera = r;
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
    if (  (strcmp(".raw", &type[strlen(type)-4]) == 0) ||
	  (strcmp(".RAW", &type[strlen(type)-4]) == 0) ) {
      Pulnix_Controllable_Video *f = new Pulnix_Controllable_Video(type);
      *camera = f;
      *video = f;

    // If the extension is ".spe" then we assume it is a Roper file and open
    // it that way.
    } else if ( (strcmp(".spe", &type[strlen(type)-4]) == 0) ||
		(strcmp(".SPE", &type[strlen(type)-4]) == 0) ) {
      SPE_Controllable_Video *f = new SPE_Controllable_Video(type);
      *camera = f;
      *video = f;
      g_bitdepth = 12;

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

    // If the extension is ".stk"  then we assume it is a Metamorph file
    // to be opened by the Metamorph reader.
#ifdef	USE_METAMORPH
    } else if (strcmp(".stk", &type[strlen(type)-4]) == 0) {
      MetamorphStack_Controllable_Video *s = new MetamorphStack_Controllable_Video(type);
      *camera = s;
      *video = s;

    // File of unknown type.  We assume that it is something that DirectX knows
    // how to open.
#endif
    } else {
      Directx_Controllable_Video *f = new Directx_Controllable_Video(type);
      *camera = f;
      *video = f;
    }
  }
  return true;
}


/// Create a pointer to a new tracker of the appropriate type,
// given the global settings for interpolation and inversion.
// Return NULL on failure.

spot_tracker_XY  *create_appropriate_xytracker(void)
{
  // If we are using the oriented-rod kernel, we create a new one depending on the type
  // of subordinate spot tracker that is being used.
  if (g_rod) {
    if (g_kernel_type == KERNEL_SYMMETRIC) {
      symmetric_spot_tracker_interp *notused = NULL;
      g_interpolate = 1;
      return new rod3_spot_tracker_interp(notused, g_Radius,(g_invert != 0), g_precision, 0.1, g_sampleSpacing, g_length, 0.0);
    } else if (g_kernel_type == KERNEL_CONE) {
      cone_spot_tracker_interp *notused = NULL;
      g_interpolate = 1;
      return new rod3_spot_tracker_interp(notused, g_Radius,(g_invert != 0), g_precision, 0.1, g_sampleSpacing, g_length, 0.0);
    } else if (g_interpolate) {
      disk_spot_tracker_interp *notused = NULL;
      return new rod3_spot_tracker_interp(notused, g_Radius,(g_invert != 0), g_precision, 0.1, g_sampleSpacing, g_length, 0.0);
    } else {
      disk_spot_tracker *notused = NULL;
      return new rod3_spot_tracker_interp(notused, g_Radius,(g_invert != 0), g_precision, 0.1, g_sampleSpacing, g_length, 0.0);
    }

  // Not building a compound kernel, so just make a simple kernel of the appropriate type.
  } else {
    if (g_kernel_type == KERNEL_SYMMETRIC) {
      g_interpolate = 1;
      return new symmetric_spot_tracker_interp(g_Radius,(g_invert != 0), g_precision, 0.1, g_sampleSpacing);
    } else if (g_kernel_type == KERNEL_CONE) {
      g_interpolate = 1;
      return new cone_spot_tracker_interp(g_Radius,(g_invert != 0), g_precision, 0.1, g_sampleSpacing);
    } else if (g_interpolate) {
      return new disk_spot_tracker_interp(g_Radius,(g_invert != 0), g_precision, 0.1, g_sampleSpacing);
    } else {
      return new disk_spot_tracker(g_Radius,(g_invert != 0), g_precision, 0.1, g_sampleSpacing);
    }
  }
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


// This is called when someone kills the task by closing Glut or some
// other means we don't have control over.  If we try to delete the VRPN
// objects here, we get a seg fault for some reason.  VRPN must have already
// cleaned up our stuff for us.
static void  dirtyexit(void)
{
  static bool did_already = false;

  if (did_already) {
    return;
  } else {
    did_already = true;
  }

  // Done with the camera and other objects.
  printf("Exiting\n");

  list<Spot_Information *>::iterator  loop;

  for (loop = g_trackers.begin(); loop != g_trackers.end(); loop++) {
    delete *loop;
  }
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
  if (g_vrpn_connection) { delete g_vrpn_connection; g_vrpn_connection = NULL; };
  if (g_client_tracker) { delete g_client_tracker; g_client_tracker = NULL; };
  if (g_client_connection) { delete g_client_connection; g_client_connection = NULL; };
}

static	double	timediff(struct timeval t1, struct timeval t2)
{
	return (t1.tv_usec - t2.tv_usec) / 1e6 +
	       (t1.tv_sec - t2.tv_sec);
}

// XXX The start time needs to be reset whenever a new file is opened, probably,
// rather than once at the first logged message.
static	bool  save_log_frame(unsigned frame_number)
{
  static struct timeval start;
  static bool first_time = true;
  struct timeval now; gettimeofday(&now, NULL);

  g_log_frame_number_last_logged = frame_number;

  // Record the frame number into the log file
  g_vrpn_analog->channels()[0] = frame_number;
  g_vrpn_analog->report(vrpn_CONNECTION_RELIABLE);

  list<Spot_Information *>::iterator loop;
  for (loop = g_trackers.begin(); loop != g_trackers.end(); loop++) {
    vrpn_float64  pos[3] = {(*loop)->xytracker()->get_x() - g_log_offset_x,
			    flip_y((*loop)->xytracker()->get_y()) - g_log_offset_y,
			    0.0};
    if ((*loop)->ztracker()) { pos[2] = (*loop)->ztracker()->get_z() - g_log_offset_z; };
    vrpn_float64  quat[4] = { 0, 0, 0, 1};
    double orient = 0.0;
    double length = 0.0;

    // If we are tracking rods, then we adjust the orientation to match.
    if (g_rod) {
      // Horrible hack to make this work with rod type
      orient = static_cast<rod3_spot_tracker_interp*>((*loop)->xytracker())->get_orientation();
      length = static_cast<rod3_spot_tracker_interp*>((*loop)->xytracker())->get_length();
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
      fprintf(g_csv_file, "%d, %d, %lf,%lf,%lf, %lf, %lf,%lf\n", frame_number, (*loop)->index(), pos[0], pos[1], pos[2], (*loop)->xytracker()->get_radius(), orient, length);
    }
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
    int shift = g_bitdepth - 8;
    for (r = *g_minY; r <= *g_maxY; r++) {
      for (c = *g_minX; c <= *g_maxX; c++) {
	if (!g_camera->get_pixel_from_memory(c, r, uns_pix, g_colorIndex)) {
	  fprintf(stderr, "Cannot read pixel from region\n");
	  cleanup();
      	  exit(-1);
	}

	// This assumes that the pixels are actually 8-bit values
	// and will clip if they go above this.  It also writes pixels
	// from the first channel into all colors of the image.  It uses
	// RGBA so that we don't have to worry about byte-alignment problems
	// that plagued us when using RGB pixels.
	g_glut_image[0 + 4 * (c + g_camera->get_num_columns() * r)] = uns_pix >> shift;
	g_glut_image[1 + 4 * (c + g_camera->get_num_columns() * r)] = uns_pix >> shift;
	g_glut_image[2 + 4 * (c + g_camera->get_num_columns() * r)] = uns_pix >> shift;
	g_glut_image[3 + 4 * (c + g_camera->get_num_columns() * r)] = 255;

#ifdef DEBUG
	// If we're debugging, fill the border pixels with green
	if ( (r == *g_minY) || (r == *g_maxY) || (c == *g_minX) || (c == *g_maxX) ) {
	  g_glut_image[0 + 4 * (c + g_camera->get_num_columns() * r)] = 0;
	  g_glut_image[1 + 4 * (c + g_camera->get_num_columns() * r)] = 255;
	  g_glut_image[2 + 4 * (c + g_camera->get_num_columns() * r)] = 0;
	  g_glut_image[3 + 4 * (c + g_camera->get_num_columns() * r)] = 255;
	}
#endif
      }
    }

    // Store the pixels from the image into the frame buffer
    // so that they cover the entire image (starting from lower-left
    // corner, which is at (-1,-1)).
    glRasterPos2f(-1, -1);
#ifdef DEBUG
    printf("Drawing %dx%d pixels\n", g_camera->get_num_columns(), g_camera->get_num_rows());
#endif
    glDrawPixels(g_camera->get_num_columns(),g_camera->get_num_rows(),
      GL_RGBA, GL_UNSIGNED_BYTE, g_glut_image);
  }

  // If we have been asked to show the tracking markers, draw them.
  // The active one is drawn in red and the others are drawn in blue.
  // The markers may be either cross-hairs with a radius matching the
  // bead radius or a circle with a radius twice that of the bead; the
  // marker type depends on the g_round_cursor variable.
  if (g_mark) {
    list <Spot_Information *>::iterator loop;
    glEnable(GL_LINE_SMOOTH); //< Use smooth lines here to avoid aliasing showing spot in wrong place
    for (loop = g_trackers.begin(); loop != g_trackers.end(); loop++) {
      // Normalize center and radius so that they match the coordinates
      // (-1..1) in X and Y.
      double  x = -1.0 + (*loop)->xytracker()->get_x() * (2.0/g_camera->get_num_columns());
      double  y = -1.0 + ((*loop)->xytracker()->get_y()) * (2.0/g_camera->get_num_rows());
      double  dx = (*loop)->xytracker()->get_radius() * (2.0/g_camera->get_num_columns());
      double  dy = (*loop)->xytracker()->get_radius() * (2.0/g_camera->get_num_rows());

      if (*loop == g_active_tracker) {
	glColor3f(1,0,0);
      } else {
	glColor3f(0,0,1);
      }
      if (g_rod) {
	// Horrible hack to make this work with rod type
	double orient = static_cast<rod3_spot_tracker_interp*>((*loop)->xytracker())->get_orientation();
	double length = static_cast<rod3_spot_tracker_interp*>((*loop)->xytracker())->get_length();
	double dx = (length/2 * cos(orient * M_PI/180)) * (2.0/g_camera->get_num_columns());
	double dy = (length/2 * sin(orient * M_PI/180)) * (2.0/g_camera->get_num_rows());
	glBegin(GL_LINES);
	  glVertex2f(x-dx,y-dy);
	  glVertex2f(x+dx,y+dy);
	glEnd();
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

  // Draw a green border around the selected area.  It will be beyond the
  // window border if it is set to the edges; it will just surround the
  // region being selected if it is inside the window.
  glDisable(GL_LINE_SMOOTH);
  glColor3f(0,1,0);
  glBegin(GL_LINE_STRIP);
  glVertex3f( -1 + 2*((*g_minX-1) / (g_camera->get_num_columns()-1)),
	      -1 + 2*((*g_minY-1) / (g_camera->get_num_rows()-1)) , 0.0 );
  glVertex3f( -1 + 2*((*g_maxX+1) / (g_camera->get_num_columns()-1)),
	      -1 + 2*((*g_minY-1) / (g_camera->get_num_rows()-1)) , 0.0 );
  glVertex3f( -1 + 2*((*g_maxX+1) / (g_camera->get_num_columns()-1)),
	      -1 + 2*((*g_maxY+1) / (g_camera->get_num_rows()-1)) , 0.0 );
  glVertex3f( -1 + 2*((*g_minX-1) / (g_camera->get_num_columns()-1)),
	      -1 + 2*((*g_maxY+1) / (g_camera->get_num_rows()-1)) , 0.0 );
  glVertex3f( -1 + 2*((*g_minX-1) / (g_camera->get_num_columns()-1)),
	      -1 + 2*((*g_minY-1) / (g_camera->get_num_rows()-1)) , 0.0 );
  glEnd();

  // If we are running a kymograph, draw a green line through the first two
  // trackers to show where we're collecting it from
  if (g_kymograph && (g_trackers.size() >= 2)) {
    list<Spot_Information *>::iterator  loop = g_trackers.begin();
    double x0 = -1.0 + (*loop)->xytracker()->get_x() * (2.0/g_camera->get_num_columns());
    double y0 = -1.0 + ((*loop)->xytracker()->get_y()) * (2.0/g_camera->get_num_rows());
    loop++;
    double x1 = -1.0 + (*loop)->xytracker()->get_x() * (2.0/g_camera->get_num_columns());
    double y1 = -1.0 + ((*loop)->xytracker()->get_y()) * (2.0/g_camera->get_num_rows());

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
    double px = dy  * g_camera->get_num_rows() / g_camera->get_num_columns();
    double py = -dx * g_camera->get_num_columns() / g_camera->get_num_rows();

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
      double cx0 = -1.0 + (*loop)->xytracker()->get_x() * (2.0/g_camera->get_num_columns());
      double cy0 = -1.0 + ((*loop)->xytracker()->get_y()) * (2.0/g_camera->get_num_rows());
      loop++;
      double cx1 = -1.0 + (*loop)->xytracker()->get_x() * (2.0/g_camera->get_num_columns());
      double cy1 = -1.0 + ((*loop)->xytracker()->get_y()) * (2.0/g_camera->get_num_rows());

      // Find the center of the two (origin of cell coordinates) and the unit vector (dx,dy)
      // going towards the second from the first.
      double cxcenter = (cx0 + cx1) / 2;
      double cycenter = (cy0 + cy1) / 2;
      double cdist = sqrt( (cx1-cx0)*(cx1-cx0) + (cy1-cy0)*(cy1-cy0) );
      if (cdist == 0) { cdist = 1; }  //< Avoid divide-by-zero below.
      double cdx = (cx1 - cx0) / cdist;
      double cdy = (cy1 - cy0) / cdist;

      // Find the perpendicular to the line between them
      double cpx = cdy  * g_camera->get_num_rows() / g_camera->get_num_columns();
      double cpy = -cdx * g_camera->get_num_columns() / g_camera->get_num_rows();

      // Draw the line
      glBegin(GL_LINES);
	glVertex2f(cxcenter + 500 * cpx, cycenter + 500 * cpy);
	glVertex2f(cxcenter - 500 * cpx, cycenter - 500 * cpy);
      glEnd();
    }
  }

  // Swap buffers so we can see it.
  if (g_show_video || g_mark) {
    glutSwapBuffers();
  }

  // Capture timing information and print out how many frames per second
  // are being drawn.

  { static struct timeval last_print_time;
    struct timeval now;
    static bool first_time = true;
    static int frame_count = 0;

    if (first_time) {
      gettimeofday(&last_print_time, NULL);
      first_time = false;
    } else {
      frame_count++;
      gettimeofday(&now, NULL);
      double timesecs = 0.001 * vrpn_TimevalMsecs(vrpn_TimevalDiff(now, last_print_time));
      if (timesecs >= 5) {
	double frames_per_sec = frame_count / timesecs;
	frame_count = 0;
	printf("Displayed frames per second = %lg\n", frames_per_sec);
	last_print_time = now;
      }
    }
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
  glDrawBuffer(GL_BACK);
  glClearColor(0.0, 0.0, 0.0, 0.0);
  glClear(GL_COLOR_BUFFER_BIT);

  if (g_show_video && g_active_tracker) {
    // Copy pixels into the image buffer.
    int x,y;
    float xImageOffset = g_active_tracker->xytracker()->get_x() - (g_beadseye_size+1)/2.0;
    float yImageOffset = g_active_tracker->xytracker()->get_y() - (g_beadseye_size+1)/2.0;
    double  double_pix;

    // If we are outside 2 radii, then leave it blank to avoid having a
    // moving border that will make us think the spot is moving when it
    // is not.
    for (x = 0; x < x; x++) {
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

    int shift = g_bitdepth - 8;
    for (x = min_x; x < max_x; x++) {
      for (y = min_y; y < max_y; y++) {
	if (!g_camera->read_pixel_bilerp(x+xImageOffset, y+yImageOffset, double_pix)) {
	  g_beadseye_image[0 + 4 * (x + g_beadseye_size * (y))] = 0;
	  g_beadseye_image[1 + 4 * (x + g_beadseye_size * (y))] = 0;
	  g_beadseye_image[2 + 4 * (x + g_beadseye_size * (y))] = 0;
	  g_beadseye_image[3 + 4 * (x + g_beadseye_size * (y))] = 255;
	} else {
	  g_beadseye_image[0 + 4 * (x + g_beadseye_size * (y))] = ((uns16)(double_pix)) >> shift;
	  g_beadseye_image[1 + 4 * (x + g_beadseye_size * (y))] = ((uns16)(double_pix)) >> shift;
	  g_beadseye_image[2 + 4 * (x + g_beadseye_size * (y))] = ((uns16)(double_pix)) >> shift;
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
	this_val = g_active_tracker->xytracker()->check_fitness(*g_camera);
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
      this_val = g_active_tracker->xytracker()->check_fitness(*g_camera);
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
    // when we rewind.
    if (*g_rewind) {
      *g_play = 0;
      *g_rewind = 0;
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
    *g_maxX = g_camera->get_num_columns() - 1;
    *g_maxY = g_camera->get_num_rows() - 1;
    g_full_area = 0;
  }

  // If we are asking for a small region around the tracked dot,
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
    if ((*g_maxX) >= g_camera->get_num_columns()) { (*g_maxX) = g_camera->get_num_columns() - 1; }
    if ((*g_maxY) >= g_camera->get_num_rows()) { (*g_maxY) = g_camera->get_num_rows() - 1; }
  }

  // If we're doing a search for local maximum during optimization, then make a
  // copy of the previous image before reading a new one.

  if (g_search_radius > 0) {
    if (g_last_image == NULL) {
      g_last_image = new copy_of_image(*g_camera);
    } else {
      *g_last_image = *g_camera;
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

  if (!g_tracker_is_lost) {
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
  #ifdef	_WIN32
	  if (!PlaySound("end_of_video.wav", NULL, SND_FILENAME | SND_ASYNC)) {
	    fprintf(stderr,"Cannot play sound %s\n", "end_of_video.wav");
	  }
  #endif
	  *g_play = 0;
        }
      }
    } else {
      // Got a valid video frame; can log it.  Add to the frame number.
      g_video_valid = true;
      g_frame_number++;
    }
  }
  g_ready_to_display = true;

  if (g_active_tracker) { 
    g_active_tracker->xytracker()->set_radius(g_Radius);
    if (g_rod) {
      // Horrible hack to enable adjustment of a parameter that only exists on
      // the rod tracker
      static_cast<rod3_spot_tracker_interp*>(g_active_tracker->xytracker())->set_length(g_length);
      static_cast<rod3_spot_tracker_interp*>(g_active_tracker->xytracker())->set_orientation(g_orientation);
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
    if (g_vrpn_tracker && g_video_valid) {
      if (!save_log_frame(g_frame_number-1)) {
	fprintf(stderr,"Could not save data to log file\n");
	cleanup();
	exit(-1);
      }
    }
  }

  // Nobody is known to be lost yet...
  g_tracker_is_lost = false;

  // Optimize all trackers and see if they are lost.
  if (g_opt && g_active_tracker) {
    double  x, y;

    // This variable is used to determine if we should consider doing maximum fits,
    // by determining if the frame number has changed since last time.
    static int last_optimized_frame_number = -1000;

    // Optimize to find the best fit starting from last position for each tracker.
    // Invert the Y values on the way in and out.
    // Don't let it adjust the radius here (otherwise it gets too
    // jumpy).
    int kymocount = 0;
    for (loop = g_trackers.begin(); loop != g_trackers.end(); loop++) {
      double last_pos[2];
      last_pos[0] = (*loop)->xytracker()->get_x();
      last_pos[1] = (*loop)->xytracker()->get_y();
      double last_vel[2];
      (*loop)->get_velocity(last_vel);

      // If we are doing prediction, apply the estimated last velocity to
      // move the estimated position to a new location
      if ( g_predict && (last_optimized_frame_number != g_frame_number) ) {
	double new_pos[2];
	new_pos[0] = last_pos[0] + last_vel[0];
	new_pos[1] = last_pos[1] + last_vel[1];
	(*loop)->xytracker()->set_location(new_pos[0], new_pos[1]);
      }

      // If the frame-number has changed, and we are doing global searches
      // within a radius, then create an image-based tracker at the last
      // location for the current tracker on the last frame; scan all locations
      // within radius and find the maximum fit on this frame; move the tracker
      // location to that maximum before doing the optimization.  We use the
      // image-based tracker for this because other trackers may have maximum
      // fits outside the region where the bead is -- the symmetric tracker often
      // has a local maximum at best fit and global maxima elsewhere.
      if ( g_last_image && (g_search_radius > 0) && (last_optimized_frame_number != g_frame_number) ) {

	double x_base = (*loop)->xytracker()->get_x();
	double y_base = (*loop)->xytracker()->get_y();

	// If we are doing prediction, reduce the search radius to 3 because we rely
	// on the prediction to get us close to the global maximum.  If we make it 2.5,
	// it starts to lose track with an accuracy of 1 pixel for a bead on cilia in
	// pulnix video (acquired at 120 frames/second).
	double used_search_radius = g_search_radius;
	if ( g_predict && (g_search_radius > 3) ) {
	  used_search_radius = 3;
	}

	// Create an image spot tracker and initize it at the location where the current
	// tracker started this frame (before prediction), but in the last image.  Grab enough
	// of the image that we will be able to check over the used_search_radius for a match.
	// Use the faster twolines version of the image-based tracker.
	twolines_image_spot_tracker_interp max_find((*loop)->xytracker()->get_radius(), (g_invert != 0), g_precision,
	  0.1, g_sampleSpacing);
	max_find.set_location(last_pos[0], last_pos[1]);
	max_find.set_image(*g_last_image, last_pos[0], last_pos[1], (*loop)->xytracker()->get_radius() + used_search_radius);

	// Loop over the pixels within used_search_radius of the initial location and find the
	// location with the best match over all of these points.  Do this in the current image,
	// at the (possibly-predicted) starting location and find the offset from the (possibly
	// predicted) current location to get to the right place.
	double radsq = used_search_radius * used_search_radius;
	int x_offset, y_offset;
	int best_x_offset = 0;
	int best_y_offset = 0;
	double best_value = max_find.check_fitness(*g_camera);
	for (x_offset = -floor(used_search_radius); x_offset <= floor(used_search_radius); x_offset++) {
	  for (y_offset = -floor(used_search_radius); y_offset <= floor(used_search_radius); y_offset++) {
	    if ( (x_offset * x_offset) + (y_offset * y_offset) <= radsq) {
	      max_find.set_location(x_base + x_offset, y_base + y_offset);
	      double val = max_find.check_fitness(*g_camera);
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
	(*loop)->xytracker()->set_location(x_base + best_x_offset, y_base + best_y_offset);
      }

      // Here's where the tracker is optimized to its new location
      if (g_parabolafit) {
	(*loop)->xytracker()->optimize_xy_parabolafit(*g_camera, x, y, (*loop)->xytracker()->get_x(), (*loop)->xytracker()->get_y() );
      } else {
	(*loop)->xytracker()->optimize_xy(*g_camera, x, y, (*loop)->xytracker()->get_x(), (*loop)->xytracker()->get_y() );
      }

      // If we are doing prediction, update the estimated velocity based on the
      // step taken.
      if ( g_predict && (last_optimized_frame_number != g_frame_number) ) {
	const double vel_frac_to_use = 0.9; //< 0.85-0.95 seems optimal for cilia in pulnix; 1 is too much, 0.83 is too little
	const double acc_frac_to_use = 0.0; //< Due to the noise, acceleration estimation makes things worse

	// Compute the new velocity estimate as the subtraction of the
	// last position from the current position.
	double new_vel[2];
	new_vel[0] = ((*loop)->xytracker()->get_x() - last_pos[0]) * vel_frac_to_use;
	new_vel[1] = ((*loop)->xytracker()->get_y() - last_pos[1]) * vel_frac_to_use;

	// Compute the new acceleration estimate as the subtraction of the new
	// estimate from the old.
	double new_acc[2];
	new_acc[0] = (new_vel[0] - last_vel[0]) * acc_frac_to_use;
	new_acc[1] = (new_vel[1] - last_vel[1]) * acc_frac_to_use;

	// Re-estimate the new velocity by taking into account the acceleration.
	new_vel[0] += new_acc[0];
	new_vel[1] += new_acc[1];

	// Store the quantities for use next time around.
	(*loop)->set_velocity(new_vel);
	(*loop)->set_acceleration(new_acc);
      }

      // If we have a non-zero threshold for determining if we are lost,
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
      if (g_lossSensitivity > 0.0) {
        double min_val = 1e100; //< Min over all circles
        double start_x = (*loop)->xytracker()->get_x();
        double start_y = (*loop)->xytracker()->get_y();
        double this_val;
        double r, theta;
        double x, y;
        // Only look every two-pixel radius from three out to the radius of the tracker.
        for (r = 3; r <= (*loop)->xytracker()->get_radius(); r += 2) {
          double max_val = -1e100;  //< Max over this particular circle
          // Look at every pixel around the circle.
          for (theta = 0; theta < 2*M_PI; theta += r / (2 * M_PI) ) {
            x = r * cos(theta);
            y = r * sin(theta);
	    (*loop)->xytracker()->set_location(x + start_x, y + start_y);
	    this_val = (*loop)->xytracker()->check_fitness(*g_camera);
	    if (this_val > max_val) { max_val = this_val; }
          }
	  if (max_val < min_val) { min_val = max_val; }
        }
        //Put the tracker back where it started.
        (*loop)->xytracker()->set_location(start_x, start_y);

        // See if we are lost.  The way we tell if we are lost or not depends
        // on the type of kernel we are using.  It also depends on the setting of
        // the "lost sensitivity" parameter, which varies from 0 (not sensitive at
        // all) to 1 (most sensitive).
        double starting_value = (*loop)->xytracker()->check_fitness(*g_camera);
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
            g_tracker_is_lost = true;
            g_active_tracker = *loop;
          }
        } else if (g_kernel_type == KERNEL_CONE) {
          // Differences (not factors) of 0-5 seem more appropriate for a quick test of the cone kernel.
          double difference = 5*g_lossSensitivity;
          if (starting_value - min_val < difference) {
            g_tracker_is_lost = true;
            g_active_tracker = *loop;
          }
        } else {  // Must be a disk kernel
          // Differences (not factors) of 0-5 seem more appropriate for a quick test of the disk kernel.
          double difference = 5*g_lossSensitivity;
          if (starting_value - min_val < difference) {
            g_tracker_is_lost = true;
            g_active_tracker = *loop;
          }
        }

        if (g_tracker_is_lost) {
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

          // XXX What to do about video that cannot be paused at the exact instant, like AVI?
          // XXX Do not attempt to pause live video, but do tell that we're lost -- only tell once.
        }
      }

      // If we are optimizing in Z, then do it here.
      if (g_opt_z) {
        double  z = 0;
        (*loop)->ztracker()->locate_close_fit_in_depth(*g_camera, (*loop)->xytracker()->get_x(), (*loop)->xytracker()->get_y(), z);
        (*loop)->ztracker()->optimize(*g_camera, (*loop)->xytracker()->get_x(), (*loop)->xytracker()->get_y(), z);
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

    last_optimized_frame_number = g_frame_number;
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
      int shift = g_bitdepth - 8;
      for (step = -g_kymograph_width/2.0; step <= g_kymograph_width / 2.0; step++) {

	// Figure out where to look in the image
	double x = xcenter + step * dx;
	double y = ycenter + step * dy;

	// Figure out where to put this in the kymograph
	int kx = (int)(step + g_kymograph_width/2.0);
	int ky = g_kymograph_filled;

	// Look up the pixel in the image and put it into the kymograph if we get a reading.
	if (!g_camera->read_pixel_bilerp(x, y, double_pix)) {
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
	  g_kymograph_image[0 + 4 * (kx + g_kymograph_width * (g_kymograph_height - 1 - ky))] = (uns16)(double_pix) >> shift;
	  g_kymograph_image[1 + 4 * (kx + g_kymograph_width * (g_kymograph_height - 1 - ky))] = (uns16)(double_pix) >> shift;
	  g_kymograph_image[2 + 4 * (kx + g_kymograph_width * (g_kymograph_height - 1 - ky))] = (uns16)(double_pix) >> shift;
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
    if (g_rod) {
      // Horrible hack to make this work with rod type
      g_orientation = static_cast<rod3_spot_tracker_interp*>(g_active_tracker->xytracker())->get_orientation();
      g_length = static_cast<rod3_spot_tracker_interp*>(g_active_tracker->xytracker())->get_length();
    }
  }

  //------------------------------------------------------------
  // Post a redisplay so that Glut will draw the new image
  if (!g_already_posted) {
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
  if (g_vrpn_connection) { g_vrpn_connection->mainloop(); }

  //------------------------------------------------------------
  // Let the logging connection do its thing if it is open.
  if (g_client_tracker != NULL) {
    g_client_tracker->mainloop();
  }
  if (g_client_connection != NULL) {
    g_client_connection->mainloop();
    g_client_connection->save_log_so_far();
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
	delete g_active_tracker->xytracker();
	if ( g_active_tracker->ztracker() ) {
	  delete g_active_tracker->ztracker();
	}
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

    return true;
  }
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

void mouseCallbackForGLUT(int button, int state, int x, int y)
{
    // Record where the button was pressed for use in the motion
    // callback, flipping the Y axis to make the coordinates match
    // image coordinates.
    g_mousePressX = x;
    g_mousePressY = y = flip_y(y);

    switch(button) {
      // The left button will create a new tracker and let the
      // user specify its radius if they move far enough away
      // from the pick point (it starts with a default of the same
      // as the current active tracker).
      // The new tracker becomes the active tracker.
      case GLUT_LEFT_BUTTON:
	if (state == GLUT_DOWN) {
	  // Make sure that we don't have too many trackers.  If we do, then don't do anything.
	  // The number is limited for purposes of logging to the maximum number of sensors in
	  // a VRPN tracker.
	  if (g_trackers.size() >= vrpn_TRACKER_MAX_SENSORS) {
	    fprintf(stderr, "Too many trackers, only %d allowed\n", vrpn_TRACKER_MAX_SENSORS);
	    return;
	  }

	  g_whichDragAction = 1;
	  g_trackers.push_back(new Spot_Information(create_appropriate_xytracker(),create_appropriate_ztracker()));
	  g_active_tracker = g_trackers.back();
	  g_active_tracker->xytracker()->set_location(x, y);
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

void motionCallbackForGLUT(int x, int y) {

  // Make mouse coordinates match image coordinates.
  y = flip_y(y);

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
      if (x >= (int)g_camera->get_num_columns()) { x = g_camera->get_num_columns() - 1; }
      if (y >= (int)g_camera->get_num_rows()) { y = g_camera->get_num_rows() - 1; };

      // Set the radius based on how far the user has moved from click
      double radius = sqrt( (x - g_mousePressX) * (x - g_mousePressX) + (y - g_mousePressY) * (y - g_mousePressY) );
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

void  logfilename_changed(char *newvalue, void *)
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

  // Close the old connection, if there was one.
  if (g_client_tracker != NULL) {
    delete g_client_tracker;
    g_client_tracker = NULL;
  }
  if (g_client_connection != NULL) {
    delete g_client_connection;
    g_client_connection = NULL;
  }

  // Close the old CSV log file, if there was one.
  if (g_csv_file != NULL) {
    fclose(g_csv_file);
    g_csv_file = NULL;
  }

  // Open a new connection and .csv file, if we have a non-empty name.
  if (strlen(newvalue) > 0) {

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
      fprintf(g_csv_file, "FrameNumber,Spot ID,X,Y,Z,Radius,Orientation (if meaningful),Length (if meaningful)\n");
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
    g_log_offset_z = g_active_tracker->ztracker()->get_z();
  }
}

// If the device filename becomes non-empty, then set the global
// device name to match what it is set to.

void  device_filename_changed(char *newvalue, void *)
{
  // Open a new connection and .csv file, if we have a non-empty name.
  if (strlen(newvalue) > 0) {
    g_device_name = new char[strlen(newvalue)+1];
    strcpy(g_device_name, newvalue);
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
      (*loop)->set_xytracker(create_appropriate_xytracker());
      (*loop)->set_ztracker(create_appropriate_ztracker());
      g_active_tracker = *loop;
    } else {
      delete (*loop)->xytracker();
      if ( (*loop)->ztracker() ) { delete (*loop)->ztracker(); }
      (*loop)->set_xytracker(create_appropriate_xytracker());
      (*loop)->set_ztracker(create_appropriate_ztracker());
    }
    (*loop)->xytracker()->set_location(x,y);
    (*loop)->xytracker()->set_radius(r);
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
// it will be set to the current bead radius
void  set_maximum_search_radius(int newvalue, void *)
{
  g_search_radius = g_Radius * newvalue;
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

int main(int argc, char *argv[])
{
  //------------------------------------------------------------------
  // If there is a command-line argument, treat it as the name of a
  // file that is to be loaded.
  switch (argc) {
  case 1:
    // No arguments, so ask the user for a file name
    g_device_name = NULL;
    break;
  case 2:
    // Filename argument: open the file specified.
    g_device_name = argv[1];
    break;

  default:
    fprintf(stderr, "Usage: %s [roper|edt|diaginc|directx|directx640x480|filename]\n", argv[0]);
    exit(-1);
  };
  
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
  // Open the camera.  If we have a video file, then
  // set up the Tcl controls to run it.  Also, report the frame number.
  if (!get_camera(g_device_name, &g_camera, &g_video)) {
    fprintf(stderr,"Cannot open camera\n");
    if (g_camera) { delete g_camera; g_camera = NULL; }
    cleanup();
    exit(-1);
  }
  if (g_video) {  // Put these in a separate control panel?
    // Start out paused at the beginning of the file.
    g_play = new Tclvar_int_with_button("play_video","",0);
    g_rewind = new Tclvar_int_with_button("rewind_video","",1);
    g_step = new Tclvar_int_with_button("single_step_video","");
    sprintf(command, "label .frametitle -text FrameNum");
    if (Tcl_Eval(g_tk_control_interp, command) != TCL_OK) {
	    fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
		    g_tk_control_interp->result);
	    return(-1);
    }
    sprintf(command, "pack .frametitle");
    if (Tcl_Eval(g_tk_control_interp, command) != TCL_OK) {
	    fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
		    g_tk_control_interp->result);
	    return(-1);
    }
    sprintf(command, "label .framevalue -textvariable frame_number");
    if (Tcl_Eval(g_tk_control_interp, command) != TCL_OK) {
	    fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
		    g_tk_control_interp->result);
	    return(-1);
    }
    sprintf(command, "pack .framevalue");
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
  // report the position when tracking is turned on.
  g_vrpn_connection = new vrpn_Synchronized_Connection();
  g_vrpn_tracker = new vrpn_Tracker_Server("Spot", g_vrpn_connection, MAX_TRACKERS);
  g_vrpn_analog = new vrpn_Analog_Server("FrameNumber", g_vrpn_connection, 1);

  //------------------------------------------------------------------
  // Initialize GLUT and create the window that will display the
  // video -- name the window after the device that has been
  // opened in VRPN.  Also set mouse callbacks.
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
  glutInitWindowPosition(175, 220);
  glutInitWindowSize(g_camera->get_num_columns(), g_camera->get_num_rows());
#ifdef DEBUG
  printf("initializing window to %dx%d\n", g_camera->get_num_columns(), g_camera->get_num_rows());
#endif
  g_tracking_window = glutCreateWindow(g_device_name);
  glutMotionFunc(motionCallbackForGLUT);
  glutMouseFunc(mouseCallbackForGLUT);
  glutKeyboardFunc(keyboardCallbackForGLUT);

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
  glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
  glutInitWindowPosition(375, 140);
  glutInitWindowSize(g_beadseye_size, g_beadseye_size);
  g_beadseye_window = glutCreateWindow("Tracked");

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
  glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
  glutInitWindowPosition(375 + 10 + g_beadseye_size, 140);
  glutInitWindowSize(g_landscape_size + g_landscape_strip_width, g_landscape_size);
  g_landscape_window = glutCreateWindow("Landscape");

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
  glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
  glutInitWindowPosition(190 + g_camera->get_num_columns(), 140);
  initializekymograph();

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
  glutInitWindowSize(g_kymograph_width, g_kymograph_height);
  g_kymograph_window = glutCreateWindow("kymograph");
  glutInitWindowPosition(190 + g_camera->get_num_columns(), 440);
  g_kymograph_center_window = glutCreateWindow("kymograph_center");

  // Set the display functions for each window and idle function for GLUT (they
  // will do all the work) and then give control over to GLUT.
  glutSetWindow(g_tracking_window);
  glutDisplayFunc(myDisplayFunc);
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

  glutMainLoop();
  // glutMainLoop() NEVER returns.  Wouldn't it be nice if it did when Glut was done?
  // Nope: Glut calls exit(0);

  return 0;
}
