//XXX There are some off-by-1 errors in the way the red lines are drawn, maybe to
//    do with binning > 1.
//XXX It is not possible to select all the way to the top or right
//    because the window is only half the number of pixels in the
//    image, so it always wants to skip the last column.  This is only
//    true when binning is greater than 1.

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <vrpn_Connection.h>
#include <vrpn_nikon_controls.h>
#include "roper_server.h"
#include "directx_camera_server.h"
#include "directx_videofile_server.h"
#include "diaginc_server.h"
#include "edt_server.h"
#include <tcl.h>
#include <tk.h>
#include "Tcl_Linkvar.h"
#ifdef	_WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glut.h>

//#define	FAKE_CAMERA
//#define	FAKE_NIKON

//--------------------------------------------------------------------------
// Version string for this program
const char *Version_string = "02.00";
char  *g_device_name = NULL;			  //< Name of the device to open
double  g_focus = 0;    // Current setting for the microscope focus
bool	g_focus_changed = false;
base_camera_server  *g_camera = NULL;
int	g_max_image_width = 1392;
int	g_max_image_height = 1040;
int	g_window_size_divisor = 1;	//< How much to subset the window when drawing
int	g_window_width = g_max_image_width / g_window_size_divisor;
int	g_window_height = g_max_image_height / g_window_size_divisor;
int	g_window_id;			//< Window ID to destroy when done
unsigned char *g_image = NULL;		//< Pointer to the storage for the image
bool	g_already_posted = false;	//< Posted redisplay since the last display?
int	g_mousePressX, g_mousePressY;	//< Where the mouse was when the button was pressed

#ifndef	FAKE_NIKON
vrpn_Synchronized_Connection	*con = NULL;
vrpn_Nikon_Controls		*nikon = NULL;
vrpn_Analog_Remote		*ana = NULL;
vrpn_Analog_Output_Remote	*anaout = NULL;
#endif

// The number of ticks (counts) on the Nikon focus control to take if
// you want to move it by a micron. Since there is one count per 50
// nanometers, this will be 20 steps per micron (50 * 20 = 1000).
const double  TICKS_PER_MICRON = 20;

//--------------------------------------------------------------------------
// Tcl controls and displays

void  handle_preview_change(int newvalue, void *);
Tclvar_float_with_scale	*g_minX;
Tclvar_float_with_scale	*g_maxX;
Tclvar_float_with_scale	*g_minY;
Tclvar_float_with_scale	*g_maxY;
Tclvar_float_with_scale	g_focusDown("focus_lower_microns", "", -20, 0, -5);
Tclvar_float_with_scale	g_focusUp("focus_raise_microns", "", 0, 20, 5);
Tclvar_float_with_scale	g_focusStep("focus_step_microns", "", (float)0.05, 5, 1);
Tclvar_float_with_scale	g_repeat("repeat", "", (float)1, 20, 1);
Tclvar_float_with_scale	g_exposure("exposure_millisecs", "", 1, 1000, 10);
Tclvar_float_with_scale	g_gain("gain", "", 1, 32, 1);
Tclvar_int_with_button	g_quit("quit","");
// When the user pushes this button, the stack starts to be taken and it
// continues until the end, then the program turns this back off.
Tclvar_int_with_button	g_take_stack("take_stack","");
Tclvar_int_with_button	g_sixteenbits("save_sixteen_bits","");
Tclvar_float_with_scale	g_pixelcount("pixels_from_camera","", 8,12, 8);
Tclvar_int_with_button	g_step_past_bottom("step_past_bottom","",1);
Tclvar_int_with_button	g_preview("preview","", 1, handle_preview_change);


//--------------------------------------------------------------------------
/// Open the wrapped camera we want to use depending on the name of the
//  camera we're trying to open.
bool  get_camera(const char *type, base_camera_server **camera)
{
  if (!strcmp(type, "roper")) {
    roper_server *r = new roper_server();
    *camera = r;
    g_pixelcount = 12;
  } else if (!strcmp(type, "diaginc")) {
    diaginc_server *r = new diaginc_server();
    *camera = r;
    g_exposure = 80;	// Seems to be the minimum exposure for the one we have
    g_pixelcount = 12;
  } else if (!strcmp(type, "edt")) {
    *camera = new edt_server();
    g_exposure = 100;
  } else if (!strcmp(type, "directx")) {
    // Passing width and height as zero leaves it open to whatever the camera has
    directx_camera_server *d = new directx_camera_server(1,0,0);	// Use camera #1 (first one found)
    *camera = d;
  } else if (!strcmp(type, "directx640x480")) {
    directx_camera_server *d = new directx_camera_server(1,640,480);	// Use camera #1 (first one found)
    *camera = d;
  } else {
      return false;
  }
  g_max_image_width = (*camera)->get_num_columns();
  g_max_image_height = (*camera)->get_num_rows();
  if ( (g_max_image_width > 1024) || (g_max_image_height > 768) ) {
    g_window_size_divisor = 2;
  } else {
    g_window_size_divisor = 1;
  }
  g_window_width = g_max_image_width / g_window_size_divisor;
  g_window_height = g_max_image_height / g_window_size_divisor;
  return true;
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
  glDrawPixels(g_window_width,g_window_height, GL_RGB, GL_UNSIGNED_BYTE, g_image);

  // Draw a red border around the selected area.  It will be beyond the
  // window border if it is set to the edges; it will just surround the
  // region being selected if it is inside the window.
  glColor3f(1,0,0);
  glBegin(GL_LINE_STRIP);
  glVertex3f( -1 + 2*((*g_minX-1) / (g_max_image_width-1)),
	      -1 + 2*((*g_minY-1) / (g_max_image_height-1)) , 0.0 );
  glVertex3f( -1 + 2*((*g_maxX+1) / (g_max_image_width-1)),
	      -1 + 2*((*g_minY-1) / (g_max_image_height-1)) , 0.0 );
  glVertex3f( -1 + 2*((*g_maxX+1) / (g_max_image_width-1)),
	      -1 + 2*((*g_maxY+1) / (g_max_image_height-1)) , 0.0 );
  glVertex3f( -1 + 2*((*g_minX-1) / (g_max_image_width-1)),
	      -1 + 2*((*g_maxY+1) / (g_max_image_height-1)) , 0.0 );
  glVertex3f( -1 + 2*((*g_minX-1) / (g_max_image_width-1)),
	      -1 + 2*((*g_minY-1) / (g_max_image_height-1)) , 0.0 );
  glEnd();

  // Swap buffers so we can see it.
  glutSwapBuffers();

  // Have no longer posted redisplay since the last display.
  g_already_posted = false;
}

void  handle_preview_change(int newvalue, void *)
{
  glutSetWindow(g_window_id);
  if (newvalue) {
    glutShowWindow();
  } else {
    glutHideWindow();
  }
}

void  preview_image(base_camera_server *camera)
{
  // Copy from the camera buffer into the image buffer, making 3 copies
  // of each of the elements.
  int x,y;
  unsigned char	*rgb_i;
  for (y = *g_minY; y <= *g_maxY; y++) {
    rgb_i = &g_image[(y/g_window_size_divisor)*g_window_width*3 + (((int)(*g_minX))/g_window_size_divisor) * 3];
    for (x = *g_minX; x <= *g_maxX; x+= g_window_size_divisor) {
      // Mark the border in red
#ifdef	FAKE_CAMERA
      int val = ((x+y)%256) * g_gain;
      if (val > 255) { val = 255; }
      *(rgb_i++) = val;
      *(rgb_i++) = val;
      *(rgb_i++) = val;
#else
      int val = (int)(g_gain * g_camera->read_pixel_nocheck(x,y) ) >> (int)(g_pixelcount-8);
      if (val > 255) { val = 255; }
      *(rgb_i++) = val;
      *(rgb_i++) = val;
      *(rgb_i++) = val;
#endif
    }
  }

  if (!g_already_posted) {
    glutPostRedisplay();
    g_already_posted = true;
  }
}

/// Sets the region size based in where the mouse was first picked and
/// where it is now.  Basically covers the rectangle between the min
/// and max of each of them.
void  set_region_based_on_mouse(int sx, int sy, int fx, int fy)
{
  // Scale the x and y values by the window size divisor
  sx *= g_window_size_divisor;
  sy *= g_window_size_divisor;
  fx *= g_window_size_divisor;
  fy *= g_window_size_divisor;

  // Invert y to make it match display coordinates
  sy = (g_max_image_height-1) - sy;
  fy = (g_max_image_height-1) - fy;

  if (sx < fx) {
    *g_minX = sx;
    *g_maxX = fx;
  } else {
    *g_minX = fx;
    *g_maxX = sx;
  }

  if (sy < fy) {
    *g_minY = sy;
    *g_maxY = fy;
  } else {
    *g_minY = fy;
    *g_maxY = sy;
  }

}

void mouseCallbackForGLUT(int button, int state, int x, int y) {
 
    switch(button) {
	case GLUT_LEFT_BUTTON:
	  if (state == GLUT_DOWN) {
	    // Start the region-picking process by setting the min and
	    // max X and Y to the current pick location.
	    set_region_based_on_mouse(x,y, x,y);

	    g_mousePressX = x;
	    g_mousePressY = y;
	  } else {
	    // Don't need to do anything -- it was rubberbanding the whole time
	  }
	  break;
	case GLUT_MIDDLE_BUTTON:
	  break;
	case GLUT_RIGHT_BUTTON:
	  break;
    }
}

void motionCallbackForGLUT(int x, int y) {
  // Clip the motion to stay within the window boundaries.
  if (x < 0) { x = 0; };
  if (y < 0) { y = 0; };
  if (x >= g_window_width) { x = g_window_width - 1; }
  if (y >= g_window_height) { y = g_window_height - 1; };

  // Set the bounds based on the point first picked and this point.
  set_region_based_on_mouse(g_mousePressX,g_mousePressY, x,y);
  return;
}

//--------------------------------------------------------------------------
// Handles updates for the analog from the microscope by setting the
// focus to that location and telling that the focus has changed.

void VRPN_CALLBACK handle_focus_change(void *, const vrpn_ANALOGCB info)
{
  g_focus = info.channel[0];
  g_focus_changed = true;
}

// Where_to_go is in number of ticks, not in microns.
void  move_nikon_focus_and_wait_until_it_gets_there(double where_to_go) {
#ifndef	FAKE_NIKON
	  // Request that the Nikon focus go to where we want it
	  anaout->request_change_channel_value(0, where_to_go);
	  // Wait until the focus gets to where we asked it to be
	  while (g_focus != where_to_go) {
	    if (nikon) { nikon->mainloop(); }
	    anaout->mainloop();
	    ana->mainloop();
	    vrpn_SleepMsecs(1);
	  }
#endif
}

void  cleanup(void)
{
  glFinish();
  glutDestroyWindow(g_window_id);

  if (g_camera != NULL) { delete g_camera; };

#ifndef	FAKE_NIKON
  if (ana) { delete ana; ana = NULL; };
  if (anaout) { delete anaout; anaout = NULL; };
  if (nikon) { delete nikon; nikon = NULL; };
  if (con) { delete con; con = NULL; };
#endif
}

void myIdleFunc(void)
{
    // If we are previewing, then read from the camera and put
    // the image into the preview window
    if (g_preview) {
      g_camera->read_image_to_memory((int)*g_minX,(int)*g_maxX, (int)*g_minY,(int)*g_maxY, g_exposure);
      preview_image(g_camera);
    }

    // If we've been asked to take a stack, do it.
    if (g_take_stack) {
      double  startfocus;		      //< Where the focus started at connection time
      double  focusdown = g_focusDown * TICKS_PER_MICRON;   //< How far down to go when adjusting the focus (in ticks)
      double  focusstep = g_focusStep * TICKS_PER_MICRON;   //< Step size to change the focus by (in ticks)
      double  focusup = g_focusUp * TICKS_PER_MICRON;	    //< How far above initial to set the focus (in ticks)
      double  focusloop;		      //< Used to step the focus
      int     repeat;			      //< Used to enable multiple scans

      // Wait until we hear where the focus is from the Nikon
#ifndef	FAKE_NIKON
      g_focus_changed = false;
      printf("Waiting for response from Nikon\n");
      while (g_focus == 0) {
	if (nikon) { nikon->mainloop(); }
	anaout->mainloop();
	ana->mainloop();

	//------------------------------------------------------------
	// If the user has deselected the "take_stack" or pressed "quit" then
	// break out of the wait.
	while (Tk_DoOneEvent(TK_DONT_WAIT)) {};
	if ( g_quit || !g_take_stack) {
	  if (g_quit) {
	    cleanup();
	    exit(0);
	  }

	  return;
	}

	vrpn_SleepMsecs(1);
      }
#endif
      printf("  (Initial focus found at %ld)\n", (long)g_focus);
      startfocus = g_focus;

      // Loop through the number of repeats we have been asked to do
      for (repeat = 0; repeat < g_repeat; repeat++) {

	// Go one past the lower focus request in order to make sure we
	// always come past each plane going the same direction (avoids
	// trouble with hysteresis), if the flag is set asking for this.
	if (g_step_past_bottom) {
	  double beyond = startfocus + focusdown - focusstep;
	  printf("Stepping past bottom to %ld\n", (long)beyond);
	  move_nikon_focus_and_wait_until_it_gets_there(beyond);
	}

	// Loop through the requested focus levels and read images.
	for (focusloop = startfocus + focusdown; focusloop <= startfocus + focusup; focusloop += focusstep) {

	  printf("Going to %ld\n", (long)focusloop);
	  move_nikon_focus_and_wait_until_it_gets_there(focusloop);

	  // Read the image from the camera.  If we are previewing,
	  // then display the image in the video window.  We read
	  // two images here to make sure that we flush any image
	  // that was actually acquired before or during the move.
#ifndef	FAKE_CAMERA
	  g_camera->read_image_to_memory((int)*g_minX,(int)*g_maxX, (int)*g_minY,(int)*g_maxY, g_exposure);
	  g_camera->read_image_to_memory((int)*g_minX,(int)*g_maxX, (int)*g_minY,(int)*g_maxY, g_exposure);
	  if (g_preview) {
	    preview_image(g_camera);
	    myDisplayFunc();
	  }
#endif
	  // Write the image to a TIFF file.
	  char name[1024];
	  sprintf(name, "output_image_G%03dR%02dNM%07ld.tif", (int)g_gain, repeat, (long)(focusloop-focusdown)*50);
	  printf("Writing image to %s\n", name);
#ifndef	FAKE_CAMERA
	  // Write the selected subregion to a TIFF file.
	  g_camera->write_to_tiff_file(name, (int)(g_gain * (g_sixteenbits ? 1 : 256 )), g_sixteenbits != 0);
#endif
	  //------------------------------------------------------------
	  // This must be done in any Tcl app, to allow Tcl/Tk to handle
	  // events.

	  while (Tk_DoOneEvent(TK_DONT_WAIT)) {};

	  //------------------------------------------------------------
	  // If the user has deselected the "take_stack" or pressed "quit" then
	  // break out of the loop.
	  if ( g_quit || !g_take_stack) {
	    break;
	  }
	}
      }

      // Put the focus back where it started.
#ifndef	FAKE_NIKON
      printf("Setting focus back to %ld\n", (long)startfocus);
      anaout->request_change_channel_value(0, startfocus);
      while (g_focus != startfocus) {
	  if (nikon) { nikon->mainloop(); }
	  anaout->mainloop();
	  ana->mainloop();
	  vrpn_SleepMsecs(1);
      }
#endif
      printf("Done with stack\n");
      g_take_stack = 0;
    }

    //------------------------------------------------------------
    // This must be done in any Tcl app, to allow Tcl/Tk to handle
    // events

    while (Tk_DoOneEvent(TK_DONT_WAIT)) {};

    //------------------------------------------------------------
    // This is called once every time through the main loop.  It
    // pushes changes in the C variab`les over to Tcl.

    if (Tclvar_mainloop()) {
	    fprintf(stderr,"Mainloop failed\n");
	    exit(-1);
    }

    //------------------------------------------------------------
    // Make sure the constraints on the image locations are met
    if (*g_minX < 0) { *g_minX = 0; }
    if (*g_minY < 0) { *g_minY = 0; }
    if (*g_maxX >= g_max_image_width) { *g_maxX = g_max_image_width-1; }
    if (*g_maxY >= g_max_image_height) { *g_maxY = g_max_image_height-1; }
    if (*g_minX >= *g_maxX) { *g_minX = *g_maxX-1; }
    if (*g_minY >= *g_maxY) { *g_minY = *g_maxY-1; }

    //------------------------------------------------------------
    // Make sure the values that should be integers are integers
    g_exposure = floor(g_exposure);
    *g_minX = floor(*g_minX);
    *g_maxX = floor(*g_maxX);
    *g_minY = floor(*g_minY);
    *g_maxY = floor(*g_maxY);
    g_repeat = floor(g_repeat);
    g_gain = floor(g_gain);

    // Sleep a bit so as not to eat the whole CPU
    vrpn_SleepMsecs(5);

    if (g_quit) {
      cleanup();
      exit(0);
    }
}


int main(int argc, char *argv[])
{
  char	*Nikon_name = "Focus";
  bool	 use_local_server = true;

  //------------------------------------------------------------------
  // If there is a command-line argument, treat it as the name of a
  // camera to be opened.  If there is none, default to DirectX.
  switch (argc) {

  case 1:
    // No arguments, so ask the user for a file name
    g_device_name = "directx";
    break;

  case 2:
    // Camera device type is named
    g_device_name = argv[1];
    break;

  case 3:
    // Camera device name and Nikon analog/analogout device name both specified
    g_device_name = argv[1];
    Nikon_name = argv[2];
    use_local_server = false;
    break;

  default:
    fprintf(stderr, "Usage: %s [edt|roper|diaginc|directx|directx640x480|filename] [Nikon_server_name@hostname]\n", argv[0]);
    exit(-1);
  };

#ifndef	FAKE_NIKON
  if (use_local_server) {
    con = new vrpn_Synchronized_Connection();
    nikon = new vrpn_Nikon_Controls("Focus", con);
    ana = new vrpn_Analog_Remote("Focus", con);
    anaout = new vrpn_Analog_Output_Remote("Focus", con);
  } else {
    con = NULL;
    nikon = NULL;
    ana = new vrpn_Analog_Remote(Nikon_name);
    anaout = new vrpn_Analog_Output_Remote(Nikon_name);
  }
  ana->register_change_handler(NULL, handle_focus_change);
#endif

  //------------------------------------------------------------------
  // Verify that the camera is working.
#ifndef	FAKE_CAMERA
  if (!get_camera(g_device_name, &g_camera)) {
    fprintf(stderr,"Could not establish connection to camera\n");
    exit(-1);
  }
  if (!g_camera->working()) {
    fprintf(stderr,"Camera not working\n");
    exit(-1);
  }
#endif

  //------------------------------------------------------------------
  // Initialize the controls for the clipping based on the size of
  // the image we got.
  g_minX = new Tclvar_float_with_scale("minX", ".clip", 0, g_camera->get_num_columns()-1, 0);
  g_maxX = new Tclvar_float_with_scale("maxX", ".clip", 0, g_camera->get_num_columns()-1, g_camera->get_num_columns()-1);
  g_minY = new Tclvar_float_with_scale("minY", ".clip", 0, g_camera->get_num_rows()-1, 0);
  g_maxY = new Tclvar_float_with_scale("maxY", ".clip", 0, g_camera->get_num_rows()-1, g_camera->get_num_rows()-1);

  //------------------------------------------------------------------
  // Initialize GLUT display modes and create the window that will display the
  // video -- name the window after the device that has been
  // opened in VRPN.

  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
  glutInitWindowSize(g_window_width, g_window_height);
  glutInitWindowPosition(100, 100);
  g_window_id = glutCreateWindow("Preview");
  // Set the display function and idle function for GLUT (they
  // will do all the work) and then give control over to GLUT.
  glutDisplayFunc(myDisplayFunc);
  glutIdleFunc(myIdleFunc);
  glutMotionFunc(motionCallbackForGLUT);
  glutMouseFunc(mouseCallbackForGLUT);

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
  sprintf(command, "toplevel .clip");
  if (Tcl_Eval(tk_control_interp, command) != TCL_OK) {
          fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
                  tk_control_interp->result);
          return(-1);
  }

  //------------------------------------------------------------------
  // Put the version number into the main window.
  sprintf(command, "label .versionlabel -text CISMM_Stack_Collector_v:_%s", Version_string);
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
  // Allocate space for the image to be drawn in the preview window.
  if ( (g_image = new unsigned char[g_window_width * g_window_height * 3]) == NULL) {
    fprintf(stderr,"Out of memory when allocating image!\n");
    return -1;
  }

  glutMainLoop();

  // Clean up objects and return.
  cleanup();
  return 0;
}
