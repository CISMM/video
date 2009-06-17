#include  <math.h>
#include  <stdio.h>
#include  "spot_tracker.h"

//#define DEBUG
#ifndef	M_PI
#ifndef M_PI_DEFINED
const double M_PI = 2*asin(1.0);
#define M_PI_DEFINED
#endif
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

// Optimize starting at the specified location to find the best-fit disk.
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
  // Create three subordinate trackers of type cone_spot_tracker_interp
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
    d_radial_image = new file_stack_server(in_filename);
    if (d_radial_image == NULL) {
      fprintf(stderr,"radial_average_tracker_Z::radial_average_tracker_Z(): Could not read radial image\n");
      return;
    }

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
