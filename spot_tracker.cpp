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
    double starting_x = get_x();
    set_location(starting_x + _pixelstep, get_y());	// Try going a step in +X
    if ( _fitness < (new_fitness = check_fitness(image)) ) {
      _fitness = new_fitness;
      betterxy = true;
    } else {
      set_location(starting_x - _pixelstep, get_y());	// Try going a step in -X
      if ( _fitness < (new_fitness = check_fitness(image)) ) {
	_fitness = new_fitness;
	betterxy = true;
      } else {
	set_location(starting_x, get_y());	// Back where we started in X
      }
    }
  }

  // Try going in +/- Y and see if we find a better location.
  if (do_y) {
    double starting_y = get_y();
    set_location(get_x(), starting_y + _pixelstep);	// Try going a step in +Y
    if ( _fitness < (new_fitness = check_fitness(image)) ) {
      _fitness = new_fitness;
      betterxy = true;
    } else {
      set_location(get_x(), starting_y - _pixelstep);	// Try going a step in -Y
      if ( _fitness < (new_fitness = check_fitness(image)) ) {
	_fitness = new_fitness;
	betterxy = true;
      } else {
	set_location(get_x(), starting_y);	// Back where we started in Y
      }
    }
  }

  // Try going in +/- radius and see if we find a better value.
  // Don't let the radius get below 1 pixel.
  if (do_r) {
    double starting_rad = get_radius();
    set_radius(starting_rad + _radstep);	// Try going a step in +radius
    if ( _fitness < (new_fitness = check_fitness(image)) ) {
      _fitness = new_fitness;
      betterrad = true;
    } else {
      set_radius(starting_rad - _radstep);	// Try going a step in -radius
      if ( (_rad >= 1) && (_fitness < (new_fitness = check_fitness(image))) ) {
	_fitness = new_fitness;
	betterrad = true;
      } else {
	set_radius(starting_rad - _radstep);	// Back where we started in radius
      }
    }
  }

  // Return the new location and whether we found a better one.
  x = get_x(); y = get_y();
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
      set_location(i,j);
      if ( bestfit < (newfit = check_fitness(image)) ) {
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
  int optsteps_tried = 0;

  // Set the step sizes to a large value to start with
  _pixelstep = 2;

  // Find out what our current value is (presumably this is a new image)
  _fitness = check_fitness(image);

  // Try with ever-smaller steps until we reach the smallest size and
  // can't do any better.
  do {
    optsteps_tried += 4;
    // Repeat the optimization steps until we can't do any better.
    while (take_single_optimization_step(image, x, y, true, true, false)) {optsteps_tried += 4;};

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
void  spot_tracker::optimize_xy_parabolafit(const image_wrapper &image, double &x, double &y)
{
  double xn = get_x() - _samplesep;
  double x0 = get_x();
  double xp = get_x() + _samplesep;

  double yn = get_y() - _samplesep;
  double y0 = get_y();
  double yp = get_y() + _samplesep;

  // Check all of the fitness values at and surrounding the center.
  double fx0 = check_fitness(image);
  double fy0 = fx0;
  set_location(xn, y0);
  double fxn = check_fitness(image);
  set_location(xp, y0);
  double fxp = check_fitness(image);
  set_location(x0, yn);
  double fyn = check_fitness(image);
  set_location(x0, yp);
  double fyp = check_fitness(image);

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

  int ilow = (int)floor(get_x() - surroundfac*_rad);
  int ihigh = (int)ceil(get_x() + surroundfac*_rad);
  int jlow = (int)floor(get_y() - surroundfac*_rad);
  int jhigh = (int)ceil(get_y() + surroundfac*_rad);
  for (i = ilow; i <= ihigh; i += _samplesep) {
    for (j = jlow; j <= jhigh; j += _samplesep) {
      dist2 = (i-get_x())*(i-get_x()) + (j-get_y())*(j-get_y());

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
  if (image.read_pixel_bilerp(get_x(),get_y(),val)) {
    pixels++;
    fitness += val;
  }

  // Pixels within the radius have positive weights
  // Shift the start location by 1/2 step on each outgoing ring, to
  // keep them from lining up with each other.
  for (r = _samplesep; r <= _rad; r += _samplesep) {
    double rads_per_step = 1 / r * _samplesep;
    for (theta = r*rads_per_step*0.5; theta <= 2*M_PI + r*rads_per_step*0.5; theta += rads_per_step) {
      if (image.read_pixel_bilerp(get_x()+r*cos(theta),get_y()+r*sin(theta),val)) {
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
      if (image.read_pixel_bilerp(get_x()+r*cos(theta),get_y()+r*sin(theta),val)) {
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
  if (image.read_pixel_bilerp(get_x(),get_y(),val)) {
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
      if (image.read_pixel_bilerp(get_x()+r*cos(theta),get_y()+r*sin(theta),val)) {
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

double	symmetric_spot_tracker_interp::check_fitness(const image_wrapper &image)
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
    double squareValSum = 0.0;		//< Used to compute the variance around a ring
    double valSum = 0.0;		//< Used to compute the mean and variance around a ring
    int	pix;
    offset *list = *(which_list++);	//< Makes a speed difference to do this with increment vs. index

    pixels = 0.0;	// No pixels in this circle yet.
    count = _radius_counts[r];
    for (pix = 0; pix < count; pix++) {
// Switching to the version that does not check boundaries didn't make it faster by much at all...
// Using it would mean somehow clipping the boundaries before calling these functions, which would
// surely slow things down.
#if 1
      if (image.read_pixel_bilerp(get_x()+list->x,get_y()+list->y,val)) {
	valSum += val;
	squareValSum += val*val;
	pixels++;
	list++;	  //< Makes big speed difference to do this with increment vs. index
      }
#else
      val = image.read_pixel_bilerp_nocheck(get_x()+list->x, get_y()+list->y);
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
    ring_variance_sum += squareValSum - valSum*valSum / pixels;
  }

  return -ring_variance_sum;
}

image_spot_tracker_interp::image_spot_tracker_interp(double radius, bool inverted, double pixelaccuracy,
				     double radiusaccuracy, double sample_separation_in_pixels) :
    spot_tracker(radius, inverted, pixelaccuracy, radiusaccuracy, sample_separation_in_pixels)
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

bool	image_spot_tracker_interp::set_image(const image_wrapper &image, double x, double y, double rad)
{
  // Find out the desired test radius and make sure it is valid.
  int desired_rad = (int)ceil(rad);
  if (desired_rad <= 0) {
    fprintf(stderr,"image_spot_tracker_interp::set_image(): Non-positive radius, giving up\n");
    if (_testimage) {
      delete [] _testimage;
      _testimage = NULL;
    }
    return false;
  }

  // Make sure that the section of the image we are going to grab fits within
  // the image itself
  int minx, maxx, miny, maxy;
  image.read_range(minx, maxx, miny, maxy);
  if ( (x - desired_rad < minx) || (x + desired_rad > maxx) ||
       (y - desired_rad < miny) || (y + desired_rad > maxy) ) {
    fprintf(stderr,"image_spot_tracker_interp::set_image(): Test region goes outside of image, giving up\n");
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
      _testimage[_testx + xsamp + _testsize * (_testy + ysamp)] = image.read_pixel_bilerp_nocheck(x + xsamp, y + ysamp);
    }
  }

  return true;
}

// Check the fitness of the stored image against another image, at the current parameter settings.
// Return the fitness value there.

// We assume that we are looking at a smooth function, so we do linear
// interpolation and sample within the space of the kernel, rather than
// point-sampling the nearest pixel.

double	image_spot_tracker_interp::check_fitness(const image_wrapper &image)
{
  double  val;				//< Pixel value read from the image
  double  pixels = 0;			//< How many pixels we ended up using (used in floating-point calculations only)
  double  fitness = 0.0;		//< Accumulates the fitness values
  double  x, y;				//< Loops over coordinates, distance from the center.

  // If we haven't ever gotten the original image, go ahead and grab it now from
  // the image we've been asked to optimize from.
  if (_testimage == NULL) {
    fprintf(stderr,"image_spot_tracker_interp::check_fitness(): Called before set_image() succeeded (grabbing from image)\n");
    set_image(image, get_x(), get_y(), get_radius());
    if (_testimage == NULL) {
      return 0;
    }
  }

  for (x = -_testrad; x <= _testrad; x++) {
    for (y = -_testrad; y <= _testrad; y++) {
      if (image.read_pixel_bilerp(get_x()+x,get_y()+y,val)) {
	double myval = _testimage[(int)(_testx+x) + _testsize * (int)(_testy+y)];
	double squarediff = (val-myval) * (val-myval);
	fitness -= squarediff;
	pixels++;
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

// Check the fitness of the stored image against another image, at the current parameter settings.
// Return the fitness value there.

// We assume that we are looking at a smooth function, so we do linear
// interpolation and sample within the space of the kernel, rather than
// point-sampling the nearest pixel.

double	twolines_image_spot_tracker_interp::check_fitness(const image_wrapper &image)
{
  double  val;				//< Pixel value read from the image
  double  pixels = 0;			//< How many pixels we ended up using (used in floating-point calculations only)
  double  fitness = 0.0;		//< Accumulates the fitness values
  double  x, y;				//< Loops over coordinates, distance from the center.

  // If we haven't ever gotten the original image, go ahead and grab it now from
  // the image we've been asked to optimize from.
  if (_testimage == NULL) {
    fprintf(stderr,"image_spot_tracker_interp::check_fitness(): Called before set_image() succeeded (grabbing from image)\n");
    set_image(image, get_x(), get_y(), get_radius());
    if (_testimage == NULL) {
      return 0;
    }
  }

  // Check the X cross section through the center, then the Y.
  for (x = -_testrad; x <= _testrad; x++) {
    if (image.read_pixel_bilerp(get_x()+x,get_y(),val)) {
      double myval = _testimage[(int)(_testx+x) + _testsize * (int)(_testy)];
      double squarediff = (val-myval) * (val-myval);
      fitness -= squarediff;
      pixels++;
    }
  }
  for (y = -_testrad; y <= _testrad; y++) {
    if (image.read_pixel_bilerp(get_x(),get_y()+y,val)) {
      double myval = _testimage[(int)(_testx) + _testsize * (int)(_testy+y)];
      double squarediff = (val-myval) * (val-myval);
      fitness -= squarediff;
      pixels++;
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

rod3_spot_tracker_interp::rod3_spot_tracker_interp(const disk_spot_tracker *,
			    double radius, bool inverted,
			    double pixelaccuracy, double radiusaccuracy,
			    double sample_separation_in_pixels,
			    double length, double orientation) :
    spot_tracker(radius, inverted, pixelaccuracy, radiusaccuracy, sample_separation_in_pixels),
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
    spot_tracker(radius, inverted, pixelaccuracy, radiusaccuracy, sample_separation_in_pixels),
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
    spot_tracker(radius, inverted, pixelaccuracy, radiusaccuracy, sample_separation_in_pixels),
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
    spot_tracker(radius, inverted, pixelaccuracy, radiusaccuracy, sample_separation_in_pixels),
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

double rod3_spot_tracker_interp::check_fitness(const image_wrapper &image)
{
  if (d_center && d_beginning && d_end) {
    return d_center->check_fitness(image) + d_beginning->check_fitness(image) + d_end->check_fitness(image);
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

bool  rod3_spot_tracker_interp::take_single_optimization_step(const image_wrapper &image, double &x, double &y,
						       bool do_x, bool do_y, bool do_r)
{
  double  new_fitness;	    //< Checked fitness value to see if it is better than current one
  bool	  betterbase = false; //< Did the base class find a better fit?
  bool	  betterorient = false;//< Do we find a better orientation?

  // See if the base class fitting steps can do better
  betterbase = spot_tracker::take_single_optimization_step(image, x, y, do_x, do_y, do_r);

  // Try going in +/- orient and see if we find a better orientation.
  // XXX HACK: Orientation step is set based on the pixelstep and scaled so
  // that each endpoint will move one pixelstep unit as the orientation changes.

  double orient_step = _pixelstep * (180 / M_PI) / (d_length / 2);
  {
    double starting_o = get_orientation();
    set_orientation(starting_o + orient_step);	// Try going a step in +orient
    if ( _fitness < (new_fitness = check_fitness(image)) ) {
      _fitness = new_fitness;
      betterorient = true;
    } else {
    set_orientation(starting_o - orient_step);	// Try going a step in -orient
      if ( _fitness < (new_fitness = check_fitness(image)) ) {
	_fitness = new_fitness;
	betterorient = true;
      } else {
	set_orientation(starting_o);	// Back where we started in orientation
      }
    }
  }

  // Return the new location and whether we found a better one.
  x = get_x(); y = get_y();
  return betterbase || betterorient;
}
