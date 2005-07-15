//----------------------------------------------------------------------------
//   This program reads from a VRPN Imager server and implements several image-
// adjustment techniques to support the UNC NIH Resource, Computer-Integrated
// Systems for Microscopy and Manipulation (CISMM).
//   Supported features include:
//   Gain and offset control
//   Capturing a "background" image that is then subtracted from future images.
//   Capturing a "background" image and converting it to 128 everywhere
//   XXX Base the gain and offset on the image based on a histogram display
//
// XXX It assumes that the size of the imager does not change during the run.
// XXX Its settings and ranges assume that the pixels are actually 8-bit values.

#include <stdio.h>
#include <stdlib.h>
#include <tcl.h>
#include <tk.h>
#include "Tcl_Linkvar.h"
#ifdef	_WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glut.h>
#include <vrpn_Connection.h>
#include <vrpn_Imager.h>

//--------------------------------------------------------------------------
// Version string for this program
const char *Version_string = "01.00";

//----------------------------------------------------------------------------
// Glut insists on taking over the whole application, leaving us to use
// global variables to pass information between the various callback
// handlers.

vrpn_Imager_Remote  *g_ti;		//< TempImager client object
bool	  g_got_dimensions = false;	//< Heard image dimensions from server?
bool	  g_ready_for_region = false;	//< Everything set up to handle a region?
unsigned char *g_image = NULL;		//< Pointer to the storage for the image
int	  *g_background = NULL;		//< Background image stored before
bool	  g_already_posted = false;	//< Posted redisplay since the last display?
int	  g_window_id;			//< Window ID to destroy when done

//--------------------------------------------------------------------------
// Tcl controls and displays
Tclvar_float_with_scale	g_clip_high("clip_high", "", 0, 255, 255);
Tclvar_float_with_scale	g_clip_low("clip_low", "", 0, 255, 0);
Tclvar_int_with_button	g_subtract_background("subtract_background", "");
Tclvar_int_with_button	g_average_background("average_background", "");
Tclvar_int_with_button	g_clear_background("clear_background", "");
Tclvar_int_with_button	g_quit("quit","");

//----------------------------------------------------------------------------
// Imager callback handlers.

void  cleanup(void)
{
  printf("Exiting\n");
  delete g_ti;
  if (g_image) { delete [] g_image; };
  if (g_background) { delete [] g_background; };
}

void  VRPN_CALLBACK handle_description_message(void *, const struct timeval)
{
  // This assumes that the size of the image does not change -- application
  // should allocate a new image array and get a new window size whenever it
  // does change.  Here, we just record that the entries in the imager client
  // are valid.
  g_got_dimensions = true;
}

void myDisplayFunc();
float fps[2]={0,0};
DWORD lastCallTime[2]={0,0};
DWORD ReportInterval=5000;

// New pixels coming; fill them into the image and tell Glut to redraw.
void  VRPN_CALLBACK handle_region_change(void *, const vrpn_IMAGERREGIONCB info)
{
    int r,c;	//< Row, Column
    int ir;	//< Inverted Row
    int offset,RegionOffset;
    const vrpn_Imager_Region* region=info.region;
    double  intensity_gain;
    double  intensity_offset;

    // Compute gain and offset so that pixels at or below the low-clip value are
    // set to zero and pixels at or above the high-clip value are set to 255.
    // First, make sure we've got legal settings.
    if (g_clip_high <= g_clip_low) {
      g_clip_high = g_clip_low + 1;
    }
    intensity_gain = 255.0/(g_clip_high-g_clip_low);
    intensity_offset = g_clip_low;

    int infoLineSize=region->d_cMax-region->d_cMin+1;
    vrpn_int32 nCols=g_ti->nCols();
    vrpn_int32 nRows=g_ti->nRows();
    unsigned char uns_pix;
    static  bool  subtracting_background = false;
    static  bool  averaging_background = false;
    static  bool  clearing_background = false;

    if (!g_ready_for_region) { return; }

    // If this is the first region and we have been asked to subtract the
    // background, then start copying regions into the background buffer,
    // with a negative sign for each pixel (so as to subtract this image
    // from future images).
    // If this is the first region and we used to be grabbing into the
    // background buffer, stop copying into the background buffer.
    if (info.region->d_rMin == 0) {
      if (g_subtract_background) {
	g_subtract_background = 0;
	subtracting_background = true;
	printf("Subtracting background\n");
      } else if (subtracting_background) {
	subtracting_background = false;
      }
    }
    if (subtracting_background) {
      for (r = info.region->d_rMin,RegionOffset=(r-region->d_rMin)*infoLineSize; r <= region->d_rMax; r++,RegionOffset+=infoLineSize) {
	ir = nRows - r - 1;
	int row_offset = ir*nCols;
	for (c = info.region->d_cMin; c <= info.region->d_cMax; c++) {
	  vrpn_uint8 val;
	  info.region->read_unscaled_pixel(c,r,val);
	  g_background[c + row_offset] = -val;
	}
      }
    }

    // If this is the first region and we have been asked to average the
    // background, then start copying pixel offsets from 128 into the background buffer.
    // This will make it so that if the same image appears in the future, it will have
    // an intensity value of 128 everywhere; other values will appear as image.
    // If this is the first region and we used to be averaging into the
    // background buffer, stop averaging into the background buffer.
    if (info.region->d_rMin == 0) {
      if (g_average_background) {
	g_average_background = 0;
	averaging_background = true;
	printf("Averaging background\n");
      } else if (averaging_background) {
	averaging_background = false;
      }
    }
    if (averaging_background) {
      for (r = info.region->d_rMin,RegionOffset=(r-region->d_rMin)*infoLineSize; r <= region->d_rMax; r++,RegionOffset+=infoLineSize) {
	ir = nRows - r - 1;
	int row_offset = ir*nCols;
	for (c = info.region->d_cMin; c <= info.region->d_cMax; c++) {
	  vrpn_uint8 val;
	  info.region->read_unscaled_pixel(c,r,val);
	  g_background[c + row_offset] = 128 - val;
	}
      }
    }

    // If we have been asked to clear the background, then start do it.
    if (g_clear_background) {
      printf("Clearing background\n");
      g_clear_background = 0;
      for (r = 0,RegionOffset=r*nCols; r < nRows; r++,RegionOffset+=nCols) {
	ir = nRows - r - 1;
	int row_offset = ir*nCols;
	for (c = 0; c < nCols; c++) {
	  g_background[c + row_offset] = 0;
	}
      }
    }

    // Copy pixels into the image buffer.  Flip the image over in
    // Y so that the image coordinates display correctly in OpenGL.
    for (r = info.region->d_rMin,RegionOffset=(r-region->d_rMin)*infoLineSize; r <= region->d_rMax; r++,RegionOffset+=infoLineSize) {
      ir = g_ti->nRows() - r - 1;
      int row_offset = ir*nCols;
      for (c = info.region->d_cMin; c <= region->d_cMax; c++) {
	int per_pixel_adjustment = g_background[c + row_offset];
	int temp;
	vrpn_uint8 val;
	info.region->read_unscaled_pixel(c,r,val);

	offset = 3 * (c + row_offset);
	temp = (int)(( (val + per_pixel_adjustment) - intensity_offset) * intensity_gain);
	if (temp < 0) { temp = 0; }
	if (temp > 255) { temp = 255; }
	uns_pix = (unsigned char)temp;
		  
	g_image[0 + offset] = uns_pix;
	g_image[1 + offset] = uns_pix;
	g_image[2 + offset] = uns_pix;
      }
    }

    // Capture timing information and print out how many frames per second
    // are being received.

    { static struct timeval last_print_time;
      struct timeval now;
      static bool first_time = true;
      static int frame_count = 0;

      if (first_time) {
	gettimeofday(&last_print_time, NULL);
	first_time = false;
      } else {
	static	unsigned  last_r = 10000;
	if (last_r > info.region->d_rMin) {
	  frame_count++;
	}
	last_r = info.region->d_rMin;
	gettimeofday(&now, NULL);
	double timesecs = 0.001 * vrpn_TimevalMsecs(vrpn_TimevalDiff(now, last_print_time));
	if (timesecs >= 5) {
	  double frames_per_sec = frame_count / timesecs;
	  frame_count = 0;
	  printf("Received frames per second = %lg\n", frames_per_sec);
	  last_print_time = now;
	}
      }
    }

    // Tell Glut it is time to draw.  Make sure that we don't post the redisplay
    // operation more than once by checking to make sure that it has been handled
    // since the last time we posted it.  If we don't do this check, it gums
    // up the works with tons of redisplay requests and the program won't
    // even handle windows events.
    if (!g_already_posted) {
      glutPostRedisplay();
      g_already_posted = true;
    }
}

//----------------------------------------------------------------------------
// Glut callback handlers.

void myDisplayFunc(void)
{
  // Clear the window and prepare to draw in the back buffer
  glDrawBuffer(GL_BACK);
  glClearColor(0.0, 0.0, 0.0, 0.0);
  glClear(GL_COLOR_BUFFER_BIT);

  // Store the pixels from the image into the frame buffer
  // so that they cover the entire image (starting from lower-left
  // corner, which is at (-1,-1)).
  glRasterPos2f(-1, -1);
  glDrawPixels(g_ti->nCols(),g_ti->nRows(), GL_RGB, GL_UNSIGNED_BYTE, g_image);

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
  // events

  while (Tk_DoOneEvent(TK_DONT_WAIT)) {};

  //------------------------------------------------------------
  // This is called once every time through the main loop.  It
  // pushes changes in the C variables over to Tcl.

  if (Tclvar_mainloop()) {
    fprintf(stderr,"Tclvar Mainloop failed\n");
  }

  // See if we are done.
  if (g_quit) {
    cleanup();
    exit(0);
  }

  // See if there are any more messages from the server and then sleep
  // a little while so that we don't eat the whole CPU.
  g_ti->mainloop();

  vrpn_SleepMsecs(5);
}

int main(int argc, char **argv)
{
  char	*device_name = "TestImage@localhost:4511";
  int	i;

  //------------------------------------------------------------------
  // Initialize GLUT so that it fixes the command-line arguments.

  glutInit(&argc, argv);

  //------------------------------------------------------------------
  // Read command-line arguments for the program.

  switch (argc) {
  case 1: break;
  case 2:
    device_name = argv[1];
    break;
  default:
    fprintf(stderr,"Usage: %s [device_to_connect_to]\n", argv[0]);
    fprintf(stderr,"       device_to_connect_to: Default TestImage@localhost:4511\n");
    exit(-1);
  }

  //------------------------------------------------------------------
  // Generic Tcl startup.  Getting and interpreter and mainwindow.

  char		command[256];
  Tcl_Interp	*tk_control_interp;
  Tk_Window     tk_control_window;
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
  sprintf(command, "label .versionlabel -text Argonot_v:_%s", Version_string);
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

  // Open the Imager client and set the callback
  // for new data and for information about the size of
  // the image.
  printf("Opening %s\n", device_name);
  g_ti = new vrpn_Imager_Remote(device_name);
  g_ti->register_description_handler(NULL, handle_description_message);
  g_ti->register_region_handler(NULL, handle_region_change);

  printf("Waiting to hear the image dimensions...\n");
  while (!g_got_dimensions) {
    g_ti->mainloop();
    vrpn_SleepMsecs(1);
  }
  if ( (g_image = new unsigned char[g_ti->nCols() * g_ti->nRows() * 3]) == NULL) {
    fprintf(stderr,"Out of memory when allocating image!\n");
    return -1;
  }
  if ( (g_background = new int[g_ti->nCols() * g_ti->nRows()]) == NULL) {
    fprintf(stderr,"Out of memory when allocating image!\n");
    return -1;
  }
  for (i = 0; i < g_ti->nCols() * g_ti->nRows(); i++) {
    g_background[i] = 0;
  }
  g_ready_for_region = true;
  printf("Receiving images at size %dx%d\n", g_ti->nCols(), g_ti->nRows());

  //------------------------------------------------------------------
  // Initialize GLUT display modes and create the window that will display the
  // video -- name the window after the device that has been
  // opened in VRPN.

  glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
  glutInitWindowSize(g_ti->nCols(), g_ti->nRows());
  glutInitWindowPosition(100, 100);
  g_window_id = glutCreateWindow(vrpn_copy_service_name(device_name));
  // Set the display function and idle function for GLUT (they
  // will do all the work) and then give control over to GLUT.
  glutDisplayFunc(myDisplayFunc);
  glutIdleFunc(myIdleFunc);
  glutMainLoop();

  // Clean up objects and return.
  cleanup();
  return 0;
}
