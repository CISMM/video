#include  <math.h>
#include  <stdlib.h>
#include  <stdio.h>
#include  "image_wrapper.h"

disc_image::disc_image(int minx, int maxx, int miny, int maxy,
		       double background, double noise,
		       double diskx, double disky, double diskr,
		       double diskintensity) :
  _minx(minx), _maxx(maxx), _miny(miny), _maxy(maxy),
  _image(NULL)
{
  int i,j, index;

  // Make sure the parameters are meaningful
  if ( (_minx >= _maxx) || (_miny >= _maxy) || (_minx < 0) || (_miny < 0) ) {
    fprintf(stderr,"disc_image::disc_image(): Bad min/max coordinates\n");
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
#ifdef	DEBUG
  printf("disc_image::disc_image(): Making disk of radius %lg\n", diskr);
#endif
  double  disk2 = diskr * diskr;
  for (i = (int)floor(diskx - diskr); i <= (int)ceil(diskx + diskr); i++) {
    for (j = (int)floor(disky - diskr); j <= (int)ceil(disky + diskr); j++) {
      if ( disk2 >= (i-diskx)*(i-diskx) + (j-disky)*(j-disky) ) {
	if (find_index(i,j,index)) {
	  _image[index] = diskintensity;
	}
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
		       double centeritensity) :
  _minx(minx), _maxx(maxx), _miny(miny), _maxy(maxy),
  _image(NULL)
{
  int i,j, index;

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

  // Fill in the cone intensity (the part of it that is within the image).
#ifdef	DEBUG
  printf("cone_image::cone_image(): Making cone of radius %lg\n", diskr);
#endif
  for (i = (int)floor(diskx - diskr); i <= (int)ceil(diskx + diskr); i++) {
    for (j = (int)floor(disky - diskr); j <= (int)ceil(disky + diskr); j++) {
      double dist = (i-diskx)*(i-diskx) + (j-disky)*(j-disky);
      if ( dist <= diskr ) {
	if (find_index(i,j,index)) {
	  double frac = dist / diskr;
	  double intensity = frac * background + (1 - frac) * centeritensity;
	  _image[index] = intensity;
	}
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
