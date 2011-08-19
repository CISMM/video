#ifndef	SPOT_TRACKER_H
#define	SPOT_TRACKER_H

#include "image_wrapper.h"
#include <list>

//----------------------------------------------------------------------------
// Virtual base class for spot trackers that track in X and Y.

class spot_tracker_XY {
public:
  // Optimize starting at the specified location to find the best-fit disk.
  // Take only one optimization step.  Return whether we ended up finding a
  // better location/radius or not.  Return new location in any case.  The
  // boolean parameters tell whether to try stepping in each of X, Y, and
  // Radius.
  virtual bool	take_single_optimization_step(const image_wrapper &image, unsigned rgb, double &x, double &y,
				      bool do_x, bool do_y, bool do_r);
  // So derived classes' destructors are called when needed.
  virtual ~spot_tracker_XY() {};

  // Same thing, but say where to start.  This means that we should measure the
  // fitness at that location before trying the steps.
  virtual bool  take_single_optimization_step(const image_wrapper &image, unsigned rgb, double &x, double &y,
				      double startx, double starty)
	    { set_location(startx, starty); _fitness = check_fitness(image, rgb);
	     return take_single_optimization_step(image, rgb, x,y, true, true, true); }

  // Continue to optimize until we can't do any better (the step size drops below
  // the minimum.  Assume that we've got a new image or position, so measure the
  // fitness at our current location before starting.
  virtual void	optimize(const image_wrapper &image, unsigned rgb, double &x, double &y);
  // Same thing, but say where to start
  virtual void	optimize(const image_wrapper &image, unsigned rgb, double &x, double &y, double startx, double starty)
	    { set_location(startx, starty); optimize(image, rgb, x, y); };

  /// Optimize in X and Y only, not in radius.
  virtual void	optimize_xy(const image_wrapper &image, unsigned rgb, double &x, double &y);
  virtual void	optimize_xy(const image_wrapper &image, unsigned rgb, double &x, double &y, double startx, double starty)
	    { set_location(startx, starty); optimize_xy(image, rgb, x, y); };

  /// Optimize in X and Y by solving separately for the best-fit parabola in X and Y
  // to three samples starting from the center and separated by the sample distance.
  // The minimum for the parabola is the best location (if it has a minimum; otherwise
  // just stay where we started because it is hopeless).
  virtual void	optimize_xy_parabolafit(const image_wrapper &image, unsigned rgb, double &x, double &y);
  virtual void	optimize_xy_parabolafit(const image_wrapper &image, unsigned rgb, double &x, double &y, double startx, double starty)
	    { set_location(startx, starty); optimize_xy_parabolafit(image, rgb, x, y); };

  /// Find the best fit for the spot detector within the image, taking steps
  // that are 1/4 of the bead's radius.
  virtual void	locate_good_fit_in_image(const image_wrapper &image, unsigned rgb, double &x, double &y);

  /// Check the fitness of the disk against an image, at the current parameter settings.
  // Return the fitness value there.
  virtual double  check_fitness(const image_wrapper &image, unsigned rgb) = 0;

  /// Get at internal information
  inline double  get_radius(void) const { return _rad; };
  inline double  get_fitness(void) const { return _fitness; };
  inline double  get_x(void) const { return _x; };
  inline double  get_y(void) const { return _y; };

  /// Set the radius for the bead.  Return false on failure
  virtual bool	set_radius(const double r) { if (r <= 1) { _rad = 1; return false; } else {_rad = r; return true; } };

  /// Set the location for the bead.
  virtual void	set_location(const double x, const double y) { _x = x; _y = y; };

  /// Set the desired pixel accuracy.
  virtual bool	set_pixel_accuracy(const double a) { if (a <= 0) { return false; } else {_pixelacc = a; return true; } };

  /// Set the desired radius accuracy.
  virtual bool set_radius_accuracy(const double a) { if (a <= 0) { return false; } else {_radacc = a; return true; } };

  /// Set the spacing between samples in terms of pixels
  virtual bool set_sample_separation(const double s) { if (s <= 0) { return false; } else {_samplesep = s; return true; } };

  /// Set with the fitting functions should be inverted.
  virtual bool set_invert(const bool invert) { _invert = invert; return true; };

protected:
  double  _samplesep; //< Spacing between samples in pixels
  double  _rad;	      //< Current radius of the disk
  double  _radacc;    //< Minimum radius step size to try
  double  _radstep;   //< Current radius step
  double  _x,_y;      //< Current best-fit position of the disk
  double  _pixelacc;  //< Minimum step size in X,Y
  double  _pixelstep; //< Current X,Y pixel step size
  double  _fitness;   //< Current value of match for the disk
  bool	  _invert;    //< Do we look for a dark spot on a black background?

  spot_tracker_XY(double radius, bool inverted = false, double pixelacurracy = 0.25, double radiusaccuracy = 0.25, double sample_separation_in_pixels = 1.0);
};

//----------------------------------------------------------------------------
// Virtual base class for spot trackers that track in Z.

class spot_tracker_Z {
public:
  // Optimize starting at the specified location to find the best-fit depth.
  // Take only one optimization step.  Return whether we ended up finding a
  // better depth or not.  Return new depth in any case.  The caller tells
  // us where to look in X,Y on the image we are tracking in.
  virtual bool	take_single_optimization_step(const image_wrapper &image, unsigned rgb, double x, double y, double &z);

  // Same thing, but say where to start.  This means that we should measure the
  // fitness at that location before trying the steps.
  virtual bool  take_single_optimization_step(const image_wrapper &image, unsigned rgb, double x, double y, double &z, double startz)
	    { set_z(startz); _fitness = check_fitness(image, rgb, x,y);
	     return take_single_optimization_step(image, rgb, x,y,z); }

  // Continue to optimize until we can't do any better (the step size drops below
  // the minimum.  Assume that we've got a new image or position, so measure the
  // fitness at our current location before starting.
  virtual void	optimize(const image_wrapper &image, unsigned rgb, double x, double y, double &z);
  // Same thing, but say where to start
  virtual void	optimize(const image_wrapper &image, unsigned rgb, double x, double y, double &z, double startz)
	    { set_z(startz); optimize(image, rgb, x, y, z); };

  /// Find the best fit for the spot detector within the depth stack, checking each
  // layer but not between layers.
  virtual void	locate_best_fit_in_depth(const image_wrapper &image, unsigned rgb, double x, double y, double &z);

  /// Find the best fit for the spot detector within the depth stack, checking only a
  // subset of the layers.
  virtual void	locate_close_fit_in_depth(const image_wrapper &image, unsigned rgb, double x, double y, double &z);

  /// Check the fitness of the disk against an image, at the current parameter settings.
  // Return the fitness value there.
  virtual double  check_fitness(const image_wrapper &image, unsigned rgb, double x, double y) = 0;

  /// Get at internal information
  inline double  get_z(void) const { return _z; };
  inline double  get_fitness(void) const { return _fitness; };

  /// Set the location for the bead.
  virtual void	set_z(const double z) { _z = z; };

  /// Set the desired pixel accuracy
  virtual bool	set_depth_accuracy(const double a) { if (a <= 0) { return false; } else {_depthacc = a; return true; } };

protected:
  double  _minz, _maxz;	//< The range of available Z values.
  double  _z;		//< Current best-fit position of the tracker
  double  _depthacc;	//< Minimum step size in Z
  double  _depthstep;	//< Current Z step size
  double  _fitness;	//< Current value of match for the disk
  double  _radius;	//< Radius of the tracker

  spot_tracker_Z(double minz, double maxz, double radius, double depthaccuracy = 0.25);
};

//----------------------------------------------------------------------------
// This class will center the tracker at a local maximum within a circular
// neighborhood defined by the tracker position and radius. If several pixels 
// share the maximum value in the region, then the centroid of those pixels
// is taken to be the center

class local_max_spot_tracker : public spot_tracker_XY {
public:
	// Set initial parameters of the local max search routine
	local_max_spot_tracker(double radius,
		bool inverted = false,
		double pixelaccuracy = 1.0,
		double radiusaccuracy = 0.25,
		double sample_separation_in_pixels = 1.0);

	/// Check the fitness against an image, at the current parameter settings.
	// Return the fitness value there.
	virtual double check_fitness(const image_wrapper &image, unsigned rgb);

	//------------------------------------------------
	// The local_max tracker simply finds the pixel with the maximum value in 
	// a circular neighborhood defined by the tracker position and radius. If
	// several pixels have the same maximum value, the centroid of those positions
	// is returned.

	// This method will always "optimize" in x and y, but not r. It will always return
	// false to indicate to callers that no further optimization should take place.
	virtual bool take_single_optimization_step(const image_wrapper &image, unsigned rgb, double &x, double &y,
				      bool do_x, bool do_y, bool do_r);

protected:
};


//----------------------------------------------------------------------------
// This class will optimize the response of a disk-shaped kernel on an image.
// The class is given an image to search in, and whether to search for a bright
// spot on a dark background (the default) or a dark spot on a bright background.

class disk_spot_tracker : public spot_tracker_XY {
public:
  // Set initial parameters of the disk search routine
  disk_spot_tracker(double radius,
		    bool inverted = false,
		    double pixelaccuracy = 0.25,
		    double radiusaccuracy = 0.25,
		    double sample_separation_in_pixels = 1.0);

  /// Check the fitness of the disk against an image, at the current parameter settings.
  // Return the fitness value there.
  virtual double  check_fitness(const image_wrapper &image, unsigned rgb);

protected:
};

//----------------------------------------------------------------------------
// This class will optimize the response of a disk-shaped kernel on an image.
// It does bilinear interpolation on the four neighboring pixels when computing
// the fit, to try and improve sub-pixel accuracy.  The samples take place at
// the center of the disk and on circles of single-pixel-stepped radii away from
// the center, out to the radius (and then negative values looking for an off-
// surround outside the radius).  Around each circle, the samples are placed
// at single-pixel distances from each other, with the first pixel in each
// ring offset by 1/2 pixel from the ones in the previous ring.
// The class is given an image to search in, and whether to search for a bright
// spot on a dark background (the default) or a dark spot on a bright background.

class disk_spot_tracker_interp : public spot_tracker_XY {
public:
  // Set initial parameters of the disk search routine
  disk_spot_tracker_interp(double radius,
		    bool inverted = false,
		    double pixelaccuracy = 0.25,
		    double radiusaccuracy = 0.25,
		    double sample_separation_in_pixels = 1.0);

  /// Check the fitness of the disk against an image, at the current parameter settings.
  // Return the fitness value there.
  virtual double  check_fitness(const image_wrapper &image, unsigned rgb);

protected:
};

//----------------------------------------------------------------------------
// This class will optimize the response of a cone-shaped kernel on an image.
// It does bilinear interpolation on the four neighboring pixels when computing
// the fit, to try and improve sub-pixel accuracy.  The samples take place at
// the center of the cone and on circles of single-pixel-stepped radii away from
// the center, out to the radius.  Around each circle, the samples are placed
// at single-pixel distances from each other, with the first pixel in each
// ring offset by 1/2 pixel from the ones in the previous ring.
// The class is given an image to search in, and whether to search for a bright
// spot on a dark background (the default) or a dark spot on a bright background.

class cone_spot_tracker_interp : public spot_tracker_XY {
public:
  // Set initial parameters of the search routine
  cone_spot_tracker_interp(double radius,
		    bool inverted = false,
		    double pixelaccuracy = 0.25,
		    double radiusaccuracy = 0.25,
		    double sample_separation_in_pixels = 1.0);

  /// Check the fitness against an image, at the current parameter settings.
  // Return the fitness value there.
  virtual double  check_fitness(const image_wrapper &image, unsigned rgb);

protected:
};

//----------------------------------------------------------------------------
// This class will optimize the response of a circularly-symmetric kernel on an image.
// It does bilinear interpolation on the four neighboring pixels when computing
// the fit, to try and improve sub-pixel accuracy.  The samples take place on
// circles of single-pixel-stepped radii away from the center, out to the radius.
// Around each circle, the samples are placed at single-pixel distances from each
// other, with the first pixel in each ring offset by 1/2 pixel from the ones in
// the previous ring.  The variance for the pixels on each circle is computed.
// These are summed across the circles and given
// negative weight.  These values should approach zero as the kernel sits in an
// area of the image that has circular symmetry.
// The class is given an image to search in, and whether to search for a bright
// spot on a dark background (the default) or a dark spot on a bright background.

class symmetric_spot_tracker_interp : public spot_tracker_XY {
public:
  // Set initial parameters of the search routine
  symmetric_spot_tracker_interp(double radius,
		    bool inverted = false,
		    double pixelaccuracy = 0.25,
		    double radiusaccuracy = 0.25,
		    double sample_separation_in_pixels = 1.0);
  virtual ~symmetric_spot_tracker_interp();

  /// Check the fitness against an image, at the current parameter settings.
  // Return the fitness value there.
  virtual double  check_fitness(const image_wrapper &image, unsigned rgb);

protected:
  // These structures and functions support pre-filling the coordinate offsets
  // for the circles.  This avoids having to call all of the sin() and cos()
  // functions, as well as a lot of other math, each time through the routine
  // that determines fitness.  Because the pixels are always even integer
  // distances from the center, we can precompute them for all radii in the
  // constructor and then just loop through each list that is within the
  // current radius value at run-time.
  int _MAX_RADIUS;	//< Can't have larger radius than this
  int *_radius_counts;	//< How many values in each radius, stored in an array
  typedef struct {
    double x, y;
  } offset;
  offset  **_radius_lists;  //< List of offset values, stored in an array
};

//----------------------------------------------------------------------------
// This class is initialized with an image that it should track, and then
// it will optimize against this initial image by shifting
// over a specified range to find the image whose pixel-wise least-squares
// difference is minimized.

class image_spot_tracker_interp : public spot_tracker_XY {
public:
  // Set initial parameters of the search routine
  image_spot_tracker_interp(double radius,
		    bool inverted = false,
		    double pixelaccuracy = 0.25,
		    double radiusaccuracy = 0.25,
		    double sample_separation_in_pixels = 1.0);
  virtual ~image_spot_tracker_interp();

  // Initialize with the image that we are trying to match and the
  // position in the image we are to check.  The image is resampled
  // at sub-pixel resolution around the location.  Returns true on
  // success and false on failure.
  virtual bool	set_image(const image_wrapper &image, unsigned rgb, double x, double y, double rad);

  /// Check the fitness against an image, at the current parameter settings.
  // Return the fitness value there.
  virtual double  check_fitness(const image_wrapper &image, unsigned rgb);

protected:
  double  *_testimage;	  //< The image to test for fitness against
  int	  _testrad;	  //< The radius of pixels stored from the test image
  int	  _testsize;	  //< The size of the stored image (2 * _testrad + 1)
  int	  _testx, _testy; //< The center of the image for testing point of view
};

//----------------------------------------------------------------------------
// This class is initialized with an image that it should track, and then
// it will optimize against this initial image by shifting
// over a specified range to find the image whose pixel-wise least-squares
// difference is minimized.  It is like the image_spot_tracker_interp above,
// except that it only checks two lines of pixels, one horizontal and one
// vertical, each through the center of the image.  This subsetting is done
// to make it faster.

class twolines_image_spot_tracker_interp : public image_spot_tracker_interp {
public:
  // Set initial parameters of the search routine
  twolines_image_spot_tracker_interp(double radius,
		    bool inverted = false,
		    double pixelaccuracy = 0.25,
		    double radiusaccuracy = 0.25,
		    double sample_separation_in_pixels = 1.0) :
	    image_spot_tracker_interp(radius, inverted, pixelaccuracy,
	      radiusaccuracy, sample_separation_in_pixels) {};

  /// Check the fitness against an image, at the current parameter settings.
  // Return the fitness value there.
  virtual double  check_fitness(const image_wrapper &image, unsigned rgb);

protected:
};

//----------------------------------------------------------------------------
// This class is initialized with a description of a Gaussian profile that it
// should track, and then it will optimize against this initial image by shifting
// over a specified range to find the image whose pixel-wise least-squares
// difference is minimized.  It approximates integration of the volume of the
// Gaussian within each pixel by sampling a number of times within each pixel
// and averaging.

class Gaussian_spot_tracker : public spot_tracker_XY {
public:
  // Set initial parameters of the search routine
  Gaussian_spot_tracker(double radius,
		    bool inverted = false,
		    double pixelaccuracy = 0.25,
		    double radiusaccuracy = 0.25,
		    double sample_separation_in_pixels = 1.0,
                    double background = 127,
                    double summedvalue = 11689);
  virtual ~Gaussian_spot_tracker();

  /// Check the fitness against an image, at the current parameter settings.
  // Return the fitness value there.
  virtual double  check_fitness(const image_wrapper &image, unsigned rgb);

  // Debugging method.  Remember that the check_fitness() function has to
  // be called before it can be used, so that an image is created.
  bool read_pixel(double x, double y, double &result) const {
    if (_testimage == NULL) {
      result = 0.0;
      return false;
    } else {
      return _testimage->read_pixel(static_cast<int>(x),static_cast<int>(y),result, 0);
    }
  };

protected:
  Integrated_Gaussian_image  *_testimage;  //< The image to test for fitness against
  double          _lastwidth;   //< Last width we built the tracker for (should be twice the diameter (4x radius)) plus 1.
  double          _background;  //< The background value to use in the image (added to the Gaussian)
  double          _summedvalue; //< The summed value under the entire Gaussian (will be negative if inverted
};

//----------------------------------------------------------------------------
// This class is initialized with a description of a Gaussian profile that it
// should track, and then it will optimize against this initial image by shifting
// over a specified range to find the image whose pixel-wise least-squares
// difference is minimized.  Following the Selvin code, it uses a point-sampled
// Gaussian and it optimizes the background, magnitude, and radius of the Gaussian each
// time it optimizes its position.  Like the Selvin code, it looks at a 15-pixel
// region around the center to do its tracking.  Unlike the Selvin code, it does not allow
// for separate standard deviations in X and Y.
// The "sample_separation_in_pixels" parameter does nothing for this tracker.

class FIONA_spot_tracker : public spot_tracker_XY {
public:
  // Set initial parameters of the search routine
  FIONA_spot_tracker(double radius,
		    bool inverted = false,
		    double pixelaccuracy = 0.25,
		    double radiusaccuracy = 0.25,
		    double sample_separation_in_pixels = 1.0,
                    double background = 127,
                    double summedvalue = 11689);

  /// Check the fitness against an image, at the current parameter settings.
  // Return the fitness value there.
  virtual double  check_fitness(const image_wrapper &image, unsigned rgb);

  //------------------------------------------------
  // The FIONA tracker actually optimizes the radius and the background as
  // part of the Gaussian-fit process.  So, we override the optimization
  // routines here so that they do these extra steps.  This means that
  // the do_r parameter is always treated as TRUE not matter what.

  // Optimize starting at the specified location to find the best-fit disk.
  // Take only one optimization step.  Return whether we ended up finding a
  // better location/radius or not.  Return new location in any case.  The
  // boolean parameters tell whether to try stepping in each of X, Y, and
  // Radius.
  virtual bool	take_single_optimization_step(const image_wrapper &image, unsigned rgb, double &x, double &y,
				      bool do_x, bool do_y, bool do_r);

  double get_background(void) const { return _background; };
  double get_summedvalue(void) const { return _summedvalue; };

  //------------------------------------------------

  // Debugging method.  Remember that the check_fitness() function has to
  // be called before it can be used, so that an image is created.
  bool read_pixel(double x, double y, double &result) const {
      return _testimage.read_pixel(static_cast<int>(floor(x)),
				   static_cast<int>(floor(y)),result, 0);
  };

protected:
  Point_sampled_Gaussian_image  _testimage;  //< The image to test for fitness against
  double          _background;  //< The background value to use in the image (added to the Gaussian)
  double          _summedvalue; //< The summed value under the entire Gaussian (will be negative for a dark spot)
};


//----------------------------------------------------------------------------
// This class will optimize the response of three collinear kernels on an image.
// It uses three subordinate trackers of the same type to attempt to track the
// center and each endpoint of the rod.  It keeps track of the center location
// and the orientation of the rod, spacing the kernels 1/2 length along the
// line at the given orienation.  Orientation of 0 is along the X axis, 45 is
// between the X and Y axes.
// The class is given an image to search in, and whether to search for a bright
// spot on a dark background (the default) or a dark spot on a bright background.

class rod3_spot_tracker_interp : public spot_tracker_XY {
public:
  // Set initial parameters of the search routine.
  // Different constructors used for different subordinate tracker types.
  // You can pass a NULL pointer, so long as it is of the correct type.
  rod3_spot_tracker_interp(const disk_spot_tracker *type,
		    double radius,
		    bool inverted = false,
		    double pixelaccuracy = 0.25,
		    double radiusaccuracy = 0.25,
		    double sample_separation_in_pixels = 1.0,
		    double length = 10.0,
		    double orientation = 0.0);
  rod3_spot_tracker_interp(const disk_spot_tracker_interp *type,
		    double radius,
		    bool inverted = false,
		    double pixelaccuracy = 0.25,
		    double radiusaccuracy = 0.25,
		    double sample_separation_in_pixels = 1.0,
		    double length = 10.0,
		    double orientation = 0.0);
  rod3_spot_tracker_interp(const cone_spot_tracker_interp *type,
		    double radius,
		    bool inverted = false,
		    double pixelaccuracy = 0.25,
		    double radiusaccuracy = 0.25,
		    double sample_separation_in_pixels = 1.0,
		    double length = 10.0,
		    double orientation = 0.0);
  rod3_spot_tracker_interp(const symmetric_spot_tracker_interp *type,
		    double radius,
		    bool inverted = false,
		    double pixelaccuracy = 0.25,
		    double radiusaccuracy = 0.25,
		    double sample_separation_in_pixels = 1.0,
		    double length = 10.0,
		    double orientation = 0.0);

  // Optimize starting at the specified location to find the best-fit rod.
  // This calls the base-class optimizer (which tries position and optionally
  // radius) and then try changing the orientation.
  // Take only one optimization step.  Return whether we ended up finding a
  // better location/radius or not.  Return new location in any case.  The
  // boolean parameters tell whether to try stepping in each of X, Y, and
  // Radius.
  virtual bool	take_single_optimization_step(const image_wrapper &image, unsigned rgb, double &x, double &y,
				      bool do_x, bool do_y, bool do_r);

  /// Check the fitness of the disk against an image, at the current parameter settings.
  // Return the fitness value there.
  virtual double  check_fitness(const image_wrapper &image, unsigned rgb);

  /// Get at internal information
  double  get_length(void) const { return d_length; };
  double  get_orientation(void) const { return d_orientation; };

  /// Set the radius for the bead.  Return false on failure
  virtual bool	set_radius(const double r)
    { if (d_center == NULL) {
	return false;
      } else {
	return spot_tracker_XY::set_radius(r) && d_center->set_radius(r) && d_beginning->set_radius(r) && d_end->set_radius(r);
      }
    };

  /// Set the location for the bead.
  virtual void	set_location(const double x, const double y)
    { if (d_center && d_beginning && d_end) {
	spot_tracker_XY::set_location(x,y);
	d_center->set_location(x,y);
	update_beginning_and_end_locations();
      }
    };

  /// Set the desired pixel accuracy
  virtual bool	set_pixel_accuracy(const double a)
    { if (d_center == NULL) {
	return false;
      } else {
	return spot_tracker_XY::set_pixel_accuracy(a) && d_center->set_pixel_accuracy(a) && d_beginning->set_pixel_accuracy(a) && d_end->set_pixel_accuracy(a);
      }
    };

  /// Set the desired length
  virtual bool	set_length(const double length)
    { if (d_center == NULL) {
	return false;
      } else if (length <= 0) {
	return false;
      } else {
	d_length = length;
	update_beginning_and_end_locations();
	return true;
      }
    };

  /// Set the desired orientation
  virtual bool	set_orientation(const double orient_in_degrees)
    { if (d_center == NULL) {
	return false;
      } else {
	d_orientation = orient_in_degrees;
	update_beginning_and_end_locations();
	return true;
      }
    };

protected:
  double  d_length;	      // Full length of the rod in pixels
  double  d_orientation;      // Orientation of the rod in degrees
  spot_tracker_XY	*d_center;    // Tracker for the center
  spot_tracker_XY	*d_beginning; // Tracker for the starting end of the rod
  spot_tracker_XY	*d_end;	      // Tracker for the other end of the rod

  // Set the beginning and end locations based on the center location, length, and orientation.
  void update_beginning_and_end_locations(void);
};

//----------------------------------------------------------------------------
// This class is initialized with an image that contains a radially-averaged
// spread function for the object it should track in Z.
// It optimizes against this image by shifting a radial average of the image
// passed in around the specified location
// over the specified range to find the image whose pixel-wise least-squares
// difference is minimized.

class radial_average_tracker_Z : public spot_tracker_Z {
public:
  // Set initial parameters of the search routine
  radial_average_tracker_Z(const char *in_filename, double depth_accuracy = 0.25);
  virtual ~radial_average_tracker_Z() { if (d_radial_image) { delete d_radial_image; }; }

  /// Check the fitness against an image, at the current parameter settings.
  // Return the fitness value there.
  virtual double  check_fitness(const image_wrapper &image, unsigned rgb, double x, double y);

protected:
  image_wrapper	*d_radial_image;	    //< Radial image read from file.
};

//----------------------------------------------------------------------------------
// Application-level object to keep track of the information about each spot tracker
// that is currently active.  It includes code to provide a
// unique index to each tracker that is ever created during one run of the program
// to ensure that no two traces that might be from different beads are recorded as
// having the same bead index.  It uses a global static variable to hold the number
// of spots, so multiple instances will not re-use the same numbers.  This is not
// thread-safe for adding new spots in multiple threads.

class Spot_Information {
public:
  Spot_Information(spot_tracker_XY *xytracker, spot_tracker_Z *ztracker, bool unofficial = false) {
    d_tracker_XY = xytracker;
    d_tracker_Z = ztracker;
    if (!unofficial) {
                d_index = d_static_index++;
    } else {
                d_index = -1;
    }
    d_velocity[0] = d_acceleration[0] = d_velocity[1] = d_acceleration[1] = 0;
    d_lost = false;
  }

  ~Spot_Information() {
          if (d_tracker_XY != NULL)
                  delete d_tracker_XY;
          if (d_tracker_Z != NULL)
                  delete d_tracker_Z;
  }

  spot_tracker_XY *xytracker(void) const { return d_tracker_XY; }
  spot_tracker_Z *ztracker(void) const { return d_tracker_Z; }
  unsigned index(void) const { return d_index; }

  void set_xytracker(spot_tracker_XY *tracker) { d_tracker_XY = tracker; }
  void set_ztracker(spot_tracker_Z *tracker) { d_tracker_Z = tracker; }

  void get_velocity(double velocity[2]) const { velocity[0] = d_velocity[0]; velocity[1] = d_velocity[1]; }
  void set_velocity(const double velocity[2]) { d_velocity[0] = velocity[0]; d_velocity[1] = velocity[1]; }
  void get_acceleration(double acceleration[2]) const { acceleration[0] = d_acceleration[0]; acceleration[1] = d_acceleration[1]; }
  void set_acceleration(const double acceleration[2]) { d_acceleration[0] = acceleration[0]; d_acceleration[1] = acceleration[1]; }

  bool lost(void) const { return d_lost; };
  void lost(bool l) { d_lost = l; };

  // The index to use for the next tracker that is created
  static unsigned get_static_index();

protected:
  spot_tracker_XY	*d_tracker_XY;	    //< The tracker we're keeping information for in XY
  spot_tracker_Z	*d_tracker_Z;	    //< The tracker we're keeping information for in Z
  unsigned		d_index;	    //< The index for this instance
  double		d_velocity[2];	    //< The velocity of the particle
  double		d_acceleration[2];  //< The acceleration of the particle
  bool                  d_lost;             //< Am I lost?
  static unsigned	d_static_index;     //< The index to use for the next one (never to be re-used).
};

//----------------------------------------------------------------------------------
// Application-level object that manages a list of trackers and keeps track of autofinding,
// deleting, and causing them to track across images.  This is a single-threaded
// implementation, so it will not utilize multiple cores the way that the video
// spot tracker application does.  It pulls together the code that deals with
// fluorescent-spot finding and tracking.

class Tracker_Collection_Manager {
public:
    Tracker_Collection_Manager(unsigned image_x, unsigned image_y,
                     float default_radius = 15.0, float min_bead_separation = 30.0,
                     float min_border_distance = 20.0)
        : d_image_x(image_x)
        , d_image_y(image_y)
        , d_default_radius(default_radius)
        , d_min_bead_separation(min_bead_separation)
        , d_min_border_distance(min_border_distance)
    {};

    // Clean up (delete trackers in our vector, etc.)
    ~Tracker_Collection_Manager();

    // Delete all active trackers.
    void delete_trackers(void);

    // Autofind fluorescence beads within the image whose pointer is passed in.
    // Avoids adding trackers that are too close to other existing trackers.
    // Adds beads that are above the threshold (fraction of the way from the minimum
    // to the maximum pixel value in the image) and whose peak is more than the
    // specified number of standard deviations brighter than the mean of the surround.
    // Returns true on success (even if no beads found) and false on error.
    bool autofind_fluorescent_beads_in(const image_wrapper &s_image,
                                           float thresh = 0.2,
                                           float var_thresh = 1.5);

    // Update the positions of the trackers we are managing based on a new image.
    // Returns the number of beads in the track.
    unsigned optimize_based_on(const image_wrapper &s_image);

    // Auto-deletes trackers that have wandered off of fluorescent beads.
    // Uses the specified variance threshold to determine if they are lost.
    // Returns the number of remaining trackers after the lost ones have
    // been deleted.
    unsigned delete_lost_fluorescent_beads_in(const image_wrapper &s_image,
                                              float var_thresh = 1.5);

    // Auto-deletes trackers that have gotten too close to the image edge.
    // Uses the specified distance threshold to determine if they are too close.
    // Returns the number of remaining trackers after any have
    // been deleted.
    unsigned delete_edge_beads_in(const image_wrapper &s_image);

    // Auto-deletes trackers that have gotten too close to another tracker.
    // Uses the specified distance threshold to determine if they are too close.
    // Returns the number of remaining trackers after any have
    // been deleted.
    unsigned delete_colliding_beads_in(const image_wrapper &s_image);

    // Returns information about the trackers we're managing.
    unsigned tracker_count(void) const { return d_trackers.size(); }
    const Spot_Information  *tracker(unsigned which) const;

protected:
    unsigned                        d_image_x;              // Size of the image in X
    unsigned                        d_image_y;              // Size of the image in Y
    float                           d_default_radius;       // Radius for new trackers
    float                           d_min_bead_separation;  // How close is too close to beads
    float                           d_min_border_distance;  // How close is too close to edge?
    std::list<Spot_Information *>   d_trackers; // Trackers we're managing

    // Check to see if the specified tracker is lost given the specified image
    // and standard-deviation threshold; the tracker is lost if its center is not
    // at least the specified number of standard deviations above the mean of the
    // pixels around its border.  Also returns true if the tracker is lost and
    // false if it is not.
    bool mark_tracker_if_lost(Spot_Information *tracker, const image_wrapper &image,
                              float var_thresh = 1.5);
};

#endif
