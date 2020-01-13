//XXX Need to think about how to do a single color to RGB on the writing to OpenGL, for
// VST and optimizer we may sometimes want just one channel to be
// displayed (the one that is being tracked).
//XXX Add the new shifting policy and OpenGL renderer to Video Optimizer.

#ifndef	BASE_CAMERA_SERVER_H
#define	BASE_CAMERA_SERVER_H

#include <stdio.h>   // For NULL
#include <math.h>    // For floor()
#include  <string>
#include  <vector>
#include  <vrpn_Types.h>
#include  <vrpn_Connection.h>
#include  <vrpn_Imager.h>

#ifdef __APPLE__
#include <OPENGL/gl.h>
#else
#include  <GL/gl.h>
#endif

//----------------------------------------------------------------------------
// This class forms a basic wrapper for an image.  It treats an image as anything
// which can support requests on the number of pixels in an image and can
// perform queries by pixel coordinate in that image.  It is a set of functions
// that are needed by the spot_tracker applications.
// XXX If we template this class based on the type of pixel it has, we may be
// able to speed things up quite a bit for particular cases of interest.  Even
// better may be to implement functions that wrap the functions we need to
// have be fast (like writing to a GL_LUMINANCE OpenGL texture).

class image_wrapper {
public:

  image_wrapper() : _opengl_texture_size_x(0), _opengl_texture_size_y(0),
    _opengl_texture_have_written(false), _tex_id(65000) {};

  // Virtual destructor so that children can de-allocate space as needed.
  virtual ~image_wrapper() {};

  // Tell what the range is for the image.
  virtual void	read_range(int &minx, int &maxx, int &miny, int &maxy) const = 0;

  /// Return the number of colors that the image has
  virtual unsigned  get_num_colors() const = 0;
  //XXX These should return the maximum number of possible rows/columns,
  // like the ones in the camera server do.
  virtual unsigned  get_num_rows(void) const { 
    int	_minx, _maxx, _miny, _maxy;
    read_range(_minx, _maxx, _miny, _maxy);
    return _maxy-_miny+1;
  }
  virtual unsigned  get_num_columns(void) const {
    int	_minx, _maxx, _miny, _maxy;
    read_range(_minx, _maxx, _miny, _maxy);
    return _maxx-_minx+1;
  }

  /// Read a pixel from the image into a double; return true if the pixel
  // was in the image, false if it was not.
  virtual bool	read_pixel(int x, int y, double	&result, unsigned rgb = 0) const = 0;

  // Overloaded by result type to enable optimization later but use by any.
  virtual bool  read_pixel(int x, int y, vrpn_uint8 &result, unsigned rgb = 0) const
  { double double_pix;
    bool err = read_pixel(x,y, double_pix, rgb);
    result = static_cast<vrpn_uint8>(double_pix);
    return err;
  }
  virtual bool  read_pixel(int x, int y, vrpn_uint16 &result, unsigned rgb = 0) const
  { double double_pix;
    bool err = read_pixel(x,y, double_pix, rgb);
    result = static_cast<vrpn_uint16>(double_pix);
    return err;
  }

  /// Read a pixel from the image into a double; Don't check boundaries.
  virtual double read_pixel_nocheck(int x, int y, unsigned rgb = 0) const = 0;

  // Do bilinear interpolation to read from the image, in order to
  // smoothly interpolate between pixel values.
  // All sorts of speed tweaks in here because it is in the inner loop for
  // the spot tracker and other codes.
  // Return a result of zero and false if the coordinate its outside the image.
  // Return the correct interpolated result and true if the coordinate is inside.
  inline bool	read_pixel_bilerp(double x, double y, double &result, unsigned rgb = 0) const
  {
    result = 0;	// In case of failure.
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
    if (!read_pixel(ixlow, iylow, ll, rgb)) { return false; }
    if (!read_pixel(ixlow, iyhigh, lh, rgb)) { return false; }
    if (!read_pixel(ixhigh, iylow, hl, rgb)) { return false; }
    if (!read_pixel(ixhigh, iyhigh, hh, rgb)) { return false; }
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
  inline double read_pixel_bilerp_nocheck(double x, double y, unsigned rgb = 0) const
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
    return read_pixel_nocheck(ixlow, iylow, rgb) * xlowfrac * ylowfrac +
	   read_pixel_nocheck(ixlow, iyhigh, rgb) * xlowfrac * yhighfrac +
	   read_pixel_nocheck(ixhigh, iylow, rgb) * xhighfrac * ylowfrac +
	   read_pixel_nocheck(ixhigh, iyhigh, rgb) * xhighfrac * yhighfrac;
  };

  /// Store the memory image to a PPM file.
  virtual bool  write_to_ppm_file(const char *filename, int gain = 1, bool sixteen_bits = false) const {return false;};

  /// Store the portion of image in memory to a TIFF file (subclasses may override this to make it more efficient,
  // but we provide an instance in the C file).
  virtual bool  write_to_tiff_file(const char *filename, double scale = 1.0, double offset = 0.0, bool sixteen_bits = false,
    const char *magick_files_dir = "C:/nsrg/external/pc_win32/bin/ImageMagick-5.5.7-Q16/MAGIC_DIR_PATH", bool skip_init = false) const;

  /// Store the specified channel of the portion of image in memory to a grayscale TIFF file (subclasses may override
  // this to make it more efficient, but we provide an instance in the C file).
  virtual bool  write_to_grayscale_tiff_file(const char *filename, unsigned channel, double scale = 1.0, double offset = 0.0,
    bool sixteen_bits = false, const char *magick_files_dir = "C:/nsrg/external/pc_win32/bin/ImageMagick-5.5.7-Q16/MAGIC_DIR_PATH", bool skip_init = false) const;

  /// Send in-memory image over a vrpn connection.  Number of channels is 3 for RGB cameras, but 1 for scientific cameras.
  // The channels to be used must have been pre-allocated by the application; three contiguous named "red" "green" "blue"
  // for an RGB (or BGR) camera.
  virtual bool  send_vrpn_image(vrpn_Imager_Server* svr,vrpn_Connection* svrcon,double g_exposure,int svrchan, int num_chans = 1) {return false;};

  /// Write the in-memory image into an OpenGL texture and then texture-map it onto a quad.  The scale and offset are used to set OpenGL transfer settings.
  // The quad will have its vertex coordinates going from -1 to 1 in X and Y and lie at Z=0.  Its texture coordinates will be
  // whatever is needed to map all of the pixels into the quad.
  // The scale and offset are applied directly to the values themselves, which requires the caller to
  // know some magic about the camera for 12-in-16 cameras.
  // The type of texture (Luminance or RBG) can be determined by the particular camera driver
  // based on the type of the camera.  The pixel type (8-bit, float, 16-bit) can also be determined
  // by the camera to produce the most efficient copy of the data to the graphics card.
  // The quad is rendered upside-down to make the images the normal orientation in OpenGL;
  // this is because images are left-handed while OpenGL is right-handed.
  // Not const because we need to set _opengl_texture_size_x and _y.
  virtual bool write_to_opengl_quad(double scale = 1.0, double offset = 0.0);

  // Write the texture, using a virtual method call appropriate to the particular
  // camera type.
  virtual bool write_to_opengl_texture(GLuint tex_id) {return false;};

  // Write from the texture to a quad.  Write only the actually-filled
  // portion of the texture (parameters passed in).  This version does not
  // flip the quad over.  A derived class can flip as needed.
  virtual bool write_opengl_texture_to_quad();

protected:
  // These are used to keep track of the actual OpenGL texture size,
  // which must be an even power of two in each dimension.
  GLuint    _tex_id;
  unsigned  _opengl_texture_size_x;
  unsigned  _opengl_texture_size_y;
  unsigned  _opengl_texture_have_written;

  // Write the texture to OpenGL, where we've parameterized all of the
  // things we need to tell about the process.  Each derived class should
  // implement a specialized method (using the virtual template below) that
  // calls this one with appropriate parameters.  NOTE: At least the first time
  // this function is called,
  // we must write a complete texture, which may be larger than the actual bytes
  // allocated for the image.  After the first time, and if we don't change the
  // image size to be larger, we can use the subimage call to only write the
  // pixels we have.
  bool write_to_opengl_texture_generic(GLuint tex_id, GLint num_components,
          GLenum format, GLenum type, const GLvoid *buffer_base,
          const GLvoid *subset_base,
          unsigned minX, unsigned minY, unsigned maxX, unsigned maxY);
};

//----------------------------------------------------------------------------
// Image metric calculation functions.  These can be used as focus metrics on
// images (for example).

// Sum all the pixels for one color (defaults to the first) in an image.
double image_wrapper_sum(const image_wrapper &img, unsigned rgb = 0);

// Sum all the squared values of all pixels for one color
// (defaults to the first) in an image.
double image_wrapper_square_sum(const image_wrapper &img, unsigned rgb = 0);

//----------------------------------------------------------------------------
// Concrete version of above virtual base class that stores a single-color
// image in double-precision floating-point values.  Also includes methods for
// writing the pixel values.

class double_image: public image_wrapper {
public:
  double_image(int minx = 0, int maxx = 255, int miny = 0, int maxy = 255);
  ~double_image();

  // Tell what the range is for the image.
  virtual void	read_range(int &minx, int &maxx, int &miny, int &maxy) const;

  // Read a pixel from the image into a double; return true if the pixel
  // was in the image, false if it was not.
  using image_wrapper::read_pixel;
  virtual bool	read_pixel(int x, int y, double &result, unsigned ignored = 0) const;
  inline double read_pixel_nocheck(int x, int y, unsigned ignored = 0) const
  {
    return _image[(x-_minx) + (y-_miny)*(_maxx-_minx+1)];
  };

  /// Return the number of colors that the image has
  inline unsigned  get_num_colors() const { return 1; }

  // Write a pixel into the image; return true if the pixel was in the image,
  // false if it was not.
  inline bool	write_pixel(int x, int y, double value)
  {
    int index;
    if (find_index(x,y, index)) {
      _image[index] = value;
      return true;
    }
    // Didn't find it, return false.
    return false;
  };
  inline void  write_pixel_nocheck(int x, int y, double value)
  {
    _image[(x-_minx) + (y-_miny)*(_maxx-_minx+1)] = value;
  };

protected:
  int	  _minx, _maxx, _miny, _maxy;
  double  *_image;

  // Index the specified pixel, returning false if out of range
  inline  bool	find_index(int x, int y, int &index) const {
      if (_image == NULL) { return false; }
      if ( (x < _minx) || (x > _maxx) || (y < _miny) || (y > _maxy) ) {
	return false;
      }
      index = (x-_minx) + (y-_miny)*(_maxx-_minx+1);
      return true;
    }
};

//----------------------------------------------------------------------------
// Concrete version of above virtual base class that stores a single-color
// image in single-precision floating-point values.  Also includes methods for
// writing the pixel values.

class float_image: public image_wrapper {
public:
  float_image(int minx = 0, int maxx = 255, int miny = 0, int maxy = 255);
  ~float_image();

  // Tell what the range is for the image.
  virtual void	read_range(int &minx, int &maxx, int &miny, int &maxy) const;

  // Read a pixel from the image into a double; return true if the pixel
  // was in the image, false if it was not.
  using image_wrapper::read_pixel;
  virtual bool	read_pixel(int x, int y, double &result, unsigned ignored = 0) const;
  inline double read_pixel_nocheck(int x, int y, unsigned ignored = 0) const
  {
    return _image[(x-_minx) + (y-_miny)*(_maxx-_minx+1)];
  }

  /// Return the number of colors that the image has
  inline unsigned  get_num_colors() const { return 1; }

  // Write a pixel into the image; return true if the pixel was in the image,
  // false if it was not.
  inline bool	write_pixel(int x, int y, double value)
  {
    int index;
    if (find_index(x,y, index)) {
      _image[index] = static_cast<float>(value);
      return true;
    }
    // Didn't find it, return false.
    return false;
  };
  inline void  write_pixel_nocheck(int x, int y, double value)
  {
    _image[(x-_minx) + (y-_miny)*(_maxx-_minx+1)] = static_cast<float>(value);
  };

  // Write the texture, using a virtual method call appropriate to the particular
  // camera type.
  virtual bool write_to_opengl_texture(GLuint tex_id);

protected:
  int	  _minx, _maxx, _miny, _maxy;
  float  *_image;

  // Index the specified pixel, returning false if out of range
  inline  bool	find_index(int x, int y, int &index) const {
      if (_image == NULL) { return false; }
      if ( (x < _minx) || (x > _maxx) || (y < _miny) || (y > _maxy) ) {
	return false;
      }
      index = (x-_minx) + (y-_miny)*(_maxx-_minx+1);
      return true;
    }
};

//----------------------------------------------------------------------------
// Concrete version of above virtual base class that creates itself by copying
// from an existing image.

class copy_of_image: public image_wrapper {
public:
  copy_of_image(const image_wrapper &copyfrom);
  ~copy_of_image();

  // Override the default copy constructor so that it calls the above, deep copy.
  copy_of_image(const copy_of_image &copyfrom);

  // Tell what the range is for the image.
  virtual void read_range(int &minx, int &maxx, int &miny, int &maxy) const {
    minx = _minx; miny = _miny; maxx = _maxx; maxy = _maxy;
  }

  /// Return the number of colors that the image has
  virtual unsigned  get_num_colors() const { return _numcolors; };

  /// Read a pixel from the image into a double; return true if the pixel
  // was in the image, false if it was not.
  using image_wrapper::read_pixel;
  virtual bool	read_pixel(int x, int y, double	&result, unsigned rgb = 0) const;

  /// Read a pixel from the image into a double; Don't check boundaries.
  virtual double read_pixel_nocheck(int x, int y, unsigned rgb = 0) const;

  /// Copy new values from the image that is passed in, reallocating if needed
  void	operator= (const image_wrapper &copyfrom);

  // Override default copy behavior to avoid shallow copy
  const copy_of_image &operator= (const copy_of_image &copyfrom) { *this = (const image_wrapper &)copyfrom; return *this; }

protected:
  int _minx, _maxx, _miny, _maxy;   //< Coordinates for the pixels (copied from other image)
  int _numx, _numy;		    //< Calculated based on the above min/max values
  int _numcolors;		    //< How many colors do we have
  double  *_image;		    //< Holds the values copied from the other image

  inline int index(int x, int y, unsigned rgb) const {
    int xindex = x - _minx;
    int yindex = y - _miny;
    return rgb + get_num_colors() * (xindex + _numx * yindex);
  };
};

//----------------------------------------------------------------------------
// Concrete version of above virtual base class that creates itself by subsetting
// an existing image.  It has a smaller range than the image but is otherwise
// identical.

class cropped_image: public image_wrapper {
public:
  cropped_image(const image_wrapper &reference_image, int minx, int miny, int maxx, int maxy) :
      _ref(reference_image), _minx(minx), _miny(miny), _maxx(maxx), _maxy(maxy) {
	int rminx, rmaxx, rminy, rmaxy;
	_ref.read_range(rminx, rmaxx, rminy, rmaxy);

	// If the maxes are greater than the mins, set them to the size of
	// the image.
	if (_maxx < _minx) {
	  _minx = rminx; _maxx = rmaxx;
	}
	if (_maxy < _miny) {
	  _miny = rminy; _maxy = rmaxy;
	}

	// Clip to the boundaries of the cropped image.
	if (_minx < rminx) { _minx = rminx; }
	if (_maxx > rmaxx) { _maxx = rmaxx; }
	if (_miny < rminy) { _minx = rminy; }
	if (_maxy > rmaxy) { _maxy = rmaxy; }
  }

  // Tell what the range is for the image.
  virtual void read_range(int &minx, int &maxx, int &miny, int &maxy) const {
    minx = _minx; miny = _miny; maxx = _maxx; maxy = _maxy;
  }

  /// Return the number of colors that the image has
  virtual unsigned  get_num_colors() const { return _ref.get_num_colors(); }

  /// Read a pixel from the image into a double; return true if the pixel
  // was in the image, false if it was not.
  using image_wrapper::read_pixel;
  virtual bool	read_pixel(int x, int y, double	&result, unsigned rgb = 0) const {
    return _ref.read_pixel(x,y, result, rgb);
  }

  /// Read a pixel from the image into a double; Don't check boundaries.
  virtual double read_pixel_nocheck(int x, int y, unsigned rgb = 0) const {
    return _ref.read_pixel_nocheck(x, y, rgb);
  }

protected:
  const	image_wrapper &_ref;	    //< Image to get pixels from
  int _minx, _maxx, _miny, _maxy;   //< Coordinates for the pixels (copied from other image)
};

//----------------------------------------------------------------------------
// Concrete version of above virtual base class that applies a translation
// to the pixel coordinates for another image and lets you resample it.  It does a
// bilinear interpolation on the pixel coordinates to get the values from the
// other image.  It is a faster implementation than the more general affine_transformed
// image defined below.

class translated_image: public image_wrapper {
public:
  translated_image(const image_wrapper &reference_image, double dx, double dy) :
    _ref(reference_image), _dx(dx), _dy(dy) { };
  virtual ~translated_image() {};

  // Tell what the range is for the image.  XXX Should be offset.
  virtual void	read_range(int &minx, int &maxx, int &miny, int &maxy) const
  {
    _ref.read_range(minx, maxx, miny, maxy);
  }

  /// Return the number of colors that the image has
  virtual unsigned  get_num_colors() const { return _ref.get_num_colors(); }

  /// Read a pixel from the image into a double; return true if the pixel
  // was in the image, false if it was not.
  using image_wrapper::read_pixel;
  virtual bool	read_pixel(int x, int y, double	&result, unsigned rgb = 0) const
  {
    return _ref.read_pixel_bilerp(newx(x,y), newy(x,y), result, rgb);
  }

  /// Read a pixel from the image into a double; Don't check boundaries.
  virtual double read_pixel_nocheck(int x, int y, unsigned rgb = 0) const
  {
    return _ref.read_pixel_bilerp_nocheck(newx(x,y), newy(x,y), rgb);
  }

protected:
  const	image_wrapper &_ref;	    //< Image to get pixels from
  double  _dx, _dy;		    //< Transform to apply to coordinates

  // These compute the new coordinates from the old coordinates.
  inline  double  newx(double x, double y) const { return x + _dx; }
  inline  double  newy(double x, double y) const { return y + _dy; }
};

//----------------------------------------------------------------------------
// Concrete version of above virtual base class that applies a translation,
// rotation, and uniform scale to the pixel coordinates for another image
// and lets you resample it.  The rotation and scale are performed around
// a specified location in the coordinates of the affine_transform image,
// then the translation is applied in this rotated and scaled space.
// It does a bilinear interpolation on the pixel
// coordinates to get the values from the other image.  Rotation is in
// radians, and a positive rotation rotates the +X axis into the +Y axis.

class affine_transformed_image: public image_wrapper {
public:
  affine_transformed_image(const image_wrapper &reference_image,
			   double dx, double dy,
			   double rotradians, double scale,
			   double centerx = 0, double centery = 0) :
    _ref(reference_image), _dx(dx), _dy(dy), _rot(rotradians), _scale(scale),
    _centerx(centerx), _centery(centery)
  {
      _sinrot = sin(_rot);
      _cosrot = cos(_rot);
  };
  virtual ~affine_transformed_image() {};

  // Tell what the range is for the image.  XXX Not clear what this means for
  // a transformed image.
  virtual void	read_range(int &minx, int &maxx, int &miny, int &maxy) const
  {
    _ref.read_range(minx, maxx, miny, maxy);
  }

  /// Return the number of colors that the image has
  virtual unsigned  get_num_colors() const { return _ref.get_num_colors(); }

  /// Read a pixel from the image into a double; return true if the pixel
  // was in the image, false if it was not.
  using image_wrapper::read_pixel;
  virtual bool	read_pixel(int x, int y, double	&result, unsigned rgb = 0) const
  {
    double x_new, y_new;
    newxy(x,y, x_new, y_new);
    return _ref.read_pixel_bilerp(x_new, y_new, result, rgb);
  }

  /// Read a pixel from the image into a double; Don't check boundaries.
  virtual double read_pixel_nocheck(int x, int y, unsigned rgb = 0) const
  {
    double x_new, y_new;
    newxy(x,y, x_new, y_new);
    return _ref.read_pixel_bilerp_nocheck(x_new, y_new, rgb);
  }

protected:
  const	image_wrapper &_ref;	    //< Image to get pixels from
  double  _dx, _dy;		    //< Transform to apply to coordinates
  double  _rot;			    //< Rotation in radians to apply to around translated center
  double  _scale;		    //< Scale factor to apply at translated center.
  double  _centerx;		    //< Center of scaling and rotation
  double  _centery;		    //< Center of scaling and rotation
  double  _sinrot;		    //< Sine of the rotation angle
  double  _cosrot;		    //< Cosine of the rotation angle

  // Calculate the direction and length of a step of (x,y) along the
  // rotated and scaled X axis.
  inline double _scale_and_rotate_x_step(double x, double y) const {
    return _scale * (_cosrot*x - _sinrot*y);
  }

  // Calculate the direction and length of a step of (x,y) along the
  // rotated and scaled Y axis.
  inline double _scale_and_rotate_y_step(double x, double y) const {
    return _scale * (_sinrot*x + _cosrot*y);
  }

  // These compute the new coordinates from the old coordinates.
  inline  void  newxy(double x, double y, double &x_new, double &y_new) const {
    // Move the center of rotation and scaling to the origin
    double x_centered = x - _centerx;
    double y_centered = y - _centery;

    // Figure out the center-relative location of the desired point,
    // offset by its translation
    double x_scaled_rot = _scale_and_rotate_x_step(x_centered + _dx, y_centered + _dy);
    double y_scaled_rot = _scale_and_rotate_y_step(x_centered + _dx, y_centered + _dy);

    // Put the center of rotation and scaling back where it started
    x_new = x_scaled_rot + _centerx;
    y_new = y_scaled_rot + _centery;
  }
};

//----------------------------------------------------------------------------
// Concrete version of above virtual base class that creates itself by subtracting
// two images and adding an offset.  New = first - second + offset;

class subtracted_image: public image_wrapper {
public:
  subtracted_image(const image_wrapper &first, const image_wrapper &second, const double offset);
  ~subtracted_image();

  // Tell what the range is for the image.
  virtual void read_range(int &minx, int &maxx, int &miny, int &maxy) const {
    minx = _minx; miny = _miny; maxx = _maxx; maxy = _maxy;
  }

  /// Return the number of colors that the image has
  virtual unsigned  get_num_colors() const { return _numcolors; };

  /// Read a pixel from the image into a double; return true if the pixel
  // was in the image, false if it was not.
  using image_wrapper::read_pixel;
  virtual bool	read_pixel(int x, int y, double	&result, unsigned rgb = 0) const;

  /// Read a pixel from the image into a double; Don't check boundaries.
  virtual double read_pixel_nocheck(int x, int y, unsigned rgb = 0) const;

  // Write the texture, using a virtual method call appropriate to the particular
  // camera type.
  virtual bool write_to_opengl_texture(GLuint tex_id);

protected:
  int _minx, _maxx, _miny, _maxy;   //< Coordinates for the pixels (copied from other image)
  int _numx, _numy;		    //< Calculated based on the above min/max values
  int _numcolors;		    //< How many colors do we have
  float  *_image;		    //< Holds the values

  inline int index(int x, int y, unsigned rgb) const {
    int xindex = x - _minx;
    int yindex = y - _miny;
    return rgb + get_num_colors() * (xindex + _numx * yindex);
  };
};

//----------------------------------------------------------------------------
// Concrete version of above virtual base class that creates itself by averaging
// two images.  New = (first + second) / 2.  You could also use the mean_image
// class, below, and give it two images to compute the same metric.

class averaged_image: public image_wrapper {
public:
  averaged_image(const image_wrapper &first, const image_wrapper &second);
  ~averaged_image();

  // Tell what the range is for the image.
  virtual void read_range(int &minx, int &maxx, int &miny, int &maxy) const {
    minx = _minx; miny = _miny; maxx = _maxx; maxy = _maxy;
  }

  /// Return the number of colors that the image has
  virtual unsigned  get_num_colors() const { return _numcolors; };

  /// Read a pixel from the image into a double; return true if the pixel
  // was in the image, false if it was not.
  using image_wrapper::read_pixel;
  virtual bool	read_pixel(int x, int y, double	&result, unsigned rgb = 0) const;

  /// Read a pixel from the image into a double; Don't check boundaries.
  virtual double read_pixel_nocheck(int x, int y, unsigned rgb = 0) const;

protected:
  int _minx, _maxx, _miny, _maxy;   //< Coordinates for the pixels (copied from other image)
  int _numx, _numy;		    //< Calculated based on the above min/max values
  int _numcolors;		    //< How many colors do we have
  double  *_image;		    //< Holds the values copied from the other image

  inline int index(int x, int y, unsigned rgb) const {
    int xindex = x - _minx;
    int yindex = y - _miny;
    return rgb + get_num_colors() * (xindex + _numx * yindex);
  };
};

//----------------------------------------------------------------------------
// Image statistic calculator base class, derived from above.  Provides the
// interface needed for operators that take in a bunch of images and produce
// an image that is some average or other metric on the set of images.
// The specific metric classes are found below, derived from this class.

class image_metric: public image_wrapper {
public:

  /// Copies the size, creates buffer, and copies the image into
  // the buffer.
  image_metric(const image_wrapper &copyfrom);
  ~image_metric();

  // Add another image to those being used.
  virtual void operator+= (const image_wrapper &newimage) = 0;

  // Tell what the range is for the image.
  virtual void read_range(int &minx, int &maxx, int &miny, int &maxy) const {
    minx = _minx; miny = _miny; maxx = _maxx; maxy = _maxy;
  }

  /// Return the number of colors that the image has
  virtual unsigned  get_num_colors() const { return _numcolors; };

  /// Read a pixel from the image into a double; return true if the pixel
  // was in the image, false if it was not.
  using image_wrapper::read_pixel;
  virtual bool	read_pixel(int x, int y, double	&result, unsigned rgb = 0) const;

  /// Read a pixel from the image into a double; Don't check boundaries.
  virtual double read_pixel_nocheck(int x, int y, unsigned rgb = 0) const;

protected:
  int _minx, _maxx, _miny, _maxy;   //< Coordinates for the pixels (copied from other image)
  int _numx, _numy;		    //< Calculated based on the above min/max values
  int _numcolors;		    //< How many colors do we have
  double  *_image;		    //< Holds the values copied from the other image

  inline int index(int x, int y, unsigned rgb) const {
    int xindex = x - _minx;
    int yindex = y - _miny;
    return rgb + get_num_colors() * (xindex + _numx * yindex);
  };
};

//----------------------------------------------------------------------------
// Concrete version of the image metric that computes the minimum of all
// images that are added to it.

class minimum_image: public image_metric {
public:
  minimum_image(const image_wrapper &copyfrom) : image_metric(copyfrom) {};

  // Take the minimum of the existing image and this new image
  virtual void operator+= (const image_wrapper &newimage);
};

//----------------------------------------------------------------------------
// Concrete version of the image metric that computes the maximum of all
// images that are added to it.

class maximum_image: public image_metric {
public:
  maximum_image(const image_wrapper &copyfrom) : image_metric(copyfrom) {};

  // Take the maximum of the existing image and this new image
  virtual void operator+= (const image_wrapper &newimage);
};

//----------------------------------------------------------------------------
// Concrete version of the image metric that computes the mean of all
// images that are added to it.

class mean_image: public image_metric {
public:
  mean_image(const image_wrapper &copyfrom) : image_metric(copyfrom), d_num_images(1) {};

  // Take the mean of the existing image and this new image
  virtual void operator+= (const image_wrapper &newimage);

  /// Read a pixel from the image into a double; return true if the pixel
  // was in the image, false if it was not.  Takes the mean by dividing by
  // the number of images read.
  using image_wrapper::read_pixel;
  virtual bool	read_pixel(int x, int y, double	&result, unsigned rgb = 0) const;

  /// Read a pixel from the image into a double; Don't check boundaries.
  // Takes the mean by dividing by the number of images read.
  virtual double read_pixel_nocheck(int x, int y, unsigned rgb = 0) const;

protected:
  double    d_num_images;   //< Number of images added; stored in double for math speed
};


//----------------------------------------------------------------------------
// This class forms a basic wrapper for a camera.  It treats an camera as anything
// which can do what is needed for an imager and also includes functions for
// reading images into memory from the camera.

class base_camera_server : public image_wrapper {
public:
  virtual ~base_camera_server(void) {};

  /// Is the camera working properly?
  bool working(void) const { return _status; };

  // These functions should be used to determine the stride in the
  // image when skipping lines.  They are in terms of the full-screen
  // number of pixels with the current binning level.
  unsigned  get_num_rows(void) const { return _num_rows / _binning; };
  unsigned  get_num_columns(void) const { return _num_columns / _binning; };

  /// Read an image to a memory buffer.  Max < min means "whole range".
  /// Setting binning > 1 packs more camera pixels into each image pixel, so
  /// the maximum number of pixels has to be divided by the binning constant
  /// (the effective number of pixels is lower and pixel coordinates are set
  /// in this reduced-count pixel space).  This routine returns false if a
  /// new image could not be read (for example, because of a timeout on
  /// the reading because we're at the end of a video stream).
  virtual bool	read_image_to_memory(unsigned minX = 255, unsigned maxX = 0,
			     unsigned minY = 255, unsigned maxY = 0,
			     double exposure_time = 250.0) = 0;

  /// Get pixels out of the memory buffer, RGB indexes the colors
  virtual bool	get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint8 &val, int RGB = 0) const = 0;
  virtual bool	get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint16 &val, int RGB = 0) const = 0;

  // Makes the read routines in the base class faster by calling the above methods.
  virtual bool  read_pixel(int x, int y, vrpn_uint8 &result, unsigned rgb = 0) const
  {
    return get_pixel_from_memory(x,y, result, rgb);
  }
  virtual bool  read_pixel(int x, int y, vrpn_uint16 &result, unsigned rgb = 0) const
  {
    return get_pixel_from_memory(x,y, result, rgb);
  }

  /// Read a pixel from the image into a double; return true if the pixel
  // was in the image, false if it was not.
  virtual bool	read_pixel(int x, int y, double	&result, unsigned rgb = 0) const
  { vrpn_uint16 val = 0;
    result = 0.0; // Until we get a better one
    if (get_pixel_from_memory(x, y, val, rgb)) {
      result = val; return true;
    } else { return false; }
  };

  /// Read a pixel from the image into a double; Don't check boundaries.
  virtual double read_pixel_nocheck(int x, int y, unsigned rgb = 0) const
  {
    vrpn_uint16  val = 0;
    get_pixel_from_memory(x, y, val, rgb);
    return val;
  };

  /// Instantiation needed for image_wrapper
  virtual void read_range(int &minx, int &maxx, int &miny, int &maxy) const {
    minx = _minX; miny = _minY; maxx = _maxX; maxy = _maxY;
  }

  // Require all cameras to implement this function.
  virtual bool write_to_opengl_texture(GLuint tex_id) = 0;

protected:
  bool	    _status;			//< True is working, false is not
  unsigned  _num_rows, _num_columns;    //< Size of the memory buffer
  unsigned  _minX, _minY, _maxX, _maxY; //< Region of the image in memory
  unsigned  _binning;			//< How many camera pixels compressed into image pixel

  virtual bool	open_and_find_parameters(void) {return false;};
  base_camera_server(unsigned binning = 1) {
    _binning = binning;
    if (_binning < 1) { _binning = 1; }
  };
};

// This class will compute an object's spread function (for a point, this
// is called a Point-Spread Function (PSF)).  It assumes that a X,Y position
// is tracked so that it remains in the center of the object over depth.
// It radially averages around this center to produce a line of pixels at
// each frame.  If the frames are equally-spaced in space from lower to
// higher, then this will produce an image where the center of the bead is
// along the left-hand side of the output image and the lowest plane in the
// stack shows up at the bottom of the image.

class PSF_File : public image_wrapper {
public:

  /// Pass the name of the file to write to, the radius to average over, and whether
  // to save the file as a 16-bits-per-pixel file (8-bit if this is false).
  PSF_File(const char *filename, const unsigned radius, const bool sixteen_bits) :
      d_filename(filename), d_radius(radius), d_sixteen_bits(sixteen_bits) {};

  /// The class stores the file when the object is destroyed.
  ~PSF_File();

  // Append a line to the ones waiting to go, calculating it radially from the
  // specified location on the specified image.
  bool	append_line(const image_wrapper &image, const double x, const double y);

  //-------- image_wrapper methods ----------------
  // Tell what the range is for the image.
  virtual void	read_range(int &minx, int &maxx, int &miny, int &maxy) const
    { minx = 0; maxx = d_radius; miny = 0; maxy = static_cast<int>(d_lines.size()) - 1; }

  /// Return the number of colors that the image has
  virtual unsigned  get_num_colors() const {return 1; }

  /// Read a pixel from the image into a double; return true if the pixel
  // was in the image, false if it was not.  Assumes a stack taken bottom-to-top,
  // so that the bottom line in the image (when drawn normally) is at the bottom.
  using image_wrapper::read_pixel;
  virtual bool	read_pixel(int x, int y, double	&result, unsigned rgb = 0) const
    {
      result = 0;
      // Ignore RGB -- return greyscale image
      if ( (x < 0) || (y < 0) || (x > static_cast<int>(d_radius)) || (y >= static_cast<int>(d_lines.size())) ) {
	return false;
      }
      result = d_lines[y][x];
      return true;
    }

  /// Read a pixel from the image into a double; Don't check boundaries.
  virtual double read_pixel_nocheck(int x, int y, unsigned rgb = 0) const
    { return d_lines[y][x]; }

protected:
  unsigned	      d_radius;		//< The radius around which to average
  std::string	      d_filename;	//< Name of the file to write to
  std::vector<double *>    d_lines;	//< Lines read from the images
  bool		      d_sixteen_bits;	//< Store a 16-bit output file?
};

#endif
