#include  <math.h>
#include  <stdlib.h>
#include  <stdio.h>
#include  "image_wrapper.h"

bool	image_wrapper::read_pixel_bilerp(double x, double y, double &result) const
{
  int xlow = (int)floor(x);
  int xhigh = xlow + 1;
  int ylow = (int)floor(y);
  int yhigh = ylow + 1;
  double xhighfrac = x - xlow;
  double xlowfrac = 1 - xhighfrac;
  double yhighfrac = y - ylow;
  double ylowfrac = 1 - yhighfrac;
  double ll, lh, hl, hh;
  if (!read_pixel(xlow, ylow, ll)) { return false; }
  if (!read_pixel(xlow, yhigh, lh)) { return false; }
  if (!read_pixel(xhigh, ylow, hl)) { return false; }
  if (!read_pixel(xhigh, yhigh, hh)) { return false; }
  result = ll * xlowfrac * ylowfrac + 
	   lh * xlowfrac * yhighfrac +
	   hl * xhighfrac * ylowfrac +
	   hh * xhighfrac * yhighfrac;
  return true;
}


test_image::test_image(int minx, int maxx, int miny, int maxy,
		       double background, double noise,
		       double diskx, double disky, double diskr,
		       double diskintensity) :
  _minx(minx), _maxx(maxx), _miny(miny), _maxy(maxy),
  _image(NULL)
{
  int i,j, index;

  // Make sure the parameters are meaningful
  if ( (_minx >= _maxx) || (_miny >= _maxy) || (_minx < 0) || (_miny < 0) ) {
    fprintf(stderr,"test_image::test_image(): Bad min/max coordinates\n");
    _minx = _maxy = _minx = _maxx = 0;
    return;
  }

  // Try to allocate a large enough array to hold all of the values.
  if ( (_image = new double[(_maxx-_minx+1) * (_maxy-_miny+1)]) == NULL) {
    fprintf(stderr,"test_image::test_image(): Out of memory\n");
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
  printf("test_image::test_image(): Making disk of radius %lg\n", diskr);
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

void test_image::read_range(int &minx, int &maxx, int &miny, int &maxy) const
{
  minx = _minx; maxx = _maxx; miny = _miny; maxy = _maxy;
};

// Read a pixel from the image into a double; return true if the pixel
// was in the image, false if it was not.
bool test_image::read_pixel(int x, int y, double &result) const
{
  int index;
  if (find_index(x,y, index)) {
    result = _image[index];
    return true;
  } else {
    return false;
  }
}
