#include  <math.h>
#include  <stdlib.h>
#include  <stdio.h>
#include  "spot_tracker.h"
#include "stocc.h"                     // define random library classes

void Usage (const char * s)
{
  fprintf(stderr,"Usage: %s [-v] [-frames FRAMES]\n", s);
  fprintf(stderr,"       [-resolution WIDTH HEIGHT]\n");
  fprintf(stderr,"       [-bead MAX STD X Y] [-motion RAD SPD]\n");
  fprintf(stderr,"       [-noise F V] [BASENAME]\n");
  fprintf(stderr,"     -v: Verbose mode\n");
  fprintf(stderr,"     -frames: The number of output images (default 120)\n");
  fprintf(stderr,"     -resolution: Specify the image size in X and Y (default 101 101)\n");
  fprintf(stderr,"     -bead: Add Gaussian bead with max value MAX and standard deviation STD starting at coordinate (X,Y)\n");
  fprintf(stderr,"     -motion: Bead moves in circular motion with radius RAD at speed SPD (degrees/second)\n");
  fprintf(stderr,"     -noise: Add Poisson noise with fraction of proportionality F and Gaussian noise of variance V\n");
  fprintf(stderr,"     BASENAME: Base name for the output file (default ./test_video if not specified)\n");
  exit(0);
}

double clamp(double value) {
	if (value > 65535) {return 65535;}
	if (value < 0) {return 0;}
	return value;
}

class compression_test_image: public double_image {
public:
  // Create an image of the specified size and background intensity with
  // spots in the specified locations (may be subpixel) with respective
  // radii and intensities, as listed in the spot vector, s.
  compression_test_image(int minx = 0, int maxx = 255, int miny = 0, 
		 int maxy = 255, int minv = 0, int maxv = 255, double gaussian_var = 0, double poisson_frac = 0,
		 double bead_val = 0, double bead_std = 0, double x = 0, double y = 0);
};

compression_test_image::compression_test_image(int minx, int maxx, int miny, int maxy, 
                int minv, int maxv,
		double gaussian_var, double poisson_frac, double bead_val, double bead_std,
		double x, double y) :
  double_image(minx, maxx, miny, maxy)
{
  int i,j, index;
  double summedvolume = bead_val * bead_std * bead_std * 2 * M_PI;

  // Fill in the base intensity of each pixel.
  for (i = _minx; i <= _maxx; i++) {
    for (j = _miny; j <= _maxy; j++) {
		double intensity = minv * 255 + (i / double(maxx)) * 255 * (maxv-minv);
		write_pixel_nocheck(i, j, intensity);
	}
  }

  // Fill in the Gaussian intensity plus background.
  // Note that the area under the curve for the unit Gaussian is 1; we
  // multiply by the summedvolume (which needs to be the sum above or
  // below the background) to produce an overall volume matching the
  // one requested.
#ifdef	DEBUG
  printf("Gaussian_image::recompute(): Making Gaussian of standard deviation %lg, background %lg, volume %lg\n", std_dev, background, summedvolume);
#endif
  for (i = (int)floor(x - (3.5*bead_std)); i <= (int)ceil(x + (3.5*bead_std)); i++) {
	for (j = (int)floor(y - (3.5*bead_std)); j <= (int)ceil(y + (3.5*bead_std)); j++) {
      double x0 = (i - x) - 0.5;    // The left edge of the pixel in Gaussian space
      double x1 = x0 + 1;                 // The right edge of the pixel in Gaussian space
      double y0 = (j - y) - 0.5;    // The bottom edge of the pixel in Gaussian space
      double y1 = y0 + 1;                 // The top edge of the pixel in Gaussian space
      if (find_index(i,j,index)) {
        // For this call to ComputeGaussianVolume, we assume 1-meter pixels.
        // This makes std dev and other be in pixel units without conversion.
	    _image[index] = _image[index] + ComputeGaussianVolume(summedvolume, bead_std,
                        x0,x1, y0,y1, 100);
      }
    }
  }

  //------------------------------------------------------------------------
  // Set seeds and make random library to use for Poisson distribution
  int seed = getpid();
  srand(seed);
  static StochasticLib1 sto(seed);            // make instance of random library

  //------------------------------------------------------------------------
  // Add noise if any is specified
  for (i = _minx; i <= _maxx; i++) {
	  for (j = _miny; j <= _maxy; j++) {

		  //------------------------------------------------------------
		  // Add Poisson noise with variance proportional to pixel value 
		  // and Gaussian noise with specified variance
		  if (poisson_frac > 0 || gaussian_var > 0) {
			  if (find_index(i,j,index)) {
				  //add variance from two noise sources together
				  double variance = gaussian_var + (_image[index]*poisson_frac);
				  _image[index] = sto.Normal(_image[index], sqrt(variance));
				  _image[index] = clamp(_image[index]);
			  }
		  }
	  }
  } 

}

int main(int argc, char *argv[])
{
  unsigned width = 101;               // Width of the image
  unsigned height= 101;               // Height of the image           
  unsigned frames = 120;              // How many frames to write
  double gaussian_mean = 0;			  // Mean value of Gaussian noise
  double gaussian_var = 0;			  // Variance of Gaussian noise
  double bead_val = 0;				  // Max value of Gaussian bead
  double bead_std = 0;				  // Standard deviation of Gaussian bead
  double start_x = 0;				  // Initial x-coordinate for bead position
  double start_y = 0;				  // Initial y-coordinate for bead position
  double poisson_frac = 0;			  // Fraction of proportionality for Poisson noise
  double motion_rad = 0;			  // Radius of circular motion
  double motion_spd = 0;			  // Speed of movement around circle in degrees/second
  const char *basename = "test_video";
  bool verbose = false;               // Print out info along the way?

  unsigned minv = 0;        // min pixel intensity for bkgd 
  unsigned maxv = 255;    // max ....   ...      . .  .
 

  int	realparams = 0;
  int	i;
  i = 1;
  while (i < argc) {
    if (!strncmp(argv[i], "-frames", strlen("-frames"))) {
          if (++i > argc) { Usage(argv[0]); }
	  frames = atoi(argv[i]);
    } else if (!strncmp(argv[i], "-resolution", strlen("-resolution"))) {
          if (++i > argc) { Usage(argv[0]); }
	  width = atoi(argv[i]);
          if (++i > argc) { Usage(argv[0]); }
	  height = atoi(argv[i]);
	} else if (!strncmp(argv[i], "-bead", strlen("-bead"))) {
          if (++i > argc) { Usage(argv[0]); }
	  bead_val = 65535 * atof(argv[i]);
          if (++i > argc) { Usage(argv[0]); }
	  bead_std = atof(argv[i]);
		  if (++i > argc) { Usage(argv[0]); }
	  start_x = atof(argv[i]);
          if (++i > argc) { Usage(argv[0]); }
	  start_y = atof(argv[i]);
	} else if (!strncmp(argv[i], "-motion", strlen("-motion"))) {
          if (++i > argc) { Usage(argv[0]); }
	  motion_rad = atof(argv[i]);
          if (++i > argc) { Usage(argv[0]); }
	  motion_spd = atof(argv[i]);
    } else if (!strncmp(argv[i], "-noise", strlen("-noise"))) {
          if (++i > argc) { Usage(argv[0]); }
	  poisson_frac = atof(argv[i]);
          if (++i > argc) { Usage(argv[0]); }
	  gaussian_var = atof(argv[i]);
    } else if (!strncmp(argv[i], "-v", strlen("-v"))) {
          verbose = true;
    } else if (!strncmp(argv[i], "-minv", strlen("-minv"))) {
          if (++i > argc) { Usage(argv[0]); }
          minv = atoi(argv[i]);
    } else if (!strncmp(argv[i], "-maxv", strlen("-maxv"))) {
          if (++i > argc) { Usage(argv[0]); }
          maxv = atoi(argv[i]);
    } else if (argv[i][0] == '-') {	// Unknown flag
	  Usage(argv[0]);
    } else switch (realparams) {		// Non-flag parameters
      case 0:
          basename = argv[i];
          realparams++;
          break;
      default:
	  Usage(argv[0]);
    }
    i++;
  }
  if (realparams > 1) {
    Usage(argv[0]);
  }

  const unsigned MAX_NAME = 2048;
  char filename[MAX_NAME];
  if (strlen(basename) > MAX_NAME - strlen(".0000.tif")) {
    fprintf(stderr,"Base file name too long\n");
    return -1;
  }

  //------------------------------------------------------------------------------------
  // Generate images with specified noise and write them to files.

  unsigned frame;
  for (frame = 0; frame < frames; frame++) { 
	// Find out where bead should be
	double x = start_x, y = start_y;
	// Subtracting 1 from the cos() value so that we start at the
    // correct location, not one radius away from it.
    x = start_x + motion_rad * (cos(frame * motion_spd * M_PI/180) - 1);
    y = start_y + motion_rad * sin(frame * motion_spd * M_PI/180);

	// Flip the image in Y because the write function is going to flip it again
    // for us before saving the image.
    double flip_y = (height - 1) - y;

    // Make a new image
    image_wrapper *cur_image;
	cur_image = new compression_test_image(0, width-1, 0, height-1, minv, maxv, gaussian_var, poisson_frac, 
											bead_val, bead_std, x, flip_y);    

    // Write the image to file whose name is the base name with
    // the frame number and .tif added.
    sprintf(filename, "%s.%04d.tif", basename, frame);
    if (verbose) { printf("Writing %s:\n", filename); }
    cur_image->write_to_grayscale_tiff_file(filename, 0);

    // Clean up things allocated for this frame.
    delete cur_image;
  }

  return 0;
}
