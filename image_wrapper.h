#ifndef	IMAGE_WRAPPER_H
#define	IMAGE_WRAPPER_H

#include <stdio.h>   // For NULL

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

  // Read a pixel from the image into a double; return true if the pixel
  // was in the image, false if it was not.
  virtual bool	read_pixel(int x, int y, double	&result) const = 0;

  // Do bilinear interpolation to read from the image, in order to
  // smoothly interpolate between pixel values.
  virtual bool	read_pixel_bilerp(double x, double y, double &result) const;
};

class test_image: public image_wrapper {
public:
  // Create an image of the specified size and background intensity with a
  // disk in the specified location (may be subpixel) with the specified
  // radius and intensity.
  test_image(int minx = 0, int maxx = 255, int miny = 0, int maxy = 255,
	     double background = 127.0, double noise = 0.0,
	     double diskx = 127.25, double disky = 127.75, double diskr = 18.5,
	     double diskintensity = 250);

  // Tell what the range is for the image.
  virtual void	read_range(int &minx, int &maxx, int &miny, int &maxy) const;

  // Read a pixel from the image into a double; return true if the pixel
  // was in the image, false if it was not.
  virtual bool	read_pixel(int x, int y, double &result) const;


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
