//XXX Figure out mouse input so you can click on a place for it to go
//XXX At least with the roper, it crashes when you pick a sub-image
//XXX Image is inverted in Y...
//XXX PerfectOrbit video doesn't seem to play once it has been loaded!
//    Pausing does make it jump to the end, like other videos
//    LongOrbit behaves the same way... (both are large files)
//    BigBeadCatch plays herky-jerky, especially with larger radius optimizer
//    The test.avi program seemed to work smoothly.
//XXX Make it so you can click with the mouse where you want it to track.
//XXX Make it a vrpn_Tracker that reports the tracked position.
//XXX Make tracking not get lost so easily.
//XXX Pause seems to be broken on the video playback -- it skips to the end.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "roper_server.h"
#include "directx_camera_server.h"
#include "directx_videofile_server.h"
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

//--------------------------------------------------------------------------
// Version string for this program
const char *Version_string = "01.00";

//--------------------------------------------------------------------------
// Glut wants to take over the world when it starts, so we need to make
// global access to the objects we will be using.

base_camera_server  *g_camera;	    //< Camera used to get an image
image_wrapper	    *g_image;	    //< Image wrapper for the camera
directx_videofile_server  *g_video = NULL;  //< Video controls, if we have them
unsigned char	    *g_glut_image = NULL; //< Pointer to the storage for the image
disk_spot_tracker   g_tracker(5,true);   //< Follows the bead around.  Dark bead on light when this is true
bool		    g_ready_to_display = false;	//< Don't unless we get an image

//--------------------------------------------------------------------------
// Tcl controls and displays
Tclvar_float_with_scale	g_X("x", "", 0, 1391, 0);
Tclvar_float_with_scale	g_Y("y", "", 0, 1039, 0);
Tclvar_float_with_scale	g_Radius("radius", "", 1, 30, 5);
Tclvar_float_with_scale	g_minX("minX", "", 0, 1391, 0);
Tclvar_float_with_scale	g_maxX("maxX", "", 0, 1391, 1391);
Tclvar_float_with_scale	g_minY("minY", "", 0, 1039, 0);
Tclvar_float_with_scale	g_maxY("maxY", "", 0, 1039, 1039);
Tclvar_float_with_scale	g_exposure("exposure_millisecs", "", 1, 1000, 10);
Tclvar_int_with_button	g_opt("optimize","");
Tclvar_int_with_button	g_globalopt("global_optimize_now","",1);
Tclvar_int_with_button	g_mark("show_tracker","");
Tclvar_int_with_button	g_quit("quit","");
Tclvar_int_with_button	*g_play = NULL, *g_rewind = NULL;

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
    if (get_pixel_from_memory(x, y, val)) {
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
    if (get_pixel_from_memory(x, y, val)) {
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
    if (get_pixel_from_memory(x, y, val)) {
      result = val;
      return true;
    } else {
      return false;
    }
  }
};

/// Open the wrapped camera we want to use (Roper or DirectX)
bool  get_camera_and_imager(const char *type, base_camera_server **camera, image_wrapper **imager,
			    directx_videofile_server **video)
{
  if (!strcmp(type, "roper")) {
    roper_imager *r = new roper_imager();
    *camera = r;
    *imager = r;
  } else if (!strcmp(type, "directx")) {
    directx_imager *d = new directx_imager(1,640,480);	// Use camera #1 (first one found)
    *camera = d;
    *imager = d;
  } else {
    fprintf(stderr,"get_camera_and_imager(): Assuming filename (%s)\n", type);
    directx_file_imager	*f = new directx_file_imager(type);
    *camera = f;
    *video = f;
    *imager = f;
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
  if (g_glut_image) { delete [] g_glut_image; };
  if (g_play) { delete g_play; };
  if (g_rewind) { delete g_rewind; };
}

void myDisplayFunc(void)
{
  unsigned  r,c,ir;
  vrpn_uint16  uns_pix;

  if (!g_ready_to_display) { return; }

  // Clear the window and prepare to draw in the back buffer
  glDrawBuffer(GL_BACK);
  glClearColor(0.0, 0.0, 0.0, 0.0);
  glClear(GL_COLOR_BUFFER_BIT);

  // Copy pixels into the image buffer.  Flip the image over in
  // Y so that the image coordinates display correctly in OpenGL.
  for (r = 0; r < g_camera->get_num_rows(); r++) {
    ir = g_camera->get_num_rows() - r - 1;
    for (c = 0; c < g_camera->get_num_columns(); c++) {
      if (!g_camera->get_pixel_from_memory(c, r, uns_pix)) {
	fprintf(stderr, "Cannot read pixel from region\n");
	exit(-1);
      }
      
      // This assumes that the pixels are actually 8-bit values
      // and will clip if they go above this.  It also writes pixels
      // from the first channel into all colors of the image.
      g_glut_image[0 + 3 * (c + g_camera->get_num_columns() * ir)] = uns_pix >> 4;
      g_glut_image[1 + 3 * (c + g_camera->get_num_columns() * ir)] = uns_pix >> 4;
      g_glut_image[2 + 3 * (c + g_camera->get_num_columns() * ir)] = uns_pix >> 4;
    }
  }

  // Store the pixels from the image into the frame buffer
  // so that they cover the entire image (starting from lower-left
  // corner, which is at (-1,-1)).
  glRasterPos2f(-1, -1);
  glDrawPixels(g_camera->get_num_columns(),g_camera->get_num_rows(),
    GL_RGB, GL_UNSIGNED_BYTE, g_glut_image);

  // If we have been asked to show the tracking marker, draw it.
  if (g_mark) {
    // Normalize center and radius so that they match the coordinates
    // (-1..1) in X and Y.
    double  x = -1.0 + g_X * (2.0/g_camera->get_num_columns());
    double  y = -1.0 + g_Y * (2.0/g_camera->get_num_rows());
    double  dx = g_Radius * (2.0/g_camera->get_num_columns());
    double  dy = g_Radius * (2.0/g_camera->get_num_rows());

    // Invert y to make it match the correct image coordinate system
    y *= -1;

    glColor3f(1,0,0);
    glBegin(GL_LINES);
      glVertex2f(x-dx,y);
      glVertex2f(x+dx,y);
      glVertex2f(x,y-dy);
      glVertex2f(x,y+dy);
    glEnd();
  }

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
}

void myIdleFunc(void)
{
  // Read an image from the camera into memory, within a structure that
  // is wrapped by an image_wrapper object that the tracker can use.
  // Tell Glut that it can display an image.
  if (!g_camera->read_image_to_memory((int)g_minX,(int)g_maxX, (int)g_minY,(int)g_maxY, g_exposure)) {
    fprintf(stderr, "Can't read image to memory!\n");
    delete g_camera;
    exit(-1);
  }
  g_ready_to_display = true;

  if (g_opt) {
    double  x, y;
    g_tracker.set_radius(g_Radius);

    // If the user has requested a global search, do this once and then reset the
    // checkbox.  Otherwise, keep tracking the last feature found.
    if (g_globalopt) {
      g_tracker.locate_good_fit_in_image(*g_image, x,y);
      g_tracker.optimize(*g_image, x, y);
      g_globalopt = 0;
    } else {
      // Optimize to find the best fit starting from last position.
      // Don't let it adjust the radius here (otherwise it gets too
      // jumpy).
      g_tracker.optimize_xy(*g_image, x, y, g_X, g_Y);
    }

    // Show the result
    g_X = (float)x;
    g_Y = (float)y;
    g_Radius = (float)g_tracker.get_radius();
  }

  //------------------------------------------------------------
  // This must be done in any Tcl app, to allow Tcl/Tk to handle
  // events

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
  glutPostRedisplay();

  //------------------------------------------------------------
  // Time to quit?
  if (g_quit) {
    cleanup();
    exit(0);
  }
  
  // Sleep a little while so that we don't eat the whole CPU.
  vrpn_SleepMsecs(1);
}


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
    fprintf(stderr, "Usage: %s [filename]\n", argv[0]);
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
  sprintf(command, "label .versionlabel -text Roper_spot_tracker_v:_%s", Version_string);
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

  // Initialize GLUT and create the window that will display the
  // video -- name the window after the device that has been
  // opened in VRPN.
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
  glutInitWindowSize(g_camera->get_num_columns(), g_camera->get_num_rows());
  glutInitWindowPosition(5, 30);
  glutCreateWindow(device_name);

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
