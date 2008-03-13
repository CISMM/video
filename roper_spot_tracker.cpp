#include <stdio.h>
#include <string.h>
#include "roper_server.h"
#include "image_wrapper.h"
#include "spot_tracker.h"
#include <tcl.h>
#include <tk.h>
#include "Tcl_Linkvar.h"

//--------------------------------------------------------------------------
// Version string for this program
const char *Version_string = "01.00";

double  g_focus = 0;    // Current setting for the microscope focus
bool	g_focus_changed = false;

//--------------------------------------------------------------------------
// Tcl controls and displays
Tclvar_float_with_scale	g_Y("y", "", 0, 1039, 0);
Tclvar_float_with_scale	g_X("x", "", 0, 1391, 0);
Tclvar_float_with_scale	g_Radius("radius", "", 1, 30, 5);
Tclvar_float_with_scale	g_minX("minX", "", 0, 1391, 0);
Tclvar_float_with_scale	g_maxX("maxX", "", 0, 1391, 1391);
Tclvar_float_with_scale	g_minY("minY", "", 0, 1039, 0);
Tclvar_float_with_scale	g_maxY("maxY", "", 0, 1039, 1039);
Tclvar_float_with_scale	g_exposure("exposure_millisecs", "", 1, 1000, 10);
Tclvar_int_with_button	g_opt("optimize","");
Tclvar_int_with_button	g_globalopt("global_optimize_now","",1);
Tclvar_int_with_button	g_quit("quit","");


int main(unsigned argc, char *argv[])
{
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
  // Open the roper camera
  roper_server  *roper = new roper_server;

  // Verify that the Roper camera is working.
  if (!roper->working()) {
    fprintf(stderr,"Could not establish connection to camera\n");
    exit(-1);
  }

  //------------------------------------------------------------------
  // Create the tracker that will follow the bead around.
  disk_spot_tracker tracker(5);

  while (!g_quit) {

    if (g_opt) {
      static  double  x = 0, y = 0;

      // Read an image from the roper into memory, within a structure that
      // is wrapped by an image_wrapper object that the tracker can use.
      roper->read_image_to_memory((int)g_minX,(int)g_maxX, (int)g_minY,(int)g_maxY, g_exposure);

      // If the user has requested a global search, do this once and then reset the
      // checkbox.
      if (g_globalopt) {
	tracker.locate_good_fit_in_image(*roper, 0, x,y);
	g_globalopt = 0;
      }

      // Optimize to find the best fit in the image.
      tracker.optimize(*roper, 0, x, y);

      // Show the result
      g_X = (float)x;
      g_Y = (float)y;
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
	    return -1;
    }
  }

  // Done with the camera
  delete roper;

  return 0;
}