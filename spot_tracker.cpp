#include  <math.h>
#include  <stdio.h>
#include  "spot_tracker.h"

//#define DEBUG
#ifndef	M_PI
const double M_PI = 2*asin(1);
#endif

spot_tracker::spot_tracker(double radius, bool inverted, double pixelaccuracy, double radiusaccuracy,
			   double sample_separation_in_pixels) :
    _rad(radius),	      // Initial radius of the disk
    _invert(inverted),	      // Look for black disk on white surround?
    _pixelacc(pixelaccuracy), // Minimum step size in pixels to try
    _radacc(radiusaccuracy),  // Minimum step size in radius to try
    _fitness(-1e10),	      // No good position found yet!
    _pixelstep(2),	      // Starting pixel step size
    _radstep(2),	      // Starting radius step size
    _samplesep(sample_separation_in_pixels) // Spacing between samples taken by the kernel
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

    // Try to see if we reducing the step sizes helps, until it gets too small.
    if ( _pixelstep <= _pixelacc ) {
      break;
    }
    _pixelstep /= 2;
  } while (true);
}

disk_spot_tracker::disk_spot_tracker(double radius, bool inverted, double pixelaccuracy,
				     double radiusaccuracy, double sample_separation_in_pixels) :
    spot_tracker(radius, inverted, pixelaccuracy, radiusaccuracy, sample_separation_in_pixels)
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
  if (_samplesep <= 0) {
    fprintf(stderr, "disk_spot_tracker::disk_spot_tracker(): Invalid sample spacing, using 1.00\n");
    _samplesep = 1.0;
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
  double  i,j;
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
  for (i = ilow; i <= ihigh; i += _samplesep) {
    for (j = jlow; j <= jhigh; j += _samplesep) {
      dist2 = (i-_x)*(i-_x) + (j-_y)*(j-_y);

      // See if we are within the inner disk
      if ( dist2 <= centerr2 ) {
	if (image.read_pixel((int)i,(int)j,val)) {
	  pixels++;
	  fitness += val;
#ifdef	DEBUG
	  pos += val;
#endif
	}
      }

      // See if we are within the outer disk (surroundfac * radius)
      else if ( dist2 <= surroundr2 ) {
	if (image.read_pixel((int)i,(int)j,val)) {
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
				     double radiusaccuracy, double sample_separation_in_pixels) :
  spot_tracker(radius, inverted, pixelaccuracy, radiusaccuracy, sample_separation_in_pixels)
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
  // Shift the start location by 1/2 step on each outgoing ring, to
  // keep them from lining up with each other.
  for (r = _samplesep; r <= _rad; r += _samplesep) {
    double rads_per_step = 1 / r * _samplesep;
    for (theta = r*rads_per_step*0.5; theta <= 2*M_PI + r*rads_per_step*0.5; theta += rads_per_step) {
      if (image.read_pixel_bilerp(_x+r*cos(theta),_y+r*sin(theta),val)) {
	pixels++;
	fitness += val;
      }
    }
  }

  // Pixels outside the radius have negative weights
  // Shift the start location by 1/2 step on each outgoing ring, to
  // keep them from lining up with each other.
  for (r = r /* Keep going */; r <= surroundr; r += _samplesep) {
    double rads_per_step = 1 / r * _samplesep;
    for (theta = r*rads_per_step*0.5; theta <= 2*M_PI + r*rads_per_step*0.5; theta += rads_per_step) {
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

cone_spot_tracker_interp::cone_spot_tracker_interp(double radius, bool inverted, double pixelaccuracy,
				     double radiusaccuracy, double sample_separation_in_pixels) :
  spot_tracker(radius, inverted, pixelaccuracy, radiusaccuracy, sample_separation_in_pixels)
{
}

// Check the fitness of the disk against an image, at the current parameter settings.
// Return the fitness value there.  This is done by multiplying the image values within
// one radius of the center by a value the falls off from 1 at the center to 0 at the
// radius.  If the test is inverted, then the fitness value
// is inverted before returning it.  The fitness is normalized by the number of pixels
// tested (pixels both within the radii and within the image).

// We assume that we are looking at a smooth function, so we do linear
// interpolation and sample within the space of the kernel, rather than
// point-sampling the nearest pixel.

// XXX This function should keep track of the last radius it computed the list
// of points for and rebuild an "inside" and "outside" list when it changes.
// Then the list can be re-used and the math avoided if the radius does not
// change, making this run a lot faster.  It could also keep track of the
// multiplier at each pixel.

double	cone_spot_tracker_interp::check_fitness(const image_wrapper &image)
{
  double  r,theta;			//< Coordinates in disk space
  int	  pixels = 0;			//< How many pixels we ended up using
  double  fitness = 0.0;		//< Accumulates the fitness values
  double  val;				//< Pixel value read from the image

  // Start with the pixel in the middle.
  if (image.read_pixel_bilerp(_x,_y,val)) {
    pixels++;
    fitness += val;
  }

  // Pixels within the radius have positive weights that fall off from 1
  // in the center to 0 at the radius.
  // Shift the start location by 1/2 pixel on each outgoing ring, to
  // keep them from lining up with each other.
  for (r = 1; r <= _rad; r += _samplesep) {
    double rads_per_step  = 1 / r * _samplesep;
    double weight = 1 - (r / _rad);
    for (theta = r*rads_per_step*0.5; theta <= 2*M_PI + r*rads_per_step*0.5; theta += rads_per_step) {
      if (image.read_pixel_bilerp(_x+r*cos(theta),_y+r*sin(theta),val)) {
	pixels++;
	fitness += val * weight;
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

symmetric_spot_tracker_interp::symmetric_spot_tracker_interp(double radius, bool inverted, double pixelaccuracy,
				     double radiusaccuracy, double sample_separation_in_pixels) :
  spot_tracker(radius, inverted, pixelaccuracy, radiusaccuracy, sample_separation_in_pixels),
  _MAX_RADIUS(100)
{
  // Check the radius here so we don't need to check it in the fitness routine.
  if (_rad < 1) { _rad = 1; }
  if (_rad > _MAX_RADIUS) { _rad = _MAX_RADIUS; }
  if (_samplesep < 0.1) { _samplesep = 0.1; }

  // Allocate the space for the counts of pixel in each radius
  if ( (_radius_counts = new int[(int)(_MAX_RADIUS/_samplesep+1)]) == NULL) {
    fprintf(stderr,"symmetric_spot_tracker_interp::symmetric_spot_tracker_interp(): Out of memory!\n");
    _MAX_RADIUS = 0;
    return;
  }

  // Allocate the space for each list and fill it in.
  int	  r;				//< Coordinates in disk space
  double  theta;			//< Coordinates in disk space
  int	  pixels = 0;			//< How many pixels we ended up using
  int pixel;				//< Loop variable

  // Allocate the list that holds the lists of pixels
  if ( (_radius_lists = new offset*[(int)(_MAX_RADIUS/_samplesep+1)]) == NULL) {
    fprintf(stderr,"symmetric_spot_tracker_interp::symmetric_spot_tracker_interp(): Out of memory!\n");
    _MAX_RADIUS = 0;
    return;
  }

  // Don't make one for the pixel in the middle; it makes no difference to circular
  // symmetry.
  // Shift the start location by 1/2 pixel on each outgoing ring, to
  // keep them from lining up with each other.
  _radius_lists[0] = NULL;
  for (r = 1; r <= _MAX_RADIUS / _samplesep; r++) {
    double scaled_r = r * _samplesep;
    double rads_per_step = 1.0 / scaled_r;

    pixels = (int)(1 + 2*M_PI / rads_per_step);
    if ( (_radius_lists[r] = new offset[pixels]) == NULL) {
      fprintf(stderr,"symmetric_spot_tracker_interp::symmetric_spot_tracker_interp(): Out of memory!\n");
      _MAX_RADIUS = 0;
      return;
    }
    pixel = 0;
    for (theta = r*rads_per_step*0.5; theta <= 2*M_PI + r*rads_per_step*0.5; theta += rads_per_step) {
      _radius_lists[r][pixel].x = scaled_r*cos(theta);
      _radius_lists[r][pixel].y = scaled_r*sin(theta);
      pixel++;
    }
    _radius_counts[r] = pixel;
  }
}

symmetric_spot_tracker_interp::~symmetric_spot_tracker_interp()
{
  int i;
  if (_radius_lists != NULL) {
    for (i = 0; i <= _MAX_RADIUS / _samplesep; i++) {
      if (_radius_lists[i] != NULL) { delete [] _radius_lists[i]; _radius_lists[i] = NULL; }
    }
    delete [] _radius_lists;
    _radius_lists = NULL;
  }
  if (_radius_counts) { delete [] _radius_counts; _radius_counts = NULL; }
}

// Check the fitness of the disk against an image, at the current parameter settings.
// Return the fitness value there.

// Compute the variance of all the points using the shortcut
// formula: var = (sum of squares of measures) - (sum of measurements)^2 / n

// We assume that we are looking at a smooth function, so we do linear
// interpolation and sample within the space of the kernel, rather than
// point-sampling the nearest pixel.

double	symmetric_spot_tracker_interp::check_fitness(const image_wrapper &image)
{
  double  val;				//< Pixel value read from the image
  double  pixels;			//< How many pixels we ended up using (used in floating-point calculations only)
  double  fitness = 0.0;		//< Accumulates the fitness values
  offset  **which_list = &_radius_lists[1];  //< Point at the first list, use increment later to advance
  int	  r;				//< Loops over radii from 1 up
  int	  count;			//< How many entries in a particular list

  // Don't check the pixel in the middle; it makes no difference to circular
  // symmetry.
  for (r = 1; r <= _rad / _samplesep; r++) {
    double squareValSum = 0.0;
    double valSum = 0.0;
    int	pix;
    offset *list = *(which_list++);	//< Makes a speed difference to do this with increment vs. index

    pixels = 0.0;	// No pixels in this circle yet.
    count = _radius_counts[r];
    for (pix = 0; pix < count; pix++) {
// Switching to the version that does not check boundaries didn't make it faster by much at all...
// Using it would mean somehow clipping the boundaries before calling these functions, which would
// surely slow things down.
#if 1
      if (image.read_pixel_bilerp(_x+list->x,_y+list->y,val)) {
	valSum += val;
	squareValSum += val*val;
	pixels++;
	list++;	  //< Makes big speed difference to do this with increment vs. index
      }
#else
      val = image.read_pixel_bilerp_nocheck(_x+list->x, _y+list->y);
      valSum += val;
      squareValSum += val*val;
      pixels++;
      list++;
#endif
    }
    fitness -= squareValSum - valSum*valSum / pixels;
  }

  // We never invert the fitness: we don't care whether it is a dark
  // or bright spot.
  return fitness;
}
