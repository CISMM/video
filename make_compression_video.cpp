#include  <math.h>
#include  <stdlib.h>
#include  <stdio.h>
#include  "spot_tracker.h"
#include "stocc.h"                     // define random library classes

void Usage (const char * s)
{
  fprintf(stderr,"Usage: %s [-v] [-frames FRAMES]\n", s);
  fprintf(stderr,"       [-resolution WIDTH HEIGHT]\n");
  fprintf(stderr,"       [-noise F V] [BASENAME]\n");
  fprintf(stderr,"     -v: Verbose mode\n");
  fprintf(stderr,"     -frames: The number of output images (default 120)\n");
  fprintf(stderr,"     -oversampling: How many times oversampling in X and Y (default 100)\n");
  fprintf(stderr,"     -resolution: Specify the image size in X and Y (default 101 101)\n");
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
		 int maxy = 255, double gaussian_var = 0, double poisson_frac = 0);
};

compression_test_image::compression_test_image(int minx, int maxx, int miny, int maxy,
		double gaussian_var, double poisson_frac) :
  double_image(minx, maxx, miny, maxy)
{
  int i,j, index;

  // Fill in the base intensity of each pixel.
  for (i = _minx; i <= _maxx; i++) {
    for (j = _miny; j <= _maxy; j++) {
		double intensity = (i / double(maxx)) * 65535;
		write_pixel_nocheck(i, j, intensity);
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
  double poisson_frac = 0;			  // Fraction of proportionality for Poisson noise
  const char *basename = "test_video";
  bool verbose = false;               // Print out info along the way?

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
    } else if (!strncmp(argv[i], "-noise", strlen("-noise"))) {
          if (++i > argc) { Usage(argv[0]); }
	  poisson_frac = atof(argv[i]);
          if (++i > argc) { Usage(argv[0]); }
	  gaussian_var = atof(argv[i]);
    } else if (!strncmp(argv[i], "-v", strlen("-v"))) {
          verbose = true;
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
    // Make a new image
    image_wrapper *cur_image;
	cur_image = new compression_test_image(0, width-1, 0, height-1, gaussian_var, poisson_frac);    

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
