//XXX Make all of the units match in the program.  For now, they are half baked.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tcl.h>
#include <tk.h>
#include "Tcl_Linkvar.h"
#ifdef	_WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glut.h>
#include <vrpn_Shared.h>
#include "spot_math.h"

//#define	DEBUG

//--------------------------------------------------------------------------
// Version string for this program
const char *Version_string = "01.00";

//--------------------------------------------------------------------------
// Constants needed by the rest of the program

const unsigned	Window_Size_X = 512;
const unsigned	Window_Size_Y = Window_Size_X;
const float	Window_Units_X = 1e9;
const float	Window_Units_Y = Window_Units_X;

//--------------------------------------------------------------------------
// Glut wants to take over the world when it starts, so we need to make
// global access to the objects we will be using.

bool	g_already_posted = false;	  //< Posted redisplay since the last display?
int	g_mousePressX, g_mousePressY;	  //< Where the mouse was when the button was pressed
int	g_whichDragAction;		  //< What action to take for mouse drag

//--------------------------------------------------------------------------
// Tcl controls and displays
void  logfilename_changed(char *newvalue, void *);
Tclvar_float_with_scale	g_Wavelength("wavelength_nm", ".kernel.wavelength", 300, 750, 550);
Tclvar_float_with_scale	g_PixelSpacing("pixelSpacing_nm", ".kernel.pixel", 20, Window_Size_X/4, 50);
Tclvar_float_with_scale	g_PixelHidden("pixelFracHidden", ".kernel.pixelhide", 0, 0.5, 0.1);
Tclvar_float_with_scale	g_Radius("aperture_nm", "", 1000, 3000, 1000);
Tclvar_int_with_button	g_quit("quit","");
Tclvar_selector		g_logfilename("logfilename", NULL, NULL, "", logfilename_changed, NULL);

//--------------------------------------------------------------------------
// Glut callback routines.

static void  cleanup(void)
{
  // Done with the camera and other objects.
  printf("Exiting\n");
}

void drawSideViewVsPixels(
  double width,		//< Width in pixels of the screen.  We draw from -1 to 1 in X, 0 to 1 in Y
  double units,		//< Converts from Pixel units to screen-space units
  double pixelSpacing,	//< How far in screen pixels between imager pixels
  double pixelFrac)	//< What fraction of imager pixels are not active
{
  float	loop;	  // So we don't get integer math on coordinates

  // Draw a green area tracing out a side view of the diffraction
  // pattern as it goes radially away from the center.  This is a
  // normalized diffraction pattern, such that the peak is always at
  // 1.
  glDisable(GL_LINE_SMOOTH);
  glColor3f(0.5,0.7,0.5);
  glBegin(GL_LINES);
  double valAtZero = FraunhoferIntensity(g_Radius/1e9, 0, g_Wavelength/1e9);
  for (loop = -1; loop <= 1.0; loop += 1/(width/2.0)) {
    glVertex3f( loop,
		FraunhoferIntensity(g_Radius/1e9, loop, g_Wavelength/1e9) / valAtZero,
		0.0 );
    glVertex3f( loop,
		0.0,
		0.0 );
  }
  glEnd();

  // Obscure that portion of the pattern that lies behind the hidden parts
  // of the pixels.  It starts on pixel boundaries and draws quads to hide
  // the portion of the pixel that can't be seen.
  glColor3f(0.5,0.5,0.5);   // Set to the background color
  double pixelStepInScreen = 1/(width/2.0) * (pixelSpacing/1e9) * units;
  for (loop = pixelStepInScreen/2; loop <= 1.0; loop += pixelStepInScreen) {
    // Draw a quad to obscure the middle of this pixel.  Draw one for positive side of
    // zero and one for negative size of zero.
    glBegin(GL_QUADS);
      glVertex3f( loop - 0.5*(pixelStepInScreen*pixelFrac) , 0, 0.0 );
      glVertex3f( loop + 0.5*(pixelStepInScreen*pixelFrac) , 0, 0.0 );
      glVertex3f( loop + 0.5*(pixelStepInScreen*pixelFrac) ,  1, 0.0 );
      glVertex3f( loop - 0.5*(pixelStepInScreen*pixelFrac) ,  1, 0.0 );

      glVertex3f( -loop - 0.5*(pixelStepInScreen*pixelFrac) , 0, 0.0 );
      glVertex3f( -loop + 0.5*(pixelStepInScreen*pixelFrac) , 0, 0.0 );
      glVertex3f( -loop + 0.5*(pixelStepInScreen*pixelFrac) ,  1, 0.0 );
      glVertex3f( -loop - 0.5*(pixelStepInScreen*pixelFrac) ,  1, 0.0 );
    glEnd();
  }

  // Draw a green curve tracing out the top of the diffraction
  // pattern as it goes radially away from the center.  This is a
  // normalized diffraction pattern, such that the peak is always at
  // 1.
  glDisable(GL_LINE_SMOOTH);
  glColor3f(0.5,1,0.5);
  glBegin(GL_LINE_STRIP);
  for (loop = -1; loop <= 1.0; loop += 1/(width/2.0)) {
    glVertex3f( loop,
		FraunhoferIntensity(g_Radius/1e9, loop, g_Wavelength/1e9) / valAtZero,
		0.0 );
  }
  glEnd();

  // Draw tick marks showing the spacing of the pixels, and an axis.
  // Note that the pixel boundaries are offset one half pixel from the center.
  glDisable(GL_LINE_SMOOTH);
  glColor3f(0.7,0.7,0.7);
  glBegin(GL_LINES);
  glVertex3f( -1, 0, 0);
  glVertex3f(  1, 0, 0);
  for (loop = pixelStepInScreen/2; loop <= 1.0; loop += pixelStepInScreen) {
    glVertex3f( loop, 0.05, 0.0 );
    glVertex3f( loop, -0.05, 0.0 );
    glVertex3f( -loop, 0.05, 0.0 );
    glVertex3f( -loop, -0.05, 0.0 );
  }
  glEnd();
}

void drawPixelGrid(
  double width,		//< Width in pixels of the screen.  We draw from -1 to 1 in X and Y
  double units,		//< Converts from Pixel units to screen-space units
  double pixelSpacing)	//< How far in screen pixels between imager pixels
{
  double loop;
  // Draw tick marks showing the spacing of the pixels.
  // Note that the pixel boundaries are offset one half pixel from the center.
  glDisable(GL_LINE_SMOOTH);
  glColor3f(0.7,0.7,0.7);
  glBegin(GL_LINES);
  double pixelStepInScreen = 1/(width/2.0) * (pixelSpacing/1e9) * units;
  for (loop = pixelStepInScreen/2; loop <= 1.0; loop += pixelStepInScreen) {
    glVertex3f( loop, -1, 0.0 );    // +X line
    glVertex3f( loop, 1, 0.0 );
    glVertex3f( -loop, -1, 0.0 );   // -X line
    glVertex3f( -loop, 1, 0.0 );
    glVertex3f( -1, loop, 0.0 );    // +Y line
    glVertex3f( 1, loop, 0.0 );
    glVertex3f( -1, -loop, 0.0 );   // -Y line
    glVertex3f( 1, -loop,0.0 );
  }
  glEnd();
}

void myDisplayFunc(void)
{
  // Clear the window and prepare to draw in the back buffer
  glDrawBuffer(GL_BACK);
  glClearColor(0.5, 0.5, 0.5, 0.0);
  glClear(GL_COLOR_BUFFER_BIT);

  // Draw the curve vs. the X axis.
  glLoadIdentity();
  glScalef(0.5,1.0,1.0);
  glTranslatef(-1.0, 0.0, 0.0);
  drawSideViewVsPixels(Window_Size_X, Window_Units_X, g_PixelSpacing, g_PixelHidden);

  // Draw the curve vs. the Y axis.
  glLoadIdentity();
  glScalef(1.0,0.5,1.0);
  glTranslatef(0.0, -1.0, 0.0);
  glRotatef(-90, 0,0,1);
  drawSideViewVsPixels(Window_Size_X, Window_Units_X, g_PixelSpacing, g_PixelHidden);

  // Draw the pixel grid.
  glLoadIdentity();
  glScalef(0.5,0.5,1.0);
  glTranslatef(-1.0, -1.0, 0.0);
  drawPixelGrid(Window_Size_X, Window_Units_X, g_PixelSpacing);

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
  // Post a redisplay so that Glut will draw the new image
  if (!g_already_posted) {
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
  vrpn_SleepMsecs(5);
}

void mouseCallbackForGLUT(int button, int state, int x, int y) {

    // Record where the button was pressed for use in the motion
    // callback.
    g_mousePressX = x;
    g_mousePressY = y;

    switch(button) {
      case GLUT_RIGHT_BUTTON:
	if (state == GLUT_DOWN) {
	  printf("XXX Pressed right button\n");
	  g_whichDragAction = 1;
	} else {
	  printf("XXX Released right button\n");
	}
	break;

      case GLUT_MIDDLE_BUTTON:
	if (state == GLUT_DOWN) {
	  printf("XXX Pressed middle button\n");
	  g_whichDragAction = 0;
	}
	break;

      case GLUT_LEFT_BUTTON:
	if (state == GLUT_DOWN) {
	  printf("XXX Pressed left button\n");
	  g_whichDragAction = 1;
	}
	break;
    }
}

void motionCallbackForGLUT(int x, int y) {

  switch (g_whichDragAction) {

  case 0: //< Do nothing on drag.
    break;

  case 1:
    {
	  printf("XXX Dragging action 1\n");
    }
    break;

  default:
    fprintf(stderr,"Internal Error: Unknown drag action (%d)\n", g_whichDragAction);
  }
  return;
}


//--------------------------------------------------------------------------
// Tcl callback routines.

void  logfilename_changed(char *newvalue, void *)
{
  printf("XXX Logfile name changed to %s\n", newvalue);
}

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
  //------------------------------------------------------------------
  // If there is a command-line argument, treat it as the name of a
  // file that is to be loaded.
  switch (argc) {
  case 1:
    // No arguments, we're in good shape
    break;

  default:
    fprintf(stderr, "Usage: %s\n", argv[0]);
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
  // Put the control window where we want it.
  sprintf(command, "wm geometry . +10+10");
  if (Tcl_Eval(tk_control_interp, command) != TCL_OK) {
          fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
                  tk_control_interp->result);
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
  sprintf(command, "label .versionlabel -text airy_maker_v:_%s", Version_string);
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
  sprintf(command, "source airy_maker.tcl");
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
  // Initialize GLUT and create the window that will display the
  // video -- name the window after the device that has been
  // opened in VRPN.
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
  glutInitWindowPosition(190, 140);
  glutInitWindowSize(Window_Size_X, Window_Size_Y);
#ifdef DEBUG
  printf("XXX initializing window to %dx%d\n", g_camera->get_num_columns(), g_camera->get_num_rows());
#endif
  glutCreateWindow("airy_vs_pixels");
  glutMotionFunc(motionCallbackForGLUT);
  glutMouseFunc(mouseCallbackForGLUT);

  // Set the display function and idle function for GLUT (they
  // will do all the work) and then give control over to GLUT.
  glutDisplayFunc(myDisplayFunc);
  glutIdleFunc(myIdleFunc);
  glutMainLoop();

  cleanup();
  return 0;
}
