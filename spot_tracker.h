#ifndef	SPOT_TRACKER_H
#define	SPOT_TRACKER_H

#include "image_wrapper.h"

//----------------------------------------------------------------------------
// This class will optimize the response of a disk-shaped kernel on an image.
// The class is given an image to search in, and whether to search for a bright
// spot on a dark background (the default) or a dark spot on a bright background.

class disk_spot_tracker {
public:
  // Set initial parameters of the disk search routine
  disk_spot_tracker(double radius, bool inverted = false,
		    double pixelaccuracy = 0.25,
		    double radiusaccuracy = 0.25);

  // Optimize starting at the specified location to find the best-fit disk.
  // Take only one optimization step.  Return whether we ended up finding a
  // better location/radius or not.  Return new location in any case.  The
  // boolean parameters tell whether to try stepping in each of X, Y, and
  // Radius.
  bool	take_single_optimization_step(const image_wrapper &image, double &x, double &y,
				      bool do_x = true, bool do_y = true, bool do_r = true);
  // Same thing, but say where to start.  This means that we should measure the
  // fitness at that location before trying the steps.
  bool  take_single_optimization_step(const image_wrapper &image, double &x, double &y,
				      double startx, double starty)
	    { _x = startx; _y = starty; _fitness = check_fitness(image);
	     return take_single_optimization_step(image, x,y); }

  // Continue to optimize until we can't do any better (the step size drops below
  // the minimum.  Assume that we've got a new image or position, so measure the
  // fitness at our current location before starting.
  void	optimize(const image_wrapper &image, double &x, double &y);
  // Same thing, but say where to start
  void	optimize(const image_wrapper &image, double &x, double &y, double startx, double starty)
	    { _x = startx; _y = starty; optimize(image, x, y); };
  /// Optimize in X and Y only, not in radius.
  void	optimize_xy(const image_wrapper &image, double &x, double &y);
  void	optimize_xy(const image_wrapper &image, double &x, double &y, double startx, double starty)
	    { _x = startx; _y = starty; optimize_xy(image, x, y); };

  /// Find the best fit for the spot detector within the image, taking steps
  // that are 1/4 of the bead's radius.
  void	locate_good_fit_in_image(const image_wrapper &image, double &x, double &y);

  /// Check the fitness of the disk against an image, at the current parameter settings.
  // Return the fitness value there.
  double  check_fitness(const image_wrapper &image);

  /// Get at internal information
  double  get_radius(void) const { return _rad; };
  double  get_fitness(void) const { return _fitness; };

  /// Set the radius for the bead.
  void	set_radius(const double r) { _rad = r; };

protected:
  double  _rad;	      //< Current radius of the disk
  double  _radacc;    //< Minimum radius step size to try
  double  _radstep;   //< Current radius step
  double  _x,_y;      //< Current best-fit position of the disk
  double  _pixelacc;  //< Minimum step size in X,Y
  double  _pixelstep; //< Current X,Y pixel step size
  double  _fitness;   //< Current value of match for the disk
  bool	  _invert;    //< Do we look for a dark spot on a black background?
};

#endif
