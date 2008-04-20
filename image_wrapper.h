#ifndef	IMAGE_WRAPPER_H
#define	IMAGE_WRAPPER_H

#include  "base_camera_server.h"
#include  "spot_math.h"

class disc_image: public double_image {
public:
  // Create an image of the specified size and background intensity with a
  // disk in the specified location (may be subpixel) with the specified
  // radius and intensity.
  disc_image(int minx = 0, int maxx = 255, int miny = 0, int maxy = 255,
	     double background = 127.0, double noise = 0.0,
	     double diskx = 127.25, double disky = 127.75, double diskr = 18.5,
	     double diskintensity = 250, int oversample = 1);

protected:
  int	  _oversample;
};

class cone_image: public double_image {
public:
  // Create an image of the specified size and background intensity with a
  // cone in the specified location (may be subpixel) with the specified
  // radius and intensity.  The cone is brighter than the background, with
  // the specified brightness at its peak.
  cone_image(int minx = 0, int maxx = 255, int miny = 0, int maxy = 255,
	     double background = 127.0, double noise = 0.0,
	     double diskx = 127.25, double disky = 127.75, double diskr = 18.5,
	     double centerintensity = 250, int oversample = 1);

protected:
  int	  _oversample;
};

class Integrated_Gaussian_image: public double_image {
public:
  // Create an image of the specified size and background intensity with a
  // Gaussian in the specified location (may be subpixel) with the specified
  // standard deviation and total volume (in height-pixels, if integrated to infinity).
  // The Gaussian is brighter than the background for positive volumes.
  Integrated_Gaussian_image(int minx = 0, int maxx = 255, int miny = 0, int maxy = 255,
	     double background = 127.0, double noise = 0.0,
	     double centerx = 127.25, double centery = 127.75, double std_dev = 2.5,
	     double summedvolume = 250, int oversample = 1);

  // Recompute the Gaussian based on new parameters.
  void recompute(double background = 127.0, double noise = 0.0,
	     double centerx = 127.25, double centery = 127.75, double std_dev = 2.5,
	     double summedvolume = 250, int oversample = 1);

protected:
  int	  _oversample;
  double  _background;
};

class Point_sampled_Gaussian_image: public image_wrapper {
public:
  // Create an image of the specified size and background intensity with a
  // Gaussian in the specified location (may be subpixel) with the specified
  // standard deviation and total volume (in height-pixels, if integrated to infinity).
  // The Gaussian is brighter than the background for positive volumes.
  Point_sampled_Gaussian_image(
	     double background = 127.0, double noise = 0.0,
	     double centerx = 127.25, double centery = 127.75, double std_dev = 2.5,
	     double summedvolume = 250);

  // Set new parameters
  void set_new_parameters(double background = 127.0, double noise = 0.0,
	     double centerx = 127.25, double centery = 127.75, double std_dev = 2.5,
             double summedvolume = 250) {
    _background = background;
    _noise = noise;
    _centerx = centerx;
    _centery = centery;
    _std_dev = std_dev;
    _summedvolume = summedvolume;
  }

  // Tell what the range is for the image.
  virtual void	read_range(int &minx, int &maxx, int &miny, int &maxy) const {
    minx = miny = -32767; maxx = maxy = 32767; };

  // Read a pixel from the image into a double; return true if the pixel
  // was in the image, false if it was not.  For this image, all pixels are
  // within it -- we recompute a point-sampled value whenever we need one.
  virtual bool	read_pixel(int x, int y, double &result, unsigned /* RGB ignored */) const;
  virtual double read_pixel_nocheck(int x, int y, unsigned /* RGB ignored */) const;

  /// Return the number of colors that the image has
  virtual unsigned  get_num_colors() const { return 1; }

protected:
  double  _background;
  double  _noise;
  double  _centerx, _centery;
  double  _std_dev;
  double  _summedvolume;
};

class rod_image: public double_image {
public:
  // Create an image of the specified size and background intensity with a
  // rod in the specified location (may be subpixel) with the specified
  // radius and intensity and length and angle (in radians).
  rod_image(int minx = 0, int maxx = 255, int miny = 0, int maxy = 255,
	     double background = 127.0, double noise = 0.0,
	     double rodx = 127.25, double rody = 127.75, double rodr = 18.5,
	     double rodlength = 40, double rodangleradians = 0, double rodintensity = 250,
             int oversample = 1);
protected:
  int	  _oversample;
};

#endif
