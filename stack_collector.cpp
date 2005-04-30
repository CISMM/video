//XXX Make an HTML page in the directory describing the data set

//XXX There are some off-by-1 errors in the way the red lines are drawn, maybe to
//    do with binning > 1.
//XXX It is not possible to select all the way to the top or right
//    because the window is only half the number of pixels in the
//    image, so it always wants to skip the last column.  This is only
//    true when binning is greater than 1.
//XXX The nikon focus reports take a long time to come when the
//    user manually focuses (like eight seconds after they stop
//    moving).  This happens even with the original server.
//    With the new server and old code, it quickly reports the
//    new analog location once it has moved there.  This seems
//    to be a feature of the microscope.

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <vrpn_Connection.h>
#include <vrpn_Tracker.h>
#include <vrpn_Poser.h>
#include <vrpn_Generic_server_object.h>
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
const unsigned FAKE_CAMERA_SIZE = 256;
//#define	FAKE_STAGE

//--------------------------------------------------------------------------
// Version string for this program
const char *Version_string = "02.03";
char  *g_device_name = NULL;			  //< Name of the device to open
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

vrpn_Synchronized_Connection	*con = NULL;
vrpn_Generic_Server_Object      *svr = NULL;

vrpn_Tracker_Remote		*read_z = NULL;
vrpn_Poser_Remote	        *set_z = NULL;

// The number of meters per micron.
const double  METERS_PER_MICRON = 1e-6;

//--------------------------------------------------------------------------
// Tcl controls and displays

void  logfilename_changed(char *newvalue, void *);
void  handle_preview_change(int newvalue, void *);
void  handle_tcl_focus_change(float newvalue, void *);
void  move_focus_and_wait_until_it_gets_there(double where_to_go_meters);
Tclvar_float_with_scale	*g_minX;
Tclvar_float_with_scale	*g_maxX;
Tclvar_float_with_scale	*g_minY;
Tclvar_float_with_scale	*g_maxY;
Tclvar_float_with_scale	g_focus_command("focus_command_microns", "", 0, 100, 50, handle_tcl_focus_change);
Tclvar_float_with_scale	g_focus("focus_microns", NULL, 0, 100, -1);
Tclvar_float_with_scale	g_focusDown("focus_lower_microns", NULL, 0, 20, 5);
Tclvar_float_with_scale	g_focusUp("focus_raise_microns", NULL, 0, 20, 5);
Tclvar_float_with_scale	g_focusStep("focus_step_microns", NULL, (float)0.05, 5, 1);
Tclvar_float_with_scale	g_images_per_step("images_per_step", NULL, (float)1, 20, 1);
Tclvar_float_with_scale	g_min_image_wait_millisecs("min_image_wait_millisecs", NULL, (float)0, 1000, 0);
Tclvar_float_with_scale	g_number_of_stacks("number_of_stacks", NULL, (float)1, 20, 1);
Tclvar_float_with_scale	g_min_repeat_wait_sec("min_repeat_wait_secs", NULL, (float)0, 60, 0);
Tclvar_float_with_scale	g_exposure("exposure_millisecs", NULL, 1, 1000, 10);
Tclvar_float_with_scale	g_gain("gain", "", 1, 32, 1);
Tclvar_float_with_scale	g_close_enough("position_error_microns", "", 0.001, 1, 0.1);
Tclvar_int_with_button	g_quit("quit",NULL);
// When the user pushes this button, the stack starts to be taken and it
// continues until the end, then the program turns this back off.
Tclvar_int_with_button	g_take_stack("logging",NULL);
Tclvar_selector		g_base_filename("logfilename", NULL, NULL, "", logfilename_changed);
char *                  g_base_filename_char = NULL;
Tclvar_int_with_button	g_sixteenbits("save_sixteen_bits",NULL);
Tclvar_float_with_scale	g_pixelcount("pixels_from_camera","", 8,12, 8);
Tclvar_int_with_button	g_step_past_bottom("step_past_bottom","",1);
Tclvar_int_with_button	g_preview("preview_video","", 1, handle_preview_change);


void  cleanup(void)
{
  glFinish();
  glutDestroyWindow(g_window_id);

  if (g_camera != NULL) { delete g_camera; };

#ifndef	FAKE_STAGE
  if (read_z) { delete read_z; read_z = NULL; };
  if (set_z) { delete set_z; set_z = NULL; };
  if (svr) { delete svr; svr = NULL; };
  if (con) { delete con; con = NULL; };
#endif
}

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

// Set stack collecting to be on or off depending on whether
// the logfilename is empty.  Also, copy the newvalue to the
// string base name for the file because otherwise it can become
// truncated (Tcl seems to have a maximum string length, or
// perhaps it is the TclVars that do.
void  logfilename_changed(char *newvalue, void *)
{
  if (strlen(newvalue) == 0) {
    g_take_stack = 0;
  } else {
    if (g_base_filename_char != NULL) {
      delete [] g_base_filename_char;
    }
    g_base_filename_char = new char[strlen(newvalue)+1];
    if (g_base_filename_char == NULL) {
      fprintf(stderr,"Out of memory allocating file name\n");
      cleanup();
      exit(-1);
    }
    strcpy(g_base_filename_char, newvalue);
    g_take_stack = 1;
  }
}

void  handle_tcl_focus_change(float newvalue_microns, void *)
{
  // Send the request for a new position in meters to the Poser.
  vrpn_float64  pos[3] = { 0, 0, newvalue_microns * METERS_PER_MICRON };
  vrpn_float64  quat[4] = { 0, 0, 0, 1 };
  struct timeval now;
  gettimeofday(&now, NULL);
  set_z->request_pose(now, pos, quat);
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
      int val = ((x+y + static_cast<int>((float)g_focus) )%256) * g_gain;
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
// Handles updates for the Stage from the microscope by setting the
// focus to that location (in microns) and telling that the focus has changed.

void VRPN_CALLBACK handle_vrpn_focus_change(void *, const vrpn_TRACKERCB info)
{
  if (info.sensor == 0) {
    g_focus = info.pos[2] / METERS_PER_MICRON;
    g_focus_changed = true;
  }
}

static	double	duration(struct timeval t1, struct timeval t2)
{
    return (t1.tv_usec - t2.tv_usec) / 1000000.0 +
	   (t1.tv_sec - t2.tv_sec);
}

void  wait_until_enough_time_has_passed_since(const struct timeval last_stack_started, double g_min_repeat_wait_sec)
{
  struct timeval now;
  
  do {
    //------------------------------------------------------------
    // This must be done in any Tcl app, to allow Tcl/Tk to handle
    // events.

    while (Tk_DoOneEvent(TK_DONT_WAIT)) {};

    //------------------------------------------------------------
    // This is called once every time through the main loop.  It
    // pushes changes in the C variab`les over to Tcl.

    if (Tclvar_mainloop()) {
	    fprintf(stderr,"Mainloop failed\n");
	    exit(-1);
    }

    //------------------------------------------------------------
    // If the user has deselected the "take_stack" or pressed "quit" then
    // break out of the loop.
    if ( g_quit || !g_take_stack) {
      return;
    }

    //------------------------------------------------------------
    // Find out what time it is so that we can know if it has been
    // long enough so that we should return.
    gettimeofday(&now, NULL);
  } while (duration(now, last_stack_started) < g_min_repeat_wait_sec);
}

void  move_focus_and_wait_until_it_gets_there(double where_to_go_meters) {
#ifndef	FAKE_STAGE
  // Request that the camera focus go where we want it to
  vrpn_float64  pos[3] = { 0, 0, where_to_go_meters };
  vrpn_float64  quat[4] = { 0, 0, 0, 1 };
  struct timeval now;
  gettimeofday(&now, NULL);
  set_z->request_pose(now, pos, quat);
  // Wait until the focus gets close enough to where we asked it to be
  double err;
  do {
    if (svr) { svr->mainloop(); }
    if (con) { con->mainloop(); }
    set_z->mainloop();
    read_z->mainloop();
    vrpn_SleepMsecs(1);

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

    //------------------------------------------------------------
    // This is called once every time through the main loop.  It
    // pushes changes in the C variab`les over to Tcl.

    if (Tclvar_mainloop()) {
	    fprintf(stderr,"Mainloop failed\n");
	    exit(-1);
    }
    err = fabs((float)g_focus - where_to_go_meters/METERS_PER_MICRON);
  } while (err > ((float)g_close_enough));
  printf("Found focus at %lg\n", (float)g_focus);
#endif
}

void myIdleFunc(void)
{
    // If we are previewing, then read from the camera and put
    // the image into the preview window.  Also, mainloop the
    // tracker to ensure that we hear about the latest position
    // of the focus.
    if (g_preview) {
#ifndef	FAKE_CAMERA
      g_camera->read_image_to_memory((int)*g_minX,(int)*g_maxX, (int)*g_minY,(int)*g_maxY, g_exposure);
#endif
      preview_image(g_camera);
#ifndef	FAKE_STAGE
      if (svr) { svr->mainloop(); }
      if (con) { con->mainloop(); }
      read_z->mainloop();
#endif
    }

    // If we've been asked to take a stack, do it.
    if (g_take_stack) {
      // All of these variables are in microns, so we need to convert from user-interface values
      // (which are in microns).  The VRPN values are reported in meters, so they don't get
      // converted here.
      double  startfocus;		      //< Where the focus started at connection time
      double  focusdown = -g_focusDown * METERS_PER_MICRON;    //< How far down to go when adjusting the focus (in meters)
      double  focusstep = g_focusStep * METERS_PER_MICRON;    //< Step size to change the focus by (in meters)
      double  focusup = g_focusUp * METERS_PER_MICRON;	      //< How far above initial to set the focus (in meters)
      double  focusloop;		      //< Used to step the focus
      int     repeat;			      //< Used to enable multiple scans

      // Wait until we hear where the focus is from the stage
#ifndef	FAKE_STAGE
      g_focus_changed = false;
      printf("Waiting for response from Focus control\n");
      while (!g_focus_changed) {
	if (svr) { svr->mainloop(); }
        if (con) { con->mainloop(); }
	set_z->mainloop();
	read_z->mainloop();

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

        //------------------------------------------------------------
        // This is called once every time through the main loop.  It
        // pushes changes in the C variab`les over to Tcl.

        if (Tclvar_mainloop()) {
	        fprintf(stderr,"Mainloop failed\n");
	        exit(-1);
        }

	vrpn_SleepMsecs(1);
      }
#endif
      printf("  (Initial focus found at %g)\n", ((float)g_focus));
      startfocus = g_focus * METERS_PER_MICRON;

      struct timeval last_stack_started;
      last_stack_started.tv_sec = 0;
      last_stack_started.tv_usec = 0;
      struct timeval last_image_started;
      last_image_started.tv_sec = 0;
      last_image_started.tv_usec = 0;

      // Loop through the number of repeats we have been asked to do
      for (repeat = 0; repeat < g_number_of_stacks; repeat++) {

        // Make sure we have waited the minimum time length since the
        // last time we collected a stack before starting the next
        // one.
        if (repeat > 0) {
          wait_until_enough_time_has_passed_since(last_stack_started, g_min_repeat_wait_sec);
        }
        gettimeofday(&last_stack_started, NULL);

	// Go one past the lower focus request in order to make sure we
	// always come past each plane going the same direction (avoids
	// trouble with hysteresis), if the flag is set asking for this.
	if (g_step_past_bottom) {
	  double beyond = startfocus + focusdown - focusstep;
	  printf("Stepping past bottom to %lg\n", beyond/METERS_PER_MICRON);
	  move_focus_and_wait_until_it_gets_there(beyond);
	}

	// Loop through the requested focus levels and read images.
	for (focusloop = startfocus + focusdown; focusloop <= startfocus + focusup; focusloop += focusstep) {

	  printf("Going to %lg\n", focusloop/METERS_PER_MICRON);
	  move_focus_and_wait_until_it_gets_there(focusloop);

          // We first read one image to make sure that we flush
          // any image that was actually acquired before or during the move.
#ifndef	FAKE_CAMERA
	  g_camera->read_image_to_memory((int)*g_minX,(int)*g_maxX, (int)*g_minY,(int)*g_maxY, g_exposure);
#endif
          //-----------------------------------------------------
	  // Read the requested number of images from the camera.
          // If we are previewing, then display the image in the
          // video window.
          for (unsigned image = 0; image < g_images_per_step; image++) {

            // Make sure we have waited the minimum time length since the
            // last time we grabbed an image before starting the next
            // one.  This is true for images at the same location in the
            // stack and for the last image taken at one stack location and
            // the first image taken at the next.  It does not govern the
            // time between two stacks; the first image from a second or
            // later stack could be right after the last image from the
            // previous stack.
            if ( (focusloop != startfocus + focusdown) || (image > 0) ) {
              wait_until_enough_time_has_passed_since(last_image_started, g_min_image_wait_millisecs / 1000.0);
            }
            gettimeofday(&last_image_started, NULL);
#ifndef	FAKE_CAMERA
	    g_camera->read_image_to_memory((int)*g_minX,(int)*g_maxX, (int)*g_minY,(int)*g_maxY, g_exposure);
	    if (g_preview) {
	      preview_image(g_camera);
	      myDisplayFunc();
	    }
#endif
	    // Write the image to a TIFF file.
	    char name[4096];
            if (strlen(g_base_filename_char) + 500 > sizeof(name)) {
              fprintf(stderr,"File name too long -- not saving\n");
              g_base_filename = "";
              break;
            }

            // Figure out the name of the image, inserting numbers to show
            // variability where there is more than one element in the loop.
            char repeat_string[100];
            repeat_string[0] = 0;
            if (g_number_of_stacks > 1) { sprintf(repeat_string,"R%02d", repeat); }
            char image_string[100];
            image_string[0] = 0;
            if (g_images_per_step > 1) { sprintf(image_string,"I%02d", image); }
            // The file name stores the REQUESTED height of the stack in nanometers, rounded to the nearest nm.
            // Store the stack relative to having a zero value at its lowest sampled location.
	    sprintf(name, "%s_G%03d%sNM%07ld%s.tif", g_base_filename_char, (int)g_gain, repeat_string,
              (long)((focusloop-startfocus-focusdown)/METERS_PER_MICRON*1000 + 0.5), image_string );
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
            // This is called once every time through the main loop.  It
            // pushes changes in the C variab`les over to Tcl.

            if (Tclvar_mainloop()) {
	            fprintf(stderr,"Mainloop failed\n");
	            exit(-1);
            }

	    //------------------------------------------------------------
	    // If the user has deselected the "take_stack" or pressed "quit" then
	    // break out of the loop.
	    if ( g_quit || !g_take_stack) {
	      goto jump_out_because_user_pressed_quit;
	    }
          } // End of one image acquisition
	} // End of one stack acquisition
      } // End of set of stack acquisitions

jump_out_because_user_pressed_quit:

      // Put the focus back where it started.  Clear the logging name.
      printf("Setting focus back to %lg\n", startfocus/METERS_PER_MICRON);
      move_focus_and_wait_until_it_gets_there(startfocus);
      printf("Done with stack\n");
      g_base_filename = "";
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
    g_number_of_stacks = floor(g_number_of_stacks);
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
  g_device_name = "directx";
  char  *config_file_name = "nikon.cfg";
  char	*Stage_name = "Focus@localhost";
  bool	use_local_server = true;
  con = NULL;
  svr = NULL;

  //------------------------------------------------------------------
  // If there is a command-line argument, treat it as the name of a
  // camera to be opened.  If there is none, default to DirectX.
  switch (argc) {

  case 1:
    // No arguments, so default to directx and local nikon server
    break;

  case 2:
    // Camera device type is named; still use default local nikon server
    g_device_name = argv[1];
    break;

  case 3:
    // Camera device name and Stage Tracker/Poser device name both specified.
    // If the stage name contains "@", we try to connect to a remote stage controller;
    // otherwise, create the name of a configuration file by adding ".cfg" to the name
    // passed in.
    g_device_name = argv[1];
    Stage_name = argv[2];
    if (strchr(Stage_name, '@') != NULL) {
      use_local_server = false;
    } else {
      config_file_name = new char[strlen(Stage_name) + 4];
      if (config_file_name == NULL) {
        fprintf(stderr,"Out of memory allocating configuration file name\n");
        exit(-1);
      }

      // Store the configuration file name.  The stage name should remain
      // "Focus", as the config file should make the Tracker and Poser
      // devices report this.
      use_local_server = true;
      sprintf(config_file_name, "%s.cfg", Stage_name);
      Stage_name = "Focus";
    }
    break;

  default:
    fprintf(stderr, "Usage: %s [edt|roper|diaginc|directx|directx640x480 [nikon|mcl|Stage_server_name@hostname]]\n", argv[0]);
    fprintf(stderr, "       default camera is directx\n");
    fprintf(stderr, "       default stage control is nikon\n");
    exit(-1);
  };

#ifndef	FAKE_STAGE
  if (use_local_server) {

    // Open a connection on which to talk between the server and client
    // portions of the code.  Then, start a VRPN generic server object
    // whose job it is to set up a Tracker to report stage position (Z) in
    // meters and a Poser whose job it is to move the stage in Z in
    // meters.  Both servers should have the name "Focus".  Other
    // auxilliary servers may be needed as well.
    con = new vrpn_Synchronized_Connection();

    svr = new vrpn_Generic_Server_Object(con, config_file_name);
    Stage_name = "Focus";
  }
#endif
  read_z = new vrpn_Tracker_Remote(Stage_name, con);
  set_z = new vrpn_Poser_Remote(Stage_name, con);
  read_z->register_change_handler(NULL, handle_vrpn_focus_change);

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
#else
  g_max_image_width = g_window_width = FAKE_CAMERA_SIZE;
  g_max_image_height = g_window_height = FAKE_CAMERA_SIZE;
#endif

  //------------------------------------------------------------------
  // Initialize the controls for the clipping based on the size of
  // the image we got.
#ifndef	FAKE_CAMERA
  g_minX = new Tclvar_float_with_scale("minX", ".clip", 0, g_camera->get_num_columns()-1, 0);
  g_maxX = new Tclvar_float_with_scale("maxX", ".clip", 0, g_camera->get_num_columns()-1, g_camera->get_num_columns()-1);
  g_minY = new Tclvar_float_with_scale("minY", ".clip", 0, g_camera->get_num_rows()-1, 0);
  g_maxY = new Tclvar_float_with_scale("maxY", ".clip", 0, g_camera->get_num_rows()-1, g_camera->get_num_rows()-1);
#else
  g_minX = new Tclvar_float_with_scale("minX", ".clip", 0, FAKE_CAMERA_SIZE-1, 0);
  g_maxX = new Tclvar_float_with_scale("maxX", ".clip", 0, FAKE_CAMERA_SIZE-1, FAKE_CAMERA_SIZE-1);
  g_minY = new Tclvar_float_with_scale("minY", ".clip", 0, FAKE_CAMERA_SIZE-1, 0);
  g_maxY = new Tclvar_float_with_scale("maxY", ".clip", 0, FAKE_CAMERA_SIZE-1, FAKE_CAMERA_SIZE-1);
#endif

  //------------------------------------------------------------------
  // Initialize GLUT display modes and create the window that will display the
  // video -- name the window after the device that has been
  // opened in VRPN.

  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
  glutInitWindowSize(g_window_width, g_window_height);
  glutInitWindowPosition(370, 140);
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
  sprintf(command, "toplevel .clip");
  if (Tcl_Eval(tk_control_interp, command) != TCL_OK) {
          fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
                  tk_control_interp->result);
          return(-1);
  }
  sprintf(command, "wm geometry .clip +200+110");
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
  // Load the specialized Tcl code needed by this program.  This must
  // be loaded before the Tclvar_init() routine is called because it
  // puts together some of the windows needed by the variables.
  sprintf(command, "source stack_collector.tcl");
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
