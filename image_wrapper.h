#ifndef	IMAGE_WRAPPER_H
#define	IMAGE_WRAPPER_H

#include <stdio.h>   // For NULL
#include <math.h>    // For floor()

//----------------------------------------------------------------------------
// This class forms a basic wrapper for an image.  It treats an image as anything
// which can support requests on the number of pixels in an image and can
// perform queries by pixel coordinate in that image.  It is a set of functions
// that are needed by the spot_tracker class, and define the interface it needs
// to do its thing.

class image_wrapper {
public:
  // Tell what the range is for the image.
  virtual void	read_range(int &minx, int &maxx, int &miny, int &maxy) const = 0;

  /// Read a pixel from the image into a double; return true if the pixel
  // was in the image, false if it was not.
  virtual bool	read_pixel(int x, int y, double	&result) const = 0;

  /// Read a pixel from the image into a double; Don't check boundaries.
  virtual double read_pixel_nocheck(int x, int y) const = 0;

  // Do bilinear interpolation to read from the image, in order to
  // smoothly interpolate between pixel values.
  // All sorts of speed tweaks in here because it is in the inner loop for
  // the spot tracker and other codes.
  inline bool	read_pixel_bilerp(double x, double y, double &result) const
  {
    // The order of the following statements is optimized for speed.
    // The double version is used below for xlowfrac comp, ixlow also used later.
    // Slightly faster to explicitly compute both here to keep the answer around.
    double xlow = floor(x); int ixlow = (int)xlow;
    // The double version is used below for ylowfrac comp, ixlow also used later
    // Slightly faster to explicitly compute both here to keep the answer around.
    double ylow = floor(y); int iylow = (int)ylow;
    int ixhigh = ixlow+1;
    int iyhigh = iylow+1;
    double xhighfrac = x - xlow;
    double yhighfrac = y - ylow;
    double xlowfrac = 1.0 - xhighfrac;
    double ylowfrac = 1.0 - yhighfrac;
    double ll, lh, hl, hh;

    // Combining the if statements into one using || makes it slightly slower.
    // Interleaving the result calculation with the returns makes it slower.
    if (!read_pixel(ixlow, iylow, ll)) { return false; }
    if (!read_pixel(ixlow, iyhigh, lh)) { return false; }
    if (!read_pixel(ixhigh, iylow, hl)) { return false; }
    if (!read_pixel(ixhigh, iyhigh, hh)) { return false; }
    result = ll * xlowfrac * ylowfrac + 
	     lh * xlowfrac * yhighfrac +
	     hl * xhighfrac * ylowfrac +
	     hh * xhighfrac * yhighfrac;
    return true;
  };

  // Do bilinear interpolation to read from the image, in order to
  // smoothly interpolate between pixel values.
  // All sorts of speed tweaks in here because it is in the inner loop for
  // the spot tracker and other codes.
  // Does not check boundaries to make sure they are inside the image.
  inline double read_pixel_bilerp_nocheck(double x, double y) const
  {
    // The order of the following statements is optimized for speed.
    // The double version is used below for xlowfrac comp, ixlow also used later.
    // Slightly faster to explicitly compute both here to keep the answer around.
    double xlow = floor(x); int ixlow = (int)xlow;
    // The double version is used below for ylowfrac comp, ixlow also used later
    // Slightly faster to explicitly compute both here to keep the answer around.
    double ylow = floor(y); int iylow = (int)ylow;
    int ixhigh = ixlow+1;
    int iyhigh = iylow+1;
    double xhighfrac = x - xlow;
    double yhighfrac = y - ylow;
    double xlowfrac = 1.0 - xhighfrac;
    double ylowfrac = 1.0 - yhighfrac;

    // Combining the if statements into one using || makes it slightly slower.
    // Interleaving the result calculation with the returns makes it slower.
    return read_pixel_nocheck(ixlow, iylow) * xlowfrac * ylowfrac +
	   read_pixel_nocheck(ixlow, iyhigh) * xlowfrac * yhighfrac +
	   read_pixel_nocheck(ixhigh, iylow) * xhighfrac * ylowfrac +
	   read_pixel_nocheck(ixhigh, iyhigh) * xhighfrac * yhighfrac;
  };
};

class disc_image: public image_wrapper {
public:
  // Create an image of the specified size and background intensity with a
  // disk in the specified location (may be subpixel) with the specified
  // radius and intensity.
  disc_image(int minx = 0, int maxx = 255, int miny = 0, int maxy = 255,
	     double background = 127.0, double noise = 0.0,
	     double diskx = 127.25, double disky = 127.75, double diskr = 18.5,
	     double diskintensity = 250);
  ~disc_image();

  // Tell what the range is for the image.
  virtual void	read_range(int &minx, int &maxx, int &miny, int &maxy) const;

  // Read a pixel from the image into a double; return true if the pixel
  // was in the image, false if it was not.
  virtual bool	read_pixel(int x, int y, double &result) const;
  virtual double read_pixel_nocheck(int x, int y) const;


protected:
  int	  _minx, _maxx, _miny, _maxy;
  double  *_image;

  // Index the specified pixel, returning false if out of range
  inline  bool	find_index(int x, int y, int &index) const
    {
      if (_image == NULL) { return false; }
      if ( (x < _minx) || (x > _maxx) || (y < _miny) || (y > _maxy) ) {
	return false;
      }
      index = (x-_minx) + (y-_miny)*(_maxx-_minx+1);
      return true;
    }
};

class cone_image: public image_wrapper {
public:
  // Create an image of the specified size and background intensity with a
  // cone in the specified location (may be subpixel) with the specified
  // radius and intensity.  The cone is brighter than the background, with
  // the specified brightness at its peak.
  cone_image(int minx = 0, int maxx = 255, int miny = 0, int maxy = 255,
	     double background = 127.0, double noise = 0.0,
	     double diskx = 127.25, double disky = 127.75, double diskr = 18.5,
	     double centerintensity = 250);
  ~cone_image();

  // Tell what the range is for the image.
  virtual void	read_range(int &minx, int &maxx, int &miny, int &maxy) const;

  // Read a pixel from the image into a double; return true if the pixel
  // was in the image, false if it was not.
  virtual bool	read_pixel(int x, int y, double &result) const;
  virtual double read_pixel_nocheck(int x, int y) const;


protected:
  int	  _minx, _maxx, _miny, _maxy;
  double  *_image;

  // Index the specified pixel, returning false if out of range
  inline  bool	find_index(int x, int y, int &index) const
    {
      if (_image == NULL) { return false; }
      if ( (x < _minx) || (x > _maxx) || (y < _miny) || (y > _maxy) ) {
	return false;
      }
      index = (x-_minx) + (y-_miny)*(_maxx-_minx+1);
      return true;
    }
};

#endif
