//XXX The camera code seems to stop updating the video after about
//    five minutes of tracking.  The video in the window stops being
//    updated, the tracking optimization doesn't change, but the rest
//    of the UI keeps running (and the VRPN connect/disconnect).
//    After a while using the camera, it make DirectX think that there
//    was not a camera anymore.
//    This does not happen with the USB-connected Logitech camera,
//    it runs fine for many minutes, both with video showing and small
//    range (40 fps) and with video off (60fps).
//    It does not use up more GDI objects when it is locked up.  It
//    does seem to drop the camera offline.  Video doesn't have to be
//    off for this to happen.  This is using DirectX 9 on the laptop
//    and the Dazzle video converter.  Jeremy Cribb's Sony laptop.
//    The AMCAP example application runs fine for a long time.  Also
//    AMCAP needs to be run ahead of time to "turn on" the video from
//    the device -- it stays on after the program exits.  Maybe it is
//    timing out somehow?
//XXX Throttle to a requested frame rate (60fps, 30fps), or even better,
//    make directx driver not re-use existing video frames!
//XXX Would like to do multiple spots at the same time.
//XXX Would like to be able to specify the microns-per-pixel value
//    and have it stored in the log file.
//XXX Off-by-1 somewhere in Roper when binning (top line dark)
//XXX PerfectOrbit video doesn't seem to play once it has been loaded!
//    Pausing does make it jump to the end, like other videos
//    LongOrbit behaves the same way... (both are large files)
//    BigBeadCatch plays herky-jerky, especially with larger radius optimizer
//    The test.avi program seemed to work smoothly.
//XXX Pause seems to be broken on the video playback -- it skips to the end.
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
#include "image_wrapper.h"
#include "spot_tracker.h"
#include <tcl.h>
#include <tk.h>
#include "Tcl_Linkvar.h"
#ifdef	_WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <glut.h>
#include <vrpn_Connection.h>
#include <vrpn_Tracker.h>

//--------------------------------------------------------------------------
// Version string for this program
const char *Version_string = "01.05";

//--------------------------------------------------------------------------
// Glut wants to take over the world when it starts, so we need to make
// global access to the objects we will be using.

base_camera_server  *g_camera;	    //< Camera used to get an image
image_wrapper	    *g_image;	    //< Image wrapper for the camera
directx_videofile_server  *g_video = NULL;  //< Video controls, if we have them
unsigned char	    *g_glut_image = NULL; //< Pointer to the storage for the image
disk_spot_tracker   *g_tracker = new disk_spot_tracker(5,true);   //< Tracker to follow the bead, white-on-dark when false.
bool		    g_ready_to_display = false;	//< Don't unless we get an image
bool	g_already_posted = false;	//< Posted redisplay since the last display?
int	g_mousePressX, g_mousePressY;	//< Where the mouse was when the button was pressed
int		    g_shift = 0;	//< How many bits to shift right to get to 8
vrpn_Connection	*g_vrpn_connection = NULL;  //< Connection to send position over
vrpn_Tracker_Server *g_vrpn_tracker = NULL; //< Tracker server to send positions
vrpn_Connection	*g_client_connection = NULL;//< Connection on which to perform logging

//--------------------------------------------------------------------------
// Tcl controls and displays
void  logfilename_changed(char *newvalue, void *);
Tclvar_float_with_scale	g_X("x", "", 0, 1391, 0);
Tclvar_float_with_scale	g_Y("y", "", 0, 1039, 0);
Tclvar_float_with_scale	g_Radius("radius", "", 1, 30, 5);
Tclvar_float_with_scale	*g_minX;
Tclvar_float_with_scale	*g_maxX;
Tclvar_float_with_scale	*g_minY;
Tclvar_float_with_scale	*g_maxY;
Tclvar_float_with_scale	g_exposure("exposure_millisecs", "", 1, 1000, 10);
Tclvar_int_with_button	g_invert("dark_spot","",1);
Tclvar_int_with_button	g_opt("optimize","");
Tclvar_int_with_button	g_globalopt("global_optimize_now","",1);
Tclvar_int_with_button	g_small_area("small_area","");
Tclvar_int_with_button	g_full_area("full_area","");
Tclvar_int_with_button	g_mark("show_tracker","",1);
Tclvar_int_with_button	g_show_video("show_video","",1);
Tclvar_int_with_button	g_quit("quit","");
Tclvar_int_with_button	*g_play = NULL, *g_rewind = NULL;
Tclvar_selector		g_logfilenname("logfilename", NULL, NULL, "", logfilename_changed, NULL);

//--------------------------------------------------------------------------
// Cameras wrapped by image wrapper and function to return wrapped cameras.
class roper_imager: public roper_server, public image_wrapper
{
public:
  // XXX Starts with binning of 2 to get the processor load down to
  // where the update is around 1 Hz.
  roper_imager::roper_imager() : roper_server(2), image_wrapper() {} ;
  virtual void read_range(int &minx, int &maxx, int &miny, int &maxy) const {
    minx = _minX; miny = _minY; maxx = _maxX; maxy = _maxY;
  }
  virtual bool	read_pixel(int x, int y, double &result) const {
    uns16 val;
    if (get_pixel_from_memory(x, get_num_rows() - 1 - y, val)) {
      result = val;
      return true;
    } else {
      return false;
    }
  }
};

class diaginc_imager: public diaginc_server, public image_wrapper
{
public:
  // XXX Start with binning of 2 to get the processor load down
  diaginc_imager::diaginc_imager() : diaginc_server(1), image_wrapper() {} ;
  virtual void read_range(int &minx, int &maxx, int &miny, int &maxy) const {
    minx = _minX; miny = _minY; maxx = _maxX; maxy = _maxY;
  }
  virtual bool	read_pixel(int x, int y, double &result) const {
    uns16 val;
    if (get_pixel_from_memory(x, get_num_rows() - 1 - y, val)) {
      result = val;
      return true;
    } else {
      return false;
    }
  }
};

class directx_imager: public directx_camera_server, public image_wrapper
{
public:
  directx_imager(const int which, const unsigned w = 320, const unsigned h = 240) : directx_camera_server(which,w,h) {};
  virtual void read_range(int &minx, int &maxx, int &miny, int &maxy) const {
    minx = _minX; miny = _minY; maxx = _maxX; maxy = _maxY;
  }
  virtual bool	read_pixel(int x, int y, double &result) const {
    uns16 val;
    if (get_pixel_from_memory(x, get_num_rows() - 1 - y, val)) {
      result = val;
      return true;
    } else {
      return false;
    }
  }
};

class directx_file_imager: public directx_videofile_server, public image_wrapper
{
public:
  directx_file_imager(const char *fn) : directx_videofile_server(fn), image_wrapper() {};
  virtual void read_range(int &minx, int &maxx, int &miny, int &maxy) const {
    minx = _minX; miny = _minY; maxx = _maxX; maxy = _maxY;
  }
  virtual bool	read_pixel(int x, int y, double &result) const {
    uns16 val;
    if (get_pixel_from_memory(x, get_num_rows() - 1 - y, val)) {
      result = val;
      return true;
    } else {
      return false;
    }
  }
};

/// Open the wrapped camera we want to use (Roper, DiagInc or DirectX)
bool  get_camera_and_imager(const char *type, base_camera_server **camera, image_wrapper **imager,
			    directx_videofile_server **video)
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
    directx_file_imager	*f = new directx_file_imager(type);
    *camera = f;
    *video = f;
    *imager = f;
    g_shift = 0;
  }
  return true;
}

//--------------------------------------------------------------------------
// Glut callback routines.

static void  cleanup(void)
{
  // Done with the camera
  printf("Exiting\n");
  delete g_camera;
  delete g_tracker;
  if (g_glut_image) { delete [] g_glut_image; };
  if (g_play) { delete g_play; };
  if (g_rewind) { delete g_rewind; };
  if (g_vrpn_tracker) { delete g_vrpn_tracker; };
  if (g_vrpn_connection) { delete g_vrpn_connection; };
  if (g_client_connection) { delete g_client_connection; };
}

void myDisplayFunc(void)
{
  unsigned  r,c;
  vrpn_uint16  uns_pix;

  if (!g_ready_to_display) { return; }

  // Clear the window and prepare to draw in the back buffer
  glDrawBuffer(GL_BACK);
  glClearColor(0.0, 0.0, 0.0, 0.0);
  glClear(GL_COLOR_BUFFER_BIT);

  if (g_show_video) {
    // Copy pixels into the image buffer.  Flip the image over in
    // Y so that the image coordinates display correctly in OpenGL.
    for (r = *g_minY; r <= *g_maxY; r++) {
      for (c = *g_minX; c <= *g_maxX; c++) {
	if (!g_camera->get_pixel_from_memory(c, r, uns_pix)) {
	  fprintf(stderr, "Cannot read pixel from region\n");
	  cleanup();
	  exit(-1);
	}
      
	// This assumes that the pixels are actually 8-bit values
	// and will clip if they go above this.  It also writes pixels
	// from the first channel into all colors of the image.
	g_glut_image[0 + 3 * (c + g_camera->get_num_columns() * r)] = uns_pix >> g_shift;
	g_glut_image[1 + 3 * (c + g_camera->get_num_columns() * r)] = uns_pix >> g_shift;
	g_glut_image[2 + 3 * (c + g_camera->get_num_columns() * r)] = uns_pix >> g_shift;
      }
    }

    // Store the pixels from the image into the frame buffer
    // so that they cover the entire image (starting from lower-left
    // corner, which is at (-1,-1)).
    glRasterPos2f(-1, -1);
    glDrawPixels(g_camera->get_num_columns(),g_camera->get_num_rows(),
      GL_RGB, GL_UNSIGNED_BYTE, g_glut_image);
  }

  // If we have been asked to show the tracking marker, draw it.
  if (g_mark) {
    // Normalize center and radius so that they match the coordinates
    // (-1..1) in X and Y.
    double  x = -1.0 + g_X * (2.0/g_camera->get_num_columns());
    double  y = -1.0 + g_Y * (2.0/g_camera->get_num_rows());
    double  dx = g_Radius * (2.0/g_camera->get_num_columns());
    double  dy = g_Radius * (2.0/g_camera->get_num_rows());

    glColor3f(1,0,0);
    glBegin(GL_LINES);
      glVertex2f(x-dx,y);
      glVertex2f(x+dx,y);
      glVertex2f(x,y-dy);
      glVertex2f(x,y+dy);
    glEnd();
  }

  // Draw a green border around the selected area.  It will be beyond the
  // window border if it is set to the edges; it will just surround the
  // region being selected if it is inside the window.
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

void myIdleFunc(void)
{
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
    *g_maxY = g_camera->get_num_rows() -1;
    g_full_area = 0;
  }

  // If we are asking for a small region around the tracked dot,
  // set the borders to be around the dot.
  if (g_opt && g_small_area) {
    (*g_minX) = g_X - 4*g_Radius;
    (*g_minY) = g_Y - 4*g_Radius;
    (*g_maxX) = g_X + 4*g_Radius;
    (*g_maxY) = g_Y + 4*g_Radius;

    // Make sure not to push them off the screen
    if ((*g_minX) < 0) { (*g_minX) = 0; }
    if ((*g_minY) < 0) { (*g_minY) = 0; }
    if ((*g_maxX) >= g_camera->get_num_columns()) { (*g_maxX) = g_camera->get_num_columns() - 1; }
    if ((*g_maxY) >= g_camera->get_num_rows()) { (*g_maxY) = g_camera->get_num_rows() - 1; }
  }

  // Read an image from the camera into memory, within a structure that
  // is wrapped by an image_wrapper object that the tracker can use.
  // Tell Glut that it can display an image.
  if (!g_camera->read_image_to_memory((int)(*g_minX),(int)(*g_maxX), (int)(*g_minY),(int)(*g_maxY), g_exposure)) {
    fprintf(stderr, "Can't read image to memory!\n");
    cleanup();
    exit(-1);
  }
  g_ready_to_display = true;

  if (g_opt) {
    double  x, y;
    g_tracker->set_radius(g_Radius);

    // If the user has requested a global search, do this once and then reset the
    // checkbox.  Otherwise, keep tracking the last feature found.
    if (g_globalopt) {
      g_tracker->locate_good_fit_in_image(*g_image, x,y);
      g_tracker->optimize(*g_image, x, y);
      g_globalopt = 0;
    } else {
      // Optimize to find the best fit starting from last position.
      // Invert the Y values on the way in and out.
      // Don't let it adjust the radius here (otherwise it gets too
      // jumpy).
      g_tracker->optimize_xy(*g_image, x, y, g_X, g_camera->get_num_rows() - 1 - g_Y );
    }

    // Show the result
    g_X = (float)x;
    g_Y = (float)g_camera->get_num_rows() - 1 - y;
    g_Radius = (float)g_tracker->get_radius();

    // Update the VRPN tracker position and report it
    if (g_vrpn_tracker) {
      struct timeval now; gettimeofday(&now, NULL);
      vrpn_float64  pos[3] = {g_X, g_Y, 0};
      vrpn_float64  quat[4] = { 0, 0, 0, 1};

      g_vrpn_tracker->report_pose(0, now, pos, quat);
    }
  }

  //------------------------------------------------------------
  // If we have a video object, let the video controls operate
  // on it.
  if (g_video) {
    static  int	last_play = 0;

    // If the user has pressed rewind, go the the beginning of
    // the stream and then pause (by clearing play).
    if (*g_rewind) {
      g_video->rewind();
      *g_play = 0;
      *g_rewind = 0;
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
  }

  //------------------------------------------------------------
  // Post a redisplay so that Glut will draw the new image
  if (!g_already_posted) {
    glutPostRedisplay();
    g_already_posted = true;
  }

  //------------------------------------------------------------
  // Let the VRPN objects send their reports
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


void mouseCallbackForGLUT(int button, int state, int x, int y) {

    switch(button) {
	case GLUT_LEFT_BUTTON:
	  if (state == GLUT_DOWN) {
	    // Move the pointer to where the user clicked.
	    // Invert Y to match the coordinate systems.
	    g_X = x;
	    g_Y = g_camera->get_num_rows() - 1 - y;

	    // Record where the button was pressed so we can change radius
	    g_mousePressX = x;
	    g_mousePressY = y;

	    // Turn off optimization while we're picking.
	    g_opt = 0;
	  } else {
	    // Turn optimization on, global optimization off.
	    g_opt = 1;
	    g_globalopt = 0;
	  }
	  break;
	case GLUT_MIDDLE_BUTTON:
	  g_mousePressX = g_mousePressY = -1;
	  break;
	case GLUT_RIGHT_BUTTON:
	  g_mousePressX = g_mousePressY = -1;
	  break;
    }
}

void motionCallbackForGLUT(int x, int y) {

  // Make sure the mouse press was valid
  if ( (g_mousePressX < 0) || (g_mousePressY < 0) ) {
    return;
  }

  // Clip the motion to stay within the window boundaries.
  if (x < 0) { x = 0; };
  if (y < 0) { y = 0; };
  if (x >= (int)g_camera->get_num_columns()) { x = g_camera->get_num_columns() - 1; }
  if (y >= (int)g_camera->get_num_rows()) { y = g_camera->get_num_rows() - 1; };

  // Set the radius based on how far the user has moved from click
  double radius = sqrt( (x - g_mousePressX) * (x - g_mousePressX) + (y - g_mousePressY) * (y - g_mousePressY) );
  if (radius >= 3) {
    g_Radius = radius;
  }
  return;
}

void	handle_invert_change(int newvalue, void *userdata)
{
  delete g_tracker;
  g_tracker = new disk_spot_tracker(g_Radius, (newvalue != 0));
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
    g_client_connection = vrpn_get_connection_by_name("Spot@localhost", newvalue);
  }

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
    fprintf(stderr, "Usage: %s [roper|directx|directx640x480|filename]\n", argv[0]);
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
  // Load the specialized Tcl code needed by this program.
  sprintf(command, "source video_spot_tracker.tcl");
  if (Tcl_Eval(tk_control_interp, command) != TCL_OK) {
          fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
                  tk_control_interp->result);
          return(-1);
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
    g_play = new Tclvar_int_with_button("play_video","",1);
    g_rewind = new Tclvar_int_with_button("rewind_video","");
  }

  // Verify that the camera is working.
  if (!g_camera->working()) {
    fprintf(stderr,"Could not establish connection to camera\n");
    delete g_camera;
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
  // Set up callbacks to track changes on the Tcl side

  g_invert.set_tcl_change_callback(handle_invert_change, NULL);

  //------------------------------------------------------------------
  // Set up the VRPN server connection and the tracker object that will
  // report the position when tracking is turned on.
  g_vrpn_connection = new vrpn_Synchronized_Connection();
  g_vrpn_tracker = new vrpn_Tracker_Server("Spot", g_vrpn_connection);

  //------------------------------------------------------------------
  // Initialize GLUT and create the window that will display the
  // video -- name the window after the device that has been
  // opened in VRPN.
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
  glutInitWindowSize(g_camera->get_num_columns(), g_camera->get_num_rows());
  glutInitWindowPosition(5, 30);
  glutCreateWindow(device_name);
  glutMotionFunc(motionCallbackForGLUT);
  glutMouseFunc(mouseCallbackForGLUT);

  // Create the buffer that Glut will use to send to the screen
  if ( (g_glut_image = new unsigned char
      [g_camera->get_num_columns() * g_camera->get_num_rows() * 3]) == NULL) {
    fprintf(stderr,"Out of memory when allocating image!\n");
    fprintf(stderr,"  (Image is %u by %u)\n", g_camera->get_num_columns(), g_camera->get_num_rows());
    delete g_camera;
    exit(-1);
  }

  // Set the display function and idle function for GLUT (they
  // will do all the work) and then give control over to GLUT.
  glutDisplayFunc(myDisplayFunc);
  glutIdleFunc(myIdleFunc);
  glutMainLoop();

  cleanup();
  return 0;
}
