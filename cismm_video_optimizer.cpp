//---------------------------------------------------------------------------
// This section contains configuration settings for the Video Optimizer.
// It is used to make it possible to compile and link the code when one or
// more of the camera- or file- driver libraries are unavailable. First comes
// a list of definitions.  These should all be defined when building the
// program for distribution.  Following that comes a list of paths and other
// things that depend on these definitions.  They may need to be changed
// as well, depending on where the libraries were installed.

// Now defined in CMake
//#define	VST_USE_ROPER
//#define	VST_USE_COOKE
//#define	VST_USE_EDT
//#define	VST_USE_DIAGINC
//#define	VST_USE_SEM
//#define	VST_USE_DIRECTX
//#define VST_USE_VRPN_IMAGER
////#define USE_METAMORPH	    // Metamorph reader not completed.

// END configuration section.
//---------------------------------------------------------------------------

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tcl.h>
#include <tk.h>
#include "Tcl_Linkvar.h"
#include "file_stack_server.h"
#include "image_wrapper.h"
#include "spot_tracker.h"

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef __APPLE__
#include <OPENGL/gl.h>
#include <GLUT/glut.h>
#else
#include <GL/gl.h>
#include <GL/glut.h>
#endif

#include <quat.h>
#include <vrpn_Types.h>
#include <vrpn_FileConnection.h>

// This pragma tells the compiler not to tell us about truncated debugging info
// due to name expansion within the string, list, and vector classes.
#pragma warning( disable : 4786 4995 4996 )
#include <list>
#include <vector>
using namespace std;
#include "controllable_video.h"

//#define	DEBUG

static void cleanup();
static void dirtyexit();

//--------------------------------------------------------------------------
// Version string for this program
const char *Version_string = "02.03";

//--------------------------------------------------------------------------
// Global constants

const int KERNEL_DISC = 0;	  //< These must match the values used in cismm_video_optimizer.tcl.
const int KERNEL_CONE = 1;
const int KERNEL_SYMMETRIC = 2;

const int DISPLAY_COMPUTED = 0;	  //< These must match the values used in cismm_video_optimizer.tcl
const int DISPLAY_MIN = 1;
const int DISPLAY_MAX = 2;
const int DISPLAY_MEAN = 3;

const int SUBTRACT_NONE = 0;	  //< These must match the values used in cismm_video_optimizer.tcl
const int SUBTRACT_MIN = 1;
const int SUBTRACT_MAX = 2;
const int SUBTRACT_MEAN = 3;
const int SUBTRACT_SINGLE = 4;
const int SUBTRACT_NEIGHBORS = 5;

//--------------------------------------------------------------------------
// Glut wants to take over the world when it starts, so we need to make
// global access to the objects we will be using.

char  *g_device_name = NULL;			  //< Name of the device to open
base_camera_server  *g_camera = NULL;		  //< Camera used to get an image
image_wrapper	    *g_image_to_display = NULL;	  //< Image that we're supposed to display
copy_of_image	    *g_last_image = NULL;	  //< Copy of the last image we had, if any
copy_of_image	    *g_this_image = NULL;	  //< Copy of the current image we are using
copy_of_image	    *g_next_image = NULL;	  //< Copy of the next image we will use
copy_of_image	    *g_subtract_image = NULL;	  //< Image to subtract from the current image
averaged_image	    *g_averaged_image = NULL;	  //< Averaged image for when we are going neighbor subtraction
image_metric	    *g_min_image = NULL;	  //< Accumulates minimum of images
image_metric	    *g_max_image = NULL;	  //< Accumulates maximum of images
image_metric	    *g_mean_image = NULL;	  //< Accumulates mean of images
image_wrapper	    *g_calculated_image = NULL;	  //< Image calculated from the camera image and other parameters
float		    g_search_radius = 0;	  //< Search radius for doing local max in before optimizing.
Controllable_Video  *g_video = NULL;		  //< Video controls, if we have them
Tclvar_int_with_button	g_frame_number("frame_number",NULL,-1);  //< Keeps track of video frame number

int		    g_tracking_window;		  //< Glut window displaying tracking
unsigned char	    *g_glut_image = NULL;	  //< Pointer to the storage for the image

list <Spot_Information *>g_trackers;		  //< List of active trackers
spot_tracker_XY	    *g_active_tracker = NULL;	  //< The tracker that the controls refer to
bool		    g_ready_to_display = false;	  //< Don't unless we get an image
bool		    g_already_posted = false;	  //< Posted redisplay since the last display?
int		    g_mousePressX, g_mousePressY; //< Where the mouse was when the button was pressed
int		    g_whichDragAction;		  //< What action to take for mouse drag

//--------------------------------------------------------------------------
// Tcl controls and displays
void  device_filename_changed(char *newvalue, void *);
void  logfilename_changed(char *newvalue, void *);
void  rebuild_trackers(int newvalue, void *);
void  rebuild_trackers(float newvalue, void *);
void  set_maximum_search_radius(int newvalue, void *);
void  make_appropriate_tracker_active(int newvalue, void *);
void  subtract_changed(int newvalue, void *);
void  accumulate_min_changed(int newvalue, void *);
void  accumulate_max_changed(int newvalue, void *);
void  accumulate_mean_changed(int newvalue, void *);
void  display_which_image_changed(int newvalue, void *);
Tclvar_float		g_X("x");
Tclvar_float		g_Y("y");
Tclvar_float_with_scale	g_Radius("radius", ".kernel.radius", 1, 30, 5);
Tclvar_float_with_scale	*g_minX;
Tclvar_float_with_scale	*g_maxX;
Tclvar_float_with_scale	*g_minY;
Tclvar_float_with_scale	*g_maxY;
Tclvar_float_with_scale	g_exposure("exposure_millisecs", "", 1, 1000, 10);
Tclvar_int_with_button	g_sixteenbits("sixteenbit_log",NULL,1);
Tclvar_int_with_button	g_monochrome("monochrome_log",NULL,0);
Tclvar_int_with_button	g_log_pointspread("pointspread_log",NULL,0);
Tclvar_float_with_scale	g_colorIndex("red_green_blue", NULL, 0, 2, 0);
Tclvar_float_with_scale	g_bitdepth("bit_depth", "", 8, 16, 8);
Tclvar_float_with_scale g_precision("precision", "", 0.001, 1.0, 0.05, rebuild_trackers);
Tclvar_float_with_scale g_sampleSpacing("sample_spacing", "", 0.1, 1.0, 1.0, rebuild_trackers);
Tclvar_int_with_button	g_invert("dark_spot",NULL,1, rebuild_trackers);
Tclvar_int_with_button	g_interpolate("interpolate",NULL,1, rebuild_trackers);
Tclvar_int_with_button	g_areamax("areamax",NULL,0, set_maximum_search_radius);
Tclvar_int_with_button	g_kernel_type("kerneltype", NULL, KERNEL_SYMMETRIC, rebuild_trackers);
Tclvar_int_with_button	g_opt("optimize",".kernel.optimize");
Tclvar_int_with_button	g_reorient("reorient","",0, make_appropriate_tracker_active);
Tclvar_int_with_button	g_rescale("rescale","",0, make_appropriate_tracker_active);
Tclvar_int_with_button	g_round_cursor("round_cursor","");
Tclvar_int_with_button	g_full_area("full_area","");
Tclvar_int_with_button	g_mark("show_tracker","",1);
Tclvar_int_with_button	g_show_imagemix_control("show_imagemix_control","",0);
Tclvar_int_with_button	g_display_which_image("display",NULL,0, display_which_image_changed);
Tclvar_int_with_button	g_subtract("subtract",NULL,0, subtract_changed);
Tclvar_int_with_button	g_accumulate_min("accumulate_min_image",".imagemix.statistics",0, accumulate_min_changed);
Tclvar_int_with_button	g_accumulate_max("accumulate_max_image",".imagemix.statistics",0, accumulate_max_changed);
Tclvar_int_with_button	g_accumulate_mean("accumulate_mean_image",".imagemix.statistics",0, accumulate_mean_changed);
Tclvar_int_with_button	g_show_clipping("show_clipping","",0);
Tclvar_int_with_button	g_quit("quit", NULL);
Tclvar_int_with_button	*g_play = NULL, *g_rewind = NULL, *g_step = NULL;
Tclvar_selector		g_logfilename("logfilename", NULL, NULL, "", logfilename_changed, NULL);
double			g_log_offset_x, g_log_offset_y;
double			g_orig_length, g_orig_orient;
char			*g_logfile_base_name = NULL;
copy_of_image		*g_log_last_image = NULL;
unsigned		g_log_frame_number_last_logged = -1;
PSF_File		*g_psf_file = NULL;
bool g_video_valid = false; // Do we have a valid video frame in memory?

Tclvar_int_with_button	g_show_gain_control("show_gain_control","",0);
Tclvar_int_with_button	g_auto_gain("auto_gain",NULL,0);
Tclvar_float_with_scale	g_clip_high("clip_high", ".gain.high", 0, 1, 1);
Tclvar_float_with_scale	g_clip_low("clip_low", ".gain.low", 0, 1, 0);


//--------------------------------------------------------------------------
// Helper routine to get the Y coordinate right when going between camera
// space and openGL space.
double	flip_y(double y)
{
  return g_image_to_display->get_num_rows() - 1 - y;
}

/// Create a pointer to a new tracker of the appropriate type,
// given the global settings for interpolation and inversion.
// Return NULL on failure.

spot_tracker_XY  *create_appropriate_tracker(void)
{
    if (g_kernel_type == KERNEL_SYMMETRIC) {
      g_interpolate = 1;
      return new symmetric_spot_tracker_interp(g_Radius,(g_invert != 0), g_precision, 0.1, g_sampleSpacing);
    } else if (g_kernel_type == KERNEL_CONE) {
      g_interpolate = 1;
      return new cone_spot_tracker_interp(g_Radius,(g_invert != 0), g_precision, 0.1, g_sampleSpacing);
    } else if (g_interpolate) {
      return new disk_spot_tracker_interp(g_Radius,(g_invert != 0), g_precision, 0.1, g_sampleSpacing);
    } else {
      return new disk_spot_tracker(g_Radius,(g_invert != 0), g_precision, 0.1, g_sampleSpacing);
    }
}

static	bool  save_log_frame(unsigned frame_number)
{
    // Add a line to the PSF file if we have one.
    if (g_psf_file) {
      if (!g_psf_file->append_line(*g_log_last_image,
	    g_trackers.front()->xytracker()->get_x(),
	    g_trackers.front()->xytracker()->get_y())) {
	return false;
      }
    }

    // Store the cropped and adjusted image.
    char  *filename = new char[strlen(g_logfile_base_name) + 15];
    if (filename == NULL) {
      fprintf(stderr, "Out of memory!\n");
      return false;
    }
    g_log_frame_number_last_logged = frame_number;
    sprintf(filename, "%s.opt.%04d.tif", g_logfile_base_name, frame_number);

    list<Spot_Information *>::iterator  loop;
    loop = g_trackers.begin();
    spot_tracker_XY *first_tracker = (*loop)->xytracker() ;
    loop++;
    spot_tracker_XY *second_tracker = (*loop)->xytracker();

    // Difference in position between first and second tracker.
    double dx = second_tracker->get_x() - first_tracker->get_x();
    double dy = second_tracker->get_y() - first_tracker->get_y();

    double rotation = 0.0;
    double scale = 1.0;
    if (g_reorient) {
      double new_orient = atan2( dy, dx );
      rotation = new_orient - g_orig_orient;
    }

    if (g_rescale) {
      double new_length = sqrt( dx*dx + dy*dy);
      if (g_orig_length != 0) {
	scale = new_length/g_orig_length;
      }
    }

    // Make a transformed image class to re-index the copied image.
    // Use the faster, translate-only version if there is no rotation
    // or scaling.
    image_wrapper *shifted = NULL;
    if (g_reorient || g_rescale) {
      // Specify the center of rotation and scaling as the current
      // position of the origin tracker.
      shifted = new affine_transformed_image(*g_log_last_image, 
	  g_trackers.front()->xytracker()->get_x() - g_log_offset_x,
	  g_trackers.front()->xytracker()->get_y() - g_log_offset_y,
	  rotation,
	  scale,
	  g_trackers.front()->xytracker()->get_x(),
	  g_trackers.front()->xytracker()->get_y());
    } else {
      shifted = new translated_image(*g_log_last_image, 
	  g_trackers.front()->xytracker()->get_x() - g_log_offset_x,
	  g_trackers.front()->xytracker()->get_y() - g_log_offset_y);
    }
    if (shifted == NULL) {
      delete [] filename;
      return false;
    }

    // Figure out whether the image will be sixteen bits, and also
    // the gain to apply (256 if 8-bit, 1 if 16-bit).  If it is 8-bit
    // output, also apply a shift related to the global bit-shift,
    // so that what ends up in the file is the same as what ends up
    // on the screen.
    bool do_sixteen = (g_sixteenbits == 1);
    int bitshift_gain = 1;
    if (!do_sixteen) {
      bitshift_gain = 256;
      bitshift_gain /= pow(2,g_bitdepth - 8);
    }

    // Crop the region we want out of the image and then write it
    // to a file.  Set the offset and scale (gain again) based on the global
    // clipping values.
    vrpn_uint16 clamp = (1 << ((unsigned)(g_bitdepth))) - 1;
    double  intensity_gain;
    double  intensity_offset;

    // Compute gain and offset so that pixels at or below the low-clip value are
    // set to zero and pixels at or above the high-clip value are set to the
    // maximum value for the current global pixel-count setting.
    intensity_gain = 1.0/(g_clip_high - g_clip_low);
    intensity_offset = - g_clip_low * clamp;

    cropped_image crop(*shifted, (int)(*g_minX), (int)(*g_minY), (int)(*g_maxX), (int)(*g_maxY));
    if (g_monochrome) {
      if (!crop.write_to_grayscale_tiff_file(filename, g_colorIndex, bitshift_gain*intensity_gain, intensity_offset, do_sixteen)) {
        delete [] filename;
        delete shifted;
        return false;
      }
    } else {
      if (!crop.write_to_tiff_file(filename, bitshift_gain*intensity_gain, intensity_offset, do_sixteen)) {
        delete [] filename;
        delete shifted;
        return false;
      }
    }

    (*g_log_last_image) = *g_image_to_display;

    delete [] filename;
    delete shifted;
    return true;
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
// other means we don't have control over.
static void  dirtyexit(void)
{
  static bool did_already = false;

  if (did_already) {
    return;
  } else {
    did_already = true;
  }

  // Done with the camera and other objects.
  printf("Exiting\n");

  if (g_psf_file) {
    delete g_psf_file;
    g_psf_file = NULL;
  }

  glutDestroyWindow(g_tracking_window);

  list<Spot_Information *>::iterator  loop;
  for (loop = g_trackers.begin(); loop != g_trackers.end(); loop++) {
    delete *loop;
  }
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

  dirtyexit();
}

static bool should_draw_second_tracker(void)
{
  return g_reorient || g_rescale;
}

// Apply gain and offset based on the settings for the
// two global intensity clipping parameters.  Then clamp
// the value to the maximum representable value for the
// specified image bit depth.
inline	vrpn_uint16 offset_scale_clamp(double value)
{
  double clamp = (1 << ((unsigned)(g_bitdepth))) - 1;

  // First, apply scale and offset to the value unless the
  // clipping variables are 0 and 1.
  if ( (g_clip_low != 0) || (g_clip_high != 1) ) {
    double  intensity_gain;
    double  intensity_offset;

    // Compute gain and offset so that pixels at or below the low-clip value are
    // set to zero and pixels at or above the high-clip value are set to the
    // maximum value for the current global pixel-count setting.
    // First, make sure we've got legal settings.
    if (g_clip_high <= g_clip_low) {
      g_clip_high = g_clip_low + (1.0/clamp);
    }
    intensity_gain = 1.0/(g_clip_high - g_clip_low);
    intensity_offset = g_clip_low * clamp;

    double dval = (value - intensity_offset) * intensity_gain;
    if (dval < 0) {
      value = 0;
    } else {
      value = dval;
    }
  }

  // Clamp the value based on the number of pixels that are supposed
  // to be in the input file.
  if (value <= clamp) {
      return value;
  } else {
    return clamp;
  }
}

void myDisplayFunc(void)
{
  unsigned  r,c;
  double  pixel_val;

  if (!g_ready_to_display) { return; }

  // Clear the window and prepare to draw in the back buffer
  glDrawBuffer(GL_BACK);
  glClearColor(0.0, 0.0, 0.0, 0.0);
  glClear(GL_COLOR_BUFFER_BIT);

  // Copy pixels into the image buffer.  Flip the image over in
  // Y so that the image coordinates display correctly in OpenGL.
#ifdef DEBUG
  printf("Filling pixels %d,%d through %d,%d\n", (int)(*g_minX),(int)(*g_minY), (int)(*g_maxX), (int)(*g_maxY));
#endif
  int shift = g_bitdepth - 8;
  for (r = *g_minY; r <= *g_maxY; r++) {
    unsigned lowc = *g_minX, hic = *g_maxX; //< Speeds things up.
    unsigned char *pixel_base = &g_glut_image[ 4*(lowc + g_image_to_display->get_num_columns() * r) ]; //< Speeds things up
    for (c = lowc; c <= hic; c++) {
      if (!g_image_to_display->read_pixel(c, r, pixel_val, g_colorIndex)) {
	fprintf(stderr, "Cannot read pixel from region\n");
	cleanup();
      	exit(-1);
      }
      vrpn_uint16 val = offset_scale_clamp(pixel_val) >> shift;

      // This assumes that the pixels are actually 8-bit values
      // and will clip if they go above this.  It also writes pixels
      // from the first channel into all colors of the image.  It uses
      // RGBA so that we don't have to worry about byte-alignment problems
      // that plagued us when using RGB pixels.
      *(pixel_base++) = val;     // Stored in red
      *(pixel_base++) = val;     // Stored in green
      *(pixel_base++) = val;     // Stored in blue
      pixel_base++;   // Skip alpha, it doesn't matter. //*(pixel_base++) = 255;                  // Stored in alpha

#ifdef DEBUG
      // If we're debugging, fill the border pixels with green
      if ( (r == *g_minY) || (r == *g_maxY) || (c == *g_minX) || (c == *g_maxX) ) {
        g_glut_image[0 + 4 * (c + g_image_to_display->get_num_columns() * r)] = 0;
        g_glut_image[1 + 4 * (c + g_image_to_display->get_num_columns() * r)] = 255;
        g_glut_image[2 + 4 * (c + g_image_to_display->get_num_columns() * r)] = 0;
        g_glut_image[3 + 4 * (c + g_image_to_display->get_num_columns() * r)] = 255;
      }
#endif

    }
  }

  // Store the pixels from the image into the frame buffer
  // so that they cover the entire image (starting from lower-left
  // corner, which is at (-1,-1)).
  glRasterPos2f(-1, -1);
#ifdef DEBUG
  printf("Drawing %dx%d pixels\n", g_image_to_display->get_num_columns(), g_image_to_display->get_num_rows());
#endif
  glDrawPixels(g_image_to_display->get_num_columns(),g_image_to_display->get_num_rows(),
    GL_RGBA, GL_UNSIGNED_BYTE, g_glut_image);

  // If we have been asked to show the tracking markers, draw them.
  // The active one is drawn in red and the others are drawn in blue.
  // The markers may be either cross-hairs with a radius matching the
  // bead radius or a circle with a radius twice that of the bead; the
  // marker type depends on the g_round_cursor variable.
  if (g_mark) {
    list <Spot_Information *>::iterator loop;
    glEnable(GL_LINE_SMOOTH); //< Use smooth lines here to avoid aliasing showing spot in wrong place
    for (loop = g_trackers.begin(); loop != g_trackers.end(); loop++) {
      // Normalize center and radius so that they match the coordinates
      // (-1..1) in X and Y.
      double  x = -1.0 + (*loop)->xytracker()->get_x() * (2.0/g_image_to_display->get_num_columns());
      double  y = -1.0 + ((*loop)->xytracker()->get_y()) * (2.0/g_image_to_display->get_num_rows());
      double  dx = (*loop)->xytracker()->get_radius() * (2.0/g_image_to_display->get_num_columns());
      double  dy = (*loop)->xytracker()->get_radius() * (2.0/g_image_to_display->get_num_rows());

      if ((*loop)->xytracker() == g_active_tracker) {
	glColor3f(1,0,0);
      } else {
	glColor3f(0,0,1);
      }
      if (g_round_cursor) {
	double stepsize = M_PI / (*loop)->xytracker()->get_radius();
	double runaround;
	glBegin(GL_LINE_STRIP);
	  for (runaround = 0; runaround <= 2*M_PI; runaround += stepsize) {
	    glVertex2f(x + 2*dx*cos(runaround),y + 2*dy*sin(runaround));
	  }
	  glVertex2f(x + 2*dx, y);  // Close the circle
	glEnd();
      } else {
	glBegin(GL_LINES);
	  glVertex2f(x-dx,y);
	  glVertex2f(x+dx,y);
	  glVertex2f(x,y-dy);
	  glVertex2f(x,y+dy);
	glEnd();
      }
      // Label the marker with its index
      char numString[10];
      sprintf(numString,"%d", (*loop)->index());
      drawStringAtXY(x+dx,y, numString);

      // If we're only supposed to draw the first tracker, break out of the
      // loop here.
      if (!should_draw_second_tracker()) { break; }
    }
  }

  // Draw a green border around the selected area.  It will be beyond the
  // window border if it is set to the edges; it will just surround the
  // region being selected if it is inside the window.
  glDisable(GL_LINE_SMOOTH);
  glColor3f(0,1,0);
  glBegin(GL_LINE_STRIP);
  glVertex3f( -1 + 2*((*g_minX-1) / (g_image_to_display->get_num_columns()-1)),
	      -1 + 2*((*g_minY-1) / (g_image_to_display->get_num_rows()-1)) , 0.0 );
  glVertex3f( -1 + 2*((*g_maxX+1) / (g_image_to_display->get_num_columns()-1)),
	      -1 + 2*((*g_minY-1) / (g_image_to_display->get_num_rows()-1)) , 0.0 );
  glVertex3f( -1 + 2*((*g_maxX+1) / (g_image_to_display->get_num_columns()-1)),
	      -1 + 2*((*g_maxY+1) / (g_image_to_display->get_num_rows()-1)) , 0.0 );
  glVertex3f( -1 + 2*((*g_minX-1) / (g_image_to_display->get_num_columns()-1)),
	      -1 + 2*((*g_maxY+1) / (g_image_to_display->get_num_rows()-1)) , 0.0 );
  glVertex3f( -1 + 2*((*g_minX-1) / (g_image_to_display->get_num_columns()-1)),
	      -1 + 2*((*g_minY-1) / (g_image_to_display->get_num_rows()-1)) , 0.0 );
  glEnd();

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
static	double	timediff(struct timeval t1, struct timeval t2)
{
	return (t1.tv_usec - t2.tv_usec) / 1e6 +
	       (t1.tv_sec - t2.tv_sec);
}

void myIdleFunc(void)
{
  list<Spot_Information *>::iterator loop;

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

  // If they just asked for the full area, reset to that and
  // then turn off the check-box.
  if (g_full_area) {
    *g_minX = 0;
    *g_minY = 0;
    *g_maxX = g_camera->get_num_columns() - 1;
    *g_maxY = g_camera->get_num_rows() - 1;
    g_full_area = 0;
  }

  // Read an image from the camera into memory, within a structure that
  // is wrapped by an image_wrapper object that the tracker can use.
  // Tell Glut that it can display an image.
  // We ignore the error return if we're doing a video file because
  // this can happen due to timeouts when we're paused or at the
  // end of a file.  We always read all of the image that we can:
  // cropping happens when the file is written to avoid missing pixels
  // that could have transformed to lie within the cropping region.
  if (!g_camera->read_image_to_memory(1,0, 1,0, g_exposure)) {
    if (!g_video) {
      fprintf(stderr, "Can't read image to memory!\n");
      cleanup();
      exit(-1);
    } else {
      // We timed out; either paused or at the end.  Don't log in this case.
      g_video_valid = false;
    }
  } else {
    // Got a valid video frame.

    // Make a copy of the image if we don't yet have a "this" image.  Otherwise,
    // it will be copied down below.
    if (g_this_image == NULL) {
      g_this_image = new copy_of_image(*g_camera);

      if (g_this_image == NULL) {
        fprintf(stderr,"Cannot create current-copy image\n");
        cleanup();
        exit(-1);
      }
    }
  
    // Make a copy of "this" frame into the last frame.
    if (g_last_image == NULL) {
      //printf("XXX Creating g_last_image\n");
      g_last_image = new copy_of_image(*g_this_image);
    } else {
      //printf("XXX Copying g_last_image\n");
      *g_last_image = *g_this_image;
    }

    // If we already have a valid "next" frame, store it into "this"
    // frame.  If not, then store the just-read frame as the "this"
    // frame.

    if (g_next_image) {
      *g_this_image = *g_next_image;
    } else {
      *g_this_image = *g_camera;
    }

    // If we already have a valid "next" buffer, then store the just-
    // read image there.  If we do not, then try to read another new
    // frame to put into the next image, to prime the pump.
    if (g_next_image) {
      *g_next_image = *g_camera;
    } else {
      if (g_camera->read_image_to_memory(1,0, 1,0, g_exposure)) {
	g_next_image = new copy_of_image(*g_camera);
      }
    }

    // Say that we have a valid frame (triggers some calculations)
    // and bump the current frame number.
    g_video_valid = true;
    g_frame_number++;
  }

  // If we didn't get a valid video frame, but we have a valid next frame,
  // then we shift from the "next" image to this one and delete the next
  // image.
  // XXX Bug: When we pause the video, this will result in the next image
  // being played out, and then re-priming when we start up again, which will
  // cause a skip in the logged output for any mode that uses the next image
  // in its calculation.
  if (!g_video_valid && g_next_image) {
    *g_this_image = *g_next_image;
    delete g_next_image;
    g_next_image = NULL;
    g_video_valid = true;
    g_frame_number++;
  }

  g_ready_to_display = true;

  // If we have a valid video frame, do any required accumulation into
  // the image statistic buffers.
  if (g_video_valid) {
    if (g_min_image) {
      (*g_min_image) += *g_this_image;
    }
    if (g_max_image) {
      (*g_max_image) += *g_this_image;
    }
    if (g_mean_image) {
      (*g_mean_image) += *g_this_image;
    }
  }

  // Compute the calculated image if we're using one.  If not, then point the
  // image to display at the camera image.  If so, point the image to display
  // at the calculated image.
  if (g_subtract == SUBTRACT_NONE) {
    g_image_to_display = g_this_image;
  } else {
    image_wrapper *img = NULL;
    switch (g_subtract) {
    case SUBTRACT_SINGLE:
      img = g_subtract_image;
      break;
    case SUBTRACT_MIN:
      img = g_min_image;
      break;
    case SUBTRACT_MAX:
      img = g_max_image;
      break;
    case SUBTRACT_MEAN:
      img = g_mean_image;
      break;
    case SUBTRACT_NEIGHBORS:
      // Replace the subtraction image with the average of the next and
      // last images, if they both exist -- then point at the subtracted
      // image as the one to display.
      if (g_averaged_image) {
	delete g_averaged_image;
	g_averaged_image = NULL;
      }
      if (g_last_image && g_next_image) {
	g_averaged_image = new averaged_image(*g_last_image, *g_next_image);
	img = g_averaged_image;
      } else {
	// If we don't have the images needed to compute the average, then
	// subtract the current image (making it black).
	img = g_this_image;
      }
      break;
    };
    if (img) {
      // Get rid of the old calculated image, if we have one
      if (g_calculated_image) { delete g_calculated_image; g_calculated_image = NULL; }
      // Calculate the new image and then point the image to display at it.
      // The offset is determined by the maximum value that can be displayed in an
      // image with half as many values as the present image.
      double offset = (1 << (((unsigned)(g_bitdepth))-1) ) - 1;
      g_calculated_image = new subtracted_image(*g_this_image, *img, offset);
      if (g_calculated_image == NULL) {
	fprintf(stderr, "Out of memory when calculating image\n");
	cleanup();
	exit(-1);
      }
      g_image_to_display = g_calculated_image;
    } else {
      fprintf(stderr,"Internal error: no subtraction image found, mode %d\n", (int)g_subtract);
      cleanup();
      exit(-1);
    }
  };

  // Decide which image to display.  For calculated images, leave things
  // alone.  For the others, set to the appropriate image.
  switch (g_display_which_image) {
  case DISPLAY_COMPUTED:
    break;
  case DISPLAY_MIN:
    g_image_to_display = g_min_image;
    break;
  case DISPLAY_MAX:
    g_image_to_display = g_max_image;
    break;
  case DISPLAY_MEAN:
    g_image_to_display = g_mean_image;
    break;
  default:
    fprintf(stderr, "myIdleFunc(): Internal error: unknown mode (%d)\n", (int)g_display_which_image);
    cleanup();
    exit(-1);
  }

  // If we've gotten a new valid frame, then it is time to store the image
  // for the previous frame and get a copy of the current frame so that we
  // can store it next time around.  We do this after saving the previous
  // frame (named based on the base log file name and the past frame number).
  if (g_log_last_image && g_video_valid) {
    if (!save_log_frame(g_frame_number - 1)) {
      fprintf(stderr,"Couldn't save log file\n");
      cleanup();
      exit(-1);
    }
  }

  // If we've been asked to do auto-gain, set the
  // clip values to the minimum and maximum pixel intensities present.
  // This must be done after the saving of the image, because otherwise
  // we'll save it with the wrong gain and offset.
  if (g_auto_gain) {
    vrpn_uint16 clamp = (1 << ((unsigned)(g_bitdepth))) - 1;
    int x,y;
    double  min, max;
    max = min = g_image_to_display->read_pixel_nocheck(0, 0, g_colorIndex);
    for (x = *g_minX; x <= *g_maxX; x++) {
      for (y = *g_minY; y <= *g_maxY; y++) {
	double val = g_image_to_display->read_pixel_nocheck(x, y, g_colorIndex);
	if (val < min) { min = val; }
	if (val > max) { max = val; }
      }
    }
    g_clip_low = min / clamp;
    g_clip_high = max / clamp;
  }

  if (g_active_tracker) { 
    g_active_tracker->set_radius(g_Radius);
  }
  if (g_opt && g_active_tracker) {
    double  x, y;

    // This variable is used to determine if we should consider doing maximum fits,
    // by determining if the frame number has changed since last time.
    static int last_optimized_frame_number = -1000;

    // Optimize to find the best fit starting from last position for each tracker.
    // Invert the Y values on the way in and out.
    // Don't let it adjust the radius here (otherwise it gets too
    // jumpy).
    for (loop = g_trackers.begin(); loop != g_trackers.end(); loop++) {

      // If the frame-number has changed, and we are doing global searches
      // within a radius, then create an image-based tracker at the last
      // location for the current tracker on the last frame; scan all locations
      // within radius and find the maximum fit on this frame; move the tracker
      // location to that maximum before doing the optimization.  We use the
      // image-based tracker for this because other trackers may have maximum
      // fits outside the region where the bead is -- the symmetric tracker often
      // has a local maximum at best fit and global maxima elsewhere.
      if ( g_last_image && (g_search_radius > 0) && (last_optimized_frame_number != g_frame_number) ) {

	double x_base = (*loop)->xytracker()->get_x();
	double y_base = (*loop)->xytracker()->get_y();

	// Create an image spot tracker and initize it at the location where the current
	// tracker is, but in the last image.  Grab enough of the image that we will be able
	// to check over the g_search_radius for a match.
	image_spot_tracker_interp max_find((*loop)->xytracker()->get_radius(), (g_invert != 0), g_precision,
	  0.1, g_sampleSpacing);
	max_find.set_location(x_base, y_base);
	max_find.set_image(*g_last_image, g_colorIndex, x_base, y_base, (*loop)->xytracker()->get_radius() + g_search_radius);

	// Loop over the pixels within g_search_radius of the initial location and find the
	// location with the best match over all of these points.  Do this in the current image.
	double radsq = g_search_radius * g_search_radius;
	int x_offset, y_offset;
	int best_x_offset = 0;
	int best_y_offset = 0;
	double best_value = max_find.check_fitness(*g_this_image, g_colorIndex);
	for (x_offset = -floor(g_search_radius); x_offset <= floor(g_search_radius); x_offset++) {
	  for (y_offset = -floor(g_search_radius); y_offset <= floor(g_search_radius); y_offset++) {
	    if ( (x_offset * x_offset) + (y_offset * y_offset) <= radsq) {
	      max_find.set_location(x_base + x_offset, y_base + y_offset);
	      double val = max_find.check_fitness(*g_this_image, g_colorIndex);
	      if (val > best_value) {
		best_x_offset = x_offset;
		best_y_offset = y_offset;
		best_value = val;
	      }
	    }
	  }
	}

	// Put the tracker at the location of the maximum, so that it will find the
	// total maximum when it finds the local maximum.
	(*loop)->xytracker()->set_location(x_base + best_x_offset, y_base + best_y_offset);
      }

      // Here's where the tracker is optimized to its new location
      (*loop)->xytracker()->optimize_xy(*g_this_image, g_colorIndex, x, y, (*loop)->xytracker()->get_x(), (*loop)->xytracker()->get_y() );
    }

    last_optimized_frame_number = g_frame_number;
  }

  //------------------------------------------------------------
  // Make the GUI track the result for the active tracker
  if (g_active_tracker) {
    g_X = (float)g_active_tracker->get_x();
    g_Y = (float)flip_y(g_active_tracker->get_y());
    g_Radius = (float)g_active_tracker->get_radius();
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
    // when we rewind and delete the "next" image, which does
    // not exist anymore.
    if (*g_rewind) {
      *g_play = 0;
      *g_rewind = 0;
      g_video->rewind();
      g_frame_number = -1;
      if (g_next_image) {
	delete g_next_image;
	g_next_image = NULL;
      }
    }
  }

  //------------------------------------------------------------
  // Post a redisplay so that Glut will draw the new image
  if (!g_already_posted) {
    glutSetWindow(g_tracking_window);
    glutPostRedisplay();
    g_already_posted = true;
  }

  //------------------------------------------------------------
  // Time to quit?
  if (g_quit) {
    // If we have been logging, then see if we have saved the
    // current frame's image.  If not, go ahead and do it now.
    if (g_log_last_image && (g_log_frame_number_last_logged != g_frame_number)) {
      if (!save_log_frame(g_frame_number)) {
	fprintf(stderr, "logfile_changed: Could not save log frame\n");
	cleanup();
	exit(-1);
      }
    }

    cleanup();
    exit(0);
  }
  
  // Sleep a little while so that we don't eat the whole CPU.
  vrpn_SleepMsecs(1);
}

// This routine finds the tracker whose coordinates are
// the nearest to those specified, makes it the active
// tracker, and moved it to the specified location.
void  activate_and_drag_nearest_tracker_to(double x, double y)
{
  // Looks for the minimum squared distance and the tracker that is
  // there.
  double  minDist2 = 1e100;
  Spot_Information *minTracker = NULL;
  double  dist2;
  list<Spot_Information *>::iterator loop;

  for (loop = g_trackers.begin(); loop != g_trackers.end(); loop++) {
    dist2 = (x - (*loop)->xytracker()->get_x())*(x - (*loop)->xytracker()->get_x()) +
      (y - (*loop)->xytracker()->get_y())*(y - (*loop)->xytracker()->get_y());
    if (dist2 < minDist2) {
      minDist2 = dist2;
      minTracker = *loop;
    }
  }

  // If we aren't showing the second tracker, then we always activate
  // the first one.
  if (!should_draw_second_tracker()) {
    minTracker = g_trackers.front();
  }

  if (minTracker == NULL) {
    fprintf(stderr, "No tracker to pick out of %d\n", g_trackers.size());
  } else {
    g_active_tracker = minTracker->xytracker();
    g_active_tracker->set_location(x, y);
    g_X = g_active_tracker->get_x();
    g_Y = flip_y(g_active_tracker->get_y());
    g_Radius = g_active_tracker->get_radius();
  }
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
    // Nothing yet...
    break;
  }
}

void mouseCallbackForGLUT(int button, int state, int x, int y)
{
    // Record where the button was pressed for use in the motion
    // callback, flipping the Y axis to make the coordinates match
    // image coordinates.
    g_mousePressX = x;
    g_mousePressY = y = flip_y(y);

    switch(button) {
      // The right button will set the clipping window to a single-
      // pixel-wide spot surrounding the click location and it will
      // set the motion callback mode to expand the area as the user
      // moves.
      case GLUT_RIGHT_BUTTON:
	if (state == GLUT_DOWN) {
	  g_whichDragAction = 1;
	  *g_minX = x;
	  *g_maxX = x;
	  *g_minY = y;
	  *g_maxY = y;
	} else {
	  // Nothing to do at release.
	}
	break;

      case GLUT_MIDDLE_BUTTON:
	if (state == GLUT_DOWN) {
	  g_whichDragAction = 0;
	}
	break;

      // The left button will pull the closest existing tracker
      // to the location where the mouse button was pressed, and
      // then let the user pull it around the screen
      case GLUT_LEFT_BUTTON:
	if (state == GLUT_DOWN) {
	  g_whichDragAction = 2;
	  activate_and_drag_nearest_tracker_to(x,y);
	}
	break;
    }
}

void motionCallbackForGLUT(int x, int y) {

  // Make mouse coordinates match image coordinates.
  y = flip_y(y);

  switch (g_whichDragAction) {

  case 0: //< Do nothing on drag.
    break;

  // Expand the selection area so that it is a rectangle centered
  // on the press location, clipped to the boundaries of the video.
  case 1:
    {
      int x_dist = (int)(fabs(static_cast<double>(x-g_mousePressX)));
      int y_dist = (int)(fabs(static_cast<double>(y-g_mousePressY)));

      *g_minX = g_mousePressX - x_dist;
      *g_maxX = g_mousePressX + x_dist;
      *g_minY = g_mousePressY - y_dist;
      *g_maxY = g_mousePressY + y_dist;

      // Clip the size to stay within the window boundaries.
      if (*g_minX < 0) { *g_minX = 0; };
      if (*g_minY < 0) { *g_minY = 0; };
      if (*g_maxX >= (int)g_camera->get_num_columns()) { *g_maxX = g_camera->get_num_columns() - 1; }
      if (*g_maxY >= (int)g_camera->get_num_rows()) { *g_maxY = g_camera->get_num_rows() - 1; };
    }
    break;

  // Pull the closest existing tracker
  // to the location where the mouse button was pressed, and
  // keep pulling it around if the mouse is moved while this
  // button is held down.
  case 2:
    activate_and_drag_nearest_tracker_to(x,y);
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

void  device_filename_changed(char *newvalue, void *)
{
  if (strlen(newvalue) > 0) {
    g_device_name = new char[strlen(newvalue)+1];
    strcpy(g_device_name, newvalue);
  }
}

// If the logfilename becomes non-empty, then start a new sequence
// of images to be stored.  This is done by setting the global log
// file base name to the value of the file name.
// We use the "newvalue" here rather than the file name because the
// file name gets truncated to the maximum TCLVAR string length.
// At the same time, check to see if we are saving a point-spread
// function file.  If so, create a file saver for it.

void  logfilename_changed(char *newvalue, void *)
{
  static  char	name_buffer[4096];

  // If we have been logging, then see if we have saved the
  // current frame's image.  If not, go ahead and do it now.
  if (g_log_last_image && (g_log_frame_number_last_logged != g_frame_number)) {
    if (!save_log_frame(g_frame_number)) {
      fprintf(stderr, "logfile_changed: Could not save log frame\n");
      cleanup();
      exit(-1);
    }
    if (g_psf_file) {
      g_psf_file->append_line(*g_log_last_image,
	g_trackers.front()->xytracker()->get_x(), g_trackers.front()->xytracker()->get_y());
    }
  }

  // Stop the old logging by getting rid of the last log image
  // if there is one and resetting the logging parameters.
  if (g_log_last_image) {
    delete g_log_last_image;
    g_log_last_image = NULL;
  }
  g_log_frame_number_last_logged = -1;
  if (g_psf_file) { delete g_psf_file; g_psf_file = NULL; }
  
  // If we have an empty name, then clear the global logging base
  // name so that no more files will be saved.
  strncpy(name_buffer, newvalue, sizeof(name_buffer)-1);
  name_buffer[sizeof(name_buffer)-1] = '\0';
  if (strlen(newvalue) == 0) {
    g_logfile_base_name = NULL;
    return;
  }

  // Store the name.
  g_logfile_base_name = name_buffer;

  // Make a copy of the current image so that we can save it when
  // it is appropriate to do so.
  g_log_last_image = new copy_of_image(*g_image_to_display);

  list<Spot_Information *>::iterator  loop;
  loop = g_trackers.begin();
  spot_tracker_XY *first_tracker = (*loop)->xytracker() ;
  loop++;
  spot_tracker_XY *second_tracker = (*loop)->xytracker();

  // Set the offsets to use when logging to the current position of
  // the zeroeth tracker.
  g_log_offset_x = first_tracker->get_x();
  g_log_offset_y = first_tracker->get_y();

  // Set the length and orientation of the original pair of trackers.
  double dx = second_tracker->get_x() - first_tracker->get_x();
  double dy = second_tracker->get_y() - first_tracker->get_y();
  g_orig_length = sqrt( dx*dx + dy*dy );
  g_orig_orient = atan2( dy, dx);

  // Create a PSF image file object if we're saving one.  Give it the same
  // name as the file passed in, but append ".psf.tif" to it.
  if (g_log_pointspread) {
    char	*psfname = new char[strlen(g_logfile_base_name) + 9];
    if (psfname == NULL) {
      fprintf(stderr,"logfilename_changed(): Out of memory\n");
      return;
    }
    sprintf(psfname, "%s.psf.tif", g_logfile_base_name);
    g_psf_file = new PSF_File(psfname, ceil(g_Radius), g_sixteenbits != 0);
    delete [] psfname;
  }
}

void  make_appropriate_tracker_active(int newvalue, void *)
{
  // If we have an active tracker, then set it to the
  // appropriate tracker.  The guard is to avoid seg fault at
  // startup.
  if (g_active_tracker) {
    if (should_draw_second_tracker()) {
      g_active_tracker = g_trackers.back()->xytracker();
    } else {
      g_active_tracker = g_trackers.front()->xytracker();
    }
  }
}

// Routine that rebuilds all trackers with the format that matches
// current settings.  This callback is called when one of the Tcl
// integer-with-buttons changes its value or when the option to
// allow re-orienting or re-scaling is changed (these options require
// two trackers vs. offset-only mode, which only needs one).

void  rebuild_trackers(int newvalue, void *)
{
  // Delete all trackers and replace with the correct types.
  // Make sure to put them back where they came from.
  // Re-point the active tracker at its corresponding new
  // tracker.
  list<Spot_Information *>::iterator  loop;

  for (loop = g_trackers.begin(); loop != g_trackers.end(); loop++) {
    double x = (*loop)->xytracker()->get_x();
    double y = (*loop)->xytracker()->get_y();
    double r = (*loop)->xytracker()->get_radius();

    if (g_active_tracker == (*loop)->xytracker()) {
      delete (*loop)->xytracker();
      (*loop)->set_xytracker(create_appropriate_tracker());
      g_active_tracker = (*loop)->xytracker();
    } else {
      delete (*loop)->xytracker();
      (*loop)->set_xytracker(create_appropriate_tracker());
    }
    (*loop)->xytracker()->set_location(x,y);
    (*loop)->xytracker()->set_radius(r);
  }
}

// This version is for float sliders
void  rebuild_trackers(float newvalue, void *) {
  rebuild_trackers((int)newvalue, NULL);
}

// Sets the radius as the check-box is turned on (1) and off (0);
// it will be set to the current bead radius
void  set_maximum_search_radius(int newvalue, void *)
{
  g_search_radius = g_Radius * newvalue;
}

// Routine that stores the current image when subtract mode is set to SUBTRACT_SINGLE.
// Deletes the subtraction image and sets it to null when the button is turned off.
// Also sets the image bit depth up one level when subtraction is turned on and
// down one level when it is turned off (because there can be twice as many values
// when subtracting).  Also make sure that we are accumulating the type of image
// we are trying to subtract.

void  subtract_changed(int newvalue, void *)
{
  static  int bit_depth_bump = 0;

  if (newvalue == SUBTRACT_NONE) {
    if (bit_depth_bump == 1) {
      g_bitdepth = (float)g_bitdepth - 1;
      bit_depth_bump = 0;
    }
  } else {
    if (bit_depth_bump == 0) {
      g_bitdepth = (float)g_bitdepth + 1;
      bit_depth_bump = 1;
    }
  }

  if (g_subtract_image) {
    delete g_subtract_image;
    g_subtract_image = NULL;
  }
  if (newvalue == SUBTRACT_SINGLE) {
    g_subtract_image = new copy_of_image(*g_this_image);
  }

  switch (newvalue) {
    case SUBTRACT_MIN : if (!g_min_image) {g_accumulate_min = 1;}; break;
    case SUBTRACT_MAX : if (!g_max_image) {g_accumulate_max = 1;}; break;
    case SUBTRACT_MEAN : if (!g_mean_image) {g_accumulate_mean = 1;}; break;
  }
}

// Routine that starts accumulating min image.
// Never delete the min image, so that it can be used
// after accumulation has stopped.

void  accumulate_min_changed(int newvalue, void *)
{
  if (newvalue == 1) {
    if (!g_min_image) {
      g_min_image = new minimum_image(*g_this_image);
    }
  }
}

// Routine that start accumulating min image.
// Never delete the max image, so that it can be used
// after accumulation has stopped.

void  accumulate_max_changed(int newvalue, void *)
{
  if (newvalue == 1) {
    if (!g_max_image) {
      g_max_image = new maximum_image(*g_this_image);
    }
  }
}

// Routine that start accumulating min image.
// Never delete the mean image, so that it can be used
// after accumulation has stopped.

void  accumulate_mean_changed(int newvalue, void *)
{
  if (newvalue == 1) {
    if (!g_mean_image) {
      g_mean_image = new mean_image(*g_this_image);
    }
  }
}

// Ensure that we either already have or are accumulating the image that we want to show.
// The idle function contains code to do the actual display switching
// for the correct one.
void  display_which_image_changed(int newvalue, void *)
{
  switch (newvalue) {
  case DISPLAY_COMPUTED:
    break;
  case DISPLAY_MIN:
    if (!g_min_image) {g_accumulate_min = 1;}
    break;
  case DISPLAY_MAX:
    if (!g_max_image) {g_accumulate_max = 1;}
    break;
  case DISPLAY_MEAN:
    if (!g_mean_image) {g_accumulate_mean = 1;}
    break;
  default:
    fprintf(stderr, "display_which_image_changed(): Internal error: unknown mode (%d)\n", newvalue);
    cleanup();
    exit(-1);
  }
}


//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
  //------------------------------------------------------------------
  // If there is a command-line argument, treat it as the name of a
  // file that is to be loaded.
  switch (argc) {
  case 1:
    // No arguments, so ask the user for a file name
    g_device_name = NULL;
    break;
  case 2:
    // Filename argument: open the file specified.
    g_device_name = argv[1];
#ifdef __APPLE__
    if (argv[1][0] == '-' && argv[1][1]=='p' && argv[1][2]=='s' && argv[1][3]=='n')
	g_device_name = NULL;
#endif
    break;

  default:
    fprintf(stderr, "Usage: %s [roper|cooke|diaginc|directx|directx640x480|filename]\n", argv[0]);
    exit(-1);
  };
  
  // Set up exit handler to make sure we clean things up no matter
  // how we are quit.  We hope that we exit in a good way and so
  // cleanup() gets called, but if not then we do a dirty exit.
  atexit(dirtyexit);

  //------------------------------------------------------------------
  // VRPN state setting so that we don't try to preload a video file
  // when it is opened, which wastes time.  Also tell it not to
  // accumulate messages, which can cause us to run out of memory.
  vrpn_FILE_CONNECTIONS_SHOULD_PRELOAD = false;
  vrpn_FILE_CONNECTIONS_SHOULD_ACCUMULATE = false;

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

  // Figure out where the executable is; look for the .tcl files there.
  char  executable_directory[256];
  const char *last_slash = strrchr(argv[0], '/');
  const char *last_backslash = strrchr(argv[0], '\\');
  const char *last_mark = max(last_slash, last_backslash);
  int root_length = 0;
  if (last_mark != NULL) {
        root_length = last_mark - argv[0] + 1;
  }
  if (root_length >= sizeof(executable_directory)) {
        fprintf(stderr,"Too long filename for executable directory\n");
        return(-1);
  }
  strncpy(executable_directory, argv[0], root_length);
  executable_directory[root_length] = '\0';
  if (root_length == 0) {
        strncpy(executable_directory, ".", sizeof(executable_directory));
  }

  /* Load the Tcl scripts that handle widget definition and
   * variable controls */
  sprintf(command, "source %s/russ_widgets.tcl", executable_directory);
  if (Tcl_Eval(tk_control_interp, command) != TCL_OK) {
          fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
                  tk_control_interp->result);
          return(-1);
  }

  //------------------------------------------------------------------
  // Put the version number into the main window.
  sprintf(command, "label .versionlabel -text CISMM_Video_Optimizer_v:%s", Version_string);
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
  // Put the "Thank-you Ware" button into the main window.
  sprintf(command, "package require http");
  if (Tcl_Eval(tk_control_interp, command) != TCL_OK) {
          fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
                  tk_control_interp->result);
          return(-1);
  }
  sprintf(command, "set thanks_text \"Say Thank You!\"");
  if (Tcl_Eval(tk_control_interp, command) != TCL_OK) {
          fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
                  tk_control_interp->result);
          return(-1);
  }
  sprintf(command, "button .thankyouware -textvariable thanks_text -command { ::http::geturl \"http://www.cs.unc.edu/Research/nano/cismm/thankyou/yourewelcome.htm?program=video_spot_tracker&Version=%s\" ; set thanks_text \"Paid for by NIH/NIBIB\" }", Version_string);
  if (Tcl_Eval(tk_control_interp, command) != TCL_OK) {
          fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
                  tk_control_interp->result);
          return(-1);
  }
  sprintf(command, "pack .thankyouware");
  if (Tcl_Eval(tk_control_interp, command) != TCL_OK) {
          fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
                  tk_control_interp->result);
          return(-1);
  }

  //------------------------------------------------------------------
  // Load the specialized Tcl code needed by this program.  This must
  // be loaded before the Tclvar_init() routine is called because it
  // puts together some of the windows needed by the variables.
  sprintf(command, "source %s/cismm_video_optimizer.tcl", executable_directory);
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
  sprintf(command, "wm geometry . +10+10");
  if (Tcl_Eval(tk_control_interp, command) != TCL_OK) {
          fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
                  tk_control_interp->result);
          return(-1);
  }

  //------------------------------------------------------------------
  // If we don't have a device name, then throw a Tcl dialog asking
  // the user for the name of a file to use and wait until they respond.
  if (g_device_name == NULL) {

    //------------------------------------------------------------
    // Create a callback for a variable that will hold the device
    // name and then create a dialog box that will ask the user
    // to either fill it in or quit.
    Tclvar_selector filename("device_filename", NULL, NULL, "", device_filename_changed, NULL);
    if (Tcl_Eval(tk_control_interp, "ask_user_for_filename") != TCL_OK) {
      fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command, tk_control_interp->result);
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
  // Open the camera and image wrapper.  If we have a video file, then
  // set up the Tcl controls to run it.  Also, report the frame number.
  unsigned bitdepth = g_bitdepth;
  float exposure = g_exposure;
  if (!get_camera(g_device_name, &bitdepth, &exposure, &g_camera, &g_video,
	  648,484, 1, 0, 0)) {
    fprintf(stderr,"Cannot open camera/imager\n");
    if (g_camera) { delete g_camera; g_camera = NULL; }
    cleanup();
    exit(-1);
  }
  g_bitdepth = bitdepth;
  g_exposure = exposure;

  // Verify that the camera is working.
  if (!g_camera->working()) {
    fprintf(stderr,"Could not establish connection to camera\n");
    if (g_camera) { delete g_camera; g_camera = NULL; }
    cleanup();
    exit(-1);
  }

  if (g_video) {  // Put these in a separate control panel?
    // Start out paused at the beginning of the file.
    g_play = new Tclvar_int_with_button("play_video","",0);
    g_rewind = new Tclvar_int_with_button("rewind_video","",1);
    g_step = new Tclvar_int_with_button("single_step_video","");
    sprintf(command, "label .frametitle -text FrameNum");
    if (Tcl_Eval(tk_control_interp, command) != TCL_OK) {
	    fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
		    tk_control_interp->result);
	    return(-1);
    }
    sprintf(command, "pack .frametitle");
    if (Tcl_Eval(tk_control_interp, command) != TCL_OK) {
	    fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
		    tk_control_interp->result);
	    return(-1);
    }
    sprintf(command, "label .framevalue -textvariable frame_number");
    if (Tcl_Eval(tk_control_interp, command) != TCL_OK) {
	    fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
		    tk_control_interp->result);
	    return(-1);
    }
    sprintf(command, "pack .framevalue");
    if (Tcl_Eval(tk_control_interp, command) != TCL_OK) {
	    fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
		    tk_control_interp->result);
	    return(-1);
    }

    // Cause the video to rewind, so we get a valid frame.
    g_video->rewind();
  }

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
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
  glutInitWindowPosition(200, 230);
  glutInitWindowSize(g_camera->get_num_columns(), g_camera->get_num_rows());
#ifdef DEBUG
  printf("initializing window to %dx%d\n", g_camera->get_num_columns(), g_camera->get_num_rows());
#endif
  g_tracking_window = glutCreateWindow(g_device_name);
  glutMotionFunc(motionCallbackForGLUT);
  glutMouseFunc(mouseCallbackForGLUT);
  glutKeyboardFunc(keyboardCallbackForGLUT);

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

  // Create two trackers and place them near the center of the window.
  g_trackers.push_back(new Spot_Information(g_active_tracker = create_appropriate_tracker(), NULL));
  g_active_tracker->set_location(g_camera->get_num_columns()/2, g_camera->get_num_rows()/2);
  spot_tracker_XY *temp;
  g_trackers.push_back(new Spot_Information(temp = create_appropriate_tracker(), NULL));
  temp->set_location(g_camera->get_num_columns()*3/4, g_camera->get_num_rows()*3/4);

  // Set the display functions for each window and idle function for GLUT (they
  // will do all the work) and then give control over to GLUT.
  glutSetWindow(g_tracking_window);
  glutDisplayFunc(myDisplayFunc);
  glutIdleFunc(myIdleFunc);

  glutMainLoop();
  // glutMainLoop() NEVER returns.  Wouldn't it be nice if it did when Glut was done?
  // Nope: Glut calls exit(0);

  return 0;
}
