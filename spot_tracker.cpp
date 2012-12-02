#include  <math.h>
#include  <stdio.h>
#include  "spot_tracker.h"

// For PlaySound()
#ifdef _WIN32
#include <MMSystem.h>
#endif

//#define DEBUG
#ifndef	M_PI
#ifndef M_PI_DEFINED
const double M_PI = 2*asin(1.0);
#define M_PI_DEFINED
#endif
#endif

#ifndef min
#define min(a,b) ( (a)<(b)?(a):(b) )
#endif

// Find the maximum of three elements.  Return the
// index of which one was picked.
double max3(double v0, double v1, double v2, unsigned &index) {
  double max = v0; index = 0;
  if (v1 > max) { max = v1; index = 1; }
  if (v2 > max) { max = v2; index = 2; }
  return max;
}

spot_tracker_XY::spot_tracker_XY(double radius, bool inverted, double pixelaccuracy, double radiusaccuracy,
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

// Optimize starting at the specified location to find the best-fit location.
// Take only one optimization step.  Return whether we ended up finding a
// better location or not.  Return new location in any case.  One step means
// one step in X,Y, and radius space each.  The boolean parameters tell
// whether to optimize in each direction (X, Y, and Radius).
bool  spot_tracker_XY::take_single_optimization_step(const image_wrapper &image, unsigned rgb, double &x, double &y,
						       bool do_x, bool do_y, bool do_r)
{
  double  new_fitness;	    //< Checked fitness value to see if it is better than current one
  bool	  betterxy = false; //< Do we find a better location?
  bool	  betterrad = false;//< Do we find a better radius?

  // Try going in +/- X and see if we find a better location.  It is
  // important that we check both directions before deciding to step
  // to avoid unbiased estimations.
  if (do_x) {
    double v0, vplus, vminus;
    double starting_x = get_x();
    v0 = _fitness;                                      // Value at starting location
    set_location(starting_x + _pixelstep, get_y());	// Try going a step in +X
    vplus = check_fitness(image, rgb);
    set_location(starting_x - _pixelstep, get_y());	// Try going a step in -X
    vminus = check_fitness(image, rgb);
    unsigned which;
    new_fitness = max3(v0, vplus, vminus, which);
    switch (which) {
      case 0: set_location(starting_x, get_y());
              break;
      case 1: set_location(starting_x + _pixelstep, get_y());
              betterxy = true;
              break;
      case 2: set_location(starting_x - _pixelstep, get_y());
              betterxy = true;
              break;
    }
    _fitness = new_fitness;
  }

  // Try going in +/- Y and see if we find a better location.  It is
  // important that we check both directions before deciding to step
  // to avoid unbiased estimations.
  if (do_y) {
    double v0, vplus, vminus;
    double starting_y = get_y();
    v0 = _fitness;                                      // Value at starting location
    set_location(get_x(), starting_y + _pixelstep);	// Try going a step in +Y
    vplus = check_fitness(image, rgb);
    set_location(get_x(), starting_y - _pixelstep);	// Try going a step in -Y
    vminus = check_fitness(image, rgb);
    unsigned which;
    new_fitness = max3(v0, vplus, vminus, which);
    switch (which) {
      case 0: set_location(get_x(), starting_y);
              break;
      case 1: set_location(get_x(), starting_y + _pixelstep);
              betterxy = true;
              break;
      case 2: set_location(get_x(), starting_y - _pixelstep);
              betterxy = true;
              break;
    }
    _fitness = new_fitness;
  }

  // Try going in +/- radius and see if we find a better value.
  // Don't let the radius get below 1 pixel.  It is
  // important that we check both directions before deciding to step
  // to avoid unbiased estimations.
  if (do_r) {
    double v0, vplus, vminus;
    double starting_rad = get_radius();
    v0 = _fitness;                                      // Value at starting radius
    set_radius(starting_rad + _radstep);	// Try going a step in +radius
    vplus = check_fitness(image, rgb);
    if (_rad - _radstep >= 1) {  // Don't let it get less than 1
      set_radius(starting_rad - _radstep);	// Try going a step in -radius
      vminus = check_fitness(image, rgb);
    } else {
      vminus = v0 - 1;  // Don't make it want to step in this direction
    }
    unsigned which;
    new_fitness = max3(v0, vplus, vminus, which);
    switch (which) {
      case 0: set_radius(starting_rad);
              break;
      case 1: set_radius(starting_rad + _radstep);
              betterrad = true;
              break;
      case 2: set_radius(starting_rad - _radstep);
              betterrad = true;
              break;
    }
    _fitness = new_fitness;
  }

  // Return the new location and whether we found a better one.
  x = get_x(); y = get_y();
  return betterxy || betterrad;
}

// Find the best fit for the spot detector within the image, taking steps
// that are 1/4 of the bead's radius.  Stay away from edge effects by staying
// away from the edges of the image.
void  spot_tracker_XY::locate_good_fit_in_image(const image_wrapper &image, unsigned rgb, double &x, double &y)
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
      set_location(i,j);
      if ( bestfit < (newfit = check_fitness(image, rgb)) ) {
	bestfit = newfit;
	bestx = get_x();
	besty = get_y();
      }
    }
  }

  set_location(bestx, besty);
  x = get_x();
  y = get_y();
  _fitness = newfit;
}

// Continue to optimize until we can't do any better (the step size drops below
// the minimum.
void  spot_tracker_XY::optimize(const image_wrapper &image, unsigned rgb, double &x, double &y)
{
  // Set the step sizes to a large value to start with
  _pixelstep = 2;
  _radstep = 2;

  // Find out what our current value is (presumably this is a new image)
  _fitness = check_fitness(image, rgb);

  // Try with ever-smaller steps until we reach the smallest size and
  // can't do any better.
  do {
    // Repeat the optimization steps until we can't do any better.
    while (take_single_optimization_step(image, rgb, x, y, true, true, true)) {};

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
void  spot_tracker_XY::optimize_xy(const image_wrapper &image, unsigned rgb, double &x, double &y)
{
  int optsteps_tried = 0;

  // Set the step sizes to a large value to start with
  _pixelstep = 2;

  // Find out what our current value is (presumably this is a new image)
  _fitness = check_fitness(image, rgb);

  // Try with ever-smaller steps until we reach the smallest size and
  // can't do any better.
  do {
    optsteps_tried += 4;
    // Repeat the optimization steps until we can't do any better.
    while (take_single_optimization_step(image, rgb, x, y, true, true, false)) {optsteps_tried += 4;};

    // Try to see if we reducing the step sizes helps, until it gets too small.
    if ( _pixelstep <= _pixelacc ) {
      break;
    }
    _pixelstep /= 2;
  } while (true);
#ifdef	DEBUG
  printf("%d optimization steps tried\n", optsteps_tried);
#endif
}

/// Optimize in X and Y by solving separately for the best-fit parabola in X and Y
// to three samples starting from the center and separated by the sample distance.
// The minimum for the parabola is the best location (if it has a minimum; otherwise
// just stay where we started because it is hopeless).
void  spot_tracker_XY::optimize_xy_parabolafit(const image_wrapper &image, unsigned rgb, double &x, double &y)
{
  double xn = get_x() - _samplesep;
  double x0 = get_x();
  double xp = get_x() + _samplesep;

  double yn = get_y() - _samplesep;
  double y0 = get_y();
  double yp = get_y() + _samplesep;

  // Check all of the fitness values at and surrounding the center.
  double fx0 = check_fitness(image, rgb);
  double fy0 = fx0;
  set_location(xn, y0);
  double fxn = check_fitness(image, rgb);
  set_location(xp, y0);
  double fxp = check_fitness(image, rgb);
  set_location(x0, yn);
  double fyn = check_fitness(image, rgb);
  set_location(x0, yp);
  double fyp = check_fitness(image, rgb);

  // Put the location back at the center, in case we can't do any
  // better.
  set_location(x0,y0);

  // Find parabola in X that passes through the three points.  If it has
  // a maximum, set the location to that maximum.
  // Equation is f(x) = ax^2 + bx + c; to simplify things, we decide that
  // the x0 location is at x = 0 (which means we'll be solving for an offset
  // in x).  We also decide that we're going to take unit steps to make it easier
  // to solve the equations; this means that we need to scale the offser by
  // _samplesep.  This makes c = fx0.  Then we get two equations:
  // fxp = a + b + fx0, fxn = a - b + fx0.  (fxp-fx0) = a + b, (fxn-fx0) = a - b;
  // (fxp-fx0) + (fxn-fx0) = 2a; a = ((fxp-fx0) + (fxn-fx0)) / 2, a = (fxp+fxn)/2 - fx0;
  // Plugging back in gives us b = (fxp-fx0) - a.  There is a maximum of a < 0.
  // The maximum is located at the zero of the derivative of f(x): f'(x) = 2ax + b;
  // 0 = 2ax + b; 2ax = -b; x = -b/(2a).
  double c = fx0;
  double a = (fxp+fxn)/2 - fx0;
  if (a < 0) {
    double b = (fxp-fx0) - a;
    double xmax = -b / (2*a);
    // Don't move more than 1.5 _samplesep units in one step, that's too much
    // extrapolation
    if ( fabs(xmax) <= 1.5) {
      set_location(get_x() + xmax * _samplesep, get_y());
    }
  }

  // Find parabola in Y that passes through the three points.  If it has
  // a maximum, set the location to that maximum.
  // Use the same equations as above, but putting y in place of x and fy in
  // place of fx.
  c = fy0;
  a = (fyp+fyn)/2 - fy0;
  if (a < 0) {
    double b = (fyp-fy0) - a;
    double ymax = -b / (2*a);
    // Don't move more than 1.5 _samplesep units in one step, that's too much
    // extrapolation
    if ( fabs(ymax) <= 1.5) {
      set_location(get_x(), get_y() + ymax * _samplesep);
    }
  }

  // Report where we ended up.
  x = get_x();
  y = get_y();
}

spot_tracker_Z::spot_tracker_Z(double minz, double maxz, double radius, double depthaccuracy) :
    _z(0.0),
    _minz(minz), _maxz(maxz),
    _radius(radius),
    _depthacc(depthaccuracy)  // Minimum step size in pixels to try
{
}

// Optimize starting at the specified depth to find the best-fit slice.
// Take only one optimization step.  Return whether we ended up finding a
// better depth or not.  Return new depth in any case.
bool  spot_tracker_Z::take_single_optimization_step(const image_wrapper &image, unsigned rgb, double x, double y, double &z)
{
  double  new_fitness;	    //< Checked fitness value to see if it is better than current one
  bool	  betterz = false;  //< Do we find a better depth?

  // Try going in +/- Z and see if we find a better location.
  double v0, vplus, vminus;
  double starting_z = get_z();
  v0 = _fitness;                                      // Value at starting location
  set_z(starting_z + _depthstep);	// Try going a step in +Z
  vplus = check_fitness(image, rgb, x,y);
  set_z(starting_z - _depthstep);	// Try going a step in -Z
  vminus = check_fitness(image, rgb, x,y);
  unsigned which;
  new_fitness = max3(v0, vplus, vminus, which);
  switch (which) {
    case 0: set_z(starting_z);
            break;
    case 1: set_z(starting_z + _depthstep);
            betterz = true;
            break;
    case 2: set_z(starting_z - _depthstep);
            betterz = true;
            break;
  }
  _fitness = new_fitness;

  // Return the new location and whether we found a better one.
  z = get_z();
  return betterz;
}

// Find the best fit for the spot detector within the image, taking steps
// that are 1 slice apart.  Stay away from edge effects by staying
// away from the edges of the image.
void  spot_tracker_Z::locate_best_fit_in_depth(const image_wrapper &image, unsigned rgb, double x, double y, double &z)
{
  int i;
  int bestz = 0;
  double bestfit = -1e10;
  double  newfit;

  int ilow = (int)(_minz + 1);
  int ihigh = (int)(_maxz - 1);
  for (i = ilow; i <= ihigh; i ++) {
    set_z(i);
    if ( bestfit < (newfit = check_fitness(image, rgb, x,y)) ) {
      bestfit = newfit;
      bestz = i;
    }
  }

  set_z(bestz);
  z = get_z();
  _fitness = newfit;
}

// Find a close fit for the spot detector within the image, taking ten steps
// through the whole stack.  Stay away from edge effects by staying
// away from the edges of the image.
void  spot_tracker_Z::locate_close_fit_in_depth(const image_wrapper &image, unsigned rgb, double x, double y, double &z)
{
  int i;
  int bestz = 0;
  double bestfit = -1e10;
  double  newfit;

  int ilow = static_cast<int>((_minz + 1));
  int ihigh = static_cast<int>((_maxz - 1));
  int step = static_cast<int>((_maxz - _minz - 2) / 10);
  if (step == 0) { step = 1; }
  for (i = ilow; i <= ihigh; i += step) {
    set_z(i);
    if ( bestfit < (newfit = check_fitness(image, rgb, x,y)) ) {
      bestfit = newfit;
      bestz = i;
    }
  }

  set_z(bestz);
  z = get_z();
  _fitness = newfit;
}

// Continue to optimize until we can't do any better (the step size drops below
// the minimum.
void  spot_tracker_Z::optimize(const image_wrapper &image, unsigned rgb, double x, double y, double &z)
{
  // Set the step size to a large value to start with
  _depthstep = 2;

  // Find out what our current value is (presumably this is a new image)
  _fitness = check_fitness(image, rgb, x,y);

  // Try with ever-smaller steps until we reach the smallest size and
  // can't do any better.
  do {
    // Repeat the optimization steps until we can't do any better.
    while (take_single_optimization_step(image, rgb, x, y, z)) {};

    // Try to see if we reducing the step sizes helps.
    if ( _depthstep > _depthacc ) {
      _depthstep /= 2;
    } else {
      break;
    }
  } while (true);
}

local_max_spot_tracker::local_max_spot_tracker(double radius, bool inverted, double pixelaccuracy,
											   double radiusaccuracy, double sample_separation_in_pixels) :
	spot_tracker_XY(radius, inverted, pixelaccuracy, radiusaccuracy, sample_separation_in_pixels)
{
	
}

double local_max_spot_tracker::check_fitness(const image_wrapper &image, unsigned rgb)
{
	// It doesn't matter what we return, because we don't even call this method.
	return 0.0;
}

bool local_max_spot_tracker::take_single_optimization_step(const image_wrapper &image, unsigned rgb, 
														   double &x, double &y, bool do_x, bool do_y, bool do_r)
{
	// Find maximum value within the radius of the spot.
	double maximum = 0.0;
	double radius2 = this->get_radius() * this->get_radius();
	double val = 0.0;
	double i, j;
	int ilow = (int)floor(get_x() - _rad);
	int ihigh = (int)ceil(get_x() + _rad);
	int jlow = (int)floor(get_y() - _rad);
	int jhigh = (int)ceil(get_y() + _rad);
	for (i = ilow; i <= ihigh; i += _samplesep) {
		for (j = jlow; j <= jhigh; j += _samplesep) {
			double dist2 = (i-get_x())*(i-get_x()) + (j-get_y())*(j-get_y());
			if (dist2 <= radius2) {
				if (image.read_pixel((int) i, (int) j, val, rgb) && val > maximum) { 
					maximum = val;
				}
			}
		}
	}

	// Find centroid of all pixels with maximum value
	double pixelCount = 0.0;
	double centroidX = 0.0, centroidY = 0.0;
	for (i = ilow; i <= ihigh; i += _samplesep) {
		for (j = jlow; j <= jhigh; j += _samplesep) {
			double dist2 = (i-get_x())*(i-get_x()) + (j-get_y())*(j-get_y());
			if (dist2 <= radius2) {
				if (image.read_pixel((int)i, (int)j, val, rgb) && val == maximum) {
					centroidX += (int) i;
					centroidY += (int) j;
					pixelCount += 1.0;
				}
			}
		}
	}

	set_location(centroidX / pixelCount, centroidY / pixelCount);
	x = get_x();
	y = get_y();

	// We'll always find the local max with a single call, so we'll trick the
	// caller into thinking we haven't found a better solution, causing the caller
	// to stop optimization.
	return false;
}


disk_spot_tracker::disk_spot_tracker(double radius, bool inverted, double pixelaccuracy,
				     double radiusaccuracy, double sample_separation_in_pixels) :
    spot_tracker_XY(radius, inverted, pixelaccuracy, radiusaccuracy, sample_separation_in_pixels)
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

double	disk_spot_tracker::check_fitness(const image_wrapper &image, unsigned rgb)
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

  int ilow = (int)floor(get_x() - surroundfac*_rad);
  int ihigh = (int)ceil(get_x() + surroundfac*_rad);
  int jlow = (int)floor(get_y() - surroundfac*_rad);
  int jhigh = (int)ceil(get_y() + surroundfac*_rad);
  for (i = ilow; i <= ihigh; i += _samplesep) {
    for (j = jlow; j <= jhigh; j += _samplesep) {
      dist2 = (i-get_x())*(i-get_x()) + (j-get_y())*(j-get_y());

      // See if we are within the inner disk
      if ( dist2 <= centerr2 ) {
	if (image.read_pixel((int)i,(int)j,val, rgb)) {
	  pixels++;
	  fitness += val;
#ifdef	DEBUG
	  pos += val;
#endif
	}
      }

      // See if we are within the outer disk (surroundfac * radius)
      else if ( dist2 <= surroundr2 ) {
	if (image.read_pixel((int)i,(int)j,val, rgb)) {
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
  spot_tracker_XY(radius, inverted, pixelaccuracy, radiusaccuracy, sample_separation_in_pixels)
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

double	disk_spot_tracker_interp::check_fitness(const image_wrapper &image, unsigned rgb)
{
  double  r,theta;			//< Coordinates in disk space
  int	  pixels = 0;			//< How many pixels we ended up using
  double  fitness = 0.0;		//< Accumulates the fitness values
  double  val;				//< Pixel value read from the image
  double  surroundfac = 1.3;		//< How much larger the surround is
  double  surroundr = _rad*surroundfac;  //< The surround "off" disk radius

  // Start with the pixel in the middle.
  if (image.read_pixel_bilerp(get_x(),get_y(),val, rgb)) {
    pixels++;
    fitness += val;
  }

  // Pixels within the radius have positive weights
  // Shift the start location by 1/2 step on each outgoing ring, to
  // keep them from lining up with each other.
  for (r = _samplesep; r <= _rad; r += _samplesep) {
    double rads_per_step = 1 / r * _samplesep;
    for (theta = r*rads_per_step*0.5; theta <= 2*M_PI + r*rads_per_step*0.5; theta += rads_per_step) {
      if (image.read_pixel_bilerp(get_x()+r*cos(theta),get_y()+r*sin(theta),val, rgb)) {
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
      if (image.read_pixel_bilerp(get_x()+r*cos(theta),get_y()+r*sin(theta),val, rgb)) {
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
  spot_tracker_XY(radius, inverted, pixelaccuracy, radiusaccuracy, sample_separation_in_pixels)
{
}

// Check the fitness of the disk against an image, at the current parameter settings.
// Return the fitness value there.  This is done by multiplying the image values within
// one radius of the center by a value the falls off from 1 at the center to -1 at the
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

double	cone_spot_tracker_interp::check_fitness(const image_wrapper &image, unsigned rgb)
{
  double  r,theta;			//< Coordinates in disk space
  int	  pixels = 0;			//< How many pixels we ended up using
  double  fitness = 0.0;		//< Accumulates the fitness values
  double  val;				//< Pixel value read from the image

  // Start with the pixel in the middle.
  if (image.read_pixel_bilerp(get_x(),get_y(),val, rgb)) {
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
      if (image.read_pixel_bilerp(get_x()+r*cos(theta),get_y()+r*sin(theta),val, rgb)) {
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
  spot_tracker_XY(radius, inverted, pixelaccuracy, radiusaccuracy, sample_separation_in_pixels),
  _MAX_RADIUS(100), _radius_lists(NULL)
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

    // The floating-point value of how far we've come from the center of
    // the circle; the integer count of steps times the step size.
    double scaled_r = r * _samplesep;

    // How many radians are in a step of one _samplesep at the given
    // radius.  There are 2pi radians at a distance of 1 from the
    // center, and 2pi*scaled_r at the scaled_r distance.  So the
    // radians/2pi = _samplestep/(2pi*scaled_r), so
    // radians = _samplesep / scaled_r;
    double rads_per_step = _samplesep / scaled_r;

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

double	symmetric_spot_tracker_interp::check_fitness(const image_wrapper &image, unsigned rgb)
{
  double  val;				//< Pixel value read from the image
  double  pixels;			//< How many pixels we ended up using (used in floating-point calculations only)
  double  ring_variance_sum = 0.0;	//< Accumulates the variance around the rings
  offset  **which_list = &_radius_lists[1];  //< Point at the first list, use increment later to advance
  int	  r;				//< Loops over radii from 1 up
  int	  count;			//< How many entries in a particular list

  // Don't check the pixel in the middle; it makes no difference to circular
  // symmetry.
  for (r = 1; r <= _rad / _samplesep; r++) {
    double squareValSum = 0.0;		//< Accumulates the variance around a ring
    double valSum = 0.0;		//< Accumulates the mean around a ring
    int	pix;
    offset *list = *(which_list++);	//< Makes a speed difference to do this with increment vs. index

    pixels = 0.0;	// No pixels in this circle yet.
    count = _radius_counts[r];
    for (pix = 0; pix < count; pix++) {
// Switching to the version that does not check boundaries didn't make it faster by much at all...
// Using it would mean somehow clipping the boundaries before calling these functions, which would
// surely slow things down.
#if 1
      if (image.read_pixel_bilerp(get_x()+list->x,get_y()+list->y,val, rgb)) {
	valSum += val;
	squareValSum += val*val;
	pixels++;
	list++;	  //< Makes big speed difference to do this with increment vs. index
      }
#else
      val = image.read_pixel_bilerp_nocheck(get_x()+list->x, get_y()+list->y, rgb);
      valSum += val;
      squareValSum += val*val;
      pixels++;
      list++;
#endif
    }

    // Calculate the variance around the ring using the formulation
    // variance = sum_over_points(val^2) - sum_over_points(val)^2 / count,
    // where val is the value at each point and count is the number of points.
    // Accumulate these into the ring variance sum.
    if (pixels) {
      ring_variance_sum += squareValSum - valSum*valSum / pixels;
    }
  }

  return -ring_variance_sum;
}

image_spot_tracker_interp::image_spot_tracker_interp(double radius, bool inverted, double pixelaccuracy,
				     double radiusaccuracy, double sample_separation_in_pixels) :
    spot_tracker_XY(radius, inverted, pixelaccuracy, radiusaccuracy, sample_separation_in_pixels)
{
  // Make sure the parameters make sense
  if (_rad <= 0) {
    fprintf(stderr, "image_spot_tracker::image_spot_tracker(): Invalid radius, using 1.0\n");
    _rad = 1.0;
  }
  if (_pixelacc <= 0) {
    fprintf(stderr, "image_spot_tracker::image_spot_tracker(): Invalid pixel accuracy, using 0.25\n");
    _pixelacc = 0.25;
  }
  if (_radacc <= 0) {
    fprintf(stderr, "image_spot_tracker::image_spot_tracker(): Invalid radius accuracy, using 0.25\n");
    _radacc = 0.25;
  }
  if (_samplesep <= 0) {
    fprintf(stderr, "image_spot_tracker::image_spot_tracker(): Invalid sample spacing, using 1.00\n");
    _samplesep = 1.0;
  }

  // Set the initial step sizes for radius and pixels
  _pixelstep = 2.0; if (_pixelstep < 4*_pixelacc) { _pixelstep = 4*_pixelacc; };
  _radstep = 2.0; if (_radstep < 4*_radacc) { _radstep = 4*_radacc; };

  // No test image yet
  _testimage = NULL;
}

image_spot_tracker_interp::~image_spot_tracker_interp()
{
  if (_testimage != NULL) {
    delete [] _testimage;
    _testimage = NULL;
  }
}

bool	image_spot_tracker_interp::set_image(const image_wrapper &image, unsigned rgb, double x, double y, double rad)
{
  // Find out the desired test radius, clip it to fit within the test
  // image boundaries, and make sure it is valid.
  int desired_rad = (int)ceil(rad);

  int minx, maxx, miny, maxy;
  image.read_range(minx, maxx, miny, maxy);
  // Subtract one because bilerp will sometimes go one beyond the
  // pixel you want, which can be beyond the image.
  desired_rad = (int)min(desired_rad, x - minx - 1);
  desired_rad = (int)min(desired_rad, y - miny - 1);
  desired_rad = (int)min(desired_rad, maxx - x - 1);
  desired_rad = (int)min(desired_rad, maxy - y - 1);

  if (desired_rad <= 0) {
    fprintf(stderr,"image_spot_tracker_interp::set_image(): Non-positive radius, giving up\n");
    if (_testimage) {
      delete [] _testimage;
      _testimage = NULL;
    }
    return false;
  }

  // If we have a test image already whose radius is too small, then
  // delete the existing image.
  if (_testrad && (desired_rad > _testrad)) {
    delete [] _testimage;
    _testimage = NULL;
  }

  // Set the parameters for the test image.  The x and y values are set to
  // point at the pixel in the middle of the stored test image, NOT at the locations
  // in the original image.
  _testrad = desired_rad;
  _testx = desired_rad;
  _testy = desired_rad;
  _testsize = 2 * desired_rad + 1;

  // If we have not test image (we may have just deleted it), then
  // allocate space to store a new one.
  if ( (_testimage = new double[_testsize*_testsize]) == NULL) {
    fprintf(stderr,"image_spot_tracker_interp::set_image(): Out of memory allocating image (%dx%d)\n", _testsize, _testsize);
    return false;
  }

  // Sample the input image into the test image, interpolating between pixels.
  int xsamp, ysamp;
  for (xsamp = -desired_rad; xsamp <= desired_rad; xsamp++) {
    for (ysamp = -desired_rad; ysamp <= desired_rad; ysamp++) {
      _testimage[_testx + xsamp + _testsize * (_testy + ysamp)] = image.read_pixel_bilerp_nocheck(x + xsamp, y + ysamp, rgb);
    }
  }

  return true;
}

// Check the fitness of the stored image against another image, at the current parameter settings.
// Return the fitness value there.

// We assume that we are looking at a smooth function, so we do linear
// interpolation and sample within the space of the kernel, rather than
// point-sampling the nearest pixel.

double	image_spot_tracker_interp::check_fitness(const image_wrapper &image, unsigned rgb)
{
  double  val;				//< Pixel value read from the image
  double  pixels = 0;			//< How many pixels we ended up using (used in floating-point calculations only)
  double  fitness = 0.0;		//< Accumulates the fitness values
  double  x, y;				//< Loops over coordinates, distance from the center.

  // If we haven't ever gotten the original image, go ahead and grab it now from
  // the image we've been asked to optimize from.
  if (_testimage == NULL) {
    fprintf(stderr,"image_spot_tracker_interp::check_fitness(): Called before set_image() succeeded (grabbing from image)\n");
    set_image(image, rgb, get_x(), get_y(), get_radius());
    if (_testimage == NULL) {
      return 0;
    }
  }

  // If our center is outside of the image, return a very low fitness value.
  int minx, miny, maxx, maxy;
  image.read_range(minx, maxx, miny, maxy);
  if ( (get_x() < minx) || (get_x() > maxx) || (get_y() < miny) || (get_y() > maxy) ) {
    fitness = -1e10;
    return fitness;
  }

  // Find the fitness.
  for (x = -_testrad; x <= _testrad; x++) {
    for (y = -_testrad; y <= _testrad; y++) {
      if (image.read_pixel_bilerp(get_x()+x,get_y()+y,val, rgb)) {
	double myval = _testimage[(int)(_testx+x) + _testsize * (int)(_testy+y)];
	double squarediff = (val-myval) * (val-myval);
	fitness -= squarediff;
	pixels++;
      }
    }
  }

  // Normalize the fitness value by the number of pixels we have chosen,
  // or set it low if we never found any.
  if (pixels) {
    fitness /= pixels;
  } else {
    fitness = -1e10;
  }

  // We never invert the fitness: we don't care whether it is a dark
  // or bright spot.
  return fitness;
}

// Check the fitness of the stored image against another image, at the current parameter settings.
// Return the fitness value there.

// We assume that we are looking at a smooth function, so we do linear
// interpolation and sample within the space of the kernel, rather than
// point-sampling the nearest pixel.

double	twolines_image_spot_tracker_interp::check_fitness(const image_wrapper &image, unsigned rgb)
{
  double  val;				//< Pixel value read from the image
  double  pixels = 0;			//< How many pixels we ended up using (used in floating-point calculations only)
  double  fitness = 0.0;		//< Accumulates the fitness values
  double  x, y;				//< Loops over coordinates, distance from the center.

  // If we haven't ever gotten the original image, go ahead and grab it now from
  // the image we've been asked to optimize from.
  if (_testimage == NULL) {
    fprintf(stderr,"image_spot_tracker_interp::check_fitness(): Called before set_image() succeeded (grabbing from image)\n");
    set_image(image, rgb, get_x(), get_y(), get_radius());
    if (_testimage == NULL) {
      return 0;
    }
  }

  // If our center is outside of the image, return a very low fitness value.
  int minx, miny, maxx, maxy;
  image.read_range(minx, maxx, miny, maxy);
  if ( (get_x() < minx) || (get_x() > maxx) || (get_y() < miny) || (get_y() > maxy) ) {
    fitness = -1e10;
    return fitness;
  }

  // Check the X cross section through the center, then the Y.
  for (x = -_testrad; x <= _testrad; x++) {
    if (image.read_pixel_bilerp(get_x()+x,get_y(),val, rgb)) {
      double myval = _testimage[(int)(_testx+x) + _testsize * (int)(_testy)];
      double squarediff = (val-myval) * (val-myval);
      fitness -= squarediff;
      pixels++;
    }
  }
  for (y = -_testrad; y <= _testrad; y++) {
    if (image.read_pixel_bilerp(get_x(),get_y()+y,val, rgb)) {
      double myval = _testimage[(int)(_testx) + _testsize * (int)(_testy+y)];
      double squarediff = (val-myval) * (val-myval);
      fitness -= squarediff;
      pixels++;
    }
  }

  // Normalize the fitness value by the number of pixels we have chosen,
  // or set it low if we never found any.
  if (pixels) {
    fitness /= pixels;
  } else {
    fitness = -1e10;
  }

  // We never invert the fitness: we don't care whether it is a dark
  // or bright spot.
  return fitness;
}

Gaussian_spot_tracker::Gaussian_spot_tracker(double radius, bool inverted, double pixelaccuracy,
				     double radiusaccuracy, double sample_separation_in_pixels,
                                     double background, double summedvalue) :
    spot_tracker_XY(radius, inverted, pixelaccuracy, radiusaccuracy, sample_separation_in_pixels),
    _testimage(NULL), _background(background), _summedvalue(summedvalue)
{
  // Make sure the parameters make sense
  if (_rad <= 0) {
    fprintf(stderr, "Gaussian_spot_tracker::Gaussian_spot_tracker(): Invalid radius, using 1.0\n");
    _rad = 1.0;
  }
  if (_pixelacc <= 0) {
    fprintf(stderr, "Gaussian_spot_tracker::Gaussian_spot_tracker(): Invalid pixel accuracy, using 0.25\n");
    _pixelacc = 0.25;
  }
  if (_radacc <= 0) {
    fprintf(stderr, "Gaussian_spot_tracker::Gaussian_spot_tracker(): Invalid radius accuracy, using 0.25\n");
    _radacc = 0.25;
  }
  if (_samplesep <= 0) {
    fprintf(stderr, "Gaussian_spot_tracker::Gaussian_spot_tracker(): Invalid sample spacing, using 1.00\n");
    _samplesep = 1.0;
  }

  // Set the initial step sizes for radius and pixels
  _pixelstep = 2.0; if (_pixelstep < 4*_pixelacc) { _pixelstep = 4*_pixelacc; };
  _radstep = 2.0; if (_radstep < 4*_radacc) { _radstep = 4*_radacc; };

  // Store an invalid last width so we allocate the testimage the first time.
  _lastwidth = -1;
}

Gaussian_spot_tracker::~Gaussian_spot_tracker()
{
  if (_testimage != NULL) {
    delete _testimage;
    _testimage = NULL;
  }
}

// Check the fitness of the stored image against another image, at the current parameter settings.
// Return the fitness value there.

// This is an image-to-image least-squares match.  We do not interpolate between pixels
// in the image we're fitting, but rather shift the Gaussian within the pixels of our
// test image.  We then sum the per-pixel squared error between the test image and
// the image we're optimizing to.

double	Gaussian_spot_tracker::check_fitness(const image_wrapper &image, unsigned rgb)
{
  // If we're inverting the Gaussian, do so by negating the summed value.
  double summed_value = _summedvalue;
  if (_invert) { summed_value *= -1; }

  // Make sure that the sample separation in pixels is at most 1 so we get a
  // sample per pixel at least, and also make sure that it is nonzero to avoid
  // divide-by-zero problems.
  double sample_separation_in_pixels = _samplesep;
  if (sample_separation_in_pixels > 1) { sample_separation_in_pixels = 1; }
  if (sample_separation_in_pixels < 0.01) { sample_separation_in_pixels = 0.01; }

  // Figure out the offset for the Gaussian image we want to compute.  This is
  // the floor of the translation, so it will always be between 0 and 1 in X and Y.
  // We will translate the center pixel (0,0) of the Gaussian image so that
  // it lies at the floor location when we do our pixel-by-pixel calculations
  // below.
  int x_int = static_cast<int>(floor(get_x()));
  int y_int = static_cast<int>(floor(get_y()));
  double x_frac = get_x() - x_int;
  double y_frac = get_y() - y_int;

  // If we have changed the width since before, deallocate the last
  // image (if any).
  double new_width = get_radius() * 4 + 1;
  if (_lastwidth != new_width) {
    if (_testimage) { delete _testimage; _testimage = NULL; }
    _lastwidth = new_width;
  }

  // If we don't have an image allocated, then create a new one with the appropriate
  // parameters.  Otherwise, just fill in the existing image with new values.  Allocate
  // out to twice the radius on each side (two standard deviations).
  if (!_testimage) {
    _testimage = new Integrated_Gaussian_image(static_cast<int>(-2*get_radius()),
      static_cast<int>(2*get_radius()),
      static_cast<int>(-2*get_radius()),
      static_cast<int>(2*get_radius()),
      _background, 0.0, x_frac,y_frac, get_radius(), summed_value,
      static_cast<int>(1/sample_separation_in_pixels));
  } else {
    _testimage->recompute(_background, 0.0, x_frac,y_frac, get_radius(), summed_value, static_cast<int>(1/sample_separation_in_pixels));
  }

  // Compute the sum of the squared errors between the test image and the image
  // we're optimizing against.  The Gaussian image will have been shifted by
  // the fractional part of the offset between the Gaussian and the test image, so
  // we shift by the integral part when we do our pixel comparisons.
  // We go out to twice the standard deviation (which is how large the Gaussian
  // image is).  WE DO NOT BILERP either image, we read its pixels directly
  // because we've shifted the Gaussian within the _testimage and resampled
  // at the same resolution as the image we're optimizing against.
  int x,y;
  int pixels = 0;
  double val, myval;
  double fitness = 0.0;
  int double_rad = static_cast<int>(2*_rad);
  for (x = -double_rad; x <= double_rad; x++) {
    for (y = -double_rad; y <= double_rad; y++) {
      if (image.read_pixel(x_int+x,y_int+y,val, rgb)) {
        _testimage->read_pixel(x, y, myval, 0);
	double squarediff = (val-myval) * (val-myval);
	fitness -= squarediff;
	pixels++;
      }
    }
  }

  if (pixels == 0) {
    return 0.0;
  } else {
    return fitness / pixels;
  }
}

FIONA_spot_tracker::FIONA_spot_tracker(double radius,
		    bool inverted,
		    double pixelaccuracy,
		    double radiusaccuracy,
		    double sample_separation_in_pixels,
                    double background,
                    double summedvalue) :
    spot_tracker_XY(radius, inverted, pixelaccuracy, radiusaccuracy, sample_separation_in_pixels),
    _background(background), _summedvalue(summedvalue)
{
  // Make sure the parameters make sense
  if (_rad <= 0) {
    fprintf(stderr, "FIONA_spot_tracker::FIONA_spot_tracker(): Invalid radius, using 1.0\n");
    _rad = 1.0;
  }
  if (_pixelacc <= 0) {
    fprintf(stderr, "FIONA_spot_tracker::FIONA_spot_tracker(): Invalid pixel accuracy, using 0.25\n");
    _pixelacc = 0.25;
  }
  if (_radacc <= 0) {
    fprintf(stderr, "FIONA_spot_tracker::FIONA_spot_tracker(): Invalid radius accuracy, using 0.25\n");
    _radacc = 0.25;
  }
  if (_samplesep <= 0) {
    fprintf(stderr, "FIONA_spot_tracker::FIONA_spot_tracker(): Invalid sample spacing, using 1.00\n");
    _samplesep = 1.0;
  }
  if (_summedvalue < 0) {
    fprintf(stderr, "FIONA_spot_tracker::FIONA_spot_tracker(): Negative summed value, using 0.00\n");
    _summedvalue = 0.0;
  }

  // Set the initial step sizes for radius and pixels
  _pixelstep = 2.0; if (_pixelstep < 4*_pixelacc) { _pixelstep = 4*_pixelacc; };
  _radstep = 2.0; if (_radstep < 4*_radacc) { _radstep = 4*_radacc; };
}

// Check the fitness of the stored image against another image, at the current parameter settings.
// Return the fitness value there.

// This is an image-to-image least-squares match.  We do not interpolate between pixels
// in the image we're fitting, but rather shift the Gaussian within the pixels of our
// test image.  We then sum the per-pixel squared error between the test image and
// the image we're optimizing to.

double	FIONA_spot_tracker::check_fitness(const image_wrapper &image, unsigned rgb)
{
  // If we're inverting the Gaussian, do so by negating the summed value.
  double summed_value = _summedvalue;
  if (_invert) { summed_value *= -1; }

  // Figure out the offset for the Gaussian image we want to compute.  This is
  // the floor of the translation, so it will always be between 0 and 1 in X and Y.
  // We will translate the center pixel (0,0) of the Gaussian image so that
  // it lies at the floor location when we do our pixel-by-pixel calculations
  // below.
  int x_int = static_cast<int>(floor(get_x()));
  int y_int = static_cast<int>(floor(get_y()));
  double x_frac = get_x() - x_int;
  double y_frac = get_y() - y_int;

  _testimage.set_new_parameters(_background, 0.0, x_frac,y_frac, get_radius(), summed_value);

  // Figure out how far to check.  This should be 4 times the radius of the kernel.  In
  // Selvin's code, they pick a 15-pixel square around the code, but this causes interference
  // from nearby spots that causes optimization to fail.
  int halfwidth = static_cast<int>(4*_rad);
  //if (halfwidth < 15) { halfwidth = 15; }

  // Compute the sum of the squared errors between the test image and the image
  // we're optimizing against.  The Gaussian image will have been shifted by
  // the fractional part of the offset between the Gaussian and the test image, so
  // we shift by the integral part when we do our pixel comparisons.
  // We go out to twice the standard deviation (which is how large the Gaussian
  // image is).  WE DO NOT BILERP either image, we read its pixels directly
  // because we've shifted the Gaussian within the _testimage and resampled
  // at the same resolution as the image we're optimizing against.

  // To avoid causing sudden discontinuities at the pixel boundaries, where a
  // new set of pixels coming in/out produces a sudden cliff in the value and thus
  // makes the tracker prefer not to cross pixel boundaries, we do the core of the
  // pixels at full strength but then do the edge pixels in a separate step, with
  // their errors weighted by their fractional pixel coverage.  We then do the
  // corner pixels, with their appropriate bilerp weight, so that they don't pop
  // in and out suddenly.
  int x,y;
  unsigned pixels = 0;
  double val, myval;
  double fitness = 0.0;
  for (x = -halfwidth+1; x < halfwidth; x++) {
    for (y = -halfwidth+1; y < halfwidth; y++) {
      if (image.read_pixel(x_int+x,y_int+y,val, rgb)) {
        _testimage.read_pixel(x, y, myval, 0);
	double squarediff = (val-myval) * (val-myval);
	fitness -= squarediff;
	pixels++;
      }
    }
  }
  // Boundaries along the sides
  int xmin = x_int - halfwidth;
  double xminweight = (1 - x_frac);
  int xmax = x_int + halfwidth;
  double xmaxweight = x_frac;
  for (y = -halfwidth+1; y < halfwidth; y++) {
    if (image.read_pixel(xmin,y_int+y,val, rgb)) {
      _testimage.read_pixel(-halfwidth, y, myval, 0);
	double squarediff = (val-myval) * (val-myval);
	fitness -= squarediff * xminweight;
	pixels++;
    }
    if (image.read_pixel(xmax,y_int+y,val, rgb)) {
      _testimage.read_pixel(+halfwidth, y, myval, 0);
	double squarediff = (val-myval) * (val-myval);
	fitness -= squarediff * xmaxweight;
	pixels++;
    }
  }
  int ymin = y_int - halfwidth;
  double yminweight = (1 - y_frac);
  int ymax = y_int + halfwidth;
  double ymaxweight = y_frac;
  // Boundaries at the top and bottom
  for (x = -halfwidth+1; x < halfwidth; x++) {
    if (image.read_pixel(x_int+x,ymin,val, rgb)) {
      _testimage.read_pixel(x, -halfwidth, myval, 0);
	double squarediff = (val-myval) * (val-myval);
	fitness -= squarediff * yminweight;
	pixels++;
    }
    if (image.read_pixel(x_int+x,ymax,val, rgb)) {
      _testimage.read_pixel(x, +halfwidth, myval, 0);
	double squarediff = (val-myval) * (val-myval);
	fitness -= squarediff * ymaxweight;
	pixels++;
    }
  }
  // Four corners
  if (image.read_pixel(xmin,ymin,val, rgb)) {
    _testimage.read_pixel(-halfwidth, -halfwidth, myval, 0);
	double squarediff = (val-myval) * (val-myval);
	fitness -= squarediff * xminweight * yminweight;
	pixels++;
  }
  if (image.read_pixel(xmax,ymin,val, rgb)) {
    _testimage.read_pixel(+halfwidth, -halfwidth, myval, 0);
	double squarediff = (val-myval) * (val-myval);
	fitness -= squarediff * xmaxweight * yminweight;
	pixels++;
  }
  if (image.read_pixel(xmin,ymax,val, rgb)) {
    _testimage.read_pixel(-halfwidth, +halfwidth, myval, 0);
	double squarediff = (val-myval) * (val-myval);
	fitness -= squarediff * xminweight * ymaxweight;
	pixels++;
  }
  if (image.read_pixel(xmax,ymax,val, rgb)) {
    _testimage.read_pixel(+halfwidth, +halfwidth, myval, 0);
	double squarediff = (val-myval) * (val-myval);
	fitness -= squarediff * xmaxweight * ymaxweight;
	pixels++;
  }

  if (pixels == 0) {
    return 0.0;
  } else {
    // Note: Not dividing by the number of pixels here means that larger radii may be
    // avoided because they make more pixels tested, which can only add noise.  However, dividing by the
    // radius causes the kernel size to blow up.
    return fitness;
  }
}

// Optimize starting at the specified location to find the best-fit Gaussian.
// Take only one optimization step.  Return whether we ended up finding a
// better location or not.  Return new location in any case.  One step means
// one step in X,Y, and radius space each.  The boolean parameters tell
// whether to optimize in each direction (X, Y, and Radius).
bool  FIONA_spot_tracker::take_single_optimization_step(const image_wrapper &image, unsigned rgb, double &x, double &y,
						       bool do_x, bool do_y, bool do_r)
{
  double  new_fitness;	    //< Checked fitness value to see if it is better than current one

  // Never try to take larger than single-pixel steps in XY or single-value change in radius,
  // to avoid jumping out of the local best fit.
  if (_pixelstep > 1.0) { _pixelstep = 1.0; }
  if (_radstep > 1.0) { _radstep = 1.0; }

  // Unless our summedvalue is zero, call the base-class single-step routine
  // to optimize the position and radius.
  bool betterbase = false;
  if (_summedvalue > 0) {
    betterbase = spot_tracker_XY::take_single_optimization_step(image, rgb, x, y, do_x, do_y, true);
  }

  // Try adjusting the background up and down by a number of counts that is equal to 10X the
  // radius step size.  We use 10X to give the radius the chance to adjust itself faster.
  // XXX This should probably be a decoupled parameter.
  double v0, vplus, vminus;
  bool	  betterbackground = false; //< Do we find a better location?
  double starting_background = _background;
  v0 = _fitness;                                    // Value at start
  _background = starting_background + 10*_radstep; // Try looking for larger background
  vplus = check_fitness(image, rgb);
  _background = starting_background - 10*_radstep; // Try looking for smaller background
  vminus = check_fitness(image, rgb);
  unsigned which;
  new_fitness = max3(v0, vplus, vminus, which);
  switch (which) {
    case 0: _background = starting_background;
            break;
    case 1: _background = starting_background + 10*_radstep;
            betterbackground = true;
            break;
    case 2: _background = starting_background - 10*_radstep;
            betterbackground = true;
            break;
  }
  _fitness = new_fitness;

  // Try adjusting the total summedvalue up and down by 1000X the radius step size.
  // XXX This should almost certainly be a decoupled parameter.
  bool	  bettersummedvalue = false;//< Do we find a better radius?
  double starting_summedvalue = _summedvalue;
  v0 = _fitness;                                    // Value at start
  double plusvalue = starting_summedvalue + 1000 * _radstep; // Try looking for larger summedvalue
  double minusvalue = starting_summedvalue - 1000 * _radstep; // Try looking for smaller summedvalue

  // Don't go below zero for a summed value (inversion takes place when the kernel is made).
  if (plusvalue < 0) { plusvalue = 0; }
  if (minusvalue < 0) { minusvalue = 0; }

  _summedvalue = plusvalue;
  vplus = check_fitness(image, rgb);
  _summedvalue = minusvalue; // Try looking for smaller summedvalue
  vminus = check_fitness(image, rgb);
  new_fitness = max3(v0, vplus, vminus, which);
  switch (which) {
    case 0: _summedvalue = starting_summedvalue;
            break;
    case 1: _summedvalue = plusvalue;
            bettersummedvalue = true;
            break;
    case 2: _summedvalue = minusvalue;
            bettersummedvalue = true;
            break;
  }
  _fitness = new_fitness;

  // Return the new location and whether we found a better one.
  x = get_x(); y = get_y();
  return betterbase || betterbackground || bettersummedvalue;
}


rod3_spot_tracker_interp::rod3_spot_tracker_interp(const disk_spot_tracker *,
			    double radius, bool inverted,
			    double pixelaccuracy, double radiusaccuracy,
			    double sample_separation_in_pixels,
			    double length, double orientation) :
    spot_tracker_XY(radius, inverted, pixelaccuracy, radiusaccuracy, sample_separation_in_pixels),
    d_length(length),
    d_orientation(orientation),
    d_center(NULL),
    d_beginning(NULL),
    d_end(NULL)
{
  // Create three subordinate trackers of type cone_spot_tracker_interp
  if ( NULL == (d_center = new disk_spot_tracker(radius, inverted, pixelaccuracy, radiusaccuracy, sample_separation_in_pixels)) ) {
    fprintf(stderr,"rod3_spot_tracker_interp::rod3_spot_tracker_interp(): Cannot create center subortinate tracker\n");
    return;
  }
  if ( NULL == (d_beginning = new disk_spot_tracker(radius, inverted, pixelaccuracy, radiusaccuracy, sample_separation_in_pixels)) ) {
    fprintf(stderr,"rod3_spot_tracker_interp::rod3_spot_tracker_interp(): Cannot create beginning subortinate tracker\n");
    return;
  }
  if ( NULL == (d_end = new disk_spot_tracker(radius, inverted, pixelaccuracy, radiusaccuracy, sample_separation_in_pixels)) ) {
    fprintf(stderr,"rod3_spot_tracker_interp::rod3_spot_tracker_interp(): Cannot create end subortinate tracker\n");
    return;
  }

  // Move them to the proper locations based on the length and orientation.
  // This means offsetting the beginning and end from the center based on the orientation and length.
  update_beginning_and_end_locations();
}

rod3_spot_tracker_interp::rod3_spot_tracker_interp(const disk_spot_tracker_interp *,
			    double radius, bool inverted,
			    double pixelaccuracy, double radiusaccuracy,
			    double sample_separation_in_pixels,
			    double length, double orientation) :
    spot_tracker_XY(radius, inverted, pixelaccuracy, radiusaccuracy, sample_separation_in_pixels),
    d_length(length),
    d_orientation(orientation),
    d_center(NULL),
    d_beginning(NULL),
    d_end(NULL)
{
  // Create three subordinate trackers of type cone_spot_tracker_interp
  if ( NULL == (d_center = new disk_spot_tracker_interp(radius, inverted, pixelaccuracy, radiusaccuracy, sample_separation_in_pixels)) ) {
    fprintf(stderr,"rod3_spot_tracker_interp::rod3_spot_tracker_interp(): Cannot create center subortinate tracker\n");
    return;
  }
  if ( NULL == (d_beginning = new disk_spot_tracker_interp(radius, inverted, pixelaccuracy, radiusaccuracy, sample_separation_in_pixels)) ) {
    fprintf(stderr,"rod3_spot_tracker_interp::rod3_spot_tracker_interp(): Cannot create beginning subortinate tracker\n");
    return;
  }
  if ( NULL == (d_end = new disk_spot_tracker_interp(radius, inverted, pixelaccuracy, radiusaccuracy, sample_separation_in_pixels)) ) {
    fprintf(stderr,"rod3_spot_tracker_interp::rod3_spot_tracker_interp(): Cannot create end subortinate tracker\n");
    return;
  }

  // Move them to the proper locations based on the length and orientation.
  // This means offsetting the beginning and end from the center based on the orientation and length.
  update_beginning_and_end_locations();
}

rod3_spot_tracker_interp::rod3_spot_tracker_interp(const cone_spot_tracker_interp *,
			    double radius, bool inverted,
			    double pixelaccuracy, double radiusaccuracy,
			    double sample_separation_in_pixels,
			    double length, double orientation) :
    spot_tracker_XY(radius, inverted, pixelaccuracy, radiusaccuracy, sample_separation_in_pixels),
    d_length(length),
    d_orientation(orientation),
    d_center(NULL),
    d_beginning(NULL),
    d_end(NULL)
{
  // Create three subordinate trackers of type cone_spot_tracker_interp
  if ( NULL == (d_center = new cone_spot_tracker_interp(radius, inverted, pixelaccuracy, radiusaccuracy, sample_separation_in_pixels)) ) {
    fprintf(stderr,"rod3_spot_tracker_interp::rod3_spot_tracker_interp(): Cannot create center subortinate tracker\n");
    return;
  }
  if ( NULL == (d_beginning = new cone_spot_tracker_interp(radius, inverted, pixelaccuracy, radiusaccuracy, sample_separation_in_pixels)) ) {
    fprintf(stderr,"rod3_spot_tracker_interp::rod3_spot_tracker_interp(): Cannot create beginning subortinate tracker\n");
    return;
  }
  if ( NULL == (d_end = new cone_spot_tracker_interp(radius, inverted, pixelaccuracy, radiusaccuracy, sample_separation_in_pixels)) ) {
    fprintf(stderr,"rod3_spot_tracker_interp::rod3_spot_tracker_interp(): Cannot create end subortinate tracker\n");
    return;
  }

  // Move them to the proper locations based on the length and orientation.
  // This means offsetting the beginning and end from the center based on the orientation and length.
  update_beginning_and_end_locations();
}

rod3_spot_tracker_interp::rod3_spot_tracker_interp(const symmetric_spot_tracker_interp *,
			    double radius, bool inverted,
			    double pixelaccuracy, double radiusaccuracy,
			    double sample_separation_in_pixels,
			    double length, double orientation) :
    spot_tracker_XY(radius, inverted, pixelaccuracy, radiusaccuracy, sample_separation_in_pixels),
    d_length(length),
    d_orientation(orientation),
    d_center(NULL),
    d_beginning(NULL),
    d_end(NULL)
{
  // Create three subordinate trackers of type symmetric_spot_tracker_interp
  if ( NULL == (d_center = new symmetric_spot_tracker_interp(radius, inverted, pixelaccuracy, radiusaccuracy, sample_separation_in_pixels)) ) {
    fprintf(stderr,"rod3_spot_tracker_interp::rod3_spot_tracker_interp(): Cannot create center subortinate tracker\n");
    return;
  }
  if ( NULL == (d_beginning = new symmetric_spot_tracker_interp(radius, inverted, pixelaccuracy, radiusaccuracy, sample_separation_in_pixels)) ) {
    fprintf(stderr,"rod3_spot_tracker_interp::rod3_spot_tracker_interp(): Cannot create beginning subortinate tracker\n");
    return;
  }
  if ( NULL == (d_end = new symmetric_spot_tracker_interp(radius, inverted, pixelaccuracy, radiusaccuracy, sample_separation_in_pixels)) ) {
    fprintf(stderr,"rod3_spot_tracker_interp::rod3_spot_tracker_interp(): Cannot create end subortinate tracker\n");
    return;
  }

  // Move them to the proper locations based on the length and orientation.
  // This means offsetting the beginning and end from the center based on the orientation and length.
  update_beginning_and_end_locations();
}

double rod3_spot_tracker_interp::check_fitness(const image_wrapper &image, unsigned rgb)
{
  if (d_center && d_beginning && d_end) {
    return d_center->check_fitness(image, rgb) + d_beginning->check_fitness(image, rgb) + d_end->check_fitness(image, rgb);
  } else {
    fprintf(stderr, "rod3_spot_tracker_interp::check_fitness(): Missing subordinate tracker!\n");
    return 0;
  }
}

// Sets the locations of the beginning and end subtrackers based on the
// position of the center subtracker and the length and orientation.
void rod3_spot_tracker_interp::update_beginning_and_end_locations(void)
{
  if (d_center) {
    if (d_beginning) {
      d_beginning->set_location(d_center->get_x() - (d_length/2) * cos(d_orientation * (M_PI / 180)),
				d_center->get_y() - (d_length/2) * sin(d_orientation * (M_PI / 180)) );
    }
    if (d_end) {
      d_end->set_location(d_center->get_x() + (d_length/2) * cos(d_orientation * (M_PI / 180)),
			  d_center->get_y() + (d_length/2) * sin(d_orientation * (M_PI / 180)) );
    }
  }
}

bool  rod3_spot_tracker_interp::take_single_optimization_step(const image_wrapper &image, unsigned rgb, double &x, double &y,
						       bool do_x, bool do_y, bool do_r)
{
  double  new_fitness;	    //< Checked fitness value to see if it is better than current one
  bool	  betterbase = false; //< Did the base class find a better fit?
  bool	  betterorient = false;//< Do we find a better orientation?

  // See if the base class fitting steps can do better
  betterbase = spot_tracker_XY::take_single_optimization_step(image, rgb, x, y, do_x, do_y, do_r);

  // Try going in +/- orient and see if we find a better orientation.
  // XXX HACK: Orientation step is set based on the pixelstep and scaled so
  // that each endpoint will move one pixelstep unit as the orientation changes.

  double orient_step = _pixelstep * (180 / M_PI) / (d_length / 2);
  {
    double v0, vplus, vminus;
    double starting_o = get_orientation();
    v0 = _fitness;                                      // Value at starting location
    set_orientation(starting_o + orient_step);	// Try going a step in +orient
    vplus = check_fitness(image, rgb);
    set_orientation(starting_o - orient_step);	// Try going a step in -orient
    vminus = check_fitness(image, rgb);
    unsigned which;
    new_fitness = max3(v0, vplus, vminus, which);
    switch (which) {
      case 0: set_orientation(starting_o);
              break;
      case 1: set_orientation(starting_o + orient_step);
              betterorient = true;
              break;
      case 2: set_orientation(starting_o - orient_step);
              betterorient = true;
              break;
    }
    _fitness = new_fitness;
  }

  // Return the new location and whether we found a better one.
  x = get_x(); y = get_y();
  return betterbase || betterorient;
}

#include "file_stack_server.h"

radial_average_tracker_Z::radial_average_tracker_Z(const char *in_filename, double depth_accuracy) :
  spot_tracker_Z(0.0, 0.0, 0.0, depth_accuracy),	  // Fill in the min and max z and radius from the file later
  d_radial_image(NULL)
{
    // Read in the radially-averaged point-spread function from the file whose
    // name is given.
#if defined(VST_USE_IMAGEMAGICK)
    d_radial_image = new file_stack_server(in_filename);
    if (d_radial_image == NULL) {
      fprintf(stderr,"radial_average_tracker_Z::radial_average_tracker_Z(): Could not read radial image\n");
      return;
    }
#else
      fprintf(stderr,"radial_average_tracker_Z::radial_average_tracker_Z(): file_stack_server not compiled in\n");
      return;
#endif

    // Set the minimum and maximum Z and the radius based on the file information.
    // X size in the file maps to radius (reduced by one for the zero element)
    // Y size in the file maps to Z (reduced by one for the zero element)
    int	minx, miny, maxx, maxy;
    d_radial_image->read_range(minx, maxx, miny, maxy);
    if ( (minx != 0) || (miny != 0) || (maxx <= 0) || (maxy <= 0) ) {
      fprintf(stderr,"radial_average_tracker_Z::radial_average_tracker_Z(): Bogus radial image\n");
      return;
    }
    _minz = 0;
    _maxz = maxy - 1;
    _radius = maxx - 1;
}

// Check the fitness of the radially-symmetric image against the given image, at the current parameter settings.
// Return the fitness value there.

// We assume that we are looking at a smooth function, so we do linear
// interpolation and sample within the space of the kernel, rather than
// point-sampling the nearest pixel.

double	radial_average_tracker_Z::check_fitness(const image_wrapper &image, unsigned rgb, double x, double y)
{
  double  val;				//< Pixel value read from the image
  double  pixels = 0;			//< How many pixels we ended up using (used in floating-point calculations only)
  double  fitness = 0.0;		//< Accumulates the fitness values
  double  lx, ly;			//< Loops over coordinates, distance from the center.

  // If we're outside the z range, return a large negative fitness.
  if ( (_z < _minz) || (_z > _maxz) ) {
    return -1e100;
  }

  double rad_sq = _radius * _radius;
  for (lx = -_radius; lx <= _radius; lx++) {
    for (ly = -_radius; ly <= _radius; ly++) {
      // Only count pixels inside of our radial distance
      double dist_sq = lx*lx + ly*ly;
      if ( dist_sq <= rad_sq) {
	// Only count pixels inside the other image's boundaries
	if (image.read_pixel_bilerp(x+lx,y+ly,val, rgb)) {
	  double dist = sqrt(dist_sq);
	  double myval = d_radial_image->read_pixel_bilerp_nocheck(dist, _z);

	  double squarediff = (val-myval) * (val-myval);
	  fitness -= squarediff;

	  pixels++;
	}
      }
    }
  }

  // Normalize the fitness value by the number of pixels we have chosen,
  // or leave it at zero if we never found any.
  if (pixels) {
    fitness /= pixels;
  }

  // We never invert the fitness: we don't care whether it is a dark
  // or bright spot.
  return fitness;
}

Semaphore Spot_Information::d_index_sem;
unsigned Spot_Information::d_static_index = 0;	  //< Start the first instance of a Spot_Information index at zero.
unsigned Spot_Information::get_static_index() {
  d_index_sem.p();
  unsigned val = d_static_index;
  d_index_sem.v();
  return val;
};

//----------------------------------------------------------------------------------
// Tracker_Collection_Manager class implementation

// This function allows for much simpler freeing up of std::lists of
//  Spot_Information pointers.
static bool deleteAll( Spot_Information * theElement ) {
        if (theElement != NULL)
                delete theElement; // we'll let our Spot_Information destructor do the work
        return true;
}

// This function allows for removal of lost trackers from the list.
static bool deleteLost( Spot_Information * theElement ) {
    if (theElement->lost()) {
        delete theElement; // we'll let our Spot_Information destructor do the work
        return true;
    } else {
        return false;
    }
}

Tracker_Collection_Manager::~Tracker_Collection_Manager()
{
    delete_trackers();
}

void Tracker_Collection_Manager::delete_trackers()
{
    // Delete all of the tracker objects we had created.
    d_trackers.remove_if(deleteAll);

    // No active tracker.
    d_active_tracker = -1;
}

bool Tracker_Collection_Manager::delete_tracker(unsigned which)
{
  // Make sure we have such a tracker.
  if (which >= tracker_count()) {
    return false;
  }

  // Locate the tracker and then delete it.
  unsigned i;
  std::list<Spot_Information *>::iterator loop;
  loop = d_trackers.begin();
  for (i = 0; i < which; i++) {
      loop++;
      if (loop == d_trackers.end()) {
          return false;
      }
  }
  if (loop == d_trackers.end()) {
      return false;
  } else {
      // Delete the xytracker pointed to by our list pointer.
      delete *loop;
      // Remove this element from the list.
      d_trackers.erase(loop);

      // If this was the active tracker, set the active tracker
      // to be the first tracker (or to none if there are no more
      // trackers).
      if (which == d_active_tracker) {
        if (d_trackers.size() == 0) {
          d_active_tracker = -1;
        } else {
          d_active_tracker = 0;
        }
      }

      return true;
  }
}

bool Tracker_Collection_Manager::delete_active_tracker(void)
{
  if (d_active_tracker < 0) { return false; }

  // Play the "deleted tracker" sound.
#ifdef	_WIN32
#ifndef __MINGW32__
  if (!PlaySound("deleted_tracker.wav", NULL, SND_FILENAME | SND_ASYNC)) {
    fprintf(stderr,"Cannot play sound %s\n", "deleted_tracker.wav");
  }
#endif
#endif

  return delete_tracker( d_active_tracker );
}

void Tracker_Collection_Manager::add_tracker(double x, double y, double radius)
{
    d_trackers.push_back(
      new Spot_Information(d_xy_tracker_creator(x,y,radius),
                           d_z_tracker_creator()));
    d_active_tracker = d_trackers.size()-1;

    // Set the last position in case we're doing prediction.
    spot_tracker_XY *tkr = d_trackers.back()->xytracker();
    double last_position[2];
    last_position[0] = tkr->get_x();
    last_position[1] = tkr->get_y();
    d_trackers.back()->set_last_position(last_position);
}

// Delete all trackers and replace with the correct types.
// Make sure to put them back where they came from.
// This is done by the app when it changes the kind
// of tracker it wants, after replacing the XY and Z tracker
// creation functions.
void Tracker_Collection_Manager::rebuild_trackers(void)
{
  std::list<Spot_Information *>::iterator  loop;

  for (loop = d_trackers.begin(); loop != d_trackers.end(); loop++) {
    double x = (*loop)->xytracker()->get_x();
    double y = (*loop)->xytracker()->get_y();
    double r = (*loop)->xytracker()->get_radius();

    // Be sure to delete only the trackers and not the indices when
    // rebuilding the trackers.
    {
      delete (*loop)->xytracker();
      if ( (*loop)->ztracker() ) { delete (*loop)->ztracker(); }
      (*loop)->set_xytracker(d_xy_tracker_creator(x,y,r));
      (*loop)->set_ztracker(d_z_tracker_creator());
    }
    // XXX Make depth accuracy settable
    if ((*loop)->ztracker()) { (*loop)->ztracker()->set_depth_accuracy(0.25); }
  }
}

// XXX Replace the list with a vector to make this faster?
Spot_Information *Tracker_Collection_Manager::tracker(unsigned which) const
{
    unsigned i;
    std::list<Spot_Information *>::const_iterator loop;
    loop = d_trackers.begin();
    for (i = 0; i < which; i++) {
        loop++;
        if (loop == d_trackers.end()) {
            return NULL;
        }
    }
    if (loop == d_trackers.end()) {
        return NULL;
    } else {
        return *loop;
    }
}

// Returns a pointer to the active tracker, or NULL if there is not one.
Spot_Information *Tracker_Collection_Manager::active_tracker(void) const
{
  if (d_active_tracker < 0) { return NULL; }
  return tracker(static_cast<unsigned int>(d_active_tracker));
}

bool Tracker_Collection_Manager::set_active_tracker_index(unsigned which)
{
  if (which >= d_trackers.size()) { return false; }
  d_active_tracker = which;
  return true;
}

//----------------------------------------------------------------------------
// Class to deal with storing a list of locations on the image

class Flood_Position {
public:
  Flood_Position(int x, int y) { d_x = x; d_y = y; };
  int x(void) const { return d_x; };
  int y(void) const { return d_y; };
  void set_x(int x) { d_x = x; };
  void set_y(int y) { d_y = y; };

protected:
  int   d_x;
  int   d_y;
};

//--------------------------------------------------------------------------
// Helper routine that fills all pixels that are connected to
// the indicated pixel and which have a value of -1 with the new value that
// is specified.  Recursion failed due to stack overflow when
// the whole image was one region, so the new algorithm keeps a list of
// "border positions" and repeatedly goes through that list until it is
// empty.

static void flood_connected_component(double_image *img, int x, int y, double value)
{
  int minx, maxx, miny, maxy;
  img->read_range(minx, maxx, miny, maxy);

  //------------------------------------------------------------
  // Fill in the first pixel as requested, and add it to the
  // boundary list.
  std::list<Flood_Position>  boundary;
  img->write_pixel_nocheck(x,y, value);
  boundary.push_front(Flood_Position(x,y));

  //------------------------------------------------------------
  // Go looking for ways to expand the boundary so long as the
  // boundary is not empty.  Once each boundary pixel has been
  // completely handled, delete it from the boundary list.
  while (!boundary.empty()) {
    std::list<Flood_Position>::iterator  i;

    // We add new boundary pixels to the beginning of the list so that
    // we won't see them while traversing the list: each time through,
    // we only consider the positions that were on the boundary when
    // we started through that time.
    i = boundary.begin();
    while (i != boundary.end()) {

      // Move to the location of the pixel and check around it.
      x = i->x();
      y = i->y();

      // If any of the four neighbors are valid new boundary elements,
      // mark them and insert them into the boundary list.
      if ( x-1 >= minx ) {
        if (img->read_pixel_nocheck(x-1,y) == -1) {
          img->write_pixel_nocheck(x-1,y, value);
          boundary.push_front(Flood_Position(x-1,y));
        }
      }
      if ( x+1 <= maxx ) {
        if (img->read_pixel_nocheck(x+1,y) == -1) {
          img->write_pixel_nocheck(x+1,y, value);
          boundary.push_front(Flood_Position(x+1,y));
        }
      }
      if ( y-1 >= miny ) {
        if (img->read_pixel_nocheck(x,y-1) == -1) {
          img->write_pixel_nocheck(x,y-1, value);
          boundary.push_front(Flood_Position(x,y-1));
        }
      }
      if ( y+1 <= maxy ) {
        if (img->read_pixel_nocheck(x,y+1) == -1) {
          img->write_pixel_nocheck(x,y+1, value);
          boundary.push_front(Flood_Position(x,y+1));
        }
      }

      // Get rid of this entry, stepping to the next one before doing so.
      std::list<Flood_Position>::iterator j = i;
      ++i;
      boundary.erase(j);
    }
  }
}

// Helper function for find_more_brightfield_beads_in.
// Computes a local SMD measure (cross) at the location (x,y) with
//  the specified radius.
double Tracker_Collection_Manager::localSMD(const image_wrapper &img,
                                            int x, int y, int radius,
                                            unsigned color_index)
{
	double Ia, Ib;
	double xSMD = 0;
	int n = 0;
	for (int lx = x - radius; lx < x + radius; ++lx)
	{
		img.read_pixel(lx, y, Ia, color_index);
		img.read_pixel(lx + 1, y, Ib, color_index);
		xSMD += fabs(Ia - Ib);
		++n;
	}
	xSMD = xSMD / (double)n;

	double ySMD = 0;
	n = 0;
	for (int ly = y - radius; ly < y + radius; ++ly)
	{
		img.read_pixel(x, ly, Ia, color_index);
		img.read_pixel(x, ly + 1, Ib, color_index);
		ySMD += fabs(Ia - Ib);
		++n;
	}
	ySMD = ySMD / (double)n;

	return (xSMD + ySMD);
}


// Find the specified number of additional trackers, which should be placed
// at the highest-response locations in the image that is not within one
// tracker radius of an existing tracker or within one tracker radius of the
// border.  Create new trackers at those locations.  Returns true if it was
// able to find them, false if not (or error).
// Also returns a vector of candidate vertical and horizontal lines to provide
// debugging information back to the application.
bool Tracker_Collection_Manager::find_more_brightfield_beads_in(
                                    const image_wrapper &s_image,
                                    int windowRadius,
                                    double candidate_spot_threshold,
                                    unsigned how_many_more,
                                    std::vector<int> &vertCandidates,
                                    std::vector<int> &horiCandidates)
{
	// empty out our candidate vectors...
	vertCandidates.clear();
	horiCandidates.clear();

        int i, radius;
        int minx, maxx, miny, maxy;
        s_image.read_range(minx, maxx, miny, maxy);

        int x, y;

        // first, we calculate horizontal and vertical SMDs on the global image
	double SMD = 0;
	double Ia, Ib;

	double sum = 0, max = -1, min = -1;

	// vertical SMDs
	double* vertSMDs = new double[maxx - minx + 1];
	for (x = minx; x <= maxx; ++x) {
		SMD = 0;
		// calcualte one SMD
		for (y = miny + 1; y <= maxy; ++y)
		{
                        Ia = s_image.read_pixel_nocheck(x, y);
                        Ib = s_image.read_pixel_nocheck(x, y-1);
			SMD += fabs(Ia - Ib);
		}
		// normalize by dividing by the number of pairwise computations
		SMD = SMD / (float)(maxy - miny);
		vertSMDs[x] = SMD;

		if (max == -1)
			max = SMD;
		if (min == -1)
			min = SMD;
		
		if (SMD > max)
			max = SMD;
		if (SMD < min)
			min = SMD;		

		sum += SMD;
	}

	double vertAvg = sum / ((float)maxx - minx + 1);
	//printf("min = %f, max = %f, vertAvg = %f\n", min, max, vertAvg);
	double vertThresh = 0;
	//printf("using a vertThresh = %f\n", vertThresh);

	sum = 0;
	max = -1;
	min = -1;

	// horizontal SMDs
	double* horiSMDs = new double[maxy - miny + 1];
	for (y = miny; y <= maxy; ++y)
	{
		SMD = 0;
		// calcualte one SMD
		for (x = minx + 1; x <= maxx; ++x)
		{
                        Ia = s_image.read_pixel_nocheck(x, y);
                        Ib = s_image.read_pixel_nocheck(x-1, y);
			SMD += fabs(Ia - Ib);
		}
		// normalize by dividing by the number of pairwise computations
		SMD = SMD / (float)(maxx - minx);
		horiSMDs[y] = SMD;

		if (max == -1)
			max = SMD;
		if (min == -1)
			min = SMD;
		
		if (SMD > max)
			max = SMD;
		if (SMD < min)
			min = SMD;		

		sum += SMD;
	}

	double horiAvg = sum / ((float)maxx - minx + 1);
	//printf("min = %f, max = %f, horiAvg = %f\n", min, max, horiAvg);
	double horiThresh = 0;
	//printf("using a horiThresh = %f\n", horiThresh);

	// now we need to pick out the local maxes in our SMDs as candidate bead positions

	double curMax = 0;

	// pick out local maxes on our vertical SMDs
	for (x = minx + windowRadius; x <= maxx - windowRadius; ++x) {
		curMax = 0;
		// check for the max SMD value within our window size that's > thresh
		for (i = x - windowRadius; i < x + windowRadius; ++i) {
			if (vertSMDs[i] > curMax && vertSMDs[i] > vertThresh) {
				curMax = vertSMDs[i];
			}
		}
		// if we were the max SMD value in our window, then we're a candidate!
		if (curMax == vertSMDs[x] && curMax != 0) {
			vertCandidates.push_back(x);
			x = x + (windowRadius - 1);
		}
	}

	// pick out local maxes on our horizontal SMDs
	for (y = miny + windowRadius; y <= maxy - windowRadius; ++y) {
		curMax = 0;
		// check for the max SMD value within our window size that's > thresh
		for (i = y - windowRadius; i < y + windowRadius; ++i) {
			if (horiSMDs[i] > curMax && horiSMDs[i] > horiThresh) {
				curMax = horiSMDs[i];
			}
		}
		// if we were the max SMD value in our window, then we're a candidate!
		if (curMax == horiSMDs[y] && curMax != 0) {
			horiCandidates.push_back(y);
			y = y + (windowRadius - 1);
		}
	}

	// Calculate all candidate spot locations (intersections of horizontal and
	//  vertical candidate scan lines).
        std::vector<double> candidateSpotsX;
        std::vector<double> candidateSpotsY;
        std::vector<double> candidateSpotsSMD;

	double tooClose = 5;
	double curX, curY;
	bool safe = false;
        std::list <Spot_Information *>::iterator loop;
	int cx, cy;
	spot_tracker_XY* curTracker;
	for (y = 0; y < static_cast<int>(horiCandidates.size()); ++y) {
		cy = horiCandidates[y];
		for (x = 0; x < static_cast<int>(vertCandidates.size()); ++x) {
			cx = vertCandidates[x];
			safe = true;

			// check to make sure we don't already have a tracker too close
			for (loop = d_trackers.begin(); loop != d_trackers.end(); loop++)  {
				curTracker = (*loop)->xytracker();
				curX = curTracker->get_x();
				curY = curTracker->get_y();
				if (cx >= curX - tooClose && cx <= curX + tooClose &&
					cy >= curY - tooClose && cy <= curY + tooClose ) {
				  safe = false;
				}
			}
			if (safe) {
				candidateSpotsX.push_back(cx);
				candidateSpotsY.push_back(cy);
			}
		}
	}

	// now do local SMDs at candidate spots to determine if they're actually spots.
	double maxSMD = 0;
	double minSMD = 1e50;
	double avgSMD = 0;
	SMD = 0;
	radius = static_cast<int>(d_default_radius);
	for (i = 0; i < static_cast<int>(candidateSpotsX.size()); ++i) {
		x = static_cast<int>(candidateSpotsX[i]);
		y = static_cast<int>(candidateSpotsY[i]);
		SMD = localSMD(s_image, x, y, radius, d_color_index);
		avgSMD += SMD;
                if (SMD > maxSMD) {
			maxSMD = SMD;
                }
                if (SMD < minSMD) {
			minSMD = SMD;
                }
		candidateSpotsSMD.push_back(SMD);
	}
	avgSMD /= candidateSpotsX.size();

	double SMDthresh = avgSMD * candidate_spot_threshold;

        std::list<Spot_Information*> potentialTrackers;

	int newTrackers = 0;
	for (i = 0; i < static_cast<int>(candidateSpotsSMD.size()); ++i) {
		if (candidateSpotsSMD[i] > SMDthresh) {
			// add a new potential tracker!
			++newTrackers;

			// last argument = true tells Spot_Information that this isn't an official, logged tracker
			potentialTrackers.push_back(new Spot_Information(d_xy_tracker_creator(candidateSpotsX[i],candidateSpotsY[i],d_default_radius),default_z_tracker_creator(), true));

		}
	}

	int numlost = 0;
	int numnotlost = 0;

	// check to see which candidate spots aren't immediately lost
	for (loop = potentialTrackers.begin(); loop != potentialTrackers.end(); loop++) {
		// if our candidate tracker isn't lost, then we add it to our list of real trackers
                double x,y;
                (*loop)->xytracker()->optimize_xy(s_image, d_color_index, x, y,
                    (*loop)->xytracker()->get_x(), (*loop)->xytracker()->get_y());
                // XXX This should also check for lost in non-fluorescence...
                mark_tracker_if_lost_in_fluorescence(*loop, s_image, d_default_fluorescence_lost_threshold);
		if (!(*loop)->lost()) {
			++numnotlost;
                        d_trackers.push_back(new Spot_Information(d_xy_tracker_creator((*loop)->xytracker()->get_x(),(*loop)->xytracker()->get_y(),d_default_radius),default_z_tracker_creator()));
                } else {
			++numlost;
                }
	}

	//printf("%i candidates were lost after optimizing\n", numlost);
	//printf("%i candidates were not lost.\n", numnotlost);
	
	// clean up candidate spots memory
	potentialTrackers.remove_if( deleteAll );

	// clear up our SMD memory in reverse order from allocation to make it
        // easier for the memory manager
	delete [] horiSMDs;
	delete [] vertSMDs;

        return true;
}

// Autofind fluorescence beads within the image whose pointer is passed in.
// Avoids adding trackers that are too close to other existing trackers.
// Adds beads that are above the threshold (fraction of the way from the minimum
// to the maximum pixel value in the image) and whose peak is more than the
// specified number of standard deviations brighter than the mean of the surround.
// max_regions sets how many connected components will looked at, at most; a
// value of 0 means all of them will be looked at.  Some images fill up all of
// memory and crash for some threshold values; this setting prevents that.
// Returns true on success (even if no beads found) and false on error.
bool Tracker_Collection_Manager::autofind_fluorescent_beads_in(const image_wrapper &s_image,
                                                         float thresh,
                                                         float var_thresh,
                                                         unsigned max_regions)
{
    //printf("Autofinding fluorescent beads.\n"); fflush(stdout);
    // Find out how large the image is.
    int minx, maxx, miny, maxy;
    s_image.read_range(minx, maxx, miny, maxy);

    // first, find the max and min pixel value and set our threshold
    // to be the specified fraction of the way up from the min to the
    // max.
    int x, y;
    double maxi = 0, mini = 1e50;
    double curi;
    for (y = miny; y <= maxy; ++y) {
      for (x = minx; x <= maxx; ++x) {
        curi = s_image.read_pixel_nocheck(x, y);
        if (curi > maxi) { maxi = curi; }
        if (curi < mini) { mini = curi; }
      }
    }
    double threshold = mini + (maxi-mini)*thresh;

    // Make a threshold image that has 0 everywhere our original image was
    // below threshold and -1 everywhere it was at or above threshold.  The
    // -1 value means "no label yet selected".
    double_image *threshold_image = new double_image(minx, maxx, miny, maxy);
    if (threshold_image == NULL) {
      fprintf(stderr,"Tracker_Collection_Manager::autofind_fluorescent_beads_in(): Out of memory\n");
      return false;
    }
    #pragma omp parallel for private(x)
    for (y = miny; y <= maxy; ++y) {
      for (x = minx; x <= maxx; ++x) {
        if (s_image.read_pixel_nocheck(x,y) >= threshold) {
          threshold_image->write_pixel_nocheck(x, y, -1.0);
        } else {
          threshold_image->write_pixel_nocheck(x, y, 0.0);
        }
      }
    }

    // Go through the threshold image and label each component with a number
    // greater than zero (starting with 1).  This replaces the "-1" values from
    // above along the way.  This leaves us with a set of labeled components
    // embedded in the image.
    int index = 0;
    //printf("Looking for components.\n"); fflush(stdout);
    for (x =  minx; x <= maxx; x++) {
      for (y = miny; y <= maxy; y++) {
        if (threshold_image->read_pixel_nocheck(x,y) == -1) {
          index++;
          flood_connected_component(threshold_image, x,y, index);
        }
      }
    }
    //printf("Found %d components.\n", index); fflush(stdout);

    // If we have too many components, then only use some of them.
    // This keep the program from filling up all of memory and crashing.
    if ( (max_regions > 0) && (static_cast<unsigned>(index) > max_regions) ) {
        fprintf(stderr, "Warning:Tracker_Collection_Manager::autofind_fluorescent_beads_in(): found %d components, using %d\n",
              index, max_regions);
        index = max_regions;
    }

    // Compute the center of mass of each connected component.  If we do not have a
    // tracker already too close to this center of mass, create a tracker there
    // and see if it is immediately lost.  If not, then we add it to the list of
    // trackers.

    std::list<Spot_Information *>::iterator loop;
    double tooClose = d_min_bead_separation;

    int comp;
    for (comp = 1; comp <= index; comp++) {

      // Compute the center of mass for this component.  All components exist
      // and have at least one pixel in them.
      double cx = 0, cy = 0;
      unsigned count = 0;
      for (x = minx; x <= maxx; x++) {
        for (y = miny; y <= maxy; y++) {
          if (threshold_image->read_pixel_nocheck(x,y) == comp) {
            count++;
            cx += x;
            cy += y;
          }
        }
      }
      cx /= count;
      cy /= count;

      // check to make sure we don't already have a tracker too close to where
      // we want to put the new one.
      bool safe = true;
      for (loop = d_trackers.begin(); loop != d_trackers.end(); loop++)  {
        double curX, curY;
        spot_tracker_XY *curTracker = (*loop)->xytracker();
        curX = curTracker->get_x();
        curY = curTracker->get_y();
        if ( (cx >= curX - tooClose) && (cx <= curX + tooClose) &&
             (cy >= curY - tooClose) && (cy <= curY + tooClose) ) {
          safe = false;
        }
      }
      // Check to make sure we aren't too close to the edge of the image.
      double zone = d_min_border_distance;
      if ( (cx < (minx) + zone) || (cx > (maxx) - zone) ||
           (cy < (miny) + zone) || (cy > (maxy) - zone) ) {
        safe = false;
      }
      if (safe) {
        spot_tracker_XY *xy = d_xy_tracker_creator(cx, cy, d_default_radius);
        if (xy == NULL) {
          fprintf(stderr,"Tracker_Collection_Manager::autofind_fluorescent_beads_in(): Can't make XY tracker\n");
          break;
        }
        spot_tracker_Z *z = default_z_tracker_creator();
        Spot_Information *si = new Spot_Information(xy,z);
        if (si == NULL) {
          fprintf(stderr,"Tracker_Collection_Manager::autofind_fluorescent_beads_in(): Can't make Spot Information\n");
          break;
        }
        // XXX This should also check for lost in non-fluorescence...
        mark_tracker_if_lost_in_fluorescence( si, s_image, var_thresh );
        if (si->lost()) {
          // Deleting the SpotInformation also deletes its trackers.
          delete si;
        } else {
          d_trackers.push_back(si);
        }
      }
    }

    // Clean up our temporary images in reverse order to make it easier for the
    // memory allocator.
    delete threshold_image;

    return true;
}

void Tracker_Collection_Manager::take_prediction_step(int max_tracker_to_optimize)
{
    int i;
    for (i = 0; i < (int)(d_trackers.size()); i++) {
      if ( (max_tracker_to_optimize < 0) || (i <= max_tracker_to_optimize) ) {

        // Find out where the tracker is now.  This is after it has been
        // optimized, presumably.
        spot_tracker_XY *tkr = tracker(i)->xytracker();
        double position[2];
        position[0] = tkr->get_x();
        position[1] = tkr->get_y();

        // Compute the velocity that was taken between the last position and
        // the current position, scale it, and move the tracker based on it.
        // When it is optimized, it will now be based on this new estimated
        // starting position.  The last position will have been set in the
        // previous optimization step.
        double last_position[2];
        tracker(i)->get_last_position(last_position);
        double vel[2];
        vel[0] = position[0] - last_position[0];
        vel[1] = position[1] - last_position[1];
        const double vel_frac_to_use = 0.9; //< 0.85-0.95 seems optimal for cilia in pulnix; 1 is too much, 0.83 is too little
        double new_pos[2];
        new_pos[0] = position[0] + vel[0] * vel_frac_to_use;
        new_pos[1] = position[1] + vel[1] * vel_frac_to_use;
        tkr->set_location(new_pos[0], new_pos[1]);
      }
    }
}

bool Tracker_Collection_Manager::perform_local_image_search(int max_tracker_to_optimize,
      double search_radius,
      const image_wrapper &previous_image, const image_wrapper &new_image)
{
    int i;
    #pragma omp parallel for
    for (i = 0; i < (int)(d_trackers.size()); i++) {
      if ( (max_tracker_to_optimize < 0) || (i <= max_tracker_to_optimize) ) {
        Spot_Information  *tracker = Tracker_Collection_Manager::tracker(i);
        double last_pos[2];
        tracker->get_last_position(last_pos);
        spot_tracker_XY *tkr = tracker->xytracker();
        double x_base = tkr->get_x();
        double y_base = tkr->get_y();
        double rad = tkr->get_radius();

        // Create an image spot tracker and initialize it at the location where the current
        // tracker started this frame (before prediction), but in the last image.  Grab enough
        // of the image that we will be able to check over the used_search_radius for a match.
        // Use the faster twolines version of the image-based tracker.
        twolines_image_spot_tracker_interp max_find(rad, d_invert, 1.0, 1.0, 1.0);
        max_find.set_location(last_pos[0], last_pos[1]);
        max_find.set_image(previous_image, d_color_index, last_pos[0], last_pos[1], rad + search_radius);

        // Loop over the pixels within used_search_radius of the initial location and find the
        // location with the best match over all of these points.  Do this in the current image,
        // at the (possibly-predicted) starting location and find the offset from the (possibly
        // predicted) current location to get to the right place.
        double radsq = search_radius * search_radius;
        double x_offset, y_offset;
        double best_x_offset = 0;
        double best_y_offset = 0;
        double best_value = max_find.check_fitness(new_image, d_color_index);
        for (x_offset = -floor(search_radius); x_offset <= floor(search_radius); x_offset++) {
          for (y_offset = -floor(search_radius); y_offset <= floor(search_radius); y_offset++) {
	    if ( (x_offset * x_offset) + (y_offset * y_offset) <= radsq) {
	      max_find.set_location(x_base + x_offset, y_base + y_offset);
	      double val = max_find.check_fitness(new_image, d_color_index);
	      if (val > best_value) {
	        best_x_offset = x_offset;
	        best_y_offset = y_offset;
	        best_value = val;
	      }
	    }
          }
        }

        // Put the tracker at the location of the maximum, so that it will find the
        // total maximum when it finds the local maximum.
        tracker->xytracker()->set_location(x_base + best_x_offset, y_base + best_y_offset);
      }
    }

    return true;
}

// Update the positions of the trackers we are managing based on a new image.
// For kymograph applications, we only want to optimize the first two trackers,
// so we have the ability to tell how many to optimize; by default, they
// are all optimized (negative value).
// Returns the number of beads in the track.
unsigned Tracker_Collection_Manager::optimize_based_on(const image_wrapper &s_image,
                                                       int max_tracker_to_optimize,
                                                       unsigned color_index,
                                                       bool optimize_radius,
                                                       bool parabolic_opt)
{
  // Nothing to do if no trackers.
  if (d_trackers.size() == 0) { return 0; }

#ifdef  VST_USE_CUDA
  // Figure out if we're using a symmetric kernel by dynamic casting the first
  // tracker to a symmetric kernel and seeing if we get a NULL pointer.
  bool appropriate = true;
  if (dynamic_cast<symmetric_spot_tracker_interp *>(Tracker_Collection_Manager::tracker(0)->xytracker()) == NULL) {
    appropriate = false;
  }
  // Only do this if we're not doing parabolafit or radius optimization
  if (optimize_radius || parabolic_opt) {
    appropriate = false;
  }
  // Only do this if we don't need better than 1 part in 256 localization;
  // the bilinear interpolation only handles up to 1 part in 512 positions
  // between pixels.
  if (Tracker_Collection_Manager::tracker(0)->xytracker()->get_pixel_accuracy() < 1.0/256.0 ) {
    appropriate = false;
  }

  // If we have CUDA and symmetric trackers, then go ahead and use the CUDA-accelerated
  // optimization routines.  Otherwise, or if CUDA fails, fall back to the OpenMP
  // code.

  // Make a buffer to hold a copy of the input image that will be
  // passed down to the GPU and back.  Fill it with the image we
  // got passed in.
  // XXX See if we can cast into a floating-point buffer and ask it
  // for its buffer directly to save another round of copying here;
  // this will violate information hiding but make things faster.
  VST_cuda_image_buffer copy_of_image;
  copy_of_image.buf = NULL;
  if (appropriate) {
    copy_of_image.nx = s_image.get_num_columns();
    copy_of_image.ny = s_image.get_num_rows();
    copy_of_image.buf = new float[copy_of_image.nx*copy_of_image.ny];
    if (copy_of_image.buf != NULL) {
      int x;
      #pragma omp parallel for
      for (x = 0; x < copy_of_image.nx; x++) {
        int y;
        for (y = 0; y < copy_of_image.ny; y++) {
          copy_of_image.buf[x + y*copy_of_image.nx] =
            static_cast<float>(s_image.read_pixel_nocheck(x,y) );
        }
      }
    }
  }
  unsigned num_to_opt = d_trackers.size();
  if (max_tracker_to_optimize >= 0) {
    num_to_opt = static_cast<unsigned>(max_tracker_to_optimize);
  }
  if ( appropriate && (copy_of_image.buf != NULL) &&
       VST_cuda_optimize_symmetric_trackers(copy_of_image, d_trackers, num_to_opt) ) {

    // Free the buffer
    if (copy_of_image.buf) {
      delete [] copy_of_image.buf;
      copy_of_image.buf = NULL;
    }

    // Record the last optimized position, in case we're doing either prediction
    // or a local image-matched search.
    int i;
    for (i = 0; i < (int)(d_trackers.size()); i++) {
      if ( (max_tracker_to_optimize < 0) || (i <= max_tracker_to_optimize) ) {
        Spot_Information *tracker = Tracker_Collection_Manager::tracker(i);
        spot_tracker_XY *tkr = tracker->xytracker();
        double last_position[2];
        last_position[0] = tkr->get_x();
        last_position[1] = tkr->get_y();
        tracker->set_last_position(last_position);
      }
    }
  } else {

    // Free the buffer, if it is not NULL
    if (copy_of_image.buf) { delete [] copy_of_image.buf; }

#else
  {
#endif
    int i;
    #pragma omp parallel for
    for (i = 0; i < (int)(d_trackers.size()); i++) {
      if ( (max_tracker_to_optimize < 0) || (i <= max_tracker_to_optimize) ) {
        double x, y;
        Spot_Information *tracker = Tracker_Collection_Manager::tracker(i);
        spot_tracker_XY *tkr = tracker->xytracker();
        if (parabolic_opt) {
          tkr->optimize_xy_parabolafit(s_image, color_index, x, y, tkr->get_x(),tkr->get_y() );
        } else if (optimize_radius) {
          tkr->optimize(s_image, color_index, x, y, tkr->get_x(),tkr->get_y() );
        } else {
          tkr->optimize_xy(s_image, color_index, x, y, tkr->get_x(),tkr->get_y() );
        }

        // Record the last optimized position, in case we're doing either prediction
        // or a local image-matched search.
        double last_position[2];
        last_position[0] = tkr->get_x();
        last_position[1] = tkr->get_y();
        tracker->set_last_position(last_position);
      }
    }
  }

  // Return the number of trackers.
  return d_trackers.size();
}

bool Tracker_Collection_Manager::optimize_z_based_on(const image_wrapper &s_image,
                                                       int max_tracker_to_optimize,
                                                       unsigned color_index)
{
    int i;
    #pragma omp parallel for
    for (i = 0; i < (int)(d_trackers.size()); i++) {
      if ( (max_tracker_to_optimize < 0) || (i <= max_tracker_to_optimize) ) {
        Spot_Information *tracker = Tracker_Collection_Manager::tracker(i);
        double  z = 0;
        tracker->ztracker()->locate_close_fit_in_depth(s_image, color_index, tracker->xytracker()->get_x(), tracker->xytracker()->get_y(), z);
        tracker->ztracker()->optimize(s_image, color_index, tracker->xytracker()->get_x(), tracker->xytracker()->get_y(), z);
      }
    }

    return true;
}

void Tracker_Collection_Manager::mark_all_beads_not_lost(void)
{
    int i;
    for (i = 0; i < (int)(d_trackers.size()); i++) {
        tracker(i)->lost(false);
    }
}

// Check to see if the specified tracker is lost given the specified image
// and standard-deviation threshold; the tracker is lost if its center is not
// at least the specified number of standard deviations above the mean of the
// pixels around its border.  Also returns true if the tracker is lost and
// false if it is not.
bool Tracker_Collection_Manager::mark_tracker_if_lost_in_fluorescence(Spot_Information *tracker,
                                            const image_wrapper &image,
                                            float var_thresh)
{
    if (tracker == NULL) {
        return true;    // Tracker is lost -- it doesn't even exist!
    }

    // We do this by looking at
    // the mean and variance of the pixels along a box with sides that are
    // 1.5 times the diameter of the tracker (3x the radius) and seeing if the
    // squared difference between the center pixel and mean is greater than the
    // variance scaled by the sensitivity.  If it is, there is a well-defined
    // bead and so we're not lost.  (This routine does not check the corner
    // pixels of the square surrounding, to avoid double-weighting them based
    // on our loop strategy.)
    int halfwidth = static_cast<int>(1.5 * tracker->xytracker()->get_radius());
    int x = static_cast<int>(tracker->xytracker()->get_x());
    int y = static_cast<int>(tracker->xytracker()->get_y());
    int pixels = 0;
    double val = 0;
    double mean = 0;
    double variance = 0;
    int offset;
    for (offset = -halfwidth+1; offset < halfwidth; offset++) {
      if (image.read_pixel(x-offset, y-halfwidth, val)) {
        mean += val;
        variance += val*val;
        pixels++;
      }
      if (image.read_pixel(x-offset, y+halfwidth, val)) {
        mean += val;
        variance += val*val;
        pixels++;
      }
      if (image.read_pixel(x-halfwidth, y+offset, val)) {
        mean += val;
        variance += val*val;
        pixels++;
      }
      if (image.read_pixel(x+halfwidth, y+offset, val)) {
        mean += val;
        variance += val*val;
        pixels++;
      }
    }

    // Compute the variance of all the points using the shortcut
    // formula: var = (sum of squares of measures) - (sum of measurements)^2 / n.
    // Then compute the mean.
    if (pixels) {
      variance = variance - mean*mean/pixels;
      mean /= pixels;
    }

    // Compare to see if we're lost.  Check <= rather than < because in
    // regions where the pixels clamp to 0 or max there is no difference on
    // either side of the equation and we should be lost.  Check for either
    // being smaller than the mean value or being too close to the mean
    // value.
    image.read_pixel(x, y, val);
    //printf("    dbg Loc (%d,%d) val %lf, mean %lf, var %lf\n", x,y, val, mean, variance);
    if ( (val < mean) ||
         ((val-mean)*(val-mean) <= variance * var_thresh) ) {
      tracker->lost(true);
    } else {
      tracker->lost(false);
    }

    // Return true if lost, false if not.
    return tracker->lost();
}

// Marks trackers that have wandered off of fluorescent beads.
// Returns the number of remaining trackers after the lost ones have
// been deleted.
void Tracker_Collection_Manager::mark_lost_fluorescent_beads_in(const image_wrapper &s_image,
                                                            float var_thresh)
{
    int i;
    #pragma omp parallel for
    for (i = 0; i < (int)(d_trackers.size()); i++) {
        mark_tracker_if_lost_in_fluorescence(tracker(i), s_image, var_thresh);
    }
}

// Auto-deletes trackers that have wandered off of fluorescent beads.
// Returns the number of remaining trackers after the lost ones have
// been deleted.
unsigned Tracker_Collection_Manager::delete_lost_fluorescent_beads_in(const image_wrapper &s_image,
                                                            float var_thresh)
{
  mark_lost_fluorescent_beads_in(s_image, var_thresh);
  return delete_beads_marked_as_lost();
}

// Check for lost beads.  This is done by finding the value of
// the fitness function at the actual tracker location and comparing
// it to the maximum values located on concentric rings around the
// tracker.  We look for the minimum of these maximum values (the
// deepest moat around the peak in the center) and determine if we're
// lost based on the type of kernel we are using.
// Some ideas for determining goodness of tracking for a bead...
//   (It looks like different metrics are needed for symmetric and cone and disk.)
// For symmetric:
//    Value compared to initial value when tracking that bead: When in a flat area, it can be better.
//    Minimum over area vs. center value: get low-valued lobes in certain directions, but other directions bad.
//    Maximum of all minima as you travel in different directions from center: dunno.
//    Maximum at radius from center: How do you know what radius to select?
//      Max at radius of the minimum value from center?
//      Minimum of the maxima over a number of radii? -- Yep, we're trying this one.
// FIONA kernel operates differently; it looks to see if the value at the
//    center is more than the specified fraction of twice the standard deviation away
//    from the mean of the surrounding values.  For an inverted tracker, this
//    must be below; for a non-inverted tracker it is above.
// Returns true if the tracker is lost and false if it is not.
bool Tracker_Collection_Manager::mark_tracker_if_lost_in_brightfield(Spot_Information *tracker,
                                            const image_wrapper &image,
                                            float var_thresh,
                                            KERNEL_TYPE kernel_type)
{
    if (tracker == NULL) {
        return true;    // Tracker is lost -- it doesn't even exist!
    }

    tracker->lost(false); // Not lost yet...
     if (kernel_type == KT_FIONA) {
       // Compute the mean of the values two radii out from the
       // center of the tracker.
       double mean = 0.0, value;
       double theta;
       unsigned count = 0;
       double r = 2 * tracker->xytracker()->get_radius();
       double start_x = tracker->xytracker()->get_x();
       double start_y = tracker->xytracker()->get_y();
       double x, y;
       for (theta = 0; theta < 2*M_PI; theta += r / (2 * M_PI) ) {
         x = start_x + r * cos(theta);
         y = start_y + r * sin(theta);
         if (image.read_pixel_bilerp(x, y, value, d_color_index)) {
           mean += value;
           count++;
         }
       }
       if (count != 0) {
         mean /= count;
       }

       // Compute the standard deviation of the values
       double std_dev = 0.0;
       count = 0;
       for (theta = 0; theta < 2*M_PI; theta += r / (2 * M_PI) ) {
         x = start_x + r * cos(theta);
         y = start_y + r * sin(theta);
         if (image.read_pixel_bilerp(x, y, value, d_color_index)) {
           std_dev += (mean - value) * (mean - value);
           count++;
         }
       }
       if (count > 0) {
         std_dev /= (count-1);
       }
       std_dev = sqrt(std_dev);

       // Check to see if we're lost based on how far we are above/below the
       // mean.
       double threshold = var_thresh * (2 * std_dev);
       if (image.read_pixel_bilerp(start_x, start_y, value, d_color_index)) {
         double diff = value - mean;
         if (d_invert) { diff *= -1; }
         if (diff < threshold) {
           tracker->lost(true);
         } else {
           tracker->lost(false);
         }
       }

     } else {
      double min_val = 1e100; //< Min over all circles
      double start_x = tracker->xytracker()->get_x();
      double start_y = tracker->xytracker()->get_y();
      double this_val;
      double r, theta;
      double x, y;
      // Only look every two-pixel radius from three out to the radius of the tracker.
      for (r = 3; r <= tracker->xytracker()->get_radius(); r += 2) {
        double max_val = -1e100;  //< Max over this particular circle
        // Look at every pixel around the circle.
        for (theta = 0; theta < 2*M_PI; theta += r / (2 * M_PI) ) {
          x = r * cos(theta);
          y = r * sin(theta);
	  tracker->xytracker()->set_location(x + start_x, y + start_y);
	  this_val = tracker->xytracker()->check_fitness(image, d_color_index);
	  if (this_val > max_val) { max_val = this_val; }
        }
        if (max_val < min_val) { min_val = max_val; }
      }
      //Put the tracker back where it started.
      tracker->xytracker()->set_location(start_x, start_y);

      // See if we are lost.  The way we tell if we are lost or not depends
      // on the type of kernel we are using.  It also depends on the setting of
      // the "lost sensitivity" parameter, which varies from 0 (not sensitive at
      // all) to 1 (most sensitive).
      double starting_value = tracker->xytracker()->check_fitness(image, d_color_index);
      //printf("dbg: Center = %lg, min of maxes = %lg, scale = %lg\n", starting_value, min_val, min_val/starting_value);
      if (kernel_type == KT_SYMMETRIC) {
        // The symmetric kernel reports values that are strictly non-positive for
        // its fitness function.  Some observation of values reveals that the key factor
        // seems to be how many more times negative the "moat" is than the central peak.
        // Based on a few observations of different bead tracks, it looks like a factor
        // of 2 is almost always too small, and a factor of 4-10 is almost always fine.
        // So, we'll set the "scale factor" to be between 1 (when sensitivity is just
        // above zero) and 10 (when the scale factor is just at one).  If a tracker gets
        // lost, set it to be the active tracker so the user can tell which one is
        // causing trouble.
        double scale_factor = 1 + 9*var_thresh;
        if (starting_value * scale_factor < min_val) {
          tracker->lost(true);
        } else {
          tracker->lost(false);
        }
      } else if (kernel_type == KT_CONE) {
        // Differences (not factors) of 0-5 seem more appropriate for a quick test of the cone kernel.
        double difference = 5*var_thresh;
        if (starting_value - min_val < difference) {
          tracker->lost(true);
        } else {
          tracker->lost(false);
        }
      } else {  // Must be a disk kernel
        // Differences (not factors) of 0-5 seem more appropriate for a quick test of the disk kernel.
        double difference = 5*var_thresh;
        if (starting_value - min_val < difference) {
          tracker->lost(true);
        } else {
          tracker->lost(false);
        }
      }
     }

    // Return true if lost, false if not.
    return tracker->lost();
}

// Marks trackers that have wandered off of brightfield beads.
// Returns the number of remaining trackers after the lost ones have
// been deleted.
void Tracker_Collection_Manager::mark_lost_brightfield_beads_in(const image_wrapper &s_image,
                                                            float var_thresh,
                                                            KERNEL_TYPE kernel_type)
{
    int i;
    #pragma omp parallel for
    for (i = 0; i < (int)(d_trackers.size()); i++) {
        mark_tracker_if_lost_in_brightfield(tracker(i), s_image, var_thresh,
          kernel_type);
    }
}

// Auto-deletes trackers that have wandered off of brightfield beads.
// Returns the number of remaining trackers after the lost ones have
// been deleted.
unsigned Tracker_Collection_Manager::delete_lost_brightfield_beads_in(const image_wrapper &s_image,
                                                            float var_thresh,
                                                            KERNEL_TYPE kernel_type)
{
    mark_lost_brightfield_beads_in(s_image, var_thresh, kernel_type);
    return delete_beads_marked_as_lost();
}

// Marks trackers that have gotten too close to the image edge.
// Uses the specified distance threshold to determine if they are too close.
// Returns the number of remaining trackers after any have
// been deleted.
void Tracker_Collection_Manager::mark_edge_beads_in(const image_wrapper &s_image)
{
    int i;
    #pragma omp parallel for
    for (i = 0; i < (int)(d_trackers.size()); i++) {
        Spot_Information *tkr = tracker(i);
        double x = tkr->xytracker()->get_x();
        double y = tkr->xytracker()->get_y();
        double zone = d_min_border_distance;

        // Check against the border.
        int minX, maxX, minY, maxY;
        s_image.read_range(minX, maxX, minY, maxY);
        if ( (x < (minX) + zone) || (x > (maxX) - zone) ||
             (y < (minY) + zone) || (y > (maxY) - zone) ) {
          tkr->lost(true);
        }
    }
}

// Auto-deletes trackers that have gotten too close to the image edge.
// Uses the specified distance threshold to determine if they are too close.
// Returns the number of remaining trackers after any have
// been deleted.
unsigned Tracker_Collection_Manager::delete_edge_beads_in(const image_wrapper &s_image)
{
    // Mark beads that are too near the edge and then delete them.
    mark_edge_beads_in(s_image);
    return delete_beads_marked_as_lost();
}

// Marks trackers that have gotten too close to another tracker.
// Uses the specified distance threshold to determine if they are too close.
// Returns the number of remaining trackers after any have
// been deleted.  IMPORTANT: We delete the larger-
// numbered tracker if we find two that overlap, to make the longest possible
// tracks from earlier trackers.
void Tracker_Collection_Manager::mark_colliding_beads_in(const image_wrapper &s_image)
{
  std::list<Spot_Information *>::iterator  loop;
  for (loop = d_trackers.begin(); loop != d_trackers.end(); loop++) {
    double x = (*loop)->xytracker()->get_x();
    double y = (*loop)->xytracker()->get_y();

    std::list<Spot_Information *>::iterator loop2;
    double zone2 = d_min_bead_separation * d_min_bead_separation;
    for (loop2 = d_trackers.begin(); loop2 != loop; loop2++) {
      double x2 = (*loop2)->xytracker()->get_x();
      double y2 = (*loop2)->xytracker()->get_y();
      double dist2 = ( (x-x2)*(x-x2) + (y-y2)*(y-y2) );
      if (dist2 < zone2) {
        (*loop)->lost(true);
      }
    }
  }
}

// Auto-deletes trackers that have gotten too close to another tracker.
// Uses the specified distance threshold to determine if they are too close.
// Returns the number of remaining trackers after any have
// been deleted.
unsigned Tracker_Collection_Manager::delete_colliding_beads_in(const image_wrapper &s_image)
{
    // Find out which beads are lost and then delete them
    mark_colliding_beads_in(s_image);
    return delete_beads_marked_as_lost();
}

// Auto-deletes trackers that have gotten too close to another tracker.
// Uses the specified distance threshold to determine if they are too close.
// Returns the number of remaining trackers after any have
// been deleted.
unsigned Tracker_Collection_Manager::delete_beads_marked_as_lost(void)
{
    // See if the active tracker is lost.
    bool active_lost = false;
    if (d_active_tracker >= 0) {
      active_lost = tracker(d_active_tracker)->lost();
    }

    // Remove all lost beads.
    d_trackers.remove_if(deleteLost);
 
    // If the active tracker was lost, set the active tracker to the first one.
    if (active_lost) {
      if (d_trackers.size() == 0) {
        d_active_tracker = -1;
      } else {
        d_active_tracker = 0;
      }
    }

    return d_trackers.size();
}


// Static
spot_tracker_XY *Tracker_Collection_Manager::default_xy_tracker_creator(
  double x, double y, double r)
{
  spot_tracker_XY *tracker = new symmetric_spot_tracker_interp(r, false, 0.25, 0.1, 0.25);
  if (tracker != NULL) {
    tracker->set_location(x,y);
    tracker->set_radius(r);
  }
  return tracker;
}

// Static
spot_tracker_Z *Tracker_Collection_Manager::default_z_tracker_creator()
{
    return NULL;
}
