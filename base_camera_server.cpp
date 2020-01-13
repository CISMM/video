// To be able to read 16-bit images, we need to use a version of ImageMagick
// that is compiled with -DQuantumDepth=16.  Then, we need to #define QuantumLeap
// before includeing magick/api.h.
// We don't use ImageMagick at all if we're using the MinGW compiler set, because
// we can't link against its X11 library.

#if defined(__MINGW32__) || !defined(VST_USE_IMAGEMAGICK)
#define VIDEO_NO_IMAGEMAGICK
#endif

#include <math.h>
#include <stdio.h>
#include "base_camera_server.h"

#ifndef	M_PI
#ifndef M_PI_DEFINED
const double M_PI = 2*asin(1.0);
#define M_PI_DEFINED
#endif
#endif

#if !defined(VIDEO_NO_IMAGEMAGICK)
  #define QuantumLeap
  #if !defined(_VISUALC_)
    #include <magick/magick-config.h>
  #endif

  // When statically linking with ImageMagick, you need to define _LIB here or
  // in the preprocessor so it knows to link correctly.
  // Then you also need to link with wsock32.lib.
  #define _LIB
  #include <magick/api.h>
#endif

// Sum all the pixels for one color (defaults to the first) in an image.
double image_wrapper_sum(const image_wrapper &img, unsigned rgb)
{
	double sum = 0;
	int minx, maxx, miny, maxy;
	img.read_range(minx, maxx, miny, maxy);
	int x,y;
	for (x = minx; x <= maxx; x++) {
		for (y = miny; y <= maxy; y++) {
			sum += img.read_pixel_nocheck(x, y, rgb);
		}
	}
	return sum;
}

// Sum all the squared values of all pixels for one color
// (defaults to the first) in an image.
double image_wrapper_square_sum(const image_wrapper &img, unsigned rgb)
{
	double sum = 0;
	int minx, maxx, miny, maxy;
	img.read_range(minx, maxx, miny, maxy);
	int x,y;
	for (x = minx; x <= maxx; x++) {
		for (y = miny; y <= maxy; y++) {
			double val = img.read_pixel_nocheck(x, y, rgb);
			sum += val*val;
		}
	}
	return sum;
}

// Apply the specified gain to the value, clamp to zero and the maximum
// possible value, and return the result.  We always clamp to 65535 rather
// than 255 because the high-order byte is used when writing an 8-bit
// file.
static	vrpn_uint16 offset_scale_and_clamp(const double value, const double gain, const double offset)
{
  double result = gain * (value + offset);
  if (result < 0) { result = 0; }
  if (result > 65535) { result = 65535; }
  return (vrpn_uint16)(result);
}

/// Store the portion of the image that is in memory to a TIFF file.
bool  image_wrapper::write_to_tiff_file(const char *filename, double scale, double offset, bool sixteen_bits,
				        const char *magick_files_dir, bool skip_init) const
{
  // Make a pointer to the image data adjusted by the scale and offset.
  vrpn_uint16	  *gain_buffer = NULL;
  int		  r,c;
  int minx, maxx, miny, maxy, numcols, numrows;
  read_range(minx, maxx, miny, maxy);
  numcols = maxx-minx+1;
  numrows = maxy-miny+1;
  gain_buffer = new vrpn_uint16[numcols * numrows * 3];
  if (gain_buffer == NULL) {
      fprintf(stderr, "image_wrapper::write_memory_to_tiff_file(): Out of memory\n");
      return false;
  }
  // Flip the row values around so that the orientation matches the order expected by ImageMagick.
  // Go ahead and let it sample outside the image, filling in black there.
  // Make sure to flip on the output, not on the pixel read, because the pixel read
  // may have other hidden transforms in there.
  for (r = 0; r < numrows; r++) {
    for (c = 0; c < numcols; c++) {
      int flip_r = (numrows - 1) - r;
      double  value;
      read_pixel(minx + c, miny + r, value, 0);
      gain_buffer[0 + 3*(c + flip_r * numcols)] = offset_scale_and_clamp(value, scale, offset);
      read_pixel(minx + c, miny + r, value, 1);
      gain_buffer[1 + 3*(c + flip_r * numcols)] = offset_scale_and_clamp(value, scale, offset);
      read_pixel(minx + c, miny + r, value, 2);
      gain_buffer[2 + 3*(c + flip_r * numcols)] = offset_scale_and_clamp(value, scale, offset);
    }
  }
#if !defined(VIDEO_NO_IMAGEMAGICK)

#ifdef	_WIN32
  static bool initialized = false;
  if (!initialized && !skip_init) {
    InitializeMagick(magick_files_dir);
    initialized = true;
  }
#endif

  ExceptionInfo	  exception;
  Image		  *out_image;
  ImageInfo       *out_image_info;

  //Initialize the image info structure.
  GetExceptionInfo(&exception);
  out_image=ConstituteImage(numcols, numrows, "RGB", ShortPixel, gain_buffer, &exception);
  if (out_image == (Image *) NULL) {
      // print out something to let us know we are missing the 
      // delegates.mgk or whatever if that is the problem instead of just
      // saying the file can't be written later
      fprintf(stderr, "image_wrapper::write_memory_to_tiff_file(): Can't make out image: %s: %s\n",
             exception.reason,exception.description);
      return false;
  }
  out_image_info=CloneImageInfo((ImageInfo *) NULL);
  if (sixteen_bits) {
    out_image_info->depth = 16;
    out_image->depth = 16;
  } else {
    out_image_info->depth = 8;
    out_image->depth = 8;
  }
  strcpy(out_image_info->filename, filename);
  strcpy(out_image->filename, filename);
  if (WriteImage(out_image_info, out_image) == 0) {
      // print out something to let us know we are missing the 
      // delegates.mgk or whatever if that is the problem instead of just
      // saying the file can't be written later
      fprintf(stderr, "image_wrapper::write_memory_to_tiff_file(): WriteImage failed: %s: %s\n",
             exception.reason,exception.description);
      return false;
  }
  DestroyImageInfo(out_image_info);
  DestroyImage(out_image);
  delete [] gain_buffer;
  return true;
#else
  fprintf(stderr,"image_wrapper::write_to_tiff_file(): No ImageMagick implemented\n");
  return false;
#endif
}

/// Store the specified channel of the portion of the image that is in memory to a TIFF file.
bool  image_wrapper::write_to_grayscale_tiff_file(const char *filename, unsigned channel, double scale, double offset,
				                  bool sixteen_bits, const char *magick_files_dir, bool skip_init) const
{
  if (channel > 2) {
      fprintf(stderr, "image_wrapper::write_to_grayscale_tiff_file(): Invalid channel (%d)\n", channel);
      return false;
  }

  // Make a pointer to the image data adjusted by the scale and offset.
  vrpn_uint16	  *gain_buffer = NULL;
  int		  r,c;
  int minx, maxx, miny, maxy, numcols, numrows;
  read_range(minx, maxx, miny, maxy);
  numcols = maxx-minx+1;
  numrows = maxy-miny+1;
  gain_buffer = new vrpn_uint16[numcols * numrows];
  if (gain_buffer == NULL) {
      fprintf(stderr, "image_wrapper::write_to_grayscale_tiff_file(): Out of memory\n");
      return false;
  }
  // Flip the row values around so that the orientation matches the order expected by ImageMagick.
  // Go ahead and let it sample outside the image, filling in black there.
  // Make sure to flip on the output, not on the pixel read, because the pixel read
  // may have other hidden transforms in there.
  for (r = 0; r < numrows; r++) {
    for (c = 0; c < numcols; c++) {
      int flip_r = (numrows - 1) - r;
      double  value;
      read_pixel(minx + c, miny + r, value, channel);
      gain_buffer[c + flip_r * numcols] = offset_scale_and_clamp(value, scale, offset);
    }
  }

#if !defined(VIDEO_NO_IMAGEMAGICK)

#ifdef	_WIN32
  static bool initialized = false;
  if (!initialized && !skip_init) {
    InitializeMagick(magick_files_dir);
    initialized = true;
  }
#endif

  ExceptionInfo	  exception;
  Image		  *out_image;
  ImageInfo       *out_image_info;

  //Initialize the image info structure.
  GetExceptionInfo(&exception);
  out_image=ConstituteImage(numcols, numrows, "I", ShortPixel, gain_buffer, &exception);
  if (out_image == (Image *) NULL) {
      // print out something to let us know we are missing the 
      // delegates.mgk or whatever if that is the problem instead of just
      // saying the file can't be written later
      fprintf(stderr, "image_wrapper::write_to_grayscale_tiff_file(): Can't make out image: %s: %s\n",
             exception.reason,exception.description);
      return false;
  }
  out_image_info=CloneImageInfo((ImageInfo *) NULL);
  if (sixteen_bits) {
    out_image_info->depth = 16;
    out_image->depth = 16;
  } else {
    out_image_info->depth = 8;
    out_image->depth = 8;
  }
  strcpy(out_image_info->filename, filename);
  strcpy(out_image->filename, filename);
  if (WriteImage(out_image_info, out_image) == 0) {
      // print out something to let us know we are missing the 
      // delegates.mgk or whatever if that is the problem instead of just
      // saying the file can't be written later
      fprintf(stderr, "image_wrapper::write_to_grayscale_tiff_file(): WriteImage failed: %s: %s\n",
             exception.reason,exception.description);
      return false;
  }
  DestroyImageInfo(out_image_info);
  DestroyImage(out_image);
  delete [] gain_buffer;
  return true;
#else
  fprintf(stderr,"image_wrapper::write_to_grayscale_tiff_file(): No ImageMagick implemented\n");
  return false;
#endif
}

double_image::double_image(int minx, int maxx, int miny, int maxy) :
  _minx(minx), _maxx(maxx), _miny(miny), _maxy(maxy),
  _image(NULL)
{
  // Make sure the parameters are meaningful
  if ( (_minx >= _maxx) || (_miny >= _maxy) ) {
    fprintf(stderr,"double_image::double_image(): Bad min/max coordinates (%d,%d; %d,%d)\n",
      _minx, _miny, _maxx, _maxy);
    _minx = _maxy = _minx = _maxx = 0;
    return;
  }

  // Try to allocate a large enough array to hold all of the values.
  if ( (_image = new double[(_maxx-_minx+1) * (_maxy-_miny+1)]) == NULL) {
    fprintf(stderr,"double_image::double_image(): Out of memory\n");
    _minx = _maxy = _minx = _maxx = 0;
    return;
  }
}

double_image::~double_image()
{
  if (_image != NULL) {
    delete [] _image;
    _image = NULL;
  }
}

void double_image::read_range(int &minx, int &maxx, int &miny, int &maxy) const
{
  minx = _minx; maxx = _maxx; miny = _miny; maxy = _maxy;
};

// Read a pixel from the image into a double; return true if the pixel
// was in the image, false if it was not.
bool double_image::read_pixel(int x, int y, double &result, unsigned /* RGB ignored */) const
{
  int index;
  if (find_index(x,y, index)) {
    result = _image[index];
    return true;
  }
  // Didn't find it, return false.
  return false;
}

float_image::float_image(int minx, int maxx, int miny, int maxy) :
  _minx(minx), _maxx(maxx), _miny(miny), _maxy(maxy),
  _image(NULL)
{
  // Make sure the parameters are meaningful
  if ( (_minx >= _maxx) || (_miny >= _maxy) ) {
    fprintf(stderr,"float_image::float_image(): Bad min/max coordinates (%d,%d; %d,%d)\n",
      _minx, _miny, _maxx, _maxy);
    _minx = _maxy = _minx = _maxx = 0;
    return;
  }

  // Try to allocate a large enough array to hold all of the values.
  if ( (_image = new float[(_maxx-_minx+1) * (_maxy-_miny+1)]) == NULL) {
    fprintf(stderr,"float_image::float_image(): Out of memory\n");
    _minx = _maxy = _minx = _maxx = 0;
    return;
  }
}

float_image::~float_image()
{
  if (_image != NULL) {
    delete [] _image;
    _image = NULL;
  }
}

void float_image::read_range(int &minx, int &maxx, int &miny, int &maxy) const
{
  minx = _minx; maxx = _maxx; miny = _miny; maxy = _maxy;
};

// Read a pixel from the image into a double; return true if the pixel
// was in the image, false if it was not.
bool float_image::read_pixel(int x, int y, double &result, unsigned /* RGB ignored */) const
{
  int index;
  if (find_index(x,y, index)) {
    result = _image[index];
    return true;
  }
  // Didn't find it, return false.
  return false;
}

// Write the texture, using a virtual method call appropriate to the particular
// camera type.
bool float_image::write_to_opengl_texture(GLuint tex_id)
{
  const GLint   NUM_COMPONENTS = 1;
  const GLenum  FORMAT = GL_LUMINANCE;
  const GLenum  TYPE = GL_FLOAT;
  const void*   BASE_BUFFER = _image;
  const void*   SUBSET_BUFFER = &_image[NUM_COMPONENTS * ( _minx + get_num_columns()*_miny )];
  return write_to_opengl_texture_generic(tex_id, NUM_COMPONENTS, FORMAT, TYPE,
    BASE_BUFFER, SUBSET_BUFFER, _minx, _miny, _maxx, _maxy);
}


copy_of_image::copy_of_image(const image_wrapper &copyfrom) :
  _minx(-1), _maxx(-1), _miny(-1), _maxy(-1),
  _numx(-1), _numy(-1), _image(NULL), _numcolors(0)
{
  *this = copyfrom;
}

copy_of_image::copy_of_image(const copy_of_image &copyfrom) :
  _minx(-1), _maxx(-1), _miny(-1), _maxy(-1),
  _numx(-1), _numy(-1), _image(NULL), _numcolors(0)
{
  *this = (const image_wrapper &)copyfrom;
}

void copy_of_image::operator=(const image_wrapper &copyfrom)
{
  // If the dimensions don't match, then get a new image buffer
  int minx, miny, maxx, maxy;
  copyfrom.read_range(minx, maxx, miny, maxy);
  if ( (minx != _minx) || (maxx != _maxx) || (miny != _miny) || (maxy != _maxy) ||
       (get_num_colors() != copyfrom.get_num_colors()) ) {
    if (_image != NULL) { delete [] _image; _image = NULL; }
    _minx = minx; _maxx = maxx; _miny = miny; _maxy = maxy;
    _numx = (_maxx - _minx) + 1;
    _numy = (_maxy - _miny) + 1;
    _numcolors = copyfrom.get_num_colors();
    _image = new double[_numx * _numy * get_num_colors()];
    if (_image == NULL) {
      _numx = _numy = _minx = _maxx = _miny = _maxy = _numcolors = 0;
      return;
    }
  }

  // Copy the values from the image
  int x, y;
  unsigned c;
  for (x = _minx; x <= _maxx; x++) {
    for (y = _miny; y <= _maxy; y++) {
      for (c = 0; c < get_num_colors(); c++) {
	double val;
	copyfrom.read_pixel(x, y, val, c);  // Ignore result outside of image.
	_image[index(x, y, c)] = val;
      }
    }
  }
}

copy_of_image::~copy_of_image()
{
  if (_image) {
    delete [] _image; _image = NULL;
  }
}

bool  copy_of_image::read_pixel(int x, int y, double &result, unsigned rgb) const
{
  if ( (_image == NULL) || (x < _minx) || (x > _maxx) || (y < _miny) || (y > _maxy) ) {
    result = 0.0;
    return false;
  }
  result = _image[index(x, y, rgb)];
  return true;
}

double	copy_of_image::read_pixel_nocheck(int x, int y, unsigned rgb) const
{
  if (_image == NULL) {
    return 0.0;
  }
  return _image[index(x, y, rgb)];
}

subtracted_image::subtracted_image(const image_wrapper &first, const image_wrapper &second, const double offset) :
  _minx(-1), _maxx(-1), _miny(-1), _maxy(-1),
  _numx(-1), _numy(-1), _image(NULL), _numcolors(0)
{
  // Check to make sure that the two images match.
  int minx, miny, maxx, maxy;
  first.read_range(minx, maxx, miny, maxy);
  int minx2, miny2, maxx2, maxy2;
  second.read_range(minx2, maxx2, miny2, maxy2);
  if ( (first.get_num_colors() != second.get_num_colors()) ||
       (minx != minx2) || (miny != miny2) || (maxx != maxx2) || (maxy != maxy2) ) {
    fprintf(stderr,"subtracted_image::subtracted_image(): Two images differ in dimension\n");
    return;
  }

  // Get our image buffer
  _minx = minx; _maxx = maxx; _miny = miny; _maxy = maxy;
  _numx = (_maxx - _minx) + 1;
  _numy = (_maxy - _miny) + 1;
  _numcolors = first.get_num_colors();
  _image = new float[_numx * _numy * get_num_colors()];
  if (_image == NULL) {
    _numx = _numy = _minx = _maxx = _miny = _maxy = _numcolors = 0;
    fprintf(stderr,"subtracted_image::subtracted_image(): Out of memory\n");
    return;
  }

  // Subtract the values from the images, offsetting as we go
  int x, y;
  unsigned c;
  for (x = _minx; x <= _maxx; x++) {
    for (y = _miny; y <= _maxy; y++) {
      for (c = 0; c < get_num_colors(); c++) {
	_image[index(x, y, c)] = static_cast<float>(first.read_pixel_nocheck(x, y, c) - second.read_pixel_nocheck(x, y, c) + offset);
      }
    }
  }
}

subtracted_image::~subtracted_image()
{
  if (_image) {
    delete [] _image; _image = NULL;
  }
}

bool  subtracted_image::read_pixel(int x, int y, double &result, unsigned rgb) const
{
  if ( (_image == NULL) || (x < _minx) || (x > _maxx) || (y < _miny) || (y > _maxy) ) {
    result = 0.0;
    return false;
  }
  result = _image[index(x, y, rgb)];
  return true;
}

double	subtracted_image::read_pixel_nocheck(int x, int y, unsigned rgb) const
{
  if (_image == NULL) {
    return 0.0;
  }
  return _image[index(x, y, rgb)];
}

// Write the texture, using a virtual method call appropriate to the particular
// camera type.
bool subtracted_image::write_to_opengl_texture(GLuint tex_id)
{
  if (get_num_colors() == 1) {
    const GLint   NUM_COMPONENTS = 1;
    const GLenum  FORMAT = GL_LUMINANCE;
    const GLenum  TYPE = GL_FLOAT;
    const void*   BASE_BUFFER = _image;
    const void*   SUBSET_BUFFER = &_image[NUM_COMPONENTS * ( _minx + get_num_columns()*_miny )];
    return write_to_opengl_texture_generic(tex_id, NUM_COMPONENTS, FORMAT, TYPE,
      BASE_BUFFER, SUBSET_BUFFER, _minx, _miny, _maxx, _maxy);
  } else if (get_num_colors() == 3) {
    const GLint   NUM_COMPONENTS = 3;
    const GLenum  FORMAT = GL_RGB;
    const GLenum  TYPE = GL_FLOAT;
    const void*   BASE_BUFFER = _image;
    const void*   SUBSET_BUFFER = &_image[NUM_COMPONENTS * ( _minx + get_num_columns()*_miny )];
    return write_to_opengl_texture_generic(tex_id, NUM_COMPONENTS, FORMAT, TYPE,
      BASE_BUFFER, SUBSET_BUFFER, _minx, _miny, _maxx, _maxy);
  } else {
    fprintf(stderr,"subtracted_image::write_to_opengl_texture(): Not implemented for images with %d colors\n", get_num_colors());
    return false;
  }
}


averaged_image::averaged_image(const image_wrapper &first, const image_wrapper &second) :
  _minx(-1), _maxx(-1), _miny(-1), _maxy(-1),
  _numx(-1), _numy(-1), _image(NULL), _numcolors(0)
{
  // Check to make sure that the two images match.
  int minx, miny, maxx, maxy;
  first.read_range(minx, maxx, miny, maxy);
  int minx2, miny2, maxx2, maxy2;
  second.read_range(minx2, maxx2, miny2, maxy2);
  if ( (first.get_num_colors() != second.get_num_colors()) ||
       (minx != minx2) || (miny != miny2) || (maxx != maxx2) || (maxy != maxy2) ) {
    fprintf(stderr,"averaged_image::averaged_image(): Two images differ in dimension\n");
    return;
  }

  // Get our image buffer
  _minx = minx; _maxx = maxx; _miny = miny; _maxy = maxy;
  _numx = (_maxx - _minx) + 1;
  _numy = (_maxy - _miny) + 1;
  _numcolors = first.get_num_colors();
  _image = new double[_numx * _numy * get_num_colors()];
  if (_image == NULL) {
    _numx = _numy = _minx = _maxx = _miny = _maxy = _numcolors = 0;
    fprintf(stderr,"averaged_image::averaged_image(): Out of memory\n");
    return;
  }

  // Average the values from the images, offsetting as we go
  int x, y;
  unsigned c;
  for (x = _minx; x <= _maxx; x++) {
    for (y = _miny; y <= _maxy; y++) {
      for (c = 0; c < get_num_colors(); c++) {
	_image[index(x, y, c)] = ( first.read_pixel_nocheck(x, y, c) + second.read_pixel_nocheck(x, y, c) ) / 2;
      }
    }
  }
}

averaged_image::~averaged_image()
{
  if (_image) {
    delete [] _image; _image = NULL;
  }
}

bool  averaged_image::read_pixel(int x, int y, double &result, unsigned rgb) const
{
  if ( (_image == NULL) || (x < _minx) || (x > _maxx) || (y < _miny) || (y > _maxy) ) {
    result = 0.0;
    return false;
  }
  result = _image[index(x, y, rgb)];
  return true;
}

double	averaged_image::read_pixel_nocheck(int x, int y, unsigned rgb) const
{
  if (_image == NULL) {
    return 0.0;
  }
  return _image[index(x, y, rgb)];
}

image_metric::image_metric(const image_wrapper &copyfrom) :
  _minx(-1), _maxx(-1), _miny(-1), _maxy(-1),
  _numx(-1), _numy(-1), _image(NULL), _numcolors(0)
{
  // Allocate image buffer
  int minx, miny, maxx, maxy;
  copyfrom.read_range(minx, maxx, miny, maxy);
  _minx = minx; _maxx = maxx; _miny = miny; _maxy = maxy;
  _numx = (_maxx - _minx) + 1;
  _numy = (_maxy - _miny) + 1;
  _numcolors = copyfrom.get_num_colors();
  _image = new double[_numx * _numy * get_num_colors()];
  if (_image == NULL) {
    _numx = _numy = _minx = _maxx = _miny = _maxy = _numcolors = 0;
    fprintf(stderr, "image_metric::image_metric(): Out of memory\n");
    return;
  }

  // Copy the values from the image
  int x, y;
  unsigned c;
  for (x = _minx; x <= _maxx; x++) {
    for (y = _miny; y <= _maxy; y++) {
      for (c = 0; c < get_num_colors(); c++) {
	double val;
	copyfrom.read_pixel(x, y, val, c);  // Ignore result outside of image.
	_image[index(x, y, c)] = val;
      }
    }
  }
}

image_metric::~image_metric()
{
  if (_image) {
    delete [] _image; _image = NULL;
  }
}

bool  image_metric::read_pixel(int x, int y, double &result, unsigned rgb) const
{
  if ( (_image == NULL) || (x < _minx) || (x > _maxx) || (y < _miny) || (y > _maxy) ) {
    result = 0.0;
    return false;
  }
  result = _image[index(x, y, rgb)];
  return true;
}

double	image_metric::read_pixel_nocheck(int x, int y, unsigned rgb) const
{
  if (_image == NULL) {
    return 0.0;
  }
  return _image[index(x, y, rgb)];
}

void minimum_image::operator+=(const image_wrapper &newimage)
{
  // Check to make sure that the two images match.
  int minx, miny, maxx, maxy;
  newimage.read_range(minx, maxx, miny, maxy);
  if ( (newimage.get_num_colors() != get_num_colors()) ||
       (minx != _minx) || (miny != _miny) || (maxx != _maxx) || (maxy != _maxy) ) {
    fprintf(stderr,"minimum_image::+=(): New image differs in dimension\n");
    return;
  }

  // Store the min of the existing and new image
  int x, y;
  unsigned c;
  for (x = _minx; x <= _maxx; x++) {
    for (y = _miny; y <= _maxy; y++) {
      for (c = 0; c < get_num_colors(); c++) {
	double val = read_pixel_nocheck(x, y, c);
	double val2 = newimage.read_pixel_nocheck(x, y, c);
	_image[index(x, y, c)] = (val < val2) ? val : val2;
      }
    }
  }
}

void maximum_image::operator+=(const image_wrapper &newimage)
{
  // Check to make sure that the two images match.
  int minx, miny, maxx, maxy;
  newimage.read_range(minx, maxx, miny, maxy);
  if ( (newimage.get_num_colors() != get_num_colors()) ||
       (minx != _minx) || (miny != _miny) || (maxx != _maxx) || (maxy != _maxy) ) {
    fprintf(stderr,"maximum_image::+=(): New image differs in dimension\n");
    return;
  }

  // Store the max of the existing and new image
  int x, y;
  unsigned c;
  for (x = _minx; x <= _maxx; x++) {
    for (y = _miny; y <= _maxy; y++) {
      for (c = 0; c < get_num_colors(); c++) {
	double val = read_pixel_nocheck(x, y, c);
	double val2 = newimage.read_pixel_nocheck(x, y, c);
        _image[index(x, y, c)] = (val > val2) ? val : val2;
      }
    }
  }
}

void mean_image::operator+=(const image_wrapper &newimage)
{
  // Check to make sure that the two images match.
  int minx, miny, maxx, maxy;
  newimage.read_range(minx, maxx, miny, maxy);
  if ( (newimage.get_num_colors() != get_num_colors()) ||
       (minx != _minx) || (miny != _miny) || (maxx != _maxx) || (maxy != _maxy) ) {
    fprintf(stderr,"mean_image::+=(): New image differs in dimension\n");
    return;
  }

  // Sum the existing image and increase the image count
  int x, y;
  unsigned c;
  for (x = _minx; x <= _maxx; x++) {
    for (y = _miny; y <= _maxy; y++) {
      for (c = 0; c < get_num_colors(); c++) {
	_image[index(x, y, c)] += newimage.read_pixel_nocheck(x, y, c);
      }
    }
  }
  d_num_images++;
}

//< Take the 
bool  mean_image::read_pixel(int x, int y, double &result, unsigned rgb) const
{
  if ( (_image == NULL) || (x < _minx) || (x > _maxx) || (y < _miny) || (y > _maxy) ) {
    result = 0.0;
    return false;
  }
  result = _image[index(x, y, rgb)] / d_num_images;
  return true;
}

double	mean_image::read_pixel_nocheck(int x, int y, unsigned rgb) const
{
  if (_image == NULL) {
    return 0.0;
  }
  return _image[index(x, y, rgb)] / d_num_images;
}


PSF_File::~PSF_File()
{
  // Figure out whether the image will be sixteen bits, and also
  // the gain to apply (256 if 8-bit, 1 if 16-bit).
  bool do_sixteen = (d_sixteen_bits == 1);
  int bitshift_gain = 1;
  if (!do_sixteen) { bitshift_gain = 256; }

  // Write the file to the name specified, using 8 or 16 bits depending
  // on the setting of the global flag.
  write_to_tiff_file(d_filename.c_str(), bitshift_gain, 0.0, do_sixteen);

  // Delete all allocated space
  unsigned i;
  for (i = 0; i < d_lines.size(); i++) {
    delete [] d_lines[i];
    d_lines[i] = NULL;
  }
  d_lines.clear();
}

bool  PSF_File::append_line(const image_wrapper &image, const double x, const double y)
{
  if (d_radius == 0) {
    fprintf(stderr,"PSF_File::append_line(): Zero radius (not adding more lines)\n");
    return false;
  }

  // Allocate the buffer for a new line and push it onto the end of the list.
  double  *new_line = new double[d_radius+1];
  if (new_line == NULL) {
    fprintf(stderr,"PSF_File::append_line(): Out of memory (not adding more lines)\n");
    return false;
  }
  d_lines.push_back(new_line);

  // Fill in the values for the line by radially averaging around the center
  // specified in the image.  Make sure that at the outer layer, we sample at
  // a spacing of at least once per pixel; inner rings will be sampled with
  // greater frequency.  Make sure that we sample evenly within each ring.
  unsigned i, j;
  unsigned count = (unsigned)ceil(2 * d_radius * M_PI);
  for (i = 0; i <= d_radius; i++) {
    double step_size = (2 * i * M_PI) / count;
    new_line[i] = 0.0;
    // Step around the loop and average the results
    for (j = 0; j < count; j++) {
      new_line[i] += image.read_pixel_bilerp_nocheck(x + i*cos(j*step_size), y + i*sin(j*step_size));
    }
    new_line[i] /= count;
  }
  return true;
}

// Write the texture, using a virtual method call appropriate to the particular
// camera type.  NOTE: At least the first time this function is called,
// we must write a complete texture, which may be larger than the actual bytes
// allocated for the image.  After the first time, and if we don't change the
// image size to be larger, we can use the subimage call to only write the
// pixels we have.
bool image_wrapper::write_to_opengl_texture_generic(GLuint tex_id, GLint num_components,
          GLenum format, GLenum type, const GLvoid *buffer_base, const GLvoid *subset_base,
          unsigned minX, unsigned minY, unsigned maxX, unsigned maxY)
{
  // Note: Check the GLubyte or GLushort or whatever in the temporary buffer!
  GLint   NUM_COMPONENTS = num_components;
  GLenum  FORMAT = format;
  GLenum  TYPE = type;

  // Figure out the next power-of-two size up based on the current texture size.
  // This is because textures must be an even power of two size on each axis.
  // XXXX It no longer seems to be the case that we need power-of-two texture
  // sizes (5/7/2010), so I'm setting this to match.  This lets us do texture
  // writes that are smaller and also reserves less graphics-card memory for
  // the textures.  It also lets us do an actual write the first time to a
  // texure ID, rather than writing an overfull empty buffer.
  _opengl_texture_size_x = get_num_columns();//XXXX static_cast<unsigned>( pow(2.0, ceil( log( static_cast<double>(get_num_columns()) ) / log(2.0) ) ) );
  _opengl_texture_size_y = get_num_rows();//XXXX static_cast<unsigned>( pow(2.0, ceil( log( static_cast<double>(get_num_rows()) ) / log(2.0) ) ) );

  // Store away our state so that we can return it to normal when
  // we're done and not mess up other rendering.
  glPushAttrib(GL_ENABLE_BIT | GL_TEXTURE_BIT | GL_CURRENT_BIT | GL_PIXEL_MODE_BIT);

  // Enable 2D texture-mapping so that we can do our thing.
  glEnable(GL_TEXTURE_2D);

  // We need to write an image to the texture at least once that includes all of
  // the pixels, before we can call the subimage write method below.
  if (!_opengl_texture_have_written || (tex_id != _tex_id)) {

    // If the OpenGL texture size does not match the image size, we need to
    // write a full blank image followed by the actual-sized image.  Otherwise,
    // we can just write the correct-sized image.
    if ( (_opengl_texture_size_x != get_num_columns()) ||
         (_opengl_texture_size_y != get_num_rows()) ) {

      // Allocate enough memory to handle either 8- or 16-bit arrays.
      size_t maximum_size = 2 * NUM_COMPONENTS * _opengl_texture_size_x * _opengl_texture_size_y;
      GLubyte *tempimage = new GLubyte[maximum_size];
      if (tempimage == NULL) {
        fprintf(stderr,"image_wrapper::write_to_opengl_texture(): Out of memory allocating temporary buffer\n");
        return false;
      }
      memset(tempimage, 0, maximum_size);

      // Set the pixel storage parameters and store the total blank image.
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      glPixelStorei(GL_UNPACK_ROW_LENGTH, _opengl_texture_size_x);
      glTexImage2D(GL_TEXTURE_2D, 0, FORMAT, _opengl_texture_size_x, _opengl_texture_size_y,
        0, FORMAT, TYPE, tempimage);

      delete [] tempimage;

      // Set the pixel storage parameters for the image-sized subset and send it.
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      glPixelStorei(GL_UNPACK_ROW_LENGTH, get_num_columns());

      // Send the subset of the image that we actually have active to OpenGL.
      glTexSubImage2D(GL_TEXTURE_2D, 0,
        minX,minY, maxX-minX+1,maxY-minY+1,
        FORMAT, TYPE, subset_base);

    } else {
      // Set the pixel storage parameters and store the total blank image.
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      glPixelStorei(GL_UNPACK_ROW_LENGTH, get_num_columns());
      glTexImage2D(GL_TEXTURE_2D, 0, FORMAT,
        get_num_columns(), get_num_rows(),
        0, FORMAT, TYPE, buffer_base);
    }

    // We've done the first storage, later ones can just do subset-sized
    // writes unless we change texture IDs.
    _opengl_texture_have_written = true;
    _tex_id = tex_id;
  } else {

    // Set the pixel storage parameters.
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, get_num_columns());

    // Send the subset of the image that we actually have active to OpenGL.
    glTexSubImage2D(GL_TEXTURE_2D, 0,
      minX,minY, maxX-minX+1,maxY-minY+1,
      FORMAT, TYPE, subset_base);
  }

  // Put the state back the way it was
  glPopAttrib();

  return true;
}

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
bool image_wrapper::write_to_opengl_quad(double scale, double offset)
{
  // Check for any OpenGL errors to date, so we don't get tagged for them.
  GLenum errcode;
  if ( (errcode = glGetError()) != GL_NO_ERROR) {
    fprintf(stderr,"image_wrapper::write_to_opengl_quad(): Warning, OpenGL error %d had occured before I was called (ignoring)\n", errcode);
  }

  // Store away our state so that we can return it to normal when
  // we're done and not mess up other rendering.
  glPushAttrib(GL_ENABLE_BIT | GL_TEXTURE_BIT | GL_CURRENT_BIT | GL_PIXEL_MODE_BIT);

  // Enable 2D texture-mapping so that we can do our thing.
  glEnable(GL_TEXTURE_2D);

  // Generate the texture ID to use, if we don't have one
  if (_opengl_texture_size_x == 0) {
    glGenTextures(1, &_tex_id);
  }

  // Bind the appropriate texture
  glBindTexture(GL_TEXTURE_2D, _tex_id);

  // Set the clamping behavior and such, which should not have any
  // effect because we should always be at the right scale
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );

  // Set the offset and scale parameters for the transfer.  Set all of the RGB ones to
  // the requested one even for GL_LUMINANCE textures, because they get used.  Set the
  // Alpha ones to pass through unmodified because they eventually get used in the
  // pixel math for some cases.
  glPixelTransferf(GL_RED_SCALE, static_cast<GLfloat>(scale));
  glPixelTransferf(GL_GREEN_SCALE, static_cast<GLfloat>(scale));
  glPixelTransferf(GL_BLUE_SCALE, static_cast<GLfloat>(scale));
  glPixelTransferf(GL_ALPHA_SCALE, static_cast<GLfloat>(1.0));
  glPixelTransferf(GL_RED_BIAS, static_cast<GLfloat>(offset));
  glPixelTransferf(GL_GREEN_BIAS, static_cast<GLfloat>(offset));
  glPixelTransferf(GL_BLUE_BIAS, static_cast<GLfloat>(offset));
  glPixelTransferf(GL_ALPHA_BIAS, static_cast<GLfloat>(0.0));

  // Write the texture, using a virtual method call appropriate to the particular
  // camera type.  NOTE: At least the first time this function is called,
  // we must write a complete texture, which may be larger than the actual bytes
  // allocated for the image.  After the first time, and if we don't change the
  // image size to be larger, we can use the subimage call to only write the
  // pixels we have.
  if (!write_to_opengl_texture(_tex_id)) {
    fprintf(stderr,"image_wrapper::write_to_opengl_quad(): write_to_opengl_texture() failed!\n");
    return false;
  }

  // Write the texture into an OpenGL quad, inverting as needed by the particular
  // camera or imager.
  glColor3d(1.0, 1.0, 1.0);
  if (!write_opengl_texture_to_quad()) {
    fprintf(stderr,"image_wrapper::write_to_opengl_quad(): write_opengl_texture_to_quad() failed!\n");
    return false;
  }

  // Put the state back the way it was
  glPopAttrib();

  // Check for OpenGL errors during our action here.
  if ( (errcode = glGetError()) != GL_NO_ERROR) {
    switch (errcode) {
        case GL_INVALID_ENUM:
            fprintf(stderr,"image_wrapper::write_to_opengl_quad():Warning: GL error GL_INVALID_ENUM occurred\n");
            break;
        case GL_INVALID_VALUE:
            fprintf(stderr,"image_wrapper::write_to_opengl_quad():Warning: GL error GL_INVALID_VALUE occurred\n");
            break;
        case GL_INVALID_OPERATION:
            fprintf(stderr,"image_wrapper::write_to_opengl_quad():Warning: GL error GL_INVALID_OPERATION occurred\n");
            break;
        case GL_STACK_OVERFLOW:
            fprintf(stderr,"image_wrapper::write_to_opengl_quad():Warning: GL error GL_STACK_OVERFLOW occurred\n");
            break;
        case GL_STACK_UNDERFLOW:
            fprintf(stderr,"image_wrapper::write_to_opengl_quad():Warning: GL error GL_STACK_UNDERFLOW occurred\n");
            break;
        case GL_OUT_OF_MEMORY:
            fprintf(stderr,"image_wrapper::write_to_opengl_quad():Warning: GL error GL_OUT_OF_MEMORY occurred\n");
            break;
        default:
            fprintf(stderr,"image_wrapper::write_to_opengl_quad():Warning: GL error (code 0x%x) occurred\n", errcode);
    }
    return false;
  }

  return true;
}

bool image_wrapper::write_opengl_texture_to_quad()
{
  double xfrac = static_cast<double>(get_num_columns()) / _opengl_texture_size_x;
  double yfrac = static_cast<double>(get_num_rows()) / _opengl_texture_size_y;

  // Set the texture and vertex coordinates and write the quad to OpenGL.
  glBegin(GL_QUADS);
    glTexCoord2d(0.0, 0.0);
    glVertex2d(-1.0, -1.0);

    glTexCoord2d(xfrac, 0.0);
    glVertex2d(1.0, -1.0);

    glTexCoord2d(xfrac, yfrac);
    glVertex2d(1.0, 1.0);

    glTexCoord2d(0.0, yfrac);
    glVertex2d(-1.0, 1.0);
  glEnd();

  return true;
}
