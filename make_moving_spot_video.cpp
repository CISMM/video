#include  <math.h>
#include  <stdlib.h>
#include  <stdio.h>
#include  "spot_tracker.h"

const char *version = "01.02";

typedef enum { DISC, ELLIPSE, CONE, GAUSSIAN, ROD
    } SPOT_TYPE;
typedef enum { CIRCLE, SPIRAL, RANDOM
    } MOTION_TYPE;

void Usage (const char * s)
{
  fprintf(stderr,"Usage: %s [-v] [-background BACK] [oversampling OVER]\n", s);
  fprintf(stderr,"       [-imagesize WIDTH HEIGHT]\n");
  fprintf(stderr,"       [-start X Y] [-frames FRAMES]\n");
  fprintf(stderr,"       [-disc VALUE RADIUS | cone VALUE RADIUS | -gaussian CENTER_VALUE STD_DEV |\n");
  fprintf(stderr,"       -rod VALUE RADIUS LENGTH | -ellipse VALUE RX RY]\n");
  fprintf(stderr,"       [-circle RAD SPEED | -spiral SIZE SPEED | -random SEED MAX_STEP MAX_ANGLE]\n");
  fprintf(stderr,"       -noise n\n");
  fprintf(stderr,"       [BASENAME]\n");
  fprintf(stderr,"     -v: Verbose mode\n");
  fprintf(stderr,"     -background: Specify the background brightness, infraction of total brightness (default 0)\n");
  fprintf(stderr,"     -oversampling: How many times oversampling in X and Y (default 100)\n");
  fprintf(stderr,"     -imagesize: Specify the image size in X and Y (default 101 101)\n");
  fprintf(stderr,"     -start: Starting location for the bead in pixels (default 75 50)\n");
  fprintf(stderr,"     -frames: The number of output images (default 120)\n");
  fprintf(stderr,"     -disc: Draw a disc-shaped bead of specified value (fraction of total) and radius in pixels (default 1.0 10)\n");
  fprintf(stderr,"     -cone: Draw a cone-shaped bead of specified maximum value (fraction of total) and radius in pixels\n");
  fprintf(stderr,"     -gaussian: Draw a Gaussian-shaped bead of specified maximum fractional brightness and standard deviation in pixels\n");
  fprintf(stderr,"     -rod: Draw a rod shape of specified value (fraction of total) and radius in pixels; angle is in degrees\n");
  fprintf(stderr,"     -ellipse: Draw an ellipse-shaped bead of specified value (fraction of total) and rx, ry in pixels (default 1.0 10 5)\n");
  fprintf(stderr,"     -circle: Move the bead in a circle of specified radius and speed (degrees/frame) (default 50 3)\n");
  fprintf(stderr,"     -spiral: Move the bead in a spiral of specified size and speed (degrees/frame) (try 1 5)\n");
  fprintf(stderr,"     -random: Move the bead randomly with maximum step size in pixels and angle in degrees\n");
  fprintf(stderr,"     -noise: Add a noise in the range [-n..n] where n is fraction of intensity (default 0)\n");
  fprintf(stderr,"     -pgm: Write PGM files (default TIFF files)\n");
  fprintf(stderr,"     BASENAME: Base name for the output file (default ./moving_spot if not specified)\n");
  exit(0);
}

// Return a random double in the range [-1..1]
double	unit_random(void)
{
  double val = rand();
  return -1 + 2 * (val / RAND_MAX);
}

int main(int argc, char *argv[])
{
  double background = 0.0;            // Value in the background of the image
  unsigned oversampling = 100;        // How much to oversample in X and Y within each pixel
  unsigned width = 101;               // Width of the image
  unsigned height= 101;               // Height of the image
  double start_x = 75;                // Where to start the bead at
  double start_y = 50;                // Where to start the bead at
  unsigned frames = 120;              // How many frames to write
  const char *basename = "moving_spot";
  SPOT_TYPE spot_type = DISC;         // Type of spot to use.
  double disc_value = 65535;          // Pixel value within a disc
  double disc_radius = 10;            // Radius of a disc in pixels
  double cone_value = 65535;          // Maximum pixel value within a cone
  double cone_radius = 10;            // Radius of a cone in pixels
  double gaussian_value;              // Maximum pixel value obtained by the Gaussian
  double gaussian_std;                // Standard deviation of a Gaussian
  double rod_value = 65535;           // Pixel value within a rod
  double rod_radius = 65535;          // Raidus of a rod in pixels
  double rod_length = 65535;          // Length of a rod in pixels
  double ellipse_value = 65535;       // Pixel value within an ellipse
  double ellipse_rx = 10;             // Radius of an ellipse in pixels in X
  double ellipse_ry = 5;              // Radius of an ellipse in pixels in Y
  MOTION_TYPE motion_type = CIRCLE;
  double circle_radius = 25;          // Radius of circular motion
  double circle_speed = 3;            // Speed of moving around the circle in degrees/second
  double spiral_size = 1;             // Larger numbers make a looser spiral
  double spiral_speed = 1;            // Speed of moving around the spiral
  int random_seed = 1;                 // Seed for random number generator
  double random_step = 1;              // Maximum magnitude of noise step
  double random_angle = 1;             // Maximum degrees of angle change
  bool verbose = false;               // Print out info along the way?
  double noise = 0;                   // Pixel counts of +/-noise to add to each pixel
  bool do_pgm = false;                // Write PGM files instead of TIFF?

  int	realparams = 0;
  int	i;
  i = 1;
  while (i < argc) {
    if (!strncmp(argv[i], "-background", strlen("-background"))) {
          if (++i > argc) { Usage(argv[0]); }
	  background = 65535 * atof(argv[i]); // Scale 1 becomes maximum pixel value
    } else if (!strncmp(argv[i], "-oversampling", strlen("-oversampling"))) {
          if (++i > argc) { Usage(argv[0]); }
	  oversampling = atoi(argv[i]);
    } else if (!strncmp(argv[i], "-imagesize", strlen("-imagesize"))) {
          if (++i > argc) { Usage(argv[0]); }
	  width = atoi(argv[i]);
          if (++i > argc) { Usage(argv[0]); }
	  height = atoi(argv[i]);
    } else if (!strncmp(argv[i], "-start", strlen("-start"))) {
          if (++i > argc) { Usage(argv[0]); }
	  start_x = atof(argv[i]);
          if (++i > argc) { Usage(argv[0]); }
	  start_y = atof(argv[i]);
    } else if (!strncmp(argv[i], "-frames", strlen("-frames"))) {
          if (++i > argc) { Usage(argv[0]); }
	  frames = atoi(argv[i]);
    } else if (!strncmp(argv[i], "-disc", strlen("-disc"))) {
          if (++i > argc) { Usage(argv[0]); }
	  disc_value = 65535 * atof(argv[i]);   // Scale so 1 becomes maximum pixel value
          if (++i > argc) { Usage(argv[0]); }
	  disc_radius = atof(argv[i]);
          spot_type = DISC;
    } else if (!strncmp(argv[i], "-ellipse", strlen("-ellipse"))) {
          if (++i > argc) { Usage(argv[0]); }
	  ellipse_value = 65535 * atof(argv[i]);   // Scale so 1 becomes maximum pixel value
          if (++i > argc) { Usage(argv[0]); }
	  ellipse_rx = atof(argv[i]);
          if (++i > argc) { Usage(argv[0]); }
	  ellipse_ry = atof(argv[i]);
          spot_type = ELLIPSE;
    } else if (!strncmp(argv[i], "-cone", strlen("-cone"))) {
          if (++i > argc) { Usage(argv[0]); }
	  cone_value = 65535 * atof(argv[i]);   // Scale 1 becomes maximum pixel value
          if (++i > argc) { Usage(argv[0]); }
	  cone_radius = atof(argv[i]);
          spot_type = CONE;
    } else if (!strncmp(argv[i], "-gaussian", strlen("-gaussian"))) {
          if (++i > argc) { Usage(argv[0]); }
	  gaussian_value = 65535 * atof(argv[i]);   // Scale 1 becomes maximum pixel value
          if (++i > argc) { Usage(argv[0]); }
	  gaussian_std = atof(argv[i]);
          spot_type = GAUSSIAN;
    } else if (!strncmp(argv[i], "-rod", strlen("-rod"))) {
          if (++i > argc) { Usage(argv[0]); }
	  rod_value = 65535 * atof(argv[i]);   // Scale 1 becomes maximum pixel value
          if (++i > argc) { Usage(argv[0]); }
	  rod_radius = atof(argv[i]);
          if (++i > argc) { Usage(argv[0]); }
	  rod_length = atof(argv[i]);
          spot_type = ROD;
    } else if (!strncmp(argv[i], "-circle", strlen("-circle"))) {
          if (++i > argc) { Usage(argv[0]); }
	  circle_radius = atof(argv[i]);   
          if (++i > argc) { Usage(argv[0]); }
	  circle_speed = atof(argv[i]);
          motion_type = CIRCLE;
    } else if (!strncmp(argv[i], "-spiral", strlen("-spiral"))) {
          if (++i > argc) { Usage(argv[0]); }
	  spiral_size = atof(argv[i]);   
          if (++i > argc) { Usage(argv[0]); }
	  spiral_speed = atof(argv[i]);
          motion_type = SPIRAL;
    } else if (!strncmp(argv[i], "-random", strlen("-random"))) {
          if (++i > argc) { Usage(argv[0]); }
	  random_seed = atoi(argv[i]);   
          if (++i > argc) { Usage(argv[0]); }
	  random_step = atof(argv[i]);
          if (++i > argc) { Usage(argv[0]); }
	  random_angle = atof(argv[i]);
          motion_type = RANDOM;
          srand(random_seed);
    } else if (!strncmp(argv[i], "-noise", strlen("-noise"))) {
          if (++i > argc) { Usage(argv[0]); }
	  noise =  65535 * atof(argv[i]);
    } else if (!strncmp(argv[i], "-pgm", strlen("-pgm"))) {
	  do_pgm = true;
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

  if (verbose) { printf("Version %s\n", version); }

  const unsigned MAX_NAME = 2048;
  char filename[MAX_NAME];
  if (strlen(basename) > MAX_NAME - strlen(".0000.tif")) {
    fprintf(stderr,"Base file name too long\n");
    return -1;
  }

  //------------------------------------------------------------------------------------
  // Open the CSV file that will record the motion and put the header information into it.
  FILE  *g_csv_file = NULL;		  //< File to save data in with .csv extension
  char *csvname = new char[strlen(basename)+5];	// Remember the closing '\0'
  if (csvname == NULL) {
    fprintf(stderr, "Out of memory when allocating CSV file name\n");
    return -1;
  }
  sprintf(csvname, "%s.csv", basename);
  if ( NULL == (g_csv_file = fopen(csvname, "w")) ) {
    fprintf(stderr,"Cannot open CSV file for writing: %s\n", csvname);
  } else {
    fprintf(g_csv_file, "FrameNumber,Spot ID,X,Y,Z,Radius,Orientation (if meaningful)\n");
  }
  delete [] csvname;

  //------------------------------------------------------------------------------------
  // Generate images matching the requested motion and disc types, with the
  // specified geometries, and write them to files.

  unsigned frame;
  for (frame = 0; frame < frames; frame++) {
    double x = start_x, y = start_y, angle = 0;

    // Find out where we should be based on the motion type
    switch (motion_type) {
      case CIRCLE:
        // Subtracting 1 from the cos() value so that we start at the
        // correct location, not one radius away from it.
        x = start_x + circle_radius * (cos(frame * circle_speed * M_PI/180) - 1);
        y = start_y + circle_radius * sin(frame * circle_speed * M_PI/180);
        angle = (90 - frame * circle_speed);
        break;
      case SPIRAL:
        x = start_x + spiral_size * exp(frame/32.0) * sin(frame * spiral_speed * M_PI/180);
        y = start_y + spiral_size * exp(frame/32.0) * cos(frame * spiral_speed * M_PI/180);
        angle = (90 - frame * spiral_speed);
        break;
      case RANDOM:
		if (frame != 0) {
          x += random_step * unit_random();
          y += random_step * unit_random();
		  angle += random_angle * unit_random();
		}
    }

    // Flip the image in Y because the write function is going to flip it again
    // for us before saving the image.
    double flip_y = (height - 1) - y;

    // Make the appropriate image based on the bead type
    double_image *bead;
    double        radius;   // Used for logging
    switch (spot_type) {
    case DISC:
      bead = new disc_image(0, width-1, 0, height-1,
                            background, noise,
                            x, flip_y, disc_radius,
                            disc_value,
                            oversampling);
      radius = disc_radius;
      angle = 0.0;
      break;

    case CONE:
      bead = new cone_image(0, width-1, 0, height-1,
                            background, noise,
                            x, flip_y, cone_radius,
                            cone_value,
                            oversampling);
      radius = cone_radius;
      angle = 0.0;
      break;

    case GAUSSIAN:
      // Figure out the volume that will make the Gaussian have a maximum value
      // equal to gaussian_value.  The volume under the curve is equal to
      // std^2 * 2 * PI; this is divided by to make it unit volume, so we need
      // to multiply by this to get back to unit height and then multiply again
      // by the value we want.
      bead = new Integrated_Gaussian_image(0, width-1, 0, height-1,
                                           background, noise,
                                           x, flip_y, gaussian_std,
                                           gaussian_value * gaussian_std*gaussian_std * 2 * M_PI,
                                           oversampling);
      radius = gaussian_std;
      break;

    case ROD:
      bead = new rod_image(0, width-1, 0, height-1,
                            background, noise,
                            x, flip_y, rod_radius,
                            rod_length, angle * M_PI/180, rod_value,
                            oversampling);
      radius = rod_radius;
      break;

    case ELLIPSE:
      bead = new ellipse_image(0, width-1, 0, height-1,
                            background, noise,
                            x, flip_y, ellipse_rx, ellipse_ry,
                            ellipse_value,
                            oversampling);
      radius = ellipse_rx;
      if (ellipse_ry > radius) { radius = ellipse_ry; }
      angle = 0.0;
      break;
    }

    // Write the image to file whose name is the base name with
    // the frame number and the correct file extension added.
    const char *ext = "tif";
    if (do_pgm) { ext = "pgm"; }
    sprintf(filename, "%s.%04d.%s", basename, frame, ext);
    if (verbose) { printf("Writing %s:\n", filename); }
    if (do_pgm) {
      bead->write_to_pgm_file(filename, 0, 255.0/65535);
    } else {
      bead->write_to_grayscale_tiff_file(filename, 0, 1.0, 0.0, true);
    }

    // Write the information about the current bead to the CSV file, in a format
    // that matches the header description.  We DO NOT flip y in this report.
    fprintf(g_csv_file, "%d,0,%lg,%lg,0,%lg,%lg\n", frame, x,y, radius, angle);

    // Clean up things allocated for this frame.
    delete bead;
  }

  // Clean up things allocated for the whole sequence
  if (g_csv_file) { fclose(g_csv_file); g_csv_file = NULL; };
  return 0;
}
