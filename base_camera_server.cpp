// To be able to read 16-bit images, we need to use a version of ImageMagick
// that is compiled with -DQuantumDepth=16.  Then, we need to #define QuantumLeap
// before includeing magick/api.h.

#include <stdio.h>
#include "base_camera_server.h"
#define QuantumLeap
#include <magick/api.h>

#ifndef	M_PI
const double M_PI = 2*asin(1.0);
#endif

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
						    const char *magick_files_dir) const
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
      read_pixel(minx + c, miny + r, value, 0);
      gain_buffer[0 + 3*(c + flip_r * numcols)] = offset_scale_and_clamp(value, scale, offset);
      read_pixel(minx + c, miny + r, value, 1);
      gain_buffer[1 + 3*(c + flip_r * numcols)] = offset_scale_and_clamp(value, scale, offset);
      read_pixel(minx + c, miny + r, value, 2);
      gain_buffer[2 + 3*(c + flip_r * numcols)] = offset_scale_and_clamp(value, scale, offset);
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
  image_wrapper::~image_wrapper();
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
  _image = new double[_numx * _numy * get_num_colors()];
  if (_image == NULL) {
    _numx = _numy = _minx = _maxx = _miny = _maxy = _numcolors = 0;
    fprintf(stderr,"subtracted_image::subtracted_image(): Out of memory\n");
    return;
  }

  // Copy the values from the images, offsetting aas we go
  int x, y;
  unsigned c;
  for (x = _minx; x <= _maxx; x++) {
    for (y = _miny; y <= _maxy; y++) {
      for (c = 0; c < get_num_colors(); c++) {
	_image[index(x, y, c)] = first.read_pixel_nocheck(x, y, c) - second.read_pixel_nocheck(x, y, c) + offset;
      }
    }
  }
}

subtracted_image::~subtracted_image()
{
  if (_image) {
    delete [] _image;
  }
  image_wrapper::~image_wrapper();
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
    delete [] _image;
  }
  image_wrapper::~image_wrapper();
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
  image_wrapper::~image_wrapper();
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
