//XXX Add "Bead's-eye view" of the data to see how well it is tracking
//    Basically, define the constant and then add a GUI for turning it on and off
//XXX Add "optimization-space" view showing how good fit is in local area
//XXX Add debug view showing where all points are being sampled
//XXX Put in times based on video timestamps for samples rather than real time when we have them.
//XXX Would like to have a .ini file or something to set the starting "save" directory.
//XXX Would like to be able to specify the microns-per-pixel value
//    and have it stored in the log file.
//XXX Off-by-1 somewhere in Roper when binning (top line dark)
//XXX When we don't find the camera (or file), in debug the code hangs on camera delete.
//XXX All of the Y coordinates seem to be inverted in this code compared
//    to the image-capture code.  Mouse, display, and video clipping.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "roper_server.h"
#include "directx_camera_server.h"
#include "directx_videofile_server.h"
#include "diaginc_server.h"
#include "edt_server.h"
#include "image_wrapper.h"
#include "spot_tracker.h"
#include <tcl.h>
#include <tk.h>
#include "Tcl_Linkvar.h"
#ifdef	_WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glut.h>
#include <vrpn_Connection.h>
#include <vrpn_Tracker.h>
#include <list>
using namespace std;

//#define	DEBUG
//#define	BEADSEYEVIEW

const int MAX_TRACKERS = 100; // How many trackers can exist (for VRPN's tracker object)
#ifndef	M_PI
const double M_PI = 2*asin(1.0);
#endif

//--------------------------------------------------------------------------
// Version string for this program
const char *Version_string = "01.24";

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
  void rewind(void) { directx_videofile_server::rewind(); }
  void single_step(void) { directx_videofile_server::single_step(); }
};

class Pulnix_Controllable_Video : public Controllable_Video, public edt_pulnix_raw_file_server {
public:
  Pulnix_Controllable_Video(const char *filename) : edt_pulnix_raw_file_server(filename) {};
  virtual ~Pulnix_Controllable_Video() {};
  void play(void) { edt_pulnix_raw_file_server::play(); }
  void pause(void) { edt_pulnix_raw_file_server::pause(); }
  void rewind(void) { edt_pulnix_raw_file_server::rewind(); }
  void single_step(void) { edt_pulnix_raw_file_server::single_step(); }
};

//--------------------------------------------------------------------------
// Glut wants to take over the world when it starts, so we need to make
// global access to the objects we will be using.

base_camera_server  *g_camera;	    //< Camera used to get an image
image_wrapper	    *g_image;	    //< Image wrapper for the camera
Controllable_Video  *g_video = NULL;  //< Video controls, if we have them
unsigned char	    *g_glut_image = NULL; //< Pointer to the storage for the image
unsigned char	    *g_beadseye_image = NULL; //< Pointer to the storage for the beads-eye image
list <spot_tracker *>g_trackers;    //< List of active trackers
spot_tracker  *g_active_tracker = NULL;	//< The tracker that the controls refer to
bool		    g_ready_to_display = false;	//< Don't unless we get an image
bool	g_already_posted = false;	  //< Posted redisplay since the last display?
int	g_mousePressX, g_mousePressY;	  //< Where the mouse was when the button was pressed
int	g_whichDragAction;		  //< What action to take for mouse drag
int		    g_shift = 0;	  //< How many bits to shift right to get to 8
vrpn_Connection	*g_vrpn_connection = NULL;  //< Connection to send position over
vrpn_Tracker_Server *g_vrpn_tracker = NULL; //< Tracker server to send positions
vrpn_Connection	*g_client_connection = NULL;//< Connection on which to perform logging
int	g_tracking_window;	     //< Glut window displaying tracking
int	g_beadseye_window;	     //< Glut window showing view from active bead
int	g_beadseye_size = 151;	     //< Size of beads-eye-view window XXX should be dynamic

//--------------------------------------------------------------------------
// Tcl controls and displays
void  logfilename_changed(char *newvalue, void *);
void  rebuild_trackers(int newvalue, void *);
void  rebuild_trackers(float newvalue, void *);
//XXX X and Y range should match the image range, like the region size controls are.
Tclvar_float_with_scale	g_X("x", "", 0, 1391, 0);
Tclvar_float_with_scale	g_Y("y", "", 0, 1039, 0);
Tclvar_float_with_scale	g_Radius("radius", ".kernel.radius", 1, 30, 5);
Tclvar_float_with_scale	*g_minX;
Tclvar_float_with_scale	*g_maxX;
Tclvar_float_with_scale	*g_minY;
Tclvar_float_with_scale	*g_maxY;
Tclvar_float_with_scale	g_exposure("exposure_millisecs", "", 1, 1000, 10);
Tclvar_float_with_scale	g_colorIndex("red_green_blue", "", 0, 2, 0);
Tclvar_float_with_scale g_precision("precision", "", 0.001, 1.0, 0.05, rebuild_trackers);
Tclvar_float_with_scale g_sampleSpacing("sample_spacing", "", 0.1, 1.0, 1.0, rebuild_trackers);
Tclvar_int_with_button	g_invert("dark_spot",".kernel.invert",1, rebuild_trackers);
Tclvar_int_with_button	g_interpolate("interpolate",".kernel.interp",1, rebuild_trackers);
Tclvar_int_with_button	g_cone("cone",".kernel.cone",0, rebuild_trackers);
Tclvar_int_with_button	g_symmetric("symmetric",".kernel.symmetric",0, rebuild_trackers);
Tclvar_int_with_button	g_opt("optimize",".kernel.optimize");
Tclvar_int_with_button	g_globalopt("global_optimize_now","",0);
Tclvar_int_with_button	g_round_cursor("round_cursor","");
Tclvar_int_with_button	g_small_area("small_area","");
Tclvar_int_with_button	g_full_area("full_area","");
Tclvar_int_with_button	g_mark("show_tracker","",1);
Tclvar_int_with_button	g_show_video("show_video","",1);
Tclvar_int_with_button	g_quit("quit","");
Tclvar_int_with_button	*g_play = NULL, *g_rewind = NULL, *g_step = NULL;
Tclvar_selector		g_logfilename("logfilename", NULL, NULL, "", logfilename_changed, NULL);
bool g_video_valid = false; // Do we have a valid video frame in memory?

//--------------------------------------------------------------------------
// Helper routine to get the Y coordinate right when going between camera
// space and openGL space.
double	flip_y(double y)
{
  return g_camera->get_num_rows() - 1 - y;
}

//--------------------------------------------------------------------------
// Cameras wrapped by image wrapper and function to return wrapped cameras.
class roper_imager: public roper_server, public image_wrapper
{
public:
  // XXX Starts with binning of 2 to get the image size down so that
  // it fits on the screen.
  roper_imager::roper_imager() : roper_server(3), image_wrapper() {} ;
  virtual void read_range(int &minx, int &maxx, int &miny, int &maxy) const {
    minx = _minX; miny = _minY; maxx = _maxX; maxy = _maxY;
  }
  virtual bool	read_pixel(int x, int y, double &result) const {
    uns16 val;
    if (get_pixel_from_memory(x, flip_y(y), val)) {
      result = val;
      return true;
    } else {
      return false;
    }
  }
  virtual double read_pixel_nocheck(int x, int y) const {
    uns16 val;
    get_pixel_from_memory(x, flip_y(y), val);
    return val;
  }
};

class diaginc_imager: public diaginc_server, public image_wrapper
{
public:
  // XXX Starts with binning of 2 to get the image size down so that
  // it fits on the screen.
  diaginc_imager::diaginc_imager() : diaginc_server(2), image_wrapper() {} ;
  virtual void read_range(int &minx, int &maxx, int &miny, int &maxy) const {
    minx = _minX; miny = _minY; maxx = _maxX; maxy = _maxY;
  }
  virtual bool	read_pixel(int x, int y, double &result) const {
    uns16 val;
    if (get_pixel_from_memory(x, flip_y(y), val)) {
      result = val;
      return true;
    } else {
      return false;
    }
  }
  virtual double read_pixel_nocheck(int x, int y) const {
    uns16 val;
    get_pixel_from_memory(x, flip_y(y), val);
    return val;
  }
};

class directx_imager: public directx_camera_server, public image_wrapper
{
public:
  directx_imager(const int which, const unsigned w = 0, const unsigned h = 0) : directx_camera_server(which,w,h) {};
  virtual void read_range(int &minx, int &maxx, int &miny, int &maxy) const {
    minx = _minX; miny = _minY; maxx = _maxX; maxy = _maxY;
  }
  virtual bool	read_pixel(int x, int y, double &result) const {
    uns16 val;
    if (get_pixel_from_memory(x, flip_y(y), val, g_colorIndex)) {
      result = val;
      return true;
    } else {
      return false;
    }
  }
  virtual double read_pixel_nocheck(int x, int y) const {
    uns16 val;
    get_pixel_from_memory(x, flip_y(y), val);
    return val;
  }
};

class directx_file_imager: public Directx_Controllable_Video, public image_wrapper
{
public:
  directx_file_imager(const char *fn) : Directx_Controllable_Video(fn), image_wrapper() {};
  virtual void read_range(int &minx, int &maxx, int &miny, int &maxy) const {
    minx = _minX; miny = _minY; maxx = _maxX; maxy = _maxY;
  }
  virtual bool	read_pixel(int x, int y, double &result) const {
    uns16 val;
    if (get_pixel_from_memory(x, flip_y(y), val, g_colorIndex)) {
      result = val;
      return true;
    } else {
      return false;
    }
  }
  virtual double read_pixel_nocheck(int x, int y) const {
    uns16 val;
    get_pixel_from_memory(x, flip_y(y), val);
    return val;
  }
};

class pulnix_file_imager: public Pulnix_Controllable_Video, public image_wrapper
{
public:
  pulnix_file_imager(const char *fn) : Pulnix_Controllable_Video(fn), image_wrapper() {};
  virtual void read_range(int &minx, int &maxx, int &miny, int &maxy) const {
    minx = _minX; miny = _minY; maxx = _maxX; maxy = _maxY;
  }
  virtual bool	read_pixel(int x, int y, double &result) const {
    uns16 val;
    if (get_pixel_from_memory(x, flip_y(y), val, g_colorIndex)) {
      result = val;
      return true;
    } else {
      return false;
    }
  }
  virtual double read_pixel_nocheck(int x, int y) const {
    uns16 val;
    get_pixel_from_memory(x, flip_y(y), val);
    return val;
  }
};

/// Open the wrapped camera we want to use (Roper, DiagInc or DirectX)
bool  get_camera_and_imager(const char *type, base_camera_server **camera, image_wrapper **imager,
			    Controllable_Video **video)
{
  if (!strcmp(type, "roper")) {
    roper_imager *r = new roper_imager();
    *camera = r;
    *imager = r;
    g_shift = 4;
  } else if (!strcmp(type, "diaginc")) {
    diaginc_imager *r = new diaginc_imager();
    *camera = r;
    *imager = r;
    g_shift = 4;
    g_exposure = 80;	// Seems to be the minimum exposure for the one we have
  } else if (!strcmp(type, "directx")) {
    // Passing width and height as zero leaves it open to whatever the camera has
    directx_imager *d = new directx_imager(1,0,0);	// Use camera #1 (first one found)
    *camera = d;
    *imager = d;
    g_shift = 0;
  } else if (!strcmp(type, "directx640x480")) {
    directx_imager *d = new directx_imager(1,640,480);	// Use camera #1 (first one found)
    *camera = d;
    *imager = d;
    g_shift = 0;
  } else {
    fprintf(stderr,"get_camera_and_imager(): Assuming filename (%s)\n", type);

    // If the extension is ".raw", then we assume it is a Pulnix file and open
    // it that way.  Otherwise, we assume it is something that DirectX can open.
    if (strcmp(".raw", &type[strlen(type)-4]) == 0) {
      pulnix_file_imager *f = new pulnix_file_imager(type);
      *camera = f;
      *video = f;
      *imager = f;
      g_shift = 0;
    } else {
      directx_file_imager *f = new directx_file_imager(type);
      *camera = f;
      *video = f;
      *imager = f;
      g_shift = 0;
    }
  }
  return true;
}


/// Create a pointer to a new tracker of the appropriate type,
// given the global settings for interpolation and inversion.
// Return NULL on failure.

spot_tracker  *create_appropriate_tracker(void)
{
  if (g_symmetric) {
    g_interpolate = 1;
    g_cone = 0;
    return new symmetric_spot_tracker_interp(g_Radius,(g_invert != 0), g_precision, 0.1, g_sampleSpacing);
  } else if (g_cone) {
    g_interpolate = 1;
    return new cone_spot_tracker_interp(g_Radius,(g_invert != 0), g_precision, 0.1, g_sampleSpacing);
  } else if (g_interpolate) {
    return new disk_spot_tracker_interp(g_Radius,(g_invert != 0), g_precision, 0.1, g_sampleSpacing);
  } else {
    return new disk_spot_tracker(g_Radius,(g_invert != 0), g_precision, 0.1, g_sampleSpacing);
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


static void  cleanup(void)
{
  // Done with the camera and other objects.
  printf("Exiting\n");

  list<spot_tracker *>::iterator  loop;

  for (loop = g_trackers.begin(); loop != g_trackers.end(); loop++) {
    delete *loop;
  }
  delete g_camera;
  if (g_glut_image) { delete [] g_glut_image; };
  if (g_beadseye_image) { delete [] g_beadseye_image; };
  if (g_play) { delete g_play; };
  if (g_rewind) { delete g_rewind; };
  if (g_step) { delete g_step; };
  if (g_vrpn_tracker) { delete g_vrpn_tracker; };
  if (g_vrpn_connection) { delete g_vrpn_connection; };
  if (g_client_connection) { delete g_client_connection; };
}

void myDisplayFunc(void)
{
  unsigned  r,c;
  vrpn_uint16  uns_pix;
  int i;  //< Indexes the trackers.

  if (!g_ready_to_display) { return; }

  // Clear the window and prepare to draw in the back buffer
  glDrawBuffer(GL_BACK);
  glClearColor(0.0, 0.0, 0.0, 0.0);
  glClear(GL_COLOR_BUFFER_BIT);

  if (g_show_video) {
    // Copy pixels into the image buffer.  XXX Flip the image over in
    // Y so that the image coordinates display correctly in OpenGL.
#ifdef DEBUG
    printf("XXX Filling pixels %d,%d through %d,%d\n", (int)(*g_minX),(int)(*g_minY), (int)(*g_maxX), (int)(*g_maxY));
#endif
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
	g_glut_image[0 + 4 * (c + g_camera->get_num_columns() * r)] = uns_pix >> g_shift;
	g_glut_image[1 + 4 * (c + g_camera->get_num_columns() * r)] = uns_pix >> g_shift;
	g_glut_image[2 + 4 * (c + g_camera->get_num_columns() * r)] = uns_pix >> g_shift;
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
    printf("XXX Drawing %dx%d pixels\n", g_camera->get_num_columns(), g_camera->get_num_rows());
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
    list <spot_tracker *>::iterator loop;
    glEnable(GL_LINE_SMOOTH); //< Use smooth lines here to avoid aliasing showing spot in wrong place
    for (loop = g_trackers.begin(), i = 0; loop != g_trackers.end(); loop++, i++) {
      // Normalize center and radius so that they match the coordinates
      // (-1..1) in X and Y.
      double  x = -1.0 + (*loop)->get_x() * (2.0/g_camera->get_num_columns());
      double  y = -1.0 + flip_y((*loop)->get_y()) * (2.0/g_camera->get_num_rows());
      double  dx = (*loop)->get_radius() * (2.0/g_camera->get_num_columns());
      double  dy = (*loop)->get_radius() * (2.0/g_camera->get_num_rows());

      if (*loop == g_active_tracker) {
	glColor3f(1,0,0);
      } else {
	glColor3f(0,0,1);
      }
      if (g_round_cursor) {
	double stepsize = M_PI / (*loop)->get_radius();
	double runaround;
	glBegin(GL_LINE_STRIP);
	  for (runaround = 0; runaround <= 2*M_PI; runaround += stepsize) {
	    glVertex2f(x + 2*dx*cos(runaround),y + 2*dy*sin(runaround));
	  }
	  glVertex2f(x + 2*dx, y);  // Close the circle
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
      char numString[10];
      sprintf(numString,"%d", i);
      drawStringAtXY(x+dx,y, numString);
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

  // Swap buffers so we can see it.
  glutSwapBuffers();

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

  if (g_show_video) {
    // Copy pixels into the image buffer.  Flip the image over in
    // Y so that the image coordinates display correctly in OpenGL.
    int x,y;
    float xImageOffset = g_active_tracker->get_x() - (g_beadseye_size+1)/2.0;
    float yImageOffset = g_active_tracker->get_y() - (g_beadseye_size+1)/2.0;
    double  double_pix;
    for (x = 0; x < g_beadseye_size; x++) {
      for (y = 0; y < g_beadseye_size; y++) {
	if (!g_image->read_pixel_bilerp(x+xImageOffset, y+yImageOffset, double_pix)) {
	  g_beadseye_image[0 + 4 * (x + g_beadseye_size * (g_beadseye_size - 1 - y))] = 255;
	  g_beadseye_image[1 + 4 * (x + g_beadseye_size * (g_beadseye_size - 1 - y))] = 128;
	  g_beadseye_image[2 + 4 * (x + g_beadseye_size * (g_beadseye_size - 1 - y))] = 128;
	  g_beadseye_image[3 + 4 * (x + g_beadseye_size * (g_beadseye_size - 1 - y))] = 255;
	} else {
	  // This assumes that the pixels are actually 8-bit values
	  // and will clip if they go above this.  It also writes pixels
	  // from the first channel into all colors of the image.  It uses
	  // RGBA so that we don't have to worry about byte-alignment problems
	  // that plagued us when using RGB pixels.
	  g_beadseye_image[0 + 4 * (x + g_beadseye_size * (g_beadseye_size - 1 - y))] = (int)(double_pix) >> g_shift;
	  g_beadseye_image[1 + 4 * (x + g_beadseye_size * (g_beadseye_size - 1 - y))] = (int)(double_pix) >> g_shift;
	  g_beadseye_image[2 + 4 * (x + g_beadseye_size * (g_beadseye_size - 1 - y))] = (int)(double_pix) >> g_shift;
	  g_beadseye_image[3 + 4 * (x + g_beadseye_size * (g_beadseye_size - 1 - y))] = 255;
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

void myIdleFunc(void)
{
  list<spot_tracker *>::iterator loop;

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
  // bounding boxes.
  if (g_opt && g_small_area) {
    // Initialize it with values from the active tracker, which are stored
    // in the global variables.
    (*g_minX) = g_X - 4*g_Radius;
    (*g_minY) = g_Y - 4*g_Radius;
    (*g_maxX) = g_X + 4*g_Radius;
    (*g_maxY) = g_Y + 4*g_Radius;

    // Check them against all of the open trackers and push them to bound all
    // of them.

    list<spot_tracker *>::iterator  loop;
    for (loop = g_trackers.begin(); loop != g_trackers.end(); loop++) {
      double x = (*loop)->get_x();
      double y = flip_y((*loop)->get_y());
      double fourRad = 4 * (*loop)->get_radius();
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

  // Read an image from the camera into memory, within a structure that
  // is wrapped by an image_wrapper object that the tracker can use.
  // Tell Glut that it can display an image.
  // We ignore the error return if we're doing a video file because
  // this can happen due to timeouts when we're paused or at the
  // end of a file.
  if (!g_camera->read_image_to_memory((int)(*g_minX),(int)(*g_maxX), (int)(*g_minY),(int)(*g_maxY), g_exposure)) {
    if (!g_video) {
      fprintf(stderr, "Can't read image to memory!\n");
      cleanup();
      exit(-1);
    } else {
      // We timed out; either paused or at the end.  Don't log in this case.
      g_video_valid = false;
    }
  } else {
    // Got a valid video frame; can log it.
    g_video_valid = true;
  }
  g_ready_to_display = true;

  g_active_tracker->set_location(g_X, flip_y(g_Y));
  g_active_tracker->set_radius(g_Radius);
  if (g_opt) {
    double  x, y;

    // If the user has requested a global search, do this once for the
    // active tracker only and then reset the checkbox.
    if (g_globalopt) {
      g_active_tracker->locate_good_fit_in_image(*g_image, x,y);
      g_active_tracker->optimize(*g_image, x, y);
      g_globalopt = 0;
    }
    
    // Optimize to find the best fit starting from last position for each tracker.
    // Invert the Y values on the way in and out.
    // Don't let it adjust the radius here (otherwise it gets too
    // jumpy).
    for (loop = g_trackers.begin(); loop != g_trackers.end(); loop++) {
      (*loop)->optimize_xy(*g_image, x, y, (*loop)->get_x(), (*loop)->get_y() );
    }

    // Make the GUI track the result for the active tracker
    g_X = (float)g_active_tracker->get_x();
    g_Y = (float)flip_y(g_active_tracker->get_y());
    g_Radius = (float)g_active_tracker->get_radius();

    // Update the VRPN tracker position for each tracker and report it
    // using the same time value for each.  Don't do the update if we
    // don't currently have a valid video frame.  Sensors are indexed
    // by their position in the list.
    if (g_vrpn_tracker && g_video_valid) {
      int i = 0;
      struct timeval now; gettimeofday(&now, NULL);
      for (loop = g_trackers.begin(); loop != g_trackers.end(); loop++, i++) {
	vrpn_float64  pos[3] = {(*loop)->get_x(), flip_y((*loop)->get_y()), 0};
	vrpn_float64  quat[4] = { 0, 0, 0, 1};

	g_vrpn_tracker->report_pose(i, now, pos, quat);
      }
    }
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

    // If the user has pressed play, start the video playing.
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
    // video doesn't get paused.
    if (*g_rewind) {
      *g_play = 0;
      *g_rewind = 0;
      g_video->rewind();
    }
  }

  //------------------------------------------------------------
  // Post a redisplay so that Glut will draw the new image
  if (!g_already_posted) {
    glutSetWindow(g_tracking_window);
    glutPostRedisplay();
#ifdef BEADSEYEVIEW
    glutSetWindow(g_beadseye_window);
    glutPostRedisplay();
#endif
    g_already_posted = true;
  }

  //------------------------------------------------------------
  // Let the VRPN objects send their reports.
  if (g_vrpn_tracker) { g_vrpn_tracker->mainloop(); }
  if (g_vrpn_connection) { g_vrpn_connection->mainloop(); }

  //------------------------------------------------------------
  // Let the logging connection do its thing if it is open.
  if (g_client_connection != NULL) {
    g_client_connection->mainloop();
    g_client_connection->save_log_so_far();
  }

  //------------------------------------------------------------
  // Time to quit?
  if (g_quit) {
    cleanup();
    exit(0);
  }
  
  // Sleep a little while so that we don't eat the whole CPU.
  vrpn_SleepMsecs(1);
}

// This routine finds the tracker whose coordinates are
// the nearest to those specified, makes it the active
// tracker, and moved it to the specified location.
void  activate_and_drag_nearest_tracker_to(double x, double y)
{
  // Looks for the minimum squared distance and the tracker that is
  // there.
  double  minDist2 = 1e100;
  spot_tracker	*minTracker = NULL;
  double  dist2;
  list<spot_tracker *>::iterator loop;

  for (loop = g_trackers.begin(); loop != g_trackers.end(); loop++) {
    dist2 = (x - (*loop)->get_x())*(x - (*loop)->get_x()) +
      (y - (*loop)->get_y())*(y - (*loop)->get_y());
    if (dist2 < minDist2) {
      minDist2 = dist2;
      minTracker = *loop;
    }
  }
  if (minTracker == NULL) {
    fprintf(stderr, "No tracker to pick out of %d\n", g_trackers.size());
  } else {
    g_active_tracker = minTracker;
    g_active_tracker->set_location(x, y);
    g_X = g_active_tracker->get_x();
    g_Y = flip_y(g_active_tracker->get_y());
    g_Radius = g_active_tracker->get_radius();
  }
}


void mouseCallbackForGLUT(int button, int state, int x, int y) {

    // Record where the button was pressed for use in the motion
    // callback.
    g_mousePressX = x;
    g_mousePressY = y;

    switch(button) {
      // The right button will create a new tracker and let the
      // user specify its radius if they move far enough away
      // from the pick point (it starts with a default of the same
      // as the current active tracker).
      // The new tracker becomes the active tracker.
      case GLUT_RIGHT_BUTTON:
	if (state == GLUT_DOWN) {
	  g_whichDragAction = 1;
	  g_trackers.push_back(g_active_tracker = create_appropriate_tracker());
	  g_active_tracker->set_location(x, flip_y(y));

	  // Move the pointer to where the user clicked.
	  // Invert Y to match the coordinate systems.
	  g_X = x;
	  g_Y = flip_y(y);
	} else {
	  // Turn global optimization off when we release.
	  g_globalopt = 0;
	}
	break;

      case GLUT_MIDDLE_BUTTON:
	if (state == GLUT_DOWN) {
	  g_whichDragAction = 0;
	}
	break;

      // The left button will pull the closest existing tracker
      // to the location where the mouse button was pressed, and
      // then let the user specify the radius
      case GLUT_LEFT_BUTTON:
	if (state == GLUT_DOWN) {
	  g_whichDragAction = 1;
	  activate_and_drag_nearest_tracker_to(x,y);
	}
	break;
    }
}

void motionCallbackForGLUT(int x, int y) {

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
	g_active_tracker->set_radius(radius);
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

void  logfilename_changed(char *newvalue, void *)
{
  // Close the old connection, if there was one.
  if (g_client_connection != NULL) {
    delete g_client_connection;
    g_client_connection = NULL;
  }

  // Open a new connection, if we have a non-empty name.
  if (strlen(newvalue) > 0) {
    // Make sure that the file does not exist by deleting it if it does.
    // The Tcl code had a dialog box that asked the user if they wanted
    // to overwrite, so this is "safe."
    FILE *in_the_way;
    if ( (in_the_way = fopen(g_logfilename, "r")) != NULL) {
      fclose(in_the_way);
      int err;
      if ( (err=remove(g_logfilename)) != 0) {
	fprintf(stderr,"Error: could not delete existing logfile %s\n", (char*)(g_logfilename));
	perror("   Reason");
	exit(-1);
      }
    }

    g_client_connection = vrpn_get_connection_by_name("Spot@localhost", newvalue);
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
  list<spot_tracker *>::iterator  loop;

  for (loop = g_trackers.begin(); loop != g_trackers.end(); loop++) {
    double x = (*loop)->get_x();
    double y = (*loop)->get_y();
    double r = (*loop)->get_radius();
    if (g_active_tracker == *loop) {
      delete *loop;
      *loop = create_appropriate_tracker();
      g_active_tracker = *loop;
    } else {
      delete *loop;
      *loop = create_appropriate_tracker();
    }
    (*loop)->set_location(x,y);
    (*loop)->set_radius(r);
  }
}

// This version is for float sliders
void  rebuild_trackers(float newvalue, void *) {
  rebuild_trackers((int)newvalue, NULL);
}

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
  char	*device_name = "directx";

  //------------------------------------------------------------------
  // If there is a command-line argument, treat it as the name of a
  // file that is to be loaded.
  switch (argc) {
  case 1:
    // No arguments, so leave the camera device name alone
    break;
  case 2:
    // Filename argument: open the file specified.
    device_name = argv[1];
    break;

  default:
    fprintf(stderr, "Usage: %s [roper|diaginc|directx|directx640x480|filename]\n", argv[0]);
    exit(-1);
  };

  //------------------------------------------------------------------
  // Generic Tcl startup.  Getting and interpreter and mainwindow.

  char		command[256];
  Tcl_Interp	*tk_control_interp;
  Tk_Window       tk_control_window;
  tk_control_interp = Tcl_CreateInterp();

  /* Start a Tcl interpreter */
  if (Tcl_Init(tk_control_interp) == TCL_ERROR) {
          fprintf(stderr,
                  "Tcl_Init failed: %s\n",tk_control_interp->result);
          return(-1);
  }

  /* Start a Tk mainwindow to hold the widgets */
  if (Tk_Init(tk_control_interp) == TCL_ERROR) {
	  fprintf(stderr,
	  "Tk_Init failed: %s\n",tk_control_interp->result);
	  return(-1);
  }
  tk_control_window = Tk_MainWindow(tk_control_interp);
  if (tk_control_window == NULL) {
          fprintf(stderr,"%s\n", tk_control_interp->result);
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
  if (Tcl_Eval(tk_control_interp, command) != TCL_OK) {
          fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
                  tk_control_interp->result);
          return(-1);
  }

  //------------------------------------------------------------------
  // Put the version number into the main window.
  sprintf(command, "label .versionlabel -text Video_spot_tracker_v:_%s", Version_string);
  if (Tcl_Eval(tk_control_interp, command) != TCL_OK) {
          fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
                  tk_control_interp->result);
          return(-1);
  }
  sprintf(command, "pack .versionlabel");
  if (Tcl_Eval(tk_control_interp, command) != TCL_OK) {
          fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
                  tk_control_interp->result);
          return(-1);
  }

  //------------------------------------------------------------------
  // Load the specialized Tcl code needed by this program.  This must
  // be loaded before the Tclvar_init() routine is called because it
  // puts together some of the windows needed by the variables.
  sprintf(command, "source video_spot_tracker.tcl");
  if (Tcl_Eval(tk_control_interp, command) != TCL_OK) {
          fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
                  tk_control_interp->result);
          return(-1);
  }

  //------------------------------------------------------------------
  // This routine must be called in order to initialize all of the
  // variables that came into scope before the interpreter was set
  // up, and to tell the variables which interpreter to use.  It is
  // called once, after the interpreter exists.

  // Initialize the variables using the interpreter
  if (Tclvar_init(tk_control_interp)) {
	  fprintf(stderr,"Can't do init!\n");
	  return -1;
  }

  //------------------------------------------------------------------
  // Open the camera and image wrapper.  If we have a video file, then
  // set up the Tcl controls to run it.
  if (!get_camera_and_imager(device_name, &g_camera, &g_image, &g_video)) {
    fprintf(stderr,"Cannot open camera/imager\n");
    if (g_camera) { delete g_camera; }
    exit(-1);
  }
  if (g_video) {  // Put these in a separate control panel?
    // Start out paused at the beginning of the file.
    g_play = new Tclvar_int_with_button("play_video","",0);
    g_rewind = new Tclvar_int_with_button("rewind_video","",1);
    g_step = new Tclvar_int_with_button("single_step_video","");
  }
  sprintf(command, "wm geometry . +10+10");
  if (Tcl_Eval(tk_control_interp, command) != TCL_OK) {
          fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
                  tk_control_interp->result);
          return(-1);
  }

  // Verify that the camera is working.
  if (!g_camera->working()) {
    fprintf(stderr,"Could not establish connection to camera\n");
    if (g_camera) { delete g_camera; }
    exit(-1);
  }

  //------------------------------------------------------------------
  // Initialize the controls for the clipping based on the size of
  // the image we got.
  g_minX = new Tclvar_float_with_scale("minX", "", 0, g_camera->get_num_columns()-1, 0);
  g_maxX = new Tclvar_float_with_scale("maxX", "", 0, g_camera->get_num_columns()-1, g_camera->get_num_columns()-1);
  g_minY = new Tclvar_float_with_scale("minY", "", 0, g_camera->get_num_rows()-1, 0);
  g_maxY = new Tclvar_float_with_scale("maxY", "", 0, g_camera->get_num_rows()-1, g_camera->get_num_rows()-1);

  //------------------------------------------------------------------
  // Set up the VRPN server connection and the tracker object that will
  // report the position when tracking is turned on.
  g_vrpn_connection = new vrpn_Synchronized_Connection();
  g_vrpn_tracker = new vrpn_Tracker_Server("Spot", g_vrpn_connection, MAX_TRACKERS);

  //------------------------------------------------------------------
  // Set up an initial spot tracker based on the current settings.
  g_trackers.push_front(create_appropriate_tracker());
  g_active_tracker = g_trackers.front();
  g_active_tracker->set_location(0,0);
  g_active_tracker->set_radius(g_Radius);

  //------------------------------------------------------------------
  // Initialize GLUT and create the window that will display the
  // video -- name the window after the device that has been
  // opened in VRPN.  Also set mouse callbacks.
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
  glutInitWindowPosition(175, 140);
  glutInitWindowSize(g_camera->get_num_columns(), g_camera->get_num_rows());
#ifdef DEBUG
  printf("XXX initializing window to %dx%d\n", g_camera->get_num_columns(), g_camera->get_num_rows());
#endif
  g_tracking_window = glutCreateWindow(device_name);
  glutMotionFunc(motionCallbackForGLUT);
  glutMouseFunc(mouseCallbackForGLUT);

  // Create the buffer that Glut will use to send to the tracking window.  This is allocating an
  // RGBA buffer.  It needs to be 4-byte aligned, so we allocated it as a group of
  // words and then cast it to the right type.  We're using RGBA rather than just RGB
  // because it also solves the 4-byte alignment problem caused by funky sizes of image
  // that are RGB images.
  if ( (g_glut_image = (unsigned char *)(void*)new vrpn_uint32
      [g_camera->get_num_columns() * g_camera->get_num_rows()]) == NULL) {
    fprintf(stderr,"Out of memory when allocating image!\n");
    fprintf(stderr,"  (Image is %u by %u)\n", g_camera->get_num_columns(), g_camera->get_num_rows());
    delete g_camera;
    exit(-1);
  }

  //------------------------------------------------------------------
  // Create the window that will be used for the "Bead's-eye view"
#ifdef BEADSEYEVIEW
  glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
  glutInitWindowPosition(175, 140);
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
    delete g_camera;
    exit(-1);
  }
#endif

  // Set the display functions for each window and idle function for GLUT (they
  // will do all the work) and then give control over to GLUT.
  glutSetWindow(g_tracking_window);
  glutDisplayFunc(myDisplayFunc);
#ifdef BEADSEYEVIEW
  glutSetWindow(g_beadseye_window);
  glutDisplayFunc(myBeadDisplayFunc);
#endif
  glutIdleFunc(myIdleFunc);
  glutMainLoop();

  cleanup();
  return 0;
}
