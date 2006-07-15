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

  // Add in 0.5 pixel to the X and Y positions
  // so that we center the disc on the middle of the pixel rather than
  // having it centered between pixels.
  diskx += 0.5;
  disky += 0.5;

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
bool disc_image::read_pixel(int x, int y, double &result, unsigned /* RGB ignored */) const
{
  int index;
  if (find_index(x,y, index)) {
    result = _image[index];
    return true;
  } else {
    return false;
  }
}

double disc_image::read_pixel_nocheck(int x, int y, unsigned /* RGB ignored */) const
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
bool cone_image::read_pixel(int x, int y, double &result, unsigned /* RGB ignored */) const
{
  int index;
  if (find_index(x,y, index)) {
    result = _image[index];
    return true;
  } else {
    return false;
  }
}

double cone_image::read_pixel_nocheck(int x, int y, unsigned /* RGB ignored */) const
{
  int index;
  if (find_index(x,y, index)) {
    return _image[index];
  } else {
    return 0.0;
  }

}

Integrated_Gaussian_image::Integrated_Gaussian_image(int minx, int maxx, int miny, int maxy,
	     double background, double noise,
	     double centerx, double centery, double std_dev,
	     double summedvolume, int oversample) :
  _minx(minx), _maxx(maxx), _miny(miny), _maxy(maxy),
  _image(NULL), _oversample(oversample),
  _background(background)
{
  // Make sure the parameters are meaningful
  if ( (_minx >= _maxx) || (_miny >= _maxy) ) {
    fprintf(stderr,"Gaussian_image::Gaussian_image(): Bad min/max coordinates\n");
    _minx = _maxy = _minx = _maxx = 0;
    return;
  }

  // Try to allocate a large enough array to hold all of the values.
  if ( (_image = new double[(_maxx-_minx+1) * (_maxy-_miny+1)]) == NULL) {
    fprintf(stderr,"Gaussian_image::Gaussian_image(): Out of memory\n");
    _minx = _maxy = _minx = _maxx = 0;
    return;
  }

  recompute(background, noise, centerx, centery, std_dev, summedvolume, oversample);
}

Integrated_Gaussian_image::recompute(double background, double noise,
	     double centerx, double centery, double std_dev,
	     double summedvolume, int oversample)
{
  _oversample = oversample;
  int i,j, index;

//  XXX Verify that this centers the pixel like it should.
  // Compute where the samples should be taken.  These need to be taken
  // symmetrically around the pixel center and cover all samples within
  // the pixel exactly once.  First, compute the step size between the
  // samples, then the one with the smallest value that lies within a
  // half-pixel-width of the center of the pixel as the starting offset.
  // Proceeed until we exceed a half-pixel-radius on the high side of
  // the pixel.

  // Fill in the Gaussian intensity plus background plus noise (if any).
  // Note that the area under the curve for the unit Gaussian is 1; we
  // multiply by the summedvolume (which needs to be the sum above or
  // below the background) to produce an overall volume matching the
  // one requested.
#ifdef	DEBUG
  printf("Gaussian_image::recompute(): Making Gaussian of standard deviation %lg, background %lg, volume %lg\n", std_dev, background, summedvolume);
#endif
  for (i = _minx; i <= _maxx; i++) {
    for (j = _miny; j <= _maxy; j++) {
      double x0 = (i - centerx) - 0.5;    // The left edge of the pixel in Gaussian space
      double x1 = x0 + 1;                 // The right edge of the pixel in Gaussian space
      double y0 = (j - centery) - 0.5;    // The bottom edge of the pixel in Gaussian space
      double y1 = y0 + 1;                 // The top edge of the pixel in Gaussian space
      if (find_index(i,j,index)) {
        // For this call to ComputeGaussianVolume, we assume 1-meter pixels.
        // This makes std dev and other be in pixel units without conversion.
	_image[index] = background + ComputeGaussianVolume(summedvolume, std_dev,
                        x0,x1, y0,y1, _oversample);
        
        // Add zero-mean uniform noise to the image, with the specified width
        if (noise > 0.0) {
	  double  unit_rand = (double)(rand()) / RAND_MAX;
	  _image[index] += (unit_rand - 0.5) * 2 * noise;
        }
      }
    }
  }
}

Integrated_Gaussian_image::~Integrated_Gaussian_image()
{
  delete [] _image;
}

void Integrated_Gaussian_image::read_range(int &minx, int &maxx, int &miny, int &maxy) const
{
  minx = _minx; maxx = _maxx; miny = _miny; maxy = _maxy;
};

// Read a pixel from the image into a double; return true if the pixel
// was in the image, false if it was not.
bool Integrated_Gaussian_image::read_pixel(int x, int y, double &result, unsigned /* RGB ignored */) const
{
  int index;
  if (find_index(x,y, index)) {
    result = _image[index];
    return true;
  } else {
    result = _background;
    return false;
  }
}

double Integrated_Gaussian_image::read_pixel_nocheck(int x, int y, unsigned /* RGB ignored */) const
{
  int index;
  if (find_index(x,y, index)) {
    return _image[index];
  } else {
    return _background;
  }
}

Point_sampled_Gaussian_image::Point_sampled_Gaussian_image(
	     double background, double noise,
	     double centerx, double centery, double std_dev,
	     double summedvolume)
{
    set_new_parameters(background, noise, centerx, centery, std_dev, summedvolume);
}

double Point_sampled_Gaussian_image::read_pixel_nocheck(int x, int y, unsigned /* RGB ignored */) const
{
  double result = _background + _summedvolume * ComputeGaussianAtPoint(_std_dev, x - _centerx, y - _centery);
  if (_noise != 0.0) {
    double  unit_rand = (double)(rand()) / RAND_MAX;
    result += (unit_rand - 0.5) * 2 * _noise;
  }
  return result;
}

// Read a pixel from the image into a double; return true if the pixel
// was in the image, false if it was not.
bool Point_sampled_Gaussian_image::read_pixel(int x, int y, double &result, unsigned /* RGB ignored */) const
{
  result = read_pixel_nocheck(x, y, 0);
  return true;
}

