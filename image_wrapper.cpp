#include  <math.h>
#include  <stdlib.h>
#include  <stdio.h>
#include  "image_wrapper.h"

disc_image::disc_image(int minx, int maxx, int miny, int maxy,
		       double background, double noise,
		       double diskx, double disky, double diskr,
		       double diskintensity, int oversample) :
  _minx(minx), _maxx(maxx), _miny(miny), _maxy(maxy),
  _oversample(oversample),
  _image(NULL)
{
  int i,j, index;
  double oi,oj;	  //< Oversample steps

  // Make sure the parameters are meaningful
  if ( (_minx >= _maxx) || (_miny >= _maxy) || (_minx < 0) || (_miny < 0) ) {
    fprintf(stderr,"disc_image::disc_image(): Bad min/max coordinates\n");
    _minx = _maxy = _minx = _maxx = 0;
    return;
  }
  if ( (_oversample <= 0) ) {
    fprintf(stderr,"disc_image::disc_image(): Bad oversample\n");
    _minx = _maxy = _minx = _maxx = 0;
    return;
  }

  // Try to allocate a large enough array to hold all of the values.
  if ( (_image = new double[(_maxx-_minx+1) * (_maxy-_miny+1)]) == NULL) {
    fprintf(stderr,"disc_image::disc_image(): Out of memory\n");
    _minx = _maxy = _minx = _maxx = 0;
    return;
  }

  // Fill in the background intensity.
  for (i = _minx; i <= _maxx; i++) {
    for (j = _miny; j <= _maxy; j++) {
      if (find_index(i,j,index)) {
	_image[index] = background;
      }
    }
  }

  // Fill in the disk intensity (the part of it that is within the image).
  // Oversample the image by the factor specified in _oversample, averaging
  // all results within the pixel.
#ifdef	DEBUG
  printf("disc_image::disc_image(): Making disk of radius %lg\n", diskr);
#endif
  double  disk2 = diskr * diskr;
  for (i = (int)floor(diskx - diskr); i <= (int)ceil(diskx + diskr); i++) {
    for (j = (int)floor(disky - diskr); j <= (int)ceil(disky + diskr); j++) {
      if (find_index(i,j,index)) {
	_image[index] = 0;
	for (oi = 0; oi < _oversample; oi++) {
	  for (oj = 0; oj < _oversample; oj++) {
	    double x = i + oi/_oversample;
	    double y = j + oj/_oversample;
	    if ( disk2 >= (x - diskx)*(x - diskx) + (y - disky)*(y - disky) ) {
	      _image[index] += diskintensity;
	    } else {
	      _image[index] += background;
	    }
	  }
	}
	_image[index] /= (_oversample * _oversample);
      }
    }
  }

  // Add zero-mean uniform noise to the image, with the specified width
  for (i = _minx; i <= _maxx; i++) {
    for (j = _miny; j <= _maxy; j++) {
      if (find_index(i,j,index)) {
	double	unit_rand = (double)(rand()) / RAND_MAX;
	_image[index] += (unit_rand - 0.5) * 2 * noise;
      }
    }
  }  
}

disc_image::~disc_image()
{
  delete [] _image;
}

void disc_image::read_range(int &minx, int &maxx, int &miny, int &maxy) const
{
  minx = _minx; maxx = _maxx; miny = _miny; maxy = _maxy;
};

// Read a pixel from the image into a double; return true if the pixel
// was in the image, false if it was not.
bool disc_image::read_pixel(int x, int y, double &result) const
{
  int index;
  if (find_index(x,y, index)) {
    result = _image[index];
    return true;
  } else {
    return false;
  }
}

double disc_image::read_pixel_nocheck(int x, int y) const
{
  int index;
  if (find_index(x,y, index)) {
    return _image[index];
  } else {
    return 0.0;
  }

}


cone_image::cone_image(int minx, int maxx, int miny, int maxy,
		       double background, double noise,
		       double diskx, double disky, double diskr,
		       double centeritensity, int oversample) :
  _minx(minx), _maxx(maxx), _miny(miny), _maxy(maxy),
  _image(NULL), _oversample(oversample)
{
  int i,j, index;
  int oi,oj;	  //< Oversample steps

  // Make sure the parameters are meaningful
  if ( (_minx >= _maxx) || (_miny >= _maxy) || (_minx < 0) || (_miny < 0) ) {
    fprintf(stderr,"cone_image::cone_image(): Bad min/max coordinates\n");
    _minx = _maxy = _minx = _maxx = 0;
    return;
  }

  // Try to allocate a large enough array to hold all of the values.
  if ( (_image = new double[(_maxx-_minx+1) * (_maxy-_miny+1)]) == NULL) {
    fprintf(stderr,"cone_image::cone_image(): Out of memory\n");
    _minx = _maxy = _minx = _maxx = 0;
    return;
  }

  // Fill in the background intensity.
  for (i = _minx; i <= _maxx; i++) {
    for (j = _miny; j <= _maxy; j++) {
      if (find_index(i,j,index)) {
	_image[index] = background;
      }
    }
  }

//  XXX This doesn't seem to center the pixel like it should
  // Compute where the samples should be taken.  These need to be taken
  // symmetrically around the pixel center and cover all samples within
  // the pixel exactly once.  First, compute the step size between the
  // samples, then the one with the smallest value that lies within a
  // half-pixel-width of the center of the pixel as the starting offset.
  // Proceeed until we exceed a half-pixel-radius on the high side of
  // the pixel.

  double step = 1.0 / _oversample;
  double start;
  if (_oversample % 2 == 1) { // Odd number of steps
    start = - step * floor( _oversample / 2 );
  } else {    // Even number of steps
    start = - step * ( (_oversample / 2 - 1) + 0.5 );
  }

  // Fill in the cone intensity (the part of it that is within the image).
#ifdef	DEBUG
  printf("cone_image::cone_image(): Making cone of radius %lg\n", diskr);
#endif
  for (i = (int)floor(diskx - diskr); i <= (int)ceil(diskx + diskr); i++) {
    for (j = (int)floor(disky - diskr); j <= (int)ceil(disky + diskr); j++) {
      if (find_index(i,j,index)) {
	_image[index] = 0;
	for (oi = 0; oi < _oversample; oi++) {
	  for (oj = 0; oj < _oversample; oj++) {
	    double x = i + start + oi*step;
	    double y = j + start + oj*step;
	    double dist = sqrt((x-diskx)*(x-diskx) + (y-disky)*(y-disky));
	    if ( diskr >= dist ) {
	      double frac = dist / diskr;
	      double intensity = frac * background + (1 - frac) * centeritensity;
	      _image[index] += intensity;
	    } else {
	      _image[index] += background;
	    }
	  }
	}
	_image[index] /= (_oversample * _oversample);
      }
    }
  }

  // Add zero-mean uniform noise to the image, with the specified width
  for (i = _minx; i <= _maxx; i++) {
    for (j = _miny; j <= _maxy; j++) {
      if (find_index(i,j,index)) {
	double	unit_rand = (double)(rand()) / RAND_MAX;
	_image[index] += (unit_rand - 0.5) * 2 * noise;
      }
    }
  }  
}

cone_image::~cone_image()
{
  delete [] _image;
}

void cone_image::read_range(int &minx, int &maxx, int &miny, int &maxy) const
{
  minx = _minx; maxx = _maxx; miny = _miny; maxy = _maxy;
};

// Read a pixel from the image into a double; return true if the pixel
// was in the image, false if it was not.
bool cone_image::read_pixel(int x, int y, double &result) const
{
  int index;
  if (find_index(x,y, index)) {
    result = _image[index];
    return true;
  } else {
    return false;
  }
}

double cone_image::read_pixel_nocheck(int x, int y) const
{
  int index;
  if (find_index(x,y, index)) {
    return _image[index];
  } else {
    return 0.0;
  }

}

copy_of_image::copy_of_image(const image_wrapper &copyfrom) :
  _minx(-1), _maxx(-1), _miny(-1), _maxy(-1),
  _numx(-1), _numy(-1), _image(NULL)
{
  *this = copyfrom;
}

void copy_of_image::operator=(const image_wrapper &copyfrom)
{
  // If the dimensions don't match, then get a new image buffer
  int minx, miny, maxx, maxy;
  copyfrom.read_range(minx, maxx, miny, maxy);
  if ( (minx != _minx) || (maxx != _maxx) || (miny != _miny) || (maxy != _maxy) ) {
    if (_image != NULL) { delete [] _image; }
    _minx = minx; _maxx = maxx; _miny = miny; _maxy = maxy;
    _numx = (_maxx - _minx) + 1;
    _numy = (_maxy - _miny) + 1;
    _image = new double[_numx * _numy];
    if (_image == NULL) {
      _numx = _numy = _minx = _maxx = _miny = _maxy = 0;
      return;
    }
  }

  // Copy the values from the image
  int x, y;
  for (x = _minx; x <= _maxx; x++) {
    int xindex = x - _minx;
    for (y = _miny; y <= _maxy; y++) {
      int yindex = y - _miny;
      _image[xindex + _numx * yindex] = copyfrom.read_pixel_nocheck(x, y);
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

bool  copy_of_image::read_pixel(int x, int y, double &result) const
{
  if ( (x < _minx) || (x > _maxx) || (y < _miny) || (y > _maxy) ) {
    return false;
  }
  if (_image == NULL) {
    return false;
  }
  int xindex = x - _minx;
  int yindex = y - _miny;
  result = _image[xindex + _numx * yindex];
  return true;
}

double	copy_of_image::read_pixel_nocheck(int x, int y) const
{
  if (_image == NULL) {
    return 0.0;
  }
  int xindex = x - _minx;
  int yindex = y - _miny;
  return _image[xindex + _numx * yindex];
}
