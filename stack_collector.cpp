#include <math.h>
#include <stdio.h>
#include <string.h>
#include <vrpn_Connection.h>
#include <vrpn_nikon_controls.h>
#include "roper_server.h"
#include <tcl.h>
#include <tk.h>
#include "Tcl_Linkvar.h"
#ifdef	_WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <glut.h>

//#define	FAKE_ROPER
//#define	FAKE_NIKON

//--------------------------------------------------------------------------
// Version string for this program
const char *Version_string = "01.03";
double  g_focus = 0;    // Current setting for the microscope focus
bool	g_focus_changed = false;
roper_server  *g_roper = NULL;
int	g_max_image_width = 1392;
int	g_max_image_height = 1040;
int	g_window_size_divisor = 2;	//< How much to subset the window when drawing
int	g_window_width = g_max_image_width / g_window_size_divisor;
int	g_window_height = g_max_image_height / g_window_size_divisor;
int	g_window_id;			//< Window ID to destroy when done
unsigned char *g_image = NULL;		//< Pointer to the storage for the image
bool	g_already_posted = false;	//< Posted redisplay since the last display?

#ifndef	FAKE_NIKON
vrpn_Synchronized_Connection	*con;
vrpn_Nikon_Controls		*nikon;
vrpn_Analog_Remote		*ana;
vrpn_Analog_Output_Remote	*anaout;
#endif

//--------------------------------------------------------------------------
// Tcl controls and displays

void  handle_preview_change(int newvalue, void *);
Tclvar_float_with_scale	g_minX("minX", "", 0, 1391, 0);
Tclvar_float_with_scale	g_maxX("maxX", "", 0, 1391, 1391);
Tclvar_float_with_scale	g_minY("minY", "", 0, 1039, 0);
Tclvar_float_with_scale	g_maxY("maxY", "", 0, 1039, 1039);
Tclvar_float_with_scale	g_focusDown("focus_lower_microns", "", -20, 0, -5);
Tclvar_float_with_scale	g_focusUp("focus_raise_microns", "", 0, 20, 5);
Tclvar_float_with_scale	g_focusStep("focus_step_microns", "", (float)0.05, 5, 1);
Tclvar_float_with_scale	g_exposure("exposure_millisecs", "", 1, 1000, 10);
Tclvar_int_with_button	g_quit("quit","");
Tclvar_int_with_button	g_snap("snap","");
Tclvar_int_with_button	g_sixteenbits("sixteen_bits","");
Tclvar_int_with_button	g_preview("preview","", 0, handle_preview_change);

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
  glVertex3f( -1 + 2*((g_minX-1) / g_max_image_width),
	      -1 + 2*((g_minY-1) / g_max_image_height) , 0.0 );
  glVertex3f( -1 + 2*((g_maxX+1) / g_max_image_width),
	      -1 + 2*((g_minY-1) / g_max_image_height) , 0.0 );
  glVertex3f( -1 + 2*((g_maxX+1) / g_max_image_width),
	      -1 + 2*((g_maxY+1) / g_max_image_height) , 0.0 );
  glVertex3f( -1 + 2*((g_minX-1) / g_max_image_width),
	      -1 + 2*((g_maxY+1) / g_max_image_height) , 0.0 );
  glVertex3f( -1 + 2*((g_minX-1) / g_max_image_width),
	      -1 + 2*((g_minY-1) / g_max_image_height) , 0.0 );
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

void  preview_image(roper_server *roper)
{
  // Copy from the camera buffer into the image buffer, making 3 copies
  // of each of the elements.
  int x,y;
  unsigned char	*rgb_i;
#ifndef	FAKE_ROPER
  vrpn_uint16	*imageptr = roper->get_memory_pointer();
#endif
  for (y = g_minY; y <= g_maxY; y++) {
    rgb_i = &g_image[(y/g_window_size_divisor)*g_window_width*3 + (((int)(g_minX))/g_window_size_divisor) * 3];
    for (x = g_minX; x <= g_maxX; x+= g_window_size_divisor) {
      // Mark the border in red
#ifdef	FAKE_ROPER
      *(rgb_i++) = x % 256;
      *(rgb_i++) = x % 256;
      *(rgb_i++) = x % 256;
#else
      //XXX Draws diagonally for some settings of minX
      //XXX It doesn't do the diagonals for the test image, only with the real camera
      //XXX Tried to fix it below by changing the scope of the int casts.
      unsigned val = imageptr[x-(int)g_minX + ((int)(g_maxX) - (int)(g_minX) + 1) * (y - (int)g_minY)] >> 4;
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

//--------------------------------------------------------------------------
// Handles updates for the analog from the microscope by setting the
// focus to that location and telling that the focus has changed.

void handle_focus_change(void *, const vrpn_ANALOGCB info)
{
  g_focus = info.channel[0];
  g_focus_changed = true;
}


void  cleanup(void)
{
  glFinish();
  glutDestroyWindow(g_window_id);

  if (g_roper != NULL) { delete g_roper; };

#ifndef	FAKE_NIKON
  if (ana) { delete ana; };
  if (anaout) { delete anaout; };
  if (nikon) { delete nikon; };
  if (con) { delete con; };
#endif
//XXX This crashes for some reason  if (g_image) { delete [] g_image; };
}

void myIdleFunc(void)
{
    // If we are previewing, then read from the roper camera and put
    // the image into the preview window
    if (g_preview) {
      // Verify that the Roper camera is working.
#ifndef	FAKE_ROPER
      if (g_roper == NULL) { g_roper = new roper_server; }
      if (!g_roper->working()) {
	fprintf(stderr,"Could not establish connection to camera\n");
	exit(-1);
      }
      g_roper->read_image_to_memory((int)g_minX,(int)g_maxX, (int)g_minY,(int)g_maxY, g_exposure);
#endif
      preview_image(g_roper);
    }

    // If we've been asked to take a stack, do it.
    if (g_snap) {
      double  startfocus;		      // Where the focus started at connection time
      double  focusdown = g_focusDown * 20;   // How far down to go when adjusting the focus (in ticks)
      double  focusstep = g_focusStep * 20;   // Step size of focus (in ticks)
      double  focusup = g_focusUp * 20;	      // How far above initial to set the focus (in ticks)
      double  focusloop;		      // Used to step the focus

      // Verify that the Roper camera is working.
#ifndef	FAKE_ROPER
      if (g_roper == NULL) { g_roper = new roper_server; }
      if (!g_roper->working()) {
	fprintf(stderr,"Could not establish connection to camera\n");
	exit(-1);
      }
#endif

      // Wait until we hear where the focus is from the Nikon
#ifndef	FAKE_NIKON
      ana->register_change_handler(NULL, handle_focus_change);
      g_focus_changed = false;
      printf("Waiting for response from Nikon\n");
      while (g_focus == 0) {
	nikon->mainloop();
	anaout->mainloop();
	ana->mainloop();
	vrpn_SleepMsecs(1);
      }
#endif
      printf("  (Initial focus found at %ld)\n", (long)g_focus);
      startfocus = g_focus;

      // Loop through the requested focus levels and read images.
      for (focusloop = startfocus + focusdown; focusloop <= startfocus + focusup; focusloop += focusstep) {

	printf("Going to %ld\n", (long)focusloop);
#ifndef	FAKE_NIKON
	anaout->request_change_channel_value(0, focusloop);
	while (g_focus != focusloop) {
	  nikon->mainloop();
	  anaout->mainloop();
	  ana->mainloop();
	  vrpn_SleepMsecs(1);
	}
#endif
	// Read the image from the roper camera.  If we are previewing,
	// then display the image in the video window.
#ifndef	FAKE_ROPER
	g_roper->read_image_to_memory((int)g_minX,(int)g_maxX, (int)g_minY,(int)g_maxY, g_exposure);
	if (g_preview) {
	  preview_image(g_roper);
	  myDisplayFunc();
	}

	// Write the image to a PPM file.
	char name[1024];
	sprintf(name, "output_image_%ld.pgm", (long)focusloop);
	printf("Writing image to %s\n", name);
	g_roper->write_memory_to_ppm_file(name, g_sixteenbits != 0);
#endif
	//------------------------------------------------------------
	// This must be done in any Tcl app, to allow Tcl/Tk to handle
	// events.

	while (Tk_DoOneEvent(TK_DONT_WAIT)) {};

	//------------------------------------------------------------
	// If the user has deselected the "snap" or pressed "quit" then
	// break out of the loop.
	if ( g_quit || !g_snap) {
	  break;
	}
      }

      // Put the focus back where it started.
#ifndef	FAKE_NIKON
      printf("Setting focus back to %ld\n", (long)startfocus);
      anaout->request_change_channel_value(0, startfocus);
      while (g_focus != startfocus) {
	  nikon->mainloop();
	  anaout->mainloop();
	  ana->mainloop();
	  vrpn_SleepMsecs(1);
      }
#endif
      printf("Done with stack\n");
      g_snap = 0;
    }

    //------------------------------------------------------------
    // This must be done in any Tcl app, to allow Tcl/Tk to handle
    // events

    while (Tk_DoOneEvent(TK_DONT_WAIT)) {};

    //------------------------------------------------------------
    // This is called once every time through the main loop.  It
    // pushes changes in the C variables over to Tcl.

    if (Tclvar_mainloop()) {
	    fprintf(stderr,"Mainloop failed\n");
	    exit(-1);
    }

    // Make sure the constraints on the image locations are met
    if (g_minX < 0) { g_minX = 0; }
    if (g_minY < 0) { g_minY = 0; }
    if (g_maxX >= g_max_image_width) { g_maxX = g_max_image_width-1; }
    if (g_maxY >= g_max_image_height) { g_maxY = g_max_image_height-1; }
    if (g_minX >= g_maxX) { g_minX = g_maxX - 1; }
    if (g_minY >= g_maxY) { g_minY = g_maxY - 1; }

    // Sleep a bit so as not to eat the whole CPU
    vrpn_SleepMsecs(5);

    if (g_quit) {
      cleanup();
      exit(0);
    }
}


int main(int argc, char *argv[])
{
#ifndef	FAKE_NIKON
  con = new vrpn_Synchronized_Connection();
  nikon = new vrpn_Nikon_Controls("Focus", con);
  ana = new vrpn_Analog_Remote("Focus", con);
  anaout = new vrpn_Analog_Output_Remote("Focus", con);
#endif

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
  sprintf(command, "label .versionlabel -text Stack_Collector_v:_%s", Version_string);
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