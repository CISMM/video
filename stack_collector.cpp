#include <stdio.h>
#include <string.h>
#include <vrpn_Connection.h>
#include <vrpn_nikon_controls.h>
#include "roper_server.h"
#include <tcl.h>
#include <tk.h>
#include "Tcl_Linkvar.h"

//--------------------------------------------------------------------------
// Version string for this program
const char *Version_string = "01.01";

double  g_focus = 0;    // Current setting for the microscope focus
bool	g_focus_changed = false;

//--------------------------------------------------------------------------
// Tcl controls and displays
Tclvar_float_with_scale	g_minX("minX", "", 0, 1391, 0);
Tclvar_float_with_scale	g_maxX("maxX", "", 0, 1391, 1391);
Tclvar_float_with_scale	g_minY("minY", "", 0, 1039, 0);
Tclvar_float_with_scale	g_maxY("maxY", "", 0, 1039, 1039);
Tclvar_float_with_scale	g_focusDown("focus_lower_microns", "", -20, 0, -5);
Tclvar_float_with_scale	g_focusUp("focus_raise_microns", "", 0, 20, 5);
Tclvar_float_with_scale	g_focusStep("focus_step_microns", "", (float)0.05, 5, 1);
Tclvar_float_with_scale	g_exposure("exposure_millisecs", "", 1, 1000, 10);
Tclvar_int_with_button	g_snap("snap","");
Tclvar_int_with_button	g_quit("quit","");


// Handles updates for the analog from the microscope by setting the
// focus to that location and telling that the focus has changed.

void handle_focus_change(void *, const vrpn_ANALOGCB info)
{
  g_focus = info.channel[0];
  g_focus_changed = true;
}

int main(unsigned argc, char *argv[])
{
  vrpn_Synchronized_Connection	*con = new vrpn_Synchronized_Connection();
  vrpn_Nikon_Controls		*nikon = new vrpn_Nikon_Controls("Focus", con);
  vrpn_Analog_Remote		*ana = new vrpn_Analog_Remote("Focus", con);
  vrpn_Analog_Output_Remote	*anaout = new vrpn_Analog_Output_Remote("Focus", con);

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

  while (!g_quit) {
    if (g_snap) {
      roper_server  *roper = new roper_server;
      double  startfocus;		      // Where the focus started at connection time
      double  focusdown = g_focusDown * 20;   // How far down to go when adjusting the focus (in ticks)
      double  focusstep = g_focusStep * 20;   // Step size of focus (in ticks)
      double  focusup = g_focusUp * 20;	      // How far above initial to set the focus (in ticks)
      double  focusloop;		      // Used to step the focus

      // Verify that the Roper camera is working.
      if (!roper->working()) {
	fprintf(stderr,"Could not establish connection to camera\n");
	exit(-1);
      }

      // Wait until we hear where the focus is from the Nikon
      ana->register_change_handler(NULL, handle_focus_change);
      g_focus_changed = false;
      printf("Waiting for response from Nikon\n");
      while (g_focus == 0) {
	nikon->mainloop();
	anaout->mainloop();
	ana->mainloop();
	vrpn_SleepMsecs(1);
      }
      printf("  (Initial focus found at %ld)\n", (long)g_focus);
      startfocus = g_focus;

      // Loop through the requested focus levels and read images.
      for (focusloop = startfocus + focusdown; focusloop <= startfocus + focusup; focusloop += focusstep) {

	printf("Going to %ld\n", (long)focusloop);
	anaout->request_change_channel_value(0, focusloop);
	while (g_focus != focusloop) {
	  nikon->mainloop();
	  anaout->mainloop();
	  ana->mainloop();
	  vrpn_SleepMsecs(1);
	}
	char name[1024];
	sprintf(name, "output_image_%ld.pgm", (long)focusloop);
	printf("Writing image to %s\n", name);
	roper->read_image_to_memory((int)g_minX,(int)g_maxX, (int)g_minY,(int)g_maxY, g_exposure);
	roper->write_memory_to_ppm_file(name);
      }

      // Put the focus back where it started.
      printf("Setting focus back to %ld\n", (long)startfocus);
      anaout->request_change_channel_value(0, startfocus);
      while (g_focus != startfocus) {
	  nikon->mainloop();
	  anaout->mainloop();
	  ana->mainloop();
	  vrpn_SleepMsecs(1);
      }

      delete roper;
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
	    return -1;
    }
  }

  if (ana) { delete ana; };
  if (anaout) { delete anaout; };
  if (nikon) { delete nikon; };
  if (con) { delete con; };
  return 0;
}