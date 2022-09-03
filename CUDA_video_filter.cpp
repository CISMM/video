// This pragma tells the compiler not to tell us about truncated debugging info
// due to name expansion within the string, list, and vector classes.
#pragma warning( disable : 4786 4995 4996 )

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef VST_NO_GUI
#include <tcl.h>
#include <tk.h>
#include "Tcl_Linkvar.h"
#endif
#include "spot_tracker.h"
#ifdef	_WIN32
#include <windows.h>
#endif
#ifdef __APPLE__
#include <OPENGL/gl.h>
#include <GLUT/glut.h>
#else
#include <GL/gl.h>
#include <GL/glut.h>
#endif
#include "controllable_video.h"
#include "vrpn_FileConnection.h"

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

#ifdef VST_NO_GUI
// Need to fake the behavior for all of the Tcl variables.  Try without
// the callback and see if this works.

// One thing we need to be able to do is parse a "set" command, so we can
// read in the state file and update the variables based on it.  This means
// that we need to keep a list of the variables and have a function that
// can look through them and set the right one.  These are kept in a vector
// (added to in each constructor) that can be searched by a function that
// can parse Tcl set commands and set the value for one of the existing variables.

class	Tclvar_int;
class	Tclvar_float;
class	Tclvar_selector;
std::vector<Tclvar_int *>	g_Tclvar_ints;
std::vector<Tclvar_float *>	g_Tclvar_floats;
std::vector<Tclvar_selector *>	g_Tclvar_selectors;
// Returns false if can't find a variable to set
static bool parse_tcl_set_command(const char *cmd);

// Callback types (not currently used, but need to be declared).
typedef void    (*Linkvar_Intcall)(int new_value, void *userdata);
typedef void    (*Linkvar_Floatcall)(float new_value, void *userdata);
typedef void    (*Linkvar_Selectcall)(char *new_value, void *userdata);
typedef void    (*Linkvar_Checkcall)(char *entry, int new_value,void *userdata);

class	Tclvar_int {
  public:
	Tclvar_int(const char *name, int default_value = 0, Linkvar_Intcall = NULL, void * = NULL)
		{ d_value = default_value; strcpy(d_name, name); g_Tclvar_ints.push_back(this); };
	inline operator int() const { return d_value; };
	inline int operator =(int v) { return d_value = v; };
	inline int operator ++() { return ++d_value; };
	inline int operator ++(int) { return d_value++; };
	const char *name(void) const { return d_name; };
  protected:
	int d_value;
	char d_name[512];
};

class	Tclvar_int_with_button : public Tclvar_int {
  public:
	Tclvar_int_with_button(const char *name, const char *, int default_value = 0, Linkvar_Intcall = NULL, void * = NULL)
		: Tclvar_int(name, default_value) {};
	inline operator int() const { return d_value; };
	inline int operator =(int v) { return d_value = v; };
	inline int operator ++() { return ++d_value; };
	inline int operator ++(int) { return d_value++; };
};

class	Tclvar_float {
  public:
	Tclvar_float(const char *name, float default_value = 0, Linkvar_Floatcall = NULL, void * = NULL)
		{ d_value = default_value; strcpy(d_name, name); g_Tclvar_floats.push_back(this); };
	inline operator float() const { return d_value; };
	inline float operator =(float v) { return d_value = v; };
	const char *name(void) const { return d_name; };
  protected:
	float d_value;
	char d_name[512];
};

class	Tclvar_float_with_scale : public Tclvar_float {
  public:
	Tclvar_float_with_scale(const char *name, const char *, double = 0, double = 0, double default_value = 0, Linkvar_Floatcall = NULL, void * = NULL)
		: Tclvar_float(name, default_value) {};
	inline operator float() const { return d_value; };
	inline float operator =(float v) { return d_value = v; };
};

class	Tclvar_selector {
  public:
	Tclvar_selector(const char *default_value = "") { strcpy(d_value, default_value); };
	Tclvar_selector(const char *name, const char *, const char * = NULL,
			const char *default_value = "", Linkvar_Selectcall = NULL, void * = NULL) { strcpy(d_value, default_value); strcpy(d_name, name); g_Tclvar_selectors.push_back(this); };
	inline operator const char*() const { return d_value; };
	const char * operator =(const char *v) { strcpy(d_value, v); return d_value; };
	void Set(const char *value) { strcpy(d_value, value); };
	const char *name(void) const { return d_name; };
  private:
	char d_value[4096];
	char d_name[512];
};

static bool parse_tcl_set_command(const char *cmd)
{
	// Pull out the command, looking for set and variable name and value
	char	set[1024], varname[1024], value[1024];
	if (sscanf(cmd, "%s%s%s", set, varname, value) != 3) {
		fprintf(stderr,"parse_tcl_set_command(): Cannot parse line: %s\n", cmd);
		return false;
	}

	// See if we find this in the integers.  If so, set it.
	size_t i;
	for (i = 0; i < g_Tclvar_ints.size(); i++) {
		if (!strcmp(g_Tclvar_ints[i]->name(), varname)) {
			*(g_Tclvar_ints[i]) = atoi(value);
			return true;
		}
	}

	// See if we find this in the floats.  If so, set it.
	for (i = 0; i < g_Tclvar_floats.size(); i++) {
		if (!strcmp(g_Tclvar_floats[i]->name(), varname)) {
			*(g_Tclvar_floats[i]) = atof(value);
			return true;
		}
	}

	// See if we find this in the floats.  If so, set it.
	for (i = 0; i < g_Tclvar_selectors.size(); i++) {
		if (!strcmp(g_Tclvar_selectors[i]->name(), varname)) {
			*(g_Tclvar_selectors[i]) = value;
			return true;
		}
	}

	// Not found.
	fprintf(stderr,"parse_tcl_set_command(): No such variable: %s\n", varname);
	return false;
}

#endif

//--------------------------------------------------------------------------
// Version string for this program
const char *Version_string = "00.01";

//--------------------------------------------------------------------------
// Glut wants to take over the world when it starts, so we need to make
// global access to the objects we will be using.

#ifndef VST_NO_GUI
Tcl_Interp	    *g_tk_control_interp;
#endif

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
image_wrapper       *g_blurred_image = NULL;      //< Blurred image used for autofind/delete, if any

Controllable_Video  *g_video = NULL;		  //< Video controls, if we have them
Tclvar_int_with_button	g_frame_number("frame_number",NULL,-1);  //< Keeps track of video frame number
#ifdef _WIN32
Tclvar_int_with_button	g_window_offset_x("window_offset_x",NULL,0);  //< Offset windows more in some arch
Tclvar_int_with_button	g_window_offset_y("window_offset_y",NULL,0);  //< Offset windows more in some arch
#else
Tclvar_int_with_button	g_window_offset_x("window_offset_x",NULL,25);  //< Offset windows more in some arch
Tclvar_int_with_button	g_window_offset_y("window_offset_y",NULL,-10);  //< Offset windows more in some arch
#endif

int		    g_tracking_window = -1;	  //< Glut window displaying tracking
unsigned char	    *g_glut_image = NULL;	  //< Pointer to the storage for the image

bool		    g_ready_to_display = false;	  //< Don't unless we get an image
bool		    g_already_posted = false;	  //< Posted redisplay since the last display?
int		    g_mousePressX, g_mousePressY; //< Where the mouse was when the button was pressed
int		    g_whichDragAction;		  //< What action to take for mouse drag

bool                g_quit_at_end_of_video = false; //< When we reach the end of the video, should we quit?

#ifdef VST_NO_GUI
bool                g_use_gui = false;             //< Use OpenGL video window?
#else
bool                g_use_gui = true;             //< Use OpenGL video window?
#endif

//--------------------------------------------------------------------------
// Tcl controls and displays
void  device_filename_changed(char *newvalue, void *);
void  reset_background_image(int newvalue, void *);
Tclvar_int_with_button	g_show_clipping("show_clipping","",0);
Tclvar_float_with_scale	*g_minX;
Tclvar_float_with_scale	*g_maxX;
Tclvar_float_with_scale	*g_minY;
Tclvar_float_with_scale	*g_maxY;
Tclvar_float_with_scale	g_colorIndex("red_green_blue", NULL, 0, 2, 0);
Tclvar_float_with_scale	g_brighten("brighten", "", 0, 8, 0);
Tclvar_float_with_scale g_blurLostAndFound("blur_lost_and_found", "", 0.0, 5.0, 0.0);
Tclvar_int_with_button	g_small_area("small_area",NULL);
Tclvar_int_with_button	g_full_area("full_area",NULL);
Tclvar_int_with_button	g_show_video("show_video","",1);
Tclvar_int_with_button	g_opengl_video("use_texture_video","",1);
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
#ifdef	__MINGW32__
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

// If the device filename becomes non-empty, then set the global
// device name to match what it is set to.

void  device_filename_changed(char *newvalue, void *)
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
  if (did_already) {
    return;
  } else {
    did_already = true;
  }

  g_quit = 1;  // Lets all the threads know that it is time to exit.

  // Done with the camera and other objects.
  printf("Exiting...");

  if (g_use_gui) {
    if (g_tracking_window != -1) { glutDestroyWindow(g_tracking_window); }
    printf("OpenGL window deleted...");
  }

  if (g_camera) { delete g_camera; g_camera = NULL; }
  if (g_glut_image) { delete [] g_glut_image; g_glut_image = NULL; };
  if (g_play) { delete g_play; g_play = NULL; };
  if (g_rewind) { delete g_rewind; g_rewind = NULL; };
  if (g_step) { delete g_step; g_step = NULL; };
  printf("objects deleted and files closed.\n");
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
  if (g_show_video) {
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
  }

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

    // Figure out which image to display.  If we have a
    // blurred image, then we draw it.  Otherwise, we draw the g_image.
    image_wrapper *drawn_image = g_image;
    int blurred_dim = 0;
    if (g_blurred_image) {
      drawn_image = g_blurred_image;
      blurred_dim = -1 * g_camera_bit_depth;  // Scale to range 0..1 for floating-point
    }

    if (g_opengl_video) {
      // If we can't write using OpenGL, turn off the feature for next frame.
      if (!drawn_image->write_to_opengl_quad(pow(2.0,static_cast<double>(g_brighten + blurred_dim)))) {
        g_opengl_video = false;
      }
    } else {
      int shift = total_shift;
      for (r = *g_minY; r <= *g_maxY; r++) {
        unsigned lowc = *g_minX, hic = *g_maxX; //< Speeds things up.
        unsigned char *pixel_base = &g_glut_image[ 4*(lowc + drawn_image->get_num_columns() * r) ]; //< Speeds things up
        for (c = lowc; c <= hic; c++) {
	  if (!drawn_image->read_pixel(c, r, uns_pix, g_colorIndex)) {
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
	    g_glut_image[0 + 4 * (c + drawn_image->get_num_columns() * r)] = 0;
	    g_glut_image[1 + 4 * (c + drawn_image->get_num_columns() * r)] = 255;
	    g_glut_image[2 + 4 * (c + drawn_image->get_num_columns() * r)] = 0;
	    g_glut_image[3 + 4 * (c + drawn_image->get_num_columns() * r)] = 255;
	  }
#endif
        }
      }

      // Store the pixels from the image into the frame buffer
      // so that they cover the entire image (starting from lower-left
      // corner, which is at (-1,-1)).
      glRasterPos2f(-1, -1);
#ifdef DEBUG
      printf("Drawing %dx%d pixels\n", drawn_image->get_num_columns(), drawn_image->get_num_rows());
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
      glDrawPixels(drawn_image->get_num_columns(),drawn_image->get_num_rows(),
        GL_RGBA, GL_UNSIGNED_BYTE, g_glut_image);
    } // End of "not g_opengl_video for texture"
  }

  // Draw a green border around the selected area.  It will be beyond the
  // window border if it is set to the edges; it will just surround the
  // region being selected if it is inside the window.
  glDisable(GL_LINE_SMOOTH);
  glColor3f(0,1,0);
  glBegin(GL_LINE_STRIP);
  glVertex3f( -1 + 2*((*g_minX-1) / (g_image->get_num_columns()-1)),
	      -1 + 2*((*g_minY-1) / (g_image->get_num_rows()-1)) , 0.0 );
  glVertex3f( -1 + 2*((*g_maxX+1) / (g_image->get_num_columns()-1)),
	      -1 + 2*((*g_minY-1) / (g_image->get_num_rows()-1)) , 0.0 );
  glVertex3f( -1 + 2*((*g_maxX+1) / (g_image->get_num_columns()-1)),
	      -1 + 2*((*g_maxY+1) / (g_image->get_num_rows()-1)) , 0.0 );
  glVertex3f( -1 + 2*((*g_minX-1) / (g_image->get_num_columns()-1)),
	      -1 + 2*((*g_maxY+1) / (g_image->get_num_rows()-1)) , 0.0 );
  glVertex3f( -1 + 2*((*g_minX-1) / (g_image->get_num_columns()-1)),
	      -1 + 2*((*g_minY-1) / (g_image->get_num_rows()-1)) , 0.0 );
  glEnd();

  // Swap buffers so we can see it.
  if (g_show_video) {
    glutSwapBuffers();
  }

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

#ifndef VST_NO_GUI
  while (Tk_DoOneEvent(TK_DONT_WAIT)) {};

  //------------------------------------------------------------
  // This is called once every time through the main loop.  It
  // pushes changes in the C variables over to Tcl.

  if (Tclvar_mainloop()) {
    fprintf(stderr,"Tclvar Mainloop failed\n");
  }
#endif

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

  // If they just asked for the full area, reset to that and
  // then turn off the check-box.  Also turn off small-area.
  if (g_full_area) {
    g_small_area = 0;
    *g_minX = 0;
    *g_minY = 0;
    *g_maxX = g_image->get_num_columns() - 1;
    *g_maxY = g_image->get_num_rows() - 1;
    g_full_area = 0;
  }

  // Read an image from the camera into memory, within a structure that
  // is wrapped by an image_wrapper object that the tracker can use.
  // Tell Glut that it can display an image.
  // We ignore the error return if we're doing a video file because
  // this can happen due to timeouts when we're paused or at the
  // end of a file.

  {
    if (!g_camera->read_image_to_memory((int)(*g_minX),(int)(*g_maxX), (int)(*g_minY),(int)(*g_maxY))) {
      if (!g_video) {
	fprintf(stderr, "Can't read image (%d,%d to %d,%d) to memory!\n", (int)(*g_minX),(int)(*g_minY), (int)(*g_maxX),(int)(*g_maxY));
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
#ifndef __MINGW32__
	    if (!PlaySound("end_of_video.wav", NULL, SND_FILENAME | SND_ASYNC)) {
	      fprintf(stderr,"Cannot play sound %s\n", "end_of_video.wav");
	    }
#endif
#endif
	  }
	  *g_play = 0;
	}
      }
    } else {
      // Got a valid video frame; can log it.  Add to the frame number.
      g_video_valid = true;
      g_frame_number++;
    }
  }
  // Point the image to use at the camera's image.
  g_image = g_camera;

  // We're ready to display an image.
  g_ready_to_display = true;

  // If we have gotten a new video frame and we're making a blurred image
  // for the lost-and-found images, then make a new one here.  Then set the
  // lost-and-found (LAF) image to point to it.  If we change the setting
  // for blurring, also redo blur.
  static double last_blur_setting = 0;
  bool time_to_blur = g_video_valid;
  if (last_blur_setting != g_blurLostAndFound) {
    time_to_blur = true;
    last_blur_setting = g_blurLostAndFound;
  }
  if (g_blurred_image && time_to_blur) {
    delete g_blurred_image;
    g_blurred_image = NULL;
  }
  if ( time_to_blur && (g_blurLostAndFound > 0) ) {
    unsigned aperture = 1 + static_cast<unsigned>(2*g_blurLostAndFound);
    g_blurred_image = new gaussian_blurred_image(*g_image, aperture, g_blurLostAndFound);
    if (g_blurred_image == NULL) {
      fprintf(stderr, "Could not create blurred image!\n");
    }
  }

  //------------------------------------------------------------
  // Post a redisplay so that Glut will draw the new image
  if (g_use_gui && !g_already_posted) {
    glutSetWindow(g_tracking_window);
    glutPostRedisplay();
    g_already_posted = true;
  }

  //------------------------------------------------------------
  // Capture timing information and print out how many frames per second
  // are being tracked.

  { static struct timeval last_print_time;
    struct timeval now;
    static bool first_time = true;
    static int last_frame_number = g_frame_number;

    if (first_time) {
      gettimeofday(&last_print_time, NULL);
      first_time = false;
    } else {
      gettimeofday(&now, NULL);
      double timesecs = 0.001 * vrpn_TimevalMsecs(vrpn_TimevalDiff(now, last_print_time));
      if (timesecs >= 5) {
	double frames_per_sec = (g_frame_number - last_frame_number) / timesecs;
	last_frame_number = g_frame_number;
	printf("Tracking %lg frames per second\n", frames_per_sec);
	last_print_time = now;
      }
    }
  }

//------------------------------------------------------------
  // Time to quit?
  if (g_quit) {

    cleanup();
    printf("Exiting with code 0 (good, clean exit)\n");

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
    case GLUT_LEFT_BUTTON:
      break;

    case GLUT_MIDDLE_BUTTON:
     break;

    case GLUT_RIGHT_BUTTON:
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

  return;
}

//--------------------------------------------------------------------------

void Usage(const char *progname)
{
    fprintf(stderr, "Usage: %s [-nogui] [-gui] [-kernel disc|cone|symmetric|FIONA]\n", progname);
    fprintf(stderr, "           [-dark_spot] [-follow_jumps] [-rod3 LENGTH ORIENT] [-outfile NAME]\n");
    fprintf(stderr, "           [-precision P] [-sample_spacing S] [-show_lost_and_found]\n");
    fprintf(stderr, "           [-lost_behavior B] [-lost_tracking_sensitivity L] [-blur_lost_and_found B]\n");
    fprintf(stderr, "           [-intensity_lost_sensitivity IL] [-dead_zone_around_border DB]\n");
    fprintf(stderr, "           [-maintain_fluorescent_beads M] [-fluorescent_spot_threshold FT]\n");
    fprintf(stderr, "           [-fluorescent_max_regions FR]\n");
    fprintf(stderr, "           [-maintain_this_many_beads M] [-dead_zone_around_trackers DT]\n");
    fprintf(stderr, "           [-candidate_spot_threshold T] [-sliding_window_radius SR]\n");
    fprintf(stderr, "           [-radius R] [-tracker X Y R] [-tracker X Y R] ...\n");
    fprintf(stderr, "           [-FIONA_background BG]\n");
    fprintf(stderr, "           [-raw_camera_params sizex sizey bitdepth channels headersize frameheadersize]\n");
    fprintf(stderr, "           [-load_state FILE] [-continue_from FILE]\n");
    fprintf(stderr, "           [roper|cooke|edt|diaginc|directx|directx640x480|filename]\n");
    fprintf(stderr, "       -nogui: Run without the video display window (no Glut/OpenGL)\n");
    fprintf(stderr, "       -gui: Run with the video display window (no Glut/OpenGL)\n");
    fprintf(stderr, "       -kernel: Use kernels of the specified type (default symmetric)\n");
    fprintf(stderr, "       -rod3: Make a rod3 kernel of specified LENGTH(pixels) & ORIENT(degrees)\n");
    fprintf(stderr, "       -dark_spot: Track a dark spot (default is bright spot)\n");
    fprintf(stderr, "       -follow_jumps: Set the follow_jumps flag\n");
    fprintf(stderr, "       -outfile: Save the track to the file 'name' (.vrpn will be appended)\n");
    fprintf(stderr, "       -precision: Set the precision for created trackers to P (default 0.05)\n");
    fprintf(stderr, "       -sample_spacing: Set the sample spacing for trackers to S (default 1)\n");
    fprintf(stderr, "       -show_lost_and_found: Show the lost_and_found window on startup\n");
    fprintf(stderr, "       -lost_behavior: Set lost tracker behavior: 0:stop; 1:delete; 2:hover\n");
    fprintf(stderr, "       -blur_lost_and_found:Set blur_lost_and_found to B\n");
    fprintf(stderr, "       -lost_tracking_sensitivity: Set lost_tracking_sensitivity to L\n");
    fprintf(stderr, "       -intensity_lost_sensitivity:Set intensity_lost_tracking_sensitivity to L\n");
    fprintf(stderr, "       -dead_zone_around_border: Set a dead zone around the region of interest\n");
    fprintf(stderr, "                 edge within which new trackers will not be found\n");
    fprintf(stderr, "       -dead_zone_around_trackers: Set a dead zone around all current trackers\n");
    fprintf(stderr, "                 within which new trackers will not be found\n");
    fprintf(stderr, "       -maintain_fluorescent_beads: Try to autofind up to M fluorescent beads at every\n");
    fprintf(stderr, "                 if there are not that many already.\n");
    fprintf(stderr, "       -fluorescent_spot_threshold: Set the threshold for possible spots when\n");
    fprintf(stderr, "                 autofinding.  0 means the minimum intensity in the image, 1 means the max.\n");
    fprintf(stderr, "                 Setting this lower will not miss as many spots,\n");
    fprintf(stderr, "                 but will also find garbage (default 0.5)\n");
    fprintf(stderr, "       -fluorescent_max_regions: Only check up to FR connected regions per frame\n");
    fprintf(stderr, "       -maintain_this_many_beads: Try to autofind up to M beads at every frame\n");
    fprintf(stderr, "                 if there are not that many already.\n");
    fprintf(stderr, "       -candidate_spot_threshold: Set the threshold for possible spots when\n");
    fprintf(stderr, "                 autofinding.  Setting this lower will not miss as many spots,\n");
    fprintf(stderr, "                 but will also find garbage (default 5)\n");
    fprintf(stderr, "       -sliding_window_radius: Set the radius of the global SMD sliding window\n");
    fprintf(stderr, "                 neighborhood.  Higher values of SR may cause some spots to\n");
    fprintf(stderr, "                 take longer before they are detected, but will greatly\n");
    fprintf(stderr, "                 increase the running speed (default 9)\n");
    fprintf(stderr, "       -radius: Set the radius to use for new trackers to R (default 5)\n");
    fprintf(stderr, "       -tracker: Create a tracker with radius R at pixel X,Y and initiate\n");
    fprintf(stderr, "                 optimization.  Multiple trackers can be created\n");
    fprintf(stderr, "       -FIONA_background: Set the default background for FIONA trackers to BG\n");
    fprintf(stderr, "                 (default 0)\n");
    fprintf(stderr, "       -raw_camera_params: Set parameters in case we're opening a raw file\n");
    fprintf(stderr, "                 (default throws a dialog box to ask you for them)\n");
    fprintf(stderr, "       -load_state: Load program state from FILE\n");
    fprintf(stderr, "       -continue_from: Load trackers from last frame in the specified CSV FILE and continue tracking\n");
    fprintf(stderr, "       source: The source file for tracking can be specified here (default is\n");
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
  // VRPN state setting so that we don't try to preload a video file
  // when it is opened, which wastes time.  Also tell it not to
  // accumulate messages, which can cause us to run out of memory.
  vrpn_FILE_CONNECTIONS_SHOULD_PRELOAD = false;
  vrpn_FILE_CONNECTIONS_SHOULD_ACCUMULATE = false;

  //------------------------------------------------------------------
  // Generic Tcl startup.  Getting and interpreter and mainwindow.

#ifndef VST_NO_GUI
  char		command[256];
  Tk_Window     tk_control_window;
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
#ifdef __APPLE__
  char	executable_directory[256];
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
  if (Tcl_Eval(g_tk_control_interp, command) != TCL_OK) {
          fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
                  g_tk_control_interp->result);
          return(-1);
  }
#else
  sprintf(command, "source russ_widgets.tcl");
#endif

  if (Tcl_Eval(g_tk_control_interp, command) != TCL_OK) {
          fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
                  g_tk_control_interp->result);
          return(-1);
  }

  //------------------------------------------------------------------
  // Put the version number into the main window.
  sprintf(command, "label .versionlabel -text CUDA_Video_Filter_v:%s", Version_string);
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
  // Put the "Thank-you Ware" button into the main window.
  sprintf(command, "package require http");
  if (Tcl_Eval(g_tk_control_interp, command) != TCL_OK) {
          fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
                  g_tk_control_interp->result);
          return(-1);
  }
  sprintf(command, "set thanks_text \"Say Thank You!\"");
  if (Tcl_Eval(g_tk_control_interp, command) != TCL_OK) {
          fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
                  g_tk_control_interp->result);
          return(-1);
  }
  sprintf(command, "button .thankyouware -textvariable thanks_text -command { ::http::geturl \"http://www.cs.unc.edu/Research/nano/cismm/thankyou/yourewelcome.htm?program=CUDA_video_filter&Version=%s\" ; set thanks_text \"Paid for by NIH/NIBIB\" }", Version_string);
  if (Tcl_Eval(g_tk_control_interp, command) != TCL_OK) {
          fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
                  g_tk_control_interp->result);
          return(-1);
  }
  sprintf(command, "pack .thankyouware");
  if (Tcl_Eval(g_tk_control_interp, command) != TCL_OK) {
          fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
                  g_tk_control_interp->result);
          return(-1);
  }

  //------------------------------------------------------------------
  // Load the specialized Tcl code needed by this program.  This must
  // be loaded before the Tclvar_init() routine is called because it
  // puts together some of the windows needed by the variables.
#ifdef __APPLE__
  sprintf(command, "source %s/CUDA_video_filter.tcl", executable_directory);
#else
  sprintf(command, "source CUDA_video_filter.tcl");
#endif
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
#endif

  //------------------------------------------------------------------
  // Parse the command line.  This has to be done after a bunch of the
  // set-up, so that we can create trackers and output files and such.
  // It has to be before the imager set-up because we don't know what
  // source to use until after we parse this.
  int	i, realparams;		  // How many non-flag command-line arguments
  realparams = 0;
  // These defaults are set for a Pulnix camera
  unsigned  raw_camera_params_valid = false;  //< Have the following been set intentionally?
  unsigned  raw_camera_numx = 648;
  unsigned  raw_camera_numy = 484;
  unsigned  raw_camera_bitdepth = 8;        //< Number of bits per channel in raw file camera
  unsigned  raw_camera_channels = 1;        //< Number of channels in raw file camera
  unsigned  raw_camera_headersize = 0;      //< Number of header bytes in raw file camera
  unsigned  raw_camera_frameheadersize = 0;      //< Number of header bytes in raw file camera

  for (i = 1; i < argc; i++) {
    if (!strncmp(argv[i], "-nogui", strlen("-nogui"))) {
      g_use_gui = false;
    } else if (!strncmp(argv[i], "-gui", strlen("-gui"))) {
      g_use_gui = true;
    } else if (!strncmp(argv[i], "-raw_camera_params", strlen("-raw_camera_params"))) {
      if (++i >= argc) { Usage(argv[0]); }
      raw_camera_numx = atoi(argv[i]);
      if (++i >= argc) { Usage(argv[0]); }
      raw_camera_numy = atoi(argv[i]);
      if (++i >= argc) { Usage(argv[0]); }
      raw_camera_bitdepth = atoi(argv[i]);
      if (++i >= argc) { Usage(argv[0]); }
      raw_camera_channels = atoi(argv[i]);
      if (++i >= argc) { Usage(argv[0]); }
      raw_camera_headersize = atoi(argv[i]);
      if (++i >= argc) { Usage(argv[0]); }
      raw_camera_frameheadersize = atoi(argv[i]);
      raw_camera_params_valid = true;
    } else if (!strncmp(argv[i], "-blur_lost_and_found", strlen("-blur_lost_and_found"))) {
	if (++i >= argc) { Usage(argv[0]); }
	g_blurLostAndFound = atof(argv[i]);
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

#ifndef VST_NO_GUI
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
#endif

  //------------------------------------------------------------------
  // If we're being asked to open a raw file and we don't have a set of
  // raw-file parameters, then see if we can figure them out based on the
  // file size or else throw a dialog box asking for them.
  if (  (strcmp(".raw", &g_device_name[strlen(g_device_name)-4]) == 0) ||
        (strcmp(".RAW", &g_device_name[strlen(g_device_name)-4]) == 0) ) {

    if (!raw_camera_params_valid) {
      //------------------------------------------------------------
      // See if we can figure out the right settings for these parameters
      // based on the file size (if it is an even multiple of the particular
      // file sizes for the odd-sized Pulnix camera or the Point Grey camera that
      // we're using at UNC.  Standard sizes are going to be harder to
      // determine, because if there are 100 frames it will mask the difference
      // between power-of-2 changes on each axis (640x480 vs. 1280x960, for example).
      __int64 file_length = determine_file_length(g_device_name);
      if (file_length > 0) {   // Zero length means we can't tell, <0 means failure.
        double frame_size, num_frames;

        // Check for original Pulnix cameras used at UNC
        frame_size = 648 * 484;
        num_frames = file_length / frame_size;
        if ( num_frames == floor(num_frames) ) {
          printf("Assuming EDT/Pulnix file format (648x484)\n");
          raw_camera_numx = 648;
          raw_camera_numy = 484;
          raw_camera_bitdepth = 8;
          raw_camera_channels = 1;
          raw_camera_headersize = 0;
          raw_camera_frameheadersize = 0;
          raw_camera_params_valid = true;
        } else {
          // Check for Point Grey camera being used at UNC
          frame_size = 1024 * 768 + 112;
          num_frames = file_length / frame_size;
          if ( num_frames == floor(num_frames) ) {
            printf("Assuming Point Grey file format (1024x768, 112-byte frame headers)\n");
            raw_camera_numx = 1024;
            raw_camera_numy = 768;
            raw_camera_bitdepth = 8;
            raw_camera_channels = 1;
            raw_camera_headersize = 0;
            raw_camera_frameheadersize = 112;
            raw_camera_params_valid = true;
          }
        }
      }
    }

#ifndef VST_NO_GUI
    if (!raw_camera_params_valid) {
      //------------------------------------------------------------
      // Create a callback for a variable that will tell when the
      // params are filled and then create a dialog box that will ask the user
      // to either fill it in or quit.
      Tclvar_int  raw_params_set("raw_params_set", 0);
      if (Tcl_Eval(g_tk_control_interp, "ask_user_for_raw_file_params") != TCL_OK) {
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
      } while ( (raw_params_set == 0) && !g_quit);
      if (g_quit) {
        cleanup();
        exit(0);
      }

      // XXX Read the Tcl variables associated with this into our params
      // and then say that the params have been filled in.
      raw_camera_numx = g_tcl_raw_camera_numx;
      raw_camera_numy = g_tcl_raw_camera_numy;
      raw_camera_bitdepth = g_tcl_raw_camera_bitdepth;
      raw_camera_channels = g_tcl_raw_camera_channels;
      raw_camera_headersize = g_tcl_raw_camera_headersize;
      raw_camera_frameheadersize = g_tcl_raw_camera_frameheadersize;
    }
#endif
  }

  //------------------------------------------------------------------
  // Open the camera.  If we have a video file, then
  // set up the Tcl controls to run it.  Also, report the frame number.
  float exposure = 0.0;
  g_camera_bit_depth = raw_camera_bitdepth;
  if (!get_camera(g_device_name, &g_camera_bit_depth, &exposure,
                  &g_camera, &g_video, raw_camera_numx, raw_camera_numy,
                  raw_camera_channels, raw_camera_headersize, raw_camera_frameheadersize)) {
    fprintf(stderr,"Cannot open camera\n");
    if (g_camera) { delete g_camera; g_camera = NULL; }
    cleanup();
    exit(-1);
  }

  if (g_video) {  // Put these in a separate control panel?
    // Start out paused at the beginning of the file.
    g_play = new Tclvar_int_with_button("play_video","",0);
    g_rewind = new Tclvar_int_with_button("rewind_video","",1);
    g_step = new Tclvar_int_with_button("single_step_video","");
#ifndef VST_NO_GUI
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
#endif
  }

  // Verify that the camera is working.
  if (!g_camera->working()) {
    fprintf(stderr,"Could not establish connection to camera\n");
    if (g_camera) { delete g_camera; g_camera = NULL; }
    cleanup();
    exit(-1);
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
  if (g_use_gui) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowPosition(190 + g_window_offset_x, 210 + g_window_offset_y);
    glutInitWindowSize(g_camera->get_num_columns(), g_camera->get_num_rows());
  #ifdef DEBUG
    printf("initializing window to %dx%d\n", g_camera->get_num_columns(), g_camera->get_num_rows());
  #endif
    g_tracking_window = glutCreateWindow(g_device_name);
    glutMotionFunc(motionCallbackForGLUT);
    glutMouseFunc(mouseCallbackForGLUT);
    glutKeyboardFunc(keyboardCallbackForGLUT);
  }

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
