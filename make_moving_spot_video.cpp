#include  <math.h>
#include  <stdlib.h>
#include  <stdio.h>
#include  "spot_tracker.h"

const char *version = "01.00";

typedef enum { DISC, CONE, GAUSSIAN
    } SPOT_TYPE;
typedef enum { CIRCLE, SPIRAL
    } MOTION_TYPE;

void Usage (const char * s)
{
  fprintf(stderr,"Usage: %s [-v] [-background BACK] [oversampling OVER]\n", s);
  fprintf(stderr,"       [-imagesize WIDTH HEIGHT]\n");
  fprintf(stderr,"       [-start X Y] [-frames FRAMES]\n");
  fprintf(stderr,"       [-disc VALUE RADIUS | cone VALUE RADIUS | -gaussian VOLUME STD_DEV]\n");
  fprintf(stderr,"       [-circle RAD SPEED | -spiral XXXX]\n");
  fprintf(stderr,"       [BASENAME]\n");
  fprintf(stderr,"     -v: Verbose mode\n");
  fprintf(stderr,"     -background: Specify the background brightness (default 0)\n");
  fprintf(stderr,"     -oversampling: How many times oversampling in X and Y (default 100)\n");
  fprintf(stderr,"     -imagesize: Specify the image size in X and Y (default 101 101)\n");
  fprintf(stderr,"     -start: Starting location for the bead in pixels (default 75 50)\n");
  fprintf(stderr,"     -frames: The number of output images (default 120)\n");
  fprintf(stderr,"     -disc: Draw a disc-shaped bead of specified value (fraction of total) and radius in pixels (default 1.0 10)\n");
  fprintf(stderr,"     -cone: Draw a cone-shaped bead of specified maximum value (fraction of total) and radius in pixels (default 1.0 10)\n");
  fprintf(stderr,"     -gaussian: Draw a Gaussian-shaped bead of specified maximum fractional brightness and standard deviation in pixels (default is a disc)\n");
  fprintf(stderr,"     -circle: Move the bead in a circle of specified radius and speed (degrees/frame) (default 50 3)\n");
  fprintf(stderr,"     -spiral: Move the bead in a spiral of specified size and speed (degrees/frame) (try 1 5)\n");
  fprintf(stderr,"     BASENAME: Base name for the output file (default ./moving_spot if not specified)\n");
  exit(0);
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
  MOTION_TYPE motion_type = CIRCLE;
  double circle_radius = 25;          // Radius of circular motion
  double circle_speed = 3;            // Speed of moving around the circle in degrees/second
  double spiral_size = 1;             // Larger numbers make a looser spiral
  double spiral_speed = 1;            // Speed of moving around the spiral
  bool verbose = false;               // Print out info along the way?

  int	realparams = 0;
  int	i;
  i = 1;
  while (i < argc) {
    if (!strncmp(argv[i], "-background", strlen("-background"))) {
          if (++i > argc) { Usage(argv[0]); }
	  background = atof(argv[i]);
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
    fprintf(g_csv_file, "FrameNumber,Spot ID,X,Y,Z,Radius\n");
  }
  delete [] csvname;

  //------------------------------------------------------------------------------------
  // Generate images matching the requested motion and disc types, with the
  // specified geometries, and write them to files.

  unsigned frame;
  for (frame = 0; frame < frames; frame++) {
    double x, y;

    // Find out where we should be based on the motion type
    switch (motion_type) {
      case CIRCLE:
        // Subtracting 1 from the cos() value so that we start at the
        // correct location, not one radius away from it.
        x = start_x + circle_radius * (cos(frame * circle_speed * M_PI/180) - 1);
        y = start_y + circle_radius * sin(frame * circle_speed * M_PI/180);
        break;
      case SPIRAL:
        x = start_x + spiral_size * exp(frame/32.0) * sin(frame * spiral_speed * M_PI/180);
        y = start_y + spiral_size * exp(frame/32.0) * cos(frame * spiral_speed * M_PI/180);
        break;
    }

    // Flip the image in Y because the write function is going to flip it again
    // for us before saving the image.
    double flip_y = (height - 1) - y;

    // Make the appropriate image based on the bead type
    image_wrapper *bead;
    double        radius;   // Used for logging
    switch (spot_type) {
    case DISC:
      bead = new disc_image(0, width-1, 0, height-1,
                            background, 0.0,
                            x, flip_y, disc_radius,
                            disc_value,
                            oversampling);
      radius = disc_radius;
      break;

    case CONE:
      bead = new cone_image(0, width-1, 0, height-1,
                            background, 0.0,
                            x, flip_y, cone_radius,
                            cone_value,
                            oversampling);
      radius = cone_radius;
      break;

    case GAUSSIAN:
      // Figure out the volume that will make the Gaussian have a maximum value
      // equal to gaussian_value.  The volume under the curve is equal to
      // std^2 * 2 * PI; this is divided by to make it unit volume, so we need
      // to multiply by this to get back to unit height and then multiply again
      // by the value we want.
      bead = new Integrated_Gaussian_image(0, width-1, 0, height-1,
                                           background, 0.0,
                                           x, flip_y, gaussian_std,
                                           gaussian_value * gaussian_std*gaussian_std * 2 * M_PI,
                                           oversampling);
      radius = gaussian_std;
      break;
    }

    // Write the image to file whose name is the base name with
    // the frame number and .tif added.
    sprintf(filename, "%s.%04d.tif", basename, frame);
    if (verbose) { printf("Writing %s:\n", filename); }
    bead->write_to_grayscale_tiff_file(filename, 0);

    // Write the information about the current bead to the CSV file, in a format
    // that matches the header description.  We DO NOT flip y in this report.
    fprintf(g_csv_file, "%d,0,%lg,%lg,0,%lg\n", frame, x,y, radius);

    // Clean up things allocated for this frame.
    delete bead;
  }

  // Clean up things allocated for the whole sequence
  if (g_csv_file) { fclose(g_csv_file); g_csv_file = NULL; };
  return 0;
}
