#include  <math.h>
#include  <stdio.h>
#include  "spot_tracker.h"

//#define DEBUG
#ifndef	M_PI
const double M_PI = 2*asin(1);
#endif

spot_tracker::spot_tracker(double radius, bool inverted, double pixelaccuracy, double radiusaccuracy) :
    _rad(radius),	      // Initial radius of the disk
    _invert(inverted),	      // Look for black disk on white surround?
    _pixelacc(pixelaccuracy), // Minimum step size in pixels to try
    _radacc(radiusaccuracy),  // Minimum step size in radius to try
    _fitness(-1e10),	      // No good position found yet!
    _pixelstep(2),	      // Starting pixel step size
    _radstep(2)		      // Starting radius step size
{
}

// Optimize starting at the specified location to find the best-fit disk.
// Take only one optimization step.  Return whether we ended up finding a
// better location or not.  Return new location in any case.  One step means
// one step in X,Y, and radius space each.  The boolean parameters tell
// whether to optimize in each direction (X, Y, and Radius).
bool  spot_tracker::take_single_optimization_step(const image_wrapper &image, double &x, double &y,
						       bool do_x, bool do_y, bool do_r)
{
  double  new_fitness;	    //< Checked fitness value to see if it is better than current one
  bool	  betterxy = false; //< Do we find a better location?
  bool	  betterrad = false;//< Do we find a better radius?

  // Try going in +/- X and see if we find a better location.
  if (do_x) {
    _x += _pixelstep;	// Try going a step in +X
    if ( _fitness < (new_fitness = check_fitness(image)) ) {
      _fitness = new_fitness;
      betterxy = true;
    } else {
      _x -= 2*_pixelstep;	// Try going a step in -X
      if ( _fitness < (new_fitness = check_fitness(image)) ) {
	_fitness = new_fitness;
	betterxy = true;
      } else {
	_x += _pixelstep;	// Back where we started in X
      }
    }
  }

  // Try going in +/- Y and see if we find a better location.
  if (do_y) {
    _y += _pixelstep;	// Try going a step in +Y
    if ( _fitness < (new_fitness = check_fitness(image)) ) {
      _fitness = new_fitness;
      betterxy = true;
    } else {
      _y -= 2*_pixelstep;	// Try going a step in -Y
      if ( _fitness < (new_fitness = check_fitness(image)) ) {
	_fitness = new_fitness;
	betterxy = true;
      } else {
	_y += _pixelstep;	// Back where we started in Y
      }
    }
  }

  // Try going in +/- radius and see if we find a better value.
  // Don't let the radius get below 1 pixel.
  if (do_r) {
    _rad += _radstep;	// Try going a step in +radius
    if ( _fitness < (new_fitness = check_fitness(image)) ) {
      _fitness = new_fitness;
      betterrad = true;
    } else {
      _rad -= 2*_radstep;	// Try going a step in -radius
      if ( (_rad >= 1) && (_fitness < (new_fitness = check_fitness(image))) ) {
	_fitness = new_fitness;
	betterrad = true;
      } else {
	_rad += _radstep;	// Back where we started in radius
      }
    }
  }

  // Return the new location and whether we found a better one.
  x = _x; y = _y;
  return betterxy || betterrad;
}

// Find the best fit for the spot detector within the image, taking steps
// that are 1/4 of the bead's radius.  Stay away from edge effects by staying
// away from the edges of the image.
void  spot_tracker::locate_good_fit_in_image(const image_wrapper &image, double &x, double &y)
{
  int i,j;
  int minx, maxx, miny, maxy;
  double  bestx = 0, besty = 0, bestfit = -1e10;
  double  newfit;

  image.read_range(minx, maxx, miny, maxy);
  int ilow = (int)(minx+2*_rad+1);
  int ihigh = (int)(maxx-2*_rad-1);
  int step = (int)(_rad / 4);
  if (step < 1) { step = 1; };
  int jlow = (int)(miny+2*_rad+1);
  int jhigh = (int)(maxy-2*_rad-1);
  for (i = ilow; i <= ihigh; i += step) {
    for (j = jlow; j <= jhigh; j += step) {
      _x = i; _y = j;
      if ( bestfit < (newfit = check_fitness(image)) ) {
	bestfit = newfit;
	bestx = _x;
	besty = _y;
      }
    }
  }

  x = _x = bestx;
  y = _y = besty;
  _fitness = newfit;
}

// Continue to optimize until we can't do any better (the step size drops below
// the minimum.
void  spot_tracker::optimize(const image_wrapper &image, double &x, double &y)
{
  // Set the step sizes to a large value to start with
  _pixelstep = 2;
  _radstep = 2;

  // Find out what our current value is (presumably this is a new image)
  _fitness = check_fitness(image);

  // Try with ever-smaller steps until we reach the smallest size and
  // can't do any better.
  do {
    // Repeat the optimization steps until we can't do any better.
    while (take_single_optimization_step(image, x, y, true, true, true)) {};

    // Try to see if we reducing the step sizes helps.
    if ( _pixelstep > _pixelacc ) {
      _pixelstep /= 2;
    } else if ( _radstep > _radacc ) {
      _radstep /= 2;
    } else {
      break;
    }
  } while (true);
}

// Continue to optimize until we can't do any better (the step size drops below
// the minimum.  Only try moving in X and Y, not changing the radius,
void  spot_tracker::optimize_xy(const image_wrapper &image, double &x, double &y)
{
  // Set the step sizes to a large value to start with
  _pixelstep = 2;

  // Find out what our current value is (presumably this is a new image)
  _fitness = check_fitness(image);

  // Try with ever-smaller steps until we reach the smallest size and
  // can't do any better.
  do {
    // Repeat the optimization steps until we can't do any better.
    while (take_single_optimization_step(image, x, y, true, true, false)) {};

    // Try to see if we reducing the step sizes helps.
    if ( _pixelstep > _pixelacc ) {
      _pixelstep /= 2;
    } else {
      break;
    }
  } while (true);
}

disk_spot_tracker::disk_spot_tracker(double radius, bool inverted, double pixelaccuracy,
				     double radiusaccuracy) :
    spot_tracker(radius, inverted, pixelaccuracy, radiusaccuracy)
{
  // Make sure the parameters make sense
  if (_rad <= 0) {
    fprintf(stderr, "disk_spot_tracker::disk_spot_tracker(): Invalid radius, using 1.0\n");
    _rad = 1.0;
  }
  if (_pixelacc <= 0) {
    fprintf(stderr, "disk_spot_tracker::disk_spot_tracker(): Invalid pixel accuracy, using 0.25\n");
    _pixelacc = 0.25;
  }
  if (_radacc <= 0) {
    fprintf(stderr, "disk_spot_tracker::disk_spot_tracker(): Invalid radius accuracy, using 0.25\n");
    _radacc = 0.25;
  }

  // Set the initial step sizes for radius and pixels
  _pixelstep = 2.0; if (_pixelstep < 4*_pixelacc) { _pixelstep = 4*_pixelacc; };
  _radstep = 2.0; if (_radstep < 4*_radacc) { _radstep = 4*_radacc; };
}

// Check the fitness of the disk against an image, at the current parameter settings.
// Return the fitness value there.  This is done by multiplying the image values within
// one radius of the center by 1 and the image values beyond that but within 2 radii
// by -1 (on-center, off-surround).  If the test is inverted, then the fitness value
// is inverted before returning it.  The fitness is normalized by the number of pixels
// tested (pixels both within the radii and within the image).

// Be careful when selecting the surround fraction below.  If the area in the off-
// surround is larger than the on-area, the code seeks for dark surround more than
// for bright center, effectively causing it to seek an inverted patch that is
// that many times as large as the radius (switches dark-on-light vs. light-on-dark
// behavior).

double	disk_spot_tracker::check_fitness(const image_wrapper &image)
{
  int	  i,j;
  int	  pixels = 0;			//< How many pixels we ended up using
  double  fitness = 0.0;		//< Accumulates the fitness values
  double  val;				//< Pixel value read from the image
  double  surroundfac = 1.3;		//< How much larger the surround is
  double  centerr2 = _rad * _rad;	//< Square of the center "on" disk radius
  double  surroundr2 = centerr2*surroundfac*surroundfac;  //< Square of the surround "off" disk radius
  double  dist2;			//< Square of the distance from the center

#ifdef	DEBUG
  double  pos = 0.0, neg = 0.0;
#endif

  int ilow = (int)floor(_x - surroundfac*_rad);
  int ihigh = (int)ceil(_x + surroundfac*_rad);
  int jlow = (int)floor(_y - surroundfac*_rad);
  int jhigh = (int)ceil(_y + surroundfac*_rad);
  for (i = ilow; i <= ihigh; i++) {
    for (j = jlow; j <= jhigh; j++) {
      dist2 = (i-_x)*(i-_x) + (j-_y)*(j-_y);

      // See if we are within the inner disk
      if ( dist2 <= centerr2 ) {
	if (image.read_pixel(i,j,val)) {
	  pixels++;
	  fitness += val;
#ifdef	DEBUG
	  pos += val;
#endif
	}
      }

      // See if we are within the outer disk (surroundfac * radius)
      else if ( dist2 <= surroundr2 ) {
	if (image.read_pixel(i,j,val)) {
	  pixels++;
#ifdef	DEBUG
	  neg += val;
#endif
	  fitness -= val;
	}
      }
    }
  }

  if (_invert) { fitness *= -1; }
  if (pixels > 0) {
    fitness /= pixels;
  }

#ifdef	DEBUG
  printf("disk_spot_tracker::check_fitness(): pos %lg, neg %lg, normfit %lg\n", pos, neg, fitness);
#endif
  return fitness;
}

disk_spot_tracker_interp::disk_spot_tracker_interp(double radius, bool inverted, double pixelaccuracy,
				     double radiusaccuracy) :
  spot_tracker(radius, inverted, pixelaccuracy, radiusaccuracy)
{
}

// Check the fitness of the disk against an image, at the current parameter settings.
// Return the fitness value there.  This is done by multiplying the image values within
// one radius of the center by 1 and the image values beyond that but within surroundfac
// by -1 (on-center, off-surround).  If the test is inverted, then the fitness value
// is inverted before returning it.  The fitness is normalized by the number of pixels
// tested (pixels both within the radii and within the image).

// Be careful when selecting the surround fraction below.  If the area in the off-
// surround is larger than the on-area, the code seeks for dark surround more than
// for bright center, effectively causing it to seek an inverted patch that is
// that many times as large as the radius (switches dark-on-light vs. light-on-dark
// behavior).

// We assume that we are looking at a smooth function, so we do linear
// interpolation and sample within the space of the disk kernel, rather than
// point-sampling the nearest pixel.

// XXX This function should keep track of the last radius it computed the list
// of points for and rebuild an "inside" and "outside" list when it changes.
// Then the list can be re-used and the math avoided if the radius does not
// change, making this run a lot faster.

double	disk_spot_tracker_interp::check_fitness(const image_wrapper &image)
{
  double  r,theta;			//< Coordinates in disk space
  int	  pixels = 0;			//< How many pixels we ended up using
  double  fitness = 0.0;		//< Accumulates the fitness values
  double  val;				//< Pixel value read from the image
  double  surroundfac = 1.3;		//< How much larger the surround is
  double  surroundr = _rad*surroundfac;  //< The surround "off" disk radius

  // Start with the pixel in the middle.
  if (image.read_pixel_bilerp(_x,_y,val)) {
    pixels++;
    fitness += val;
  }

  // Pixels within the radius have positive weights
  // Shift the start location by 1/2 pixel on each outgoing ring, to
  // keep them from linking up with each other.
  for (r = 1; r <= _rad; r++) {
    double rads_per_pixel = 1 / r;
    for (theta = r*rads_per_pixel*0.5; theta <= 2*M_PI + r*rads_per_pixel*0.5; theta += rads_per_pixel) {
      if (image.read_pixel_bilerp(_x+r*cos(theta),_y+r*sin(theta),val)) {
	pixels++;
	fitness += val;
      }
    }
  }

  // Pixels outside the radius have negative weights
  // Shift the start location by 1/2 pixel on each outgoing ring, to
  // keep them from linking up with each other.
  for (r = r /* Keep going */; r <= surroundr; r++) {
    double rads_per_pixel = 1 / r;
    for (theta = r*rads_per_pixel*0.5; theta <= 2*M_PI + r*rads_per_pixel*0.5; theta += rads_per_pixel) {
      if (image.read_pixel_bilerp(_x+r*cos(theta),_y+r*sin(theta),val)) {
	pixels++;
	fitness -= val;
      }
    }
  }

  if (_invert) { fitness *= -1; }
  if (pixels == 0) {
    return 0;
  } else {
    fitness /= pixels;
  }

  return fitness;
}

disk_spot_tracker_interp2::disk_spot_tracker_interp2(double radius, bool inverted, double pixelaccuracy,
				     double radiusaccuracy) :
  spot_tracker(radius, inverted, pixelaccuracy, radiusaccuracy)
{
}

// Check the fitness of the disk against an image, at the current parameter settings.
// Return the fitness value there.  This is done by multiplying the image values within
// one radius of the center by 1 and the image values beyond that but within surroundfac
// by -1 (on-center, off-surround).  If the test is inverted, then the fitness value
// is inverted before returning it.  The fitness is normalized by the number of pixels
// tested (pixels both within the radii and within the image).

// Be careful when selecting the surround fraction below.  If the area in the off-
// surround is larger than the on-area, the code seeks for dark surround more than
// for bright center, effectively causing it to seek an inverted patch that is
// that many times as large as the radius (switches dark-on-light vs. light-on-dark
// behavior).

// We assume that we are looking at a smooth function, so we do linear
// interpolation and sample within the space of the disk kernel, rather than
// point-sampling the nearest pixel.

double	disk_spot_tracker_interp2::check_fitness(const image_wrapper &image)
{
  int	  i,j;
  int	  pixels = 0;			//< How many pixels we ended up using
  double  fitness = 0.0;		//< Accumulates the fitness values
  double  val;				//< Pixel value read from the image
  double  surroundfac = 1.3;		//< How much larger the surround is
  double  centerr2 = _rad * _rad;	//< Square of the center "on" disk radius
  double  surroundr2 = centerr2*surroundfac*surroundfac;  //< Square of the surround "off" disk radius
  double  dist2;			//< Square of the distance from the center

  int range = (int)ceil(surroundfac*_rad);
  if (range < 0) { return 0; }	//< Avoid divide-by-zero pixel errors
  for (i = -range; i <= range; i++) {
    for (j = -range; j <= range; j++) {
      dist2 = (i)*(i) + (j)*(j);

      // See if we are within the inner disk
      if ( dist2 <= centerr2 ) {
	if (image.read_pixel_bilerp(_x+i,_y+j,val)) {
	  pixels++;
	  fitness += val;
	}
      }

      // See if we are within the outer disk (surroundfac * radius)
      else if ( dist2 <= surroundr2 ) {
	if (image.read_pixel_bilerp(_x+i,_y+j,val)) {
	  pixels++;
	  fitness -= val;
	}
      }
    }
  }

  if (_invert) { fitness *= -1; }
  fitness /= pixels;

  return fitness;
}
