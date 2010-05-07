//---------------------------------------------------------------------------
// This section contains configuration settings for the Stereo Spin appliaction.
// It is used to make it possible to compile and link the code when one or
// more of the camera- or file- driver libraries are unavailable. First comes
// a list of definitions.  These should all be defined when building the
// program for distribution.  Following that comes a list of paths and other
// things that depend on these definitions.  They may need to be changed
// as well, depending on where the libraries were installed.

#ifdef _WIN32
#define	VST_USE_DIRECTX
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
#include "directx_camera_server.h"
#include "directx_videofile_server.h"
#include "file_stack_server.h"
#include "image_wrapper.h"
#ifdef	_WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glut.h>
#include "controllable_video.h"

#include <quat.h>

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
const char *Version_string = "01.00";

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

int		    g_tracking_window;		  //< Glut window displaying tracking
unsigned char	    *g_glut_image = NULL;	  //< Pointer to the storage for the image

bool		    g_ready_to_display = false;	  //< Don't unless we get an image
bool		    g_already_posted = false;	  //< Posted redisplay since the last display?
int		    g_mousePressX, g_mousePressY; //< Where the mouse was when the button was pressed
int		    g_whichDragAction;		  //< What action to take for mouse drag

bool                g_quit_at_end_of_video = false; //< When we reach the end of the video, should we quit?

bool                g_use_gui = true;             //< Use 3D and 2D GUI?

bool g_gotNewFrame = false;

//--------------------------------------------------------------------------
// Tcl controls and displays
#ifdef	_WIN32
void  device_filename_changed(char *newvalue, void *);
#else
void  device_filename_changed(const char *newvalue, void *);
#endif
Tclvar_float_with_scale	*g_minX;
Tclvar_float_with_scale	*g_maxX;
Tclvar_float_with_scale	*g_minY;
Tclvar_float_with_scale	*g_maxY;
Tclvar_float_with_scale	g_exposure("exposure_millisecs", "", 1, 1000, 10);
Tclvar_float_with_scale	g_colorIndex("red_green_blue", NULL, 0, 2, 0);
Tclvar_float_with_scale	g_brighten("brighten", "", 0, 8, 0);
Tclvar_int_with_button	g_full_area("full_area",NULL);
Tclvar_int_with_button	g_show_video("show_video","",1);
Tclvar_int_with_button	g_opengl_video("use_texture_video","",1);
Tclvar_int_with_button	g_show_clipping("show_clipping","",0);
Tclvar_int_with_button	g_quit("quit",NULL);
Tclvar_int_with_button	*g_play = NULL, *g_rewind = NULL, *g_step = NULL;
bool g_video_valid = false; // Do we have a valid video frame in memory?

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
  g_quit = 1;  // Lets all the threads know that it is time to exit.

  if (did_already) {
    return;
  } else {
    did_already = true;
  }

  // Done with the camera and other objects.
  printf("Exiting\n");

  // Get rid of any trackers.
  if (g_camera) { delete g_camera; g_camera = NULL; }
  if (g_glut_image) { delete [] g_glut_image; g_glut_image = NULL; };
  if (g_play) { delete g_play; g_play = NULL; };
  if (g_rewind) { delete g_rewind; g_rewind = NULL; };
  if (g_step) { delete g_step; g_step = NULL; };
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
}

static	double	timediff(struct timeval t1, struct timeval t2)
{
	return (t1.tv_usec - t2.tv_usec) / 1e6 +
	       (t1.tv_sec - t2.tv_sec);
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

  // Swap buffers so we can see it.
  glutSwapBuffers();
  
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

      // Do the rewind and setting after clearing the logging, so that
      // the last logged frame has the correct index.
      g_video->rewind();
      g_frame_number = -1;
    }
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

  // Point the image to use at the camera's image.
  g_image = g_camera;

  // We're ready to display an image.
  g_ready_to_display = true;

  //------------------------------------------------------------
  // Post a redisplay so that Glut will draw the new image
  if (g_use_gui && !g_already_posted) {
    glutSetWindow(g_tracking_window);
    glutPostRedisplay();
    g_already_posted = true;
  }

  //------------------------------------------------------------
  // Time to quit?
  if (g_quit) {
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

void keyboardCallbackForGLUT(unsigned char key, int x, int y)
{
  switch (key) {
  case 'q':
  case 'Q':
    g_quit = 1;
    break;

  case 8:   // Backspace
  case 127: // Delete on Windows
     // Nothing to do for now.
    break;
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
	// Nothing to do at press.
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

  default:
    fprintf(stderr,"Internal Error: Unknown drag action (%d)\n", g_whichDragAction);
  }
  return;
}


//--------------------------------------------------------------------------
// Tcl callback routines.

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

//--------------------------------------------------------------------------

void Usage(const char *progname)
{
    fprintf(stderr, "Usage: %s [-nogui] [filename]\n", progname);
    fprintf(stderr, "       filename: The source file for tracking can be specified here (default is\n");
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
  sprintf(command, "label .versionlabel -text Stereo_Spin_v:%s", Version_string);
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
  sprintf(command, "source stereo_spin.tcl");
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

  for (i = 1; i < argc; i++) {
    if (!strncmp(argv[i], "-nogui", strlen("-nogui"))) {
      g_use_gui = false;
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
  // Open the camera.  If we have a video file, then
  // set up the Tcl controls to run it.  Also, report the frame number.
  float exposure = g_exposure;
  g_camera_bit_depth = 8;
  if (!get_camera(g_device_name, &g_camera_bit_depth, &exposure,
                  &g_camera, &g_video,
                  0,0,0,0,0)) {
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
  g_image = g_camera;

  //------------------------------------------------------------------
  // Initialize the controls for the clipping based on the size of
  // the image we got.
  g_minX = new Tclvar_float_with_scale("minX", ".clipping", 0, g_camera->get_num_columns()-1, 0);
  g_maxX = new Tclvar_float_with_scale("maxX", ".clipping", 0, g_camera->get_num_columns()-1, g_camera->get_num_columns()-1);
  g_minY = new Tclvar_float_with_scale("minY", ".clipping", 0, g_camera->get_num_rows()-1, 0);
  g_maxY = new Tclvar_float_with_scale("maxY", ".clipping", 0, g_camera->get_num_rows()-1, g_camera->get_num_rows()-1);

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

  //------------------------------------------------------------------
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
  // Set the display functions for each window and idle function for GLUT (they
  // will do all the work) and then give control over to GLUT.
  if (g_use_gui) {
    glutSetWindow(g_tracking_window);
    glutDisplayFunc(myDisplayFunc);
    glutShowWindow();
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
