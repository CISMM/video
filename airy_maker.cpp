//XXX What to do when the focal plane isn't at the emitter?

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tcl.h>
#include <tk.h>
#include "Tcl_Linkvar.h"

#ifdef	_WIN32
#include <windows.h>
#include <GL/gl.h>
#include <GL/glut.h>
#elif __APPLE__
#include <OPENGL/gl.h>
#include <GLUT/glut.h>
#endif

#include <vrpn_Shared.h>
#include "spot_math.h"
#include "base_camera_server.h"

//--------------------------------------------------------------------------
#include <time.h>
#include "stocc.h"                     // define random library classes
// make random library
static int32 seed = time(0);                // random seed
static StochasticLib1 sto(seed);            // make instance of random library

//--------------------------------------------------------------------------
//#define	DEBUG

//--------------------------------------------------------------------------
// Version string for this program
const char *Version_string = "01.07";

//--------------------------------------------------------------------------
// Constants needed by the rest of the program

const float	g_Window_Units = 1e9;

// Used by mouse code
enum { M_NOTHING, M_X_ONLY, M_Y_ONLY, M_X_AND_Y };

//--------------------------------------------------------------------------
// Glut wants to take over the world when it starts, so we need to make
// global access to the objects we will be using.

bool	g_already_posted = false;	  //< Posted redisplay since the last display?
int	g_mousePressX, g_mousePressY;	  //< Where the mouse was when the button was pressed
int	g_whichDragAction;		  //< What action to take for mouse drag
char    *g_basename = NULL;               //< Base name for saving files
int     g_basenum = 0;                    //< Number to append to the next-saved file
int     g_logrepeatcount = 0;             //< Keeps track of how many frames at this location have been logged
FILE	*g_csv_file = NULL;		  //< File to save data in with .csv extension

//--------------------------------------------------------------------------
// Global variables
int	g_Window_Size_X = 800;
int	g_Window_Size_Y = g_Window_Size_X;

// Image array to hold the fraction of total Airy energy landing in
// each pixel.  NOTE: The (0,0) pixel is in the middle of the array,
// as indexed by the CX and CY parameters
const unsigned g_NX = 511;
const unsigned g_NY = g_NX;
const unsigned g_CX = g_NX/2;
const unsigned g_CY = g_CX;
class ImageArray : public image_wrapper {
public:

  ImageArray(void) { clear(); };

  void clear(void) {
    unsigned x,y;
    for (x = 0; x < g_NX; x++) {
      for (y = 0; y < g_NY; y++) {
        d_values[x][y] = 0;
      }
    }

    // Clear the range so that they are at the far boundaries
    // (this will mean no data filled in yet).  Note that these
    // assume that (0,0) is the center.
    d_minx = g_CX;
    d_miny = g_CY;
    d_maxx = -d_minx;
    d_maxy = -d_miny;
  };

  double value(int x, int y) const {
    return d_values[x+g_CX][y+g_CY];
  };

  void value(int x, int y, double val) {
    // See if we are filling in a new region (center is [0,0])
    if (x < d_minx) { d_minx = x; }
    if (x > d_maxx) { d_maxx = x; }
    if (y < d_miny) { d_miny = y; }
    if (y > d_maxy) { d_maxy = y; }

    // Write to the actual memory location (offset to center).
    //printf("XXX X range = %d,%d; Y range = %d,%d\n", d_minx, d_maxx, d_miny, d_maxy);
    d_values[x + g_CX][y + g_CY] = val;
  };

  // "ita" means "imager-to-airy-space".
  //  ita_scale: Multiples integer pixel coordinates in imager space to get Airy-space coordinates.
  //  itx_offset_x: Added to the scaled value; Airy-space offset of its center.
  void fillFromAiryFunction(int radius_from_center,
                            double ita_scale,
                            double ita_offset_x, double ita_offset_y,
                            double pixel_fraction_hidden,
                            int samples_per_pixel);

  // Methods needed for image_wrapper base class (which we use to write the TIFF file).
  void read_range(int &minx, int &maxx, int &miny, int &maxy) const {
    minx = d_minx; maxx = d_maxx; miny = d_miny; maxy = d_maxy;
  };
  unsigned get_num_colors(void) const { return 1; };
  double read_pixel_nocheck(int x, int y, unsigned rgb = 0) const {
    return value(x,y);
  };
  bool read_pixel(int x, int y, double &val, unsigned rgb = 0) const {
    if ( (x < d_minx) || (x > d_maxx) || (y < d_miny) || (y > d_maxy) ) {
      val = 0.0;
      return false;
    } else {
      val = read_pixel_nocheck(x,y, rgb);
      return true;
    }
  }

protected:
  double  d_values[g_NX][g_NY];
  int     d_minx, d_maxx, d_miny, d_maxy;  //< Range of pixels that have had something filled in, (0,0) is the center.
} g_Image, g_NoiseImage;


//--------------------------------------------------------------------------
// Tcl controls and displays
void  logfilename_changed(char *newvalue, void *);
Tclvar_float_with_scale	g_Wavelength("wavelength_nm", ".kernel.wavelength", 350, 750, 510);
Tclvar_float_with_scale	g_PixelSpacing("pixelSpacing_nm", ".kernel.pixel", 15, g_Window_Size_X/4, 70);
Tclvar_float_with_scale	g_PixelHidden("pixelFracHidden", ".kernel.pixelhide", 0, 0.5, 0.1);
Tclvar_float_with_scale	g_Radius("aperture_nm", ".kernel.aperture", 200, 2000, 500);
Tclvar_float_with_scale	g_PixelXOffset("x_offset_pix", "", -1, 1, 0);
Tclvar_float_with_scale	g_PixelYOffset("y_offset_pix", "", -1, 1, 0);
Tclvar_float_with_scale	g_SampleCount("samples", "", 64, 256, 64);
Tclvar_float_with_scale	g_PhotonCount("fluorescent_photons", "", 10, 50000, 40000.0);
Tclvar_float_with_scale	g_DarkPhotonsPerPixel("dark_photons_per_pixel", "", 0, 1000.0, 0.0);
Tclvar_int_with_button	g_PoissonNoise("poisson_noise","", 0);
Tclvar_float_with_scale	g_PhotonsPerCount("photons_per_count", "", 1,100, 20);
Tclvar_int_with_button	g_ShowSqrt("show_sqrt","", 1);
Tclvar_int_with_button	g_quit("quit",NULL);
Tclvar_selector		g_logfilename("logfilename", NULL, NULL, "", logfilename_changed, NULL);
Tclvar_float_with_scale	g_LogRepeatLocation("repeat_position", ".log", 1, 10, 1);
Tclvar_float_with_scale	g_LogDX("move_in_x", ".log", 0, 1, 0.1);
Tclvar_float_with_scale	g_LogDY("move_in_y", ".log", 0, 1, 0.1);

//--------------------------------------------------------------------------
// Glut callback routines.

static void  cleanup(void)
{
  // Done with the camera and other objects.
  printf("Exiting\n");

  if (g_csv_file) { fclose(g_csv_file); g_csv_file = NULL; };
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
		FraunhoferIntensity(g_Radius/1e9, fabs(loop + airyOffsetMatchingPixelOffset), g_Wavelength/1e9,1.0) / valAtZero,
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
		FraunhoferIntensity(g_Radius/1e9, fabs(loop + airyOffsetMatchingPixelOffset), g_Wavelength/1e9,1.0) / valAtZero,
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
		  sqrt(FraunhoferIntensity(g_Radius/1e9, fabs(loop + airyOffsetMatchingPixelOffset), g_Wavelength/1e9,1.0) / valAtZero),
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

//----------------------------------------------------------------------------
// Draws a 2D grid with the specified spacing.

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

// This function fills in the global image with values that represent
// the sum of the Airy energy falling in each pixel.
// It presumes that the (0,0) entry is at the center of the image, and
// fills in around the center to a range specified in the "radius"
// parameter.

void ImageArray::fillFromAiryFunction(int radius_from_center,
                            double ita_scale,
                            double ita_offset_x, double ita_offset_y,
                            double pixel_fraction_hidden,
                            int samples_per_pixel)
{
  int i, j; // Imager-pixel coordinates of the centers of the pixels to compute the values for
  for (i = -radius_from_center; i <= radius_from_center; i++) {
    for (j = -radius_from_center; j <= radius_from_center; j++) {

      // The coordinates of the center of the current pixel in Airy space.
      double x = i * ita_scale + ita_offset_x;
      double y = j * ita_scale + ita_offset_y;

      // Find the four corners of the Airy-space box that surround this pixel.
      double halfpixel_step = ita_scale/2;
      double xs0 = x - halfpixel_step;
      double xs1 = x + halfpixel_step;
      double ys0 = y - halfpixel_step;
      double ys1 = y + halfpixel_step;

      // Find the four corners of the pixel that actually has area to receive
      // photons.  This removes the area clipped by the pixelFrac parameter.
      // These are also found in screen space.
      double fraction_covered_step = halfpixel_step * pixel_fraction_hidden;
      double xc0 = xs0 + fraction_covered_step;
      double xc1 = xs1 - fraction_covered_step;
      double yc0 = ys0 + fraction_covered_step;
      double yc1 = ys1 - fraction_covered_step;

      // Compute the sum of Airy energy volume falling within the receiving area.
      double frac = ComputeAiryVolume(g_Radius/1e9, g_Wavelength/1e9, xc0, xc1, yc0, yc1, samples_per_pixel);

      // Fill in the value here.
      value(i,j, frac);
    }
  }
}

void drawPixelBarGraphs(
  double width,		//< Width in pixels of the screen (assumed isotropic in X and Y)
  double units,		//< Converts from Pixel units to screen-space units
  double pixelSpacing,	//< How far in screen pixels between imager pixels
  double pixelFrac,	//< What fraction of imager pixels are not active
  double xOffset,	//< How far from the center (and which direction) to offset Airy pattern in X in capture device units
  double yOffset,       //< How far from the center (and which direction) to offset Airy pattern in Y in capture device units
  double totVol)	//< Total volume under the Airy disk.
{
  // Compute how far to step between pixel centers in the screen, then
  // how far down you can go without getting below -1 when taking these
  // steps.  Then take one more step than this to get started.
  double pixelStepInScreen = 1/(width/2.0) * (pixelSpacing/1e9) * units;
  double halfStepInScreen = 0.5 * pixelStepInScreen;
  int numsteps = floor(1/pixelStepInScreen) + 1;
  int i,j;
  for (i = -numsteps; i <= numsteps; i++) {
    for (j = -numsteps; j <= numsteps; j++) {
      double x = i * pixelStepInScreen;
      double y = j * pixelStepInScreen;

      // Find the four corners of the pixel where we are going to draw the
      // bar graph.  These include the whole pixel area.  These are found in
      // the screen space, from -1..1 in X and Y.
      double xs0 = x - halfStepInScreen;
      double xs1 = x + halfStepInScreen;
      double ys0 = y - halfStepInScreen;
      double ys1 = y + halfStepInScreen;

      // Compute the fraction of volume falling within the receiving area by
      // dividing the volume within it to the volume covered by the whole Airy disk
      // computed above.  This is the fraction of the pixel to fill.
      double frac = g_Image.value(i,j) / totVol;

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

void drawPixelIntensitiesAndComputeNoiseImage(
  double width,		//< Width in pixels of the screen (assumed isotropic in X and Y)
  double units,		//< Converts from Pixel units to screen-space units
  double pixelSpacing,	//< How far in screen pixels between imager pixels
  double pixelFrac,	//< What fraction of imager pixels are not active
  double xOffset,	//< How far from the center (and which direction) to offset Airy pattern in X in capture device units
  double yOffset,       //< How far from the center (and which direction) to offset Airy pattern in Y in capture device units
  double totVol)	//< Total volume under the Airy disk.
{
  // Compute how far to step between pixel centers in the screen, then
  // how far down you can go without getting below -1 when taking these
  // steps.  Then take one more step than this to get started.
  // Make sure that the image size is at least 31x31 pixels by clamping
  // numsteps.
  // THIS CODE MUST MATCH that found in saveNoiseImage.
  double pixelStepInScreen = 1/(width/2.0) * (pixelSpacing/1e9) * units;
  double halfStepInScreen = 0.5 * pixelStepInScreen;
  int numsteps = floor(1/pixelStepInScreen) + 1;
  if (numsteps < 15) { numsteps = 15; }
  int i,j;

  // Actually draw the pixels
  for (i = -numsteps; i <= numsteps; i++) {
    for (j = -numsteps; j <= numsteps; j++) {
      double x = i * pixelStepInScreen;
      double y = j * pixelStepInScreen;

      // Find the four corners of the pixel where we are going to draw the
      // values.  These include the whole pixel area.  These are found in
      // the screen space, from -1..1 in X and Y.
      double xs0 = x - halfStepInScreen;
      double xs1 = x + halfStepInScreen;
      double ys0 = y - halfStepInScreen;
      double ys1 = y + halfStepInScreen;

      // Compute the fraction of volume falling within the receiving area by
      // dividing the volume within it to the volume covered by the whole Airy disk
      // computed above.  This is the fraction of the pixel to fill.
      double frac = g_Image.value(i,j) / totVol;

      //---------------------------------------------------------------------------
      // Find out how many photons this corresponds to.  Also find out how many
      // photons there are in the dark current.
      double light_photons = frac * g_PhotonCount;
      double dark_photons = g_DarkPhotonsPerPixel;

      // If we have are doing Poisson noise, replace each of these photon counts
      // with samples from a Poisson distribution whose expected value is
      // equal to the number of photons.
      if (g_PoissonNoise == 1) {
        light_photons = sto.Poisson(light_photons);
        dark_photons = sto.Poisson(dark_photons);
      }

      // Add up the number of photons and divide by photons/count to turn the
      // values into counts.  Leave the result back in the "frac" variable that
      // started the process.
      frac = (light_photons + dark_photons) / g_PhotonsPerCount;

      // Copy the resulting value into the noise image, dividing color by 255
      // so that a pixel count of 255 goes to a full-on white (1).
      frac /= 255.0;
      g_NoiseImage.value(i,j,frac);

      // Draw a filled-in square that covers the whole pixel with a color
      // scaled to the fraction of the Airy disk volume computed above.
      glColor3f(frac, frac, frac);
      glBegin(GL_QUADS);
	glVertex3f( xs0, ys0, 0.0 );
	glVertex3f( xs1, ys0, 0.0 );
	glVertex3f( xs1, ys1, 0.0 );
	glVertex3f( xs0, ys1, 0.0 );
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
  drawSideViewVsPixels(g_Window_Size_X, g_Window_Units, g_PixelSpacing, g_PixelHidden, g_PixelXOffset);

  // Draw the curve vs. the Y axis.
  glLoadIdentity();
  glScalef(1.0,0.5,1.0);
  glTranslatef(0.0, -1.0, 0.0);
  glRotatef(-90, 0,0,1);
  drawSideViewVsPixels(g_Window_Size_X, g_Window_Units, g_PixelSpacing, g_PixelHidden, g_PixelYOffset);

  // We adjust the viewport here rather than the viewing matrix because it
  // causes the drawing to be clipped to the boundaries of the viewport and
  // not spill over to the rest of the display.

  // Draw the pixel grid in the lower-left portion of the screen.
  glLoadIdentity();
  glViewport(0, 0, g_Window_Size_X/2, g_Window_Size_Y/2);
  drawPixelGrid(g_Window_Size_X, g_Window_Units, g_PixelSpacing);

  // Compute the values needed to figure out how to convert from imager
  // pixels to Airy-space pixels (scales and offsets).  Also figure out how
  // far to go from the center.
  // The Airy coordinates are the same as the Screen-space coordinates
  //    (see the loop in drawSideViewVsPixels(), where the area tracing
  //     is done).
  // Therefore Airy/Imager = ScreenSpace/Imager.
  // ScreenSpace goes from -1 to 1, so has a distance of 2, during which
  // the number of pixels on the whole screen is g_Window_Size_X in the
  // whole display), making the mapping from ScreenSpace/Pixel = 2/g_Window_Size_X.
  // The number of pixels/Imager step = g_PixelSpacing.
  // Make sure we fill at least 31x31 pixels.
  double display_pixel_per_imager_pixel = g_PixelSpacing;
  double airy_pixel_per_display_pixel = (2.0/g_Window_Size_X);//XXX; // XXX Fix later when we have units
  double airy_pixel_per_imager_pixel = airy_pixel_per_display_pixel * display_pixel_per_imager_pixel;
  // The g_Pixel[XY]Offset is in imager pixels, not screen pixels.
  double airy_offset_matching_pixel_offset_X = -g_PixelXOffset * airy_pixel_per_imager_pixel;
  double airy_offset_matching_pixel_offset_Y = g_PixelYOffset * airy_pixel_per_imager_pixel;
  int radius = ceil( (g_Window_Size_X/2.0) / display_pixel_per_imager_pixel );
  if (radius < 31) { radius = 31; }
  int numsamples = g_SampleCount / (radius*2);

  // Compute the values in the Airy image buffer.
  g_Image.fillFromAiryFunction(radius, airy_pixel_per_imager_pixel,
    airy_offset_matching_pixel_offset_X, airy_offset_matching_pixel_offset_Y,
    g_PixelHidden, numsamples);

  // Compute the total volume under the Airy disk.
  double totVol = ComputeAiryVolume(g_Radius/1e9, g_Wavelength/1e9, -1,1, -1,1, g_SampleCount);

  // Draw the bar graphs for the lower-left portion of the screen.
  drawPixelBarGraphs(g_Window_Size_X, g_Window_Units, g_PixelSpacing, g_PixelHidden, g_PixelXOffset, g_PixelYOffset, totVol);

  // Draw the cross-hairs to show the pixel center in the lower-left portion of the screen.
  drawCrossHairs(g_Window_Size_X, g_Window_Units, g_PixelSpacing, g_PixelXOffset, g_PixelYOffset);

  // Draw the pixel intensities in the upper-right portion of the screen.
  glLoadIdentity();
  glViewport(g_Window_Size_X/2, g_Window_Size_Y/2, g_Window_Size_X/2, g_Window_Size_Y/2);
  drawPixelIntensitiesAndComputeNoiseImage(g_Window_Size_X, g_Window_Units, g_PixelSpacing, g_PixelHidden, g_PixelXOffset, g_PixelYOffset, totVol);
  //drawPixelGrid(g_Window_Size_X, g_Window_Units, g_PixelSpacing);

  // Set the viewport back to the whole window.
  glViewport(0,0, g_Window_Size_X, g_Window_Size_Y);

  // Swap buffers so we can see it.
  glutSwapBuffers();

  // If we're supposed to be saving images, then do so
  if (g_basename != NULL) {

    // First store a new line into the CSV log file.
    if (g_csv_file) {
      int minx, maxx, miny, maxy;
      g_NoiseImage.read_range(minx, maxx, miny, maxy);
      int num_x = maxx-minx+1;
      int num_y = maxy-miny+1;
      double x = num_x/2 + g_PixelXOffset;
      double y = num_y/2 + g_PixelYOffset;

      fprintf(g_csv_file, "%d,0,%lg,%lg,0.0,0.0,0.0\n", g_basenum, x, y);
    }

    // Then save the image file.
    if (strlen(g_basename) > 2000) {
      fprintf(stderr,"Cannot save to file, name too long\n");
    } else {
      char filename[2048];
      sprintf(filename, "%s.%03d.tif", g_basename, g_basenum);

      printf("Saving frame to %s\n", filename);

      // Write to the file, scaled so that 65535 is the max (openGL had 1 as the max)
      g_NoiseImage.write_to_tiff_file(filename, 65535.0);

      g_basenum++;

      // If we saved as many frames here as we should, then increment the location
      if (++g_logrepeatcount >= g_LogRepeatLocation) {
        g_PixelXOffset = g_PixelXOffset + g_LogDX;
        g_PixelYOffset = g_PixelYOffset + g_LogDY;
        g_logrepeatcount = 0;
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

//------------------------------------------------------------------------
// Make note of the new size so that our mouse and drawing
// code continues to work.
void myReshapeFunc(int width, int height)
{
  glViewport(0,0, width, height);
  g_Window_Size_X = width;
  g_Window_Size_Y = height;
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
  double xNorm = x*2.0/g_Window_Size_X - 0.5;

  // Convert this to offset in display pixel units.
  double xOffset = xNorm * g_Window_Size_X / (g_PixelSpacing);

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
  double yNorm = y*2.0/g_Window_Size_Y - 1.5;

  // Convert this to offset in display pixel units.
  double yOffset = yNorm * g_Window_Size_Y / (g_PixelSpacing);

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
	} else {
	  printf("XXX Released middle button\n");
	}
	break;

      case GLUT_LEFT_BUTTON:
	if (state == GLUT_DOWN) {
	  // Left mouse button clicked in the lower-left quadrant
	  // of the screen (bracketed by the airey patterns) causes
	  // the offsets to be set to the point where the mouse is.
	  // Dragging here continues to reset this offset.  There is
	  // a limit of +/- 1 on the offset.
	  if ( (x <= g_Window_Size_X/2) && (y >= g_Window_Size_Y/2) ) {
    	    compute_x_offset_based_on_mouse(x);
    	    compute_y_offset_based_on_mouse(y);
	    g_whichDragAction = M_X_AND_Y;
	  }

	  // Left mouse button clicked in the upper-left quadrant
	  // causes motion of only the X offet.
	  if ( (x <= g_Window_Size_X/2) && (y < g_Window_Size_Y/2) ) {
    	    compute_x_offset_based_on_mouse(x);
	    g_whichDragAction = M_X_ONLY;
	  }

	  // Left mouse button clicked in the lower-right quadrant
	  // causes motion of only the X offet.
	  if ( (x > g_Window_Size_X/2) && (y >= g_Window_Size_Y/2) ) {
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
  // Close the old CSV log file, if there was one.
  if (g_csv_file != NULL) {
    fclose(g_csv_file);
    g_csv_file = NULL;
  }

  // Clear everything that was set before.
  g_basenum = 0;
  g_logrepeatcount = 0;
  if (g_basename) { delete [] g_basename; g_basename = NULL; }

  // If the basename has been set to empty, then we're done saving,
  // so just leave things un-set.
  if ( (newvalue == NULL) || (strlen(newvalue) == 0) ) {
    return;
  }

  // Form the base name by stripping off any file extension (probably
  // .tif) from the file name and putting it into g_basename.
  g_basename = new char[strlen(newvalue)+1];
  if (g_basename == NULL) {
    fprintf(stderr, "logfilename_changed(): Out of memory\n");
    cleanup();
    exit(-1);
  }
  strcpy(g_basename, newvalue);
  char *last_dot = strrchr(g_basename, '.');
  if (last_dot != NULL) {
    *last_dot = '\0';
  }
  printf("Saving to a sequence of images named %s.NNN.tif\n", g_basename);

  // Open the CSV file and put the titles on.
  // Make sure that the file does not exist by deleting it if it does.
  // The Tcl code had a dialog box that asked the user if they wanted
  // to overwrite, so this is "safe."
  char *csvname = new char[strlen(newvalue)+5];	// Remember the closing '\0'
  if (csvname == NULL) {
    fprintf(stderr, "Out of memory when allocating CSV file name\n");
    cleanup();
    exit(-1);
  }
  strcpy(csvname, newvalue);
  strcpy(&csvname[strlen(csvname)-4], ".csv");  // Replace ".tif" with ".csv"
  if ( NULL == (g_csv_file = fopen(csvname, "w")) ) {
    fprintf(stderr,"Cannot open CSV file for writing: %s\n", csvname);
  } else {
    fprintf(g_csv_file, "FrameNumber,Spot ID,X,Y,Z,Radius,Orientation\n");
  }
  delete [] csvname;
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
  glutInitWindowSize(g_Window_Size_X, g_Window_Size_Y);
#ifdef DEBUG
  printf("XXX initializing window to %dx%d\n", g_camera->get_num_columns(), g_camera->get_num_rows());
#endif
  glutCreateWindow("airy_vs_pixels");
  glutMotionFunc(motionCallbackForGLUT);
  glutMouseFunc(mouseCallbackForGLUT);
  glutReshapeFunc(myReshapeFunc);

  // Set the display function and idle function for GLUT (they
  // will do all the work) and then give control over to GLUT.
  glutDisplayFunc(myDisplayFunc);
  glutIdleFunc(myIdleFunc);
  glutMainLoop();

  cleanup();
  return 0;
}
