// To be able to read 16-bit images, we need to use a version of ImageMagick
// that is compiled with -DQuantumDepth=16.  Then, we need to #define QuantumLeap
// before includeing magick/api.h.

#include <stdio.h>

#include "base_camera_server.h"

#define QuantumLeap
#include <magick/api.h>

// Apply the specified gain to the value, clamp to zero and the maximum
// possible value, and return the result.
static	vrpn_uint16 scale_and_clamp(const double value, const double gain)
{
  double result = value * gain;
  if (result < 0) { result = 0; }
  if (result > 65535) { result = 65535; }
  return (vrpn_uint16)(result);
}

/// Store the memory image to a TIFF file.
bool  image_wrapper::write_to_tiff_file(const char *filename, double gain, bool sixteen_bits,
						    const char *magick_files_dir) const
{
  // Make a pointer to the image data.  If we adjust the gain, then make
  // a copy of it adjusted by the gain.
  vrpn_uint16	  *gain_buffer = NULL;
  int		  r,c;
  int minx, maxx, miny, maxy, numcols, numrows;
  read_range(minx, maxx, miny, maxy);
  numcols = maxx-minx+1;
  numrows = maxy-miny+1;
  gain_buffer = new vrpn_uint16[numcols * numrows * 3];
  if (gain_buffer == NULL) {
      fprintf(stderr, "base_camera_server::write_memory_to_tiff_file(): Out of memory\n");
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
      read_pixel(c, r, value, 0);
      gain_buffer[0 + 3*(c + flip_r * numcols)] = scale_and_clamp(value, gain);
      read_pixel(c, r, value, 1);
      gain_buffer[1 + 3*(c + flip_r * numcols)] = scale_and_clamp(value, gain);
      read_pixel(c, r, value, 2);
      gain_buffer[2 + 3*(c + flip_r * numcols)] = scale_and_clamp(value, gain);
    }
  }

#ifdef	_WIN32
  InitializeMagick(magick_files_dir);
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
      fprintf(stderr, "base_camera_server::write_memory_to_tiff_file(): %s: %s\n",
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
      fprintf(stderr, "base_camera_server::write_memory_to_tiff_file(): %s: %s\n",
             exception.reason,exception.description);
      return false;
  }
  DestroyImageInfo(out_image_info);
  DestroyImage(out_image);
  delete [] gain_buffer;
  return true;
}

copy_of_image::copy_of_image(const image_wrapper &copyfrom) :
  _minx(-1), _maxx(-1), _miny(-1), _maxy(-1),
  _numx(-1), _numy(-1), _image(NULL), _numcolors(0)
{
  *this = copyfrom;
}

void copy_of_image::operator=(const image_wrapper &copyfrom)
{
  // If the dimensions don't match, then get a new image buffer
  int minx, miny, maxx, maxy;
  copyfrom.read_range(minx, maxx, miny, maxy);
  if ( (minx != _minx) || (maxx != _maxx) || (miny != _miny) || (maxy != _maxy) ||
       (get_num_colors() != copyfrom.get_num_colors()) ) {
    if (_image != NULL) { delete [] _image; }
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
    delete [] _image;
  }
}

void  copy_of_image::read_range(int &minx, int &maxx, int &miny, int &maxy) const
{
  minx = _minx;
  maxx = _maxx;
  miny = _miny;
  maxy = _maxy;
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

