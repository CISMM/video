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

enum { M_NOTHING, M_X_ONLY, M_Y_ONLY, M_X_AND_Y };

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
Tclvar_float_with_scale	g_PixelXOffset("x_offset_pix", "", -1, 1, 0);
Tclvar_float_with_scale	g_PixelYOffset("y_offset_pix", "", -1, 1, 0);
Tclvar_float_with_scale	g_SampleCount("samples", "", 64, 256, 64);
Tclvar_int_with_button	g_ShowSqrt("show_sqrt","", 1);
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
  double pixelFrac,	//< What fraction of imager pixels are not active
  double offset)	//< How far from center (and which direction) to offset Airy pattern in capture device pixel units
{
  float	loop;	  // So we don't get integer math on coordinates

  // Draw a green area tracing out a side view of the diffraction
  // pattern as it goes radially away from the center.  This is a
  // normalized diffraction pattern, such that the peak is always at
  // 1.
  glDisable(GL_LINE_SMOOTH);
  glColor3f(0.5,0.7,0.5);
  glBegin(GL_LINES);
  double valAtZero = FraunhoferIntensity(g_Radius/1e9, 0, g_Wavelength/1e9,1.0);
  double airyOffsetMatchingPixelOffset = -offset / width * pixelSpacing * 2;
  for (loop = -1; loop <= 1.0; loop += 1/(width/2.0)) {
    glVertex3f( loop,
		FraunhoferIntensity(g_Radius/1e9, loop + airyOffsetMatchingPixelOffset, g_Wavelength/1e9,1.0) / valAtZero,
		0.0 );
    glVertex3f( loop,
		0.0,
		0.0 );
  }
  glEnd();

  // Obscure that portion of the pattern that lies behind the hidden parts
  // of the pixels.  It starts on pixel boundaries and draws quads to hide
  // the portion of the pixel that can't be seen.
  glColor3f(0.4,0.4,0.4);   // Set to the background color
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
		FraunhoferIntensity(g_Radius/1e9, loop + airyOffsetMatchingPixelOffset, g_Wavelength/1e9,1.0) / valAtZero,
		0.0 );
  }
  glEnd();

  // Draw a dimmer green curve tracing out the square root of the diffraction
  // pattern as it goes radially away from the center.  This is a
  // normalized diffraction pattern, such that the peak is always at
  // 1.
  if (g_ShowSqrt) {
    glDisable(GL_LINE_SMOOTH);
    glColor3f(0.5,0.7,0.5);
    glBegin(GL_LINE_STRIP);
    for (loop = -1; loop <= 1.0; loop += 1/(width/2.0)) {
      glVertex3f( loop,
		  sqrt(FraunhoferIntensity(g_Radius/1e9, loop + airyOffsetMatchingPixelOffset, g_Wavelength/1e9,1.0) / valAtZero),
		  0.0 );
    }
    glEnd();
  }

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

// Compute the volume under part of the Airy disk.  Do this by
// sampling the disk densely, finding the average value within the area,
// and then multiplying by the area.

double	computeAiryVolume(
  double x0,		//< Low end of X integration range in Airy-disk coordinates
  double x1,		//< High end of X integration range
  double y0,		//< Low end of Y integration range
  double y1,		//< High end of Y integration range
  int samples)		//< How many samples to take in each of X and Y
{
  int count = 0;	//< How many pixels we have summed up
  double x;		//< Steps through X
  double y;		//< Steps through Y
  double sx = (x1 - x0) / (samples + 1);
  double sy = (y1 - y0) / (samples + 1);
  double sum = 0;

  for (x = x0 + sx/2; x < x1; x += sx) {
    for (y = y0 + sy/2; y < y1; y += sy) {
      count++;
      sum += FraunhoferIntensity(g_Radius/1e9, x, y, g_Wavelength/1e9, 1.0);
    }
  }

  return (sum / count) * (x1-x0) * (y1-y0);
}

void drawPixelBarGraphs(
  double width,		//< Width in pixels of the screen (assumed isotropic in X and Y
  double units,		//< Converts from Pixel units to screen-space units
  double pixelSpacing,	//< How far in screen pixels between imager pixels
  double pixelFrac,	//< What fraction of imager pixels are not active
  double xOffset,	//< How far from the center (and which direction) to offset Airy pattern in X in capture device units
  double yOffset)	//< How far from the center (and which direction) to offset Airy pattern in Y in capture device units
{
  // Find the volume of the whole Airy disk by going way out past its
  // border.
  double totVol = computeAiryVolume(-1,1, -1,1, g_SampleCount);

  // Compute how far to step between pixel centers in the screen, then
  // how far down you can go without getting below -1 when taking these
  // steps.  Then take one more step than this to get started.
  // XXX Adjust for offsets.
  double airyOffsetMatchingPixelOffsetX = -xOffset / width * pixelSpacing * 2;
  double airyOffsetMatchingPixelOffsetY = yOffset / width * pixelSpacing * 2;
  double pixelStepInScreen = 1/(width/2.0) * (pixelSpacing/1e9) * units;
  double halfStepInScreen = 0.5 * pixelStepInScreen;
  int numsteps = floor(1/pixelStepInScreen);
  double start = - (numsteps+1) * pixelStepInScreen;
  double x, y;
  for (x = start; x < 1.0; x += pixelStepInScreen) {
    for (y = start; y < 1.0; y += pixelStepInScreen) {

      // Find the four corners of the pixel where we are going to draw the
      // bar graph.  These include the whole pixel area.  These are found in
      // the screen space, from -1..1 in X and Y.
      double xs0 = x - halfStepInScreen;
      double xs1 = x + halfStepInScreen;
      double ys0 = y - halfStepInScreen;
      double ys1 = y + halfStepInScreen;

      // Find the four corners of the pixel that actually has area to receive
      // photons.  This removes the area clipped by the pixelFrac parameter.
      // These are also found in screen space.
      double xc0 = xs0 + halfStepInScreen*pixelFrac;
      double xc1 = xs1 - halfStepInScreen*pixelFrac;
      double yc0 = ys0 + halfStepInScreen*pixelFrac;
      double yc1 = ys1 - halfStepInScreen*pixelFrac;

      // Convert the pixel coordinates from screen space space to
      // Airy-disk space.  XXX This will need scaling as well as offset
      // when we get a more formal unit description.
      double xa0 = xc0 + airyOffsetMatchingPixelOffsetX;
      double xa1 = xc1 + airyOffsetMatchingPixelOffsetX;
      double ya0 = yc0 + airyOffsetMatchingPixelOffsetY;
      double ya1 = yc1 + airyOffsetMatchingPixelOffsetY;

      // Compute the fraction of volume falling within the receiving area by
      // dividing the volume within it to the volume covered by the whole Airy disk
      // computed above.  This is the fraction of the pixel to fill.
      double frac = computeAiryVolume(xa0, xa1, ya0, ya1, g_SampleCount * pixelStepInScreen / 2) / totVol;

      // Draw a filled-in graph that covers the fraction of the whole pixel
      // that equals the fraction of the Airy disk volume computed above.
      glColor3f(0.5,0.8,0.5);
      glBegin(GL_QUADS);
	glVertex3f( xs0, ys0, 0.0 );
	glVertex3f( xs1, ys0, 0.0 );
	glVertex3f( xs1, ys0 + frac * (ys1-ys0), 0.0 );
	glVertex3f( xs0, ys0 + frac * (ys1-ys0), 0.0 );
      glEnd();
    }
  }
}

void drawCrossHairs(
  double width,		//< Width in pixels of the screen (assumed isotropic in X and Y
  double units,		//< Converts from Pixel units to screen-space units
  double pixelSpacing,	//< How far in screen pixels between imager pixels
  double xOffset,	//< How far from the center (and which direction) to offset Airy pattern in X in capture device units
  double yOffset)	//< How far from the center (and which direction) to offset Airy pattern in Y in capture device units
{
  double airyOffsetMatchingPixelOffsetX = -xOffset / width * pixelSpacing * 2;
  double airyOffsetMatchingPixelOffsetY = yOffset / width * pixelSpacing * 2;

  glColor3f(0.8, 0.6, 0.6);
  glBegin(GL_LINES);
    glVertex3f( -1.0, -airyOffsetMatchingPixelOffsetY, 0.0 );
    glVertex3f(  1.0, -airyOffsetMatchingPixelOffsetY, 0.0 );
    glVertex3f( -airyOffsetMatchingPixelOffsetX, -1.0, 0.0 );
    glVertex3f( -airyOffsetMatchingPixelOffsetX,  1.0, 0.0 );
  glEnd();
}

void myDisplayFunc(void)
{
  // Clear the window and prepare to draw in the back buffer
  glDrawBuffer(GL_BACK);
  glClearColor(0.4, 0.4, 0.4, 0.0);
  glClear(GL_COLOR_BUFFER_BIT);

  // Draw the curve vs. the X axis.
  glLoadIdentity();
  glScalef(0.5,1.0,1.0);
  glTranslatef(-1.0, 0.0, 0.0);
  drawSideViewVsPixels(Window_Size_X, Window_Units_X, g_PixelSpacing, g_PixelHidden, g_PixelXOffset);

  // Draw the curve vs. the Y axis.
  glLoadIdentity();
  glScalef(1.0,0.5,1.0);
  glTranslatef(0.0, -1.0, 0.0);
  glRotatef(-90, 0,0,1);
  drawSideViewVsPixels(Window_Size_X, Window_Units_X, g_PixelSpacing, g_PixelHidden, g_PixelYOffset);

  // Draw the pixel grid in the lower-left portion of the screen.
  glLoadIdentity();
  glScalef(0.5,0.5,1.0);
  glTranslatef(-1.0, -1.0, 0.0);
  drawPixelGrid(Window_Size_X, Window_Units_X, g_PixelSpacing);

  // Draw the bar graphs for the lower-left portion of the screen.
  drawPixelBarGraphs(Window_Size_X, Window_Units_X, g_PixelSpacing, g_PixelHidden, g_PixelXOffset, g_PixelYOffset);

  // Draw the cross-hairs to show the pixel center in the lower-left portion of the screen.
  drawCrossHairs(Window_Size_X, Window_Units_X, g_PixelSpacing, g_PixelXOffset, g_PixelYOffset);

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

// Computes offset based on the location of the mouse within
// the lower-left quadrant of the display, where the grid is
// shown bracketed by the Airy patterns.  Computes the offset
// the would place the center of the Airy pattern at that location
// in x and y.  Then clamps the offsets to the range -1..+1.

void  compute_x_offset_based_on_mouse(int x)
{
  // Compute the location of the mouse within the quadrant
  // in the range -1..+1 for each of X and Y.  +X is to the
  // right and +Y is up.
  double xNorm = x*2.0/Window_Size_X - 0.5;

  // Convert this to offset in display pixel units.
  double xOffset = xNorm * Window_Size_X / (g_PixelSpacing);

  // Clamp this offset to (-1,1) and store in the globals.
  if (xOffset < -1) { xOffset = -1; }
  if (xOffset > 1) { xOffset = 1; }
  g_PixelXOffset = xOffset;
}

// Computes offset based on the location of the mouse within
// the lower-left quadrant of the display, where the grid is
// shown bracketed by the Airy patterns.  Computes the offset
// the would place the center of the Airy pattern at that location
// in x and y.  Then clamps the offsets to the range -1..+1.

void  compute_y_offset_based_on_mouse(int y)
{
  // Compute the location of the mouse within the quadrant
  // in the range -1..+1 for each of X and Y.  +X is to the
  // right and +Y is up.
  double yNorm = y*2.0/Window_Size_Y - 1.5;

  // Convert this to offset in display pixel units.
  double yOffset = yNorm * Window_Size_Y / (g_PixelSpacing);

  // Clamp this offset to (-1,1) and store in the globals.
  if (yOffset < -1) { yOffset = -1; }
  if (yOffset > 1) { yOffset = 1; }
  g_PixelYOffset = yOffset;
}

void mouseCallbackForGLUT(int button, int state, int x, int y)
{
    // Record where the button was pressed for use in the motion
    // callback.
    g_mousePressX = x;
    g_mousePressY = y;

    switch(button) {
      case GLUT_RIGHT_BUTTON:
	if (state == GLUT_DOWN) {
	  printf("XXX Pressed right button\n");
	  g_whichDragAction = 0;
	} else {
	  printf("XXX Released right button\n");
	}
	break;

      case GLUT_MIDDLE_BUTTON:
	if (state == GLUT_DOWN) {
	  printf("XXX Pressed middle button\n");
	  g_whichDragAction = M_NOTHING;
	}
	break;

      case GLUT_LEFT_BUTTON:
	if (state == GLUT_DOWN) {
	  // Left mouse button clicked in the lower-left quadrant
	  // of the screen (bracketed by the airey patterns) causes
	  // the offsets to be set to the point where the mouse is.
	  // Dragging here continues to reset this offset.  There is
	  // a limit of +/- 1 on the offset.
	  if ( (x <= Window_Size_X/2) && (y >= Window_Size_Y/2) ) {
    	    compute_x_offset_based_on_mouse(x);
    	    compute_y_offset_based_on_mouse(y);
	    g_whichDragAction = M_X_AND_Y;
	  }

	  // Left mouse button clicked in the upper-left quadrant
	  // causes motion of only the X offet.
	  if ( (x <= Window_Size_X/2) && (y < Window_Size_Y/2) ) {
    	    compute_x_offset_based_on_mouse(x);
	    g_whichDragAction = M_X_ONLY;
	  }

	  // Left mouse button clicked in the lower-right quadrant
	  // causes motion of only the X offet.
	  if ( (x > Window_Size_X/2) && (y >= Window_Size_Y/2) ) {
    	    compute_y_offset_based_on_mouse(y);
	    g_whichDragAction = M_Y_ONLY;
	  }

	}
	break;
    }
}

void motionCallbackForGLUT(int x, int y) {

  switch (g_whichDragAction) {

  case M_NOTHING: //< Do nothing on drag.
    break;

  case M_X_ONLY:
    {
	  compute_x_offset_based_on_mouse(x);
    }
    break;

  case M_Y_ONLY:
    {
	  compute_y_offset_based_on_mouse(y);
    }
    break;

  case M_X_AND_Y:
    {
	  compute_x_offset_based_on_mouse(x);
	  compute_y_offset_based_on_mouse(y);
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
