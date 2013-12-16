#include  <math.h>
#include  <stdlib.h>
#include  <stdio.h>
#include  <vector>
#include  "spot_tracker.h"

const char *version = "01.02";

typedef enum { DISC, CONE, GAUSSIAN, ROD
    } SPOT_TYPE;

void Usage (const char * s)
{
  fprintf(stderr,"Usage: %s [-v] [-background BACK] [oversampling OVER]\n", s);
  fprintf(stderr,"       [-imagesize WIDTH HEIGHT] [-gradient MIN MAX]\n");
  fprintf(stderr,"       [-disc VALUE RADIUS X Y | cone VALUE RADIUS X Y | -gaussian CENTER_VALUE STD_DEV X Y ]\n");
  fprintf(stderr,"       [-frames F] [-motion RADIUS SPEED] [BASENAME]\n");
  fprintf(stderr,"     -v: Verbose mode\n");
  fprintf(stderr,"     -background: Specify the background brightness, infraction of total brightness (default 0)\n");
  fprintf(stderr,"     -oversampling: How many times oversampling in X and Y (default 100)\n");
  fprintf(stderr,"     -imagesize: Specify the image size in X and Y (default 101 101)\n");
  fprintf(stderr,"     -gradient: add brightness gradient with specified min and max brightness (fractions of total)\n");
  fprintf(stderr,"     -disc: Draw a disc-shaped bead of specified value (fraction of total) and radius in pixels (default 1.0 10)\n");
  fprintf(stderr,"     -cone: Draw a cone-shaped bead of specified maximum value (fraction of total) and radius in pixels\n");
  fprintf(stderr,"     -gaussian: Draw a Gaussian-shaped bead of specified maximum fractional brightness and standard deviation in pixels\n");
  fprintf(stderr,"     -frames: Make a video with F number of frmes (defaults to 1, producing a single image)\n");
  fprintf(stderr,"     -motion: Beads move in circular motion with specified RADIUS and SPEED (degrees/frame)\n");
  fprintf(stderr,"     BASENAME: Base name for the output file (default ./multi_spot if not specified)\n");
  exit(0);
}

// Return a random double in the range [-1..1]
double	unit_random(void)
{
  double val = rand();
  return -1 + 2 * (val / RAND_MAX);
}

// Represents a bead that can be added to the image
class Spot {
public:
	Spot() {
		spot_type = DISC;
		x = 75;
		y = 50;
		intensity = 65535;
		r = 10;
	}
	Spot( SPOT_TYPE type, double x_position, double y_position, double intens, double radius) {
		spot_type = type;
		x = x_position;
		y = y_position;
		intensity = 65535 * intens;
		r = radius;
	}	
	SPOT_TYPE spot_type;
	double x; // x position
	double y; // y position
	double intensity; // pixel value for a disc, maximum pixel value for a cone or Gaussian
	double r; // radius for cone or disc, standard dev for Gaussian
};

class multi_spot_image: public double_image {
public:
  // Create an image of the specified size and background intensity with
  // spots in the specified locations (may be subpixel) with respective
  // radii and intensities, as listed in the spot vector, s.
  multi_spot_image(std::vector<Spot>& b,
         int minx = 0, int maxx = 255, int miny = 0, int maxy = 255,
	     double background = 0.0, double noise = 0.0,
	     int oversample = 1, double gradientmin = 0.0, double gradientmax = 0.0,
		 unsigned frame = 0, double motion_rad = 0.0, double motion_spd = 0.0);

protected:
  int	  _oversample;
};

multi_spot_image::multi_spot_image(std::vector<Spot>& s, 
                           int minx, int maxx, int miny, int maxy,
		                   double background, double noise,
		                   int oversample, double gradientmin, double gradientmax,
						   unsigned frame, double motion_rad, double motion_spd) :
  double_image(minx, maxx, miny, maxy),
  _oversample(oversample)
{
  int i,j, index;
  double oi,oj;	// Oversample steps
  double summedvolume;

  // Fill in the background intensity.
  for (i = _minx; i <= _maxx; i++) {
    for (j = _miny; j <= _maxy; j++) {
      write_pixel_nocheck(i, j, background);
    }
  }

    // Add uniform linear brightness gradient
  if(gradientmax >= gradientmin && gradientmax > 0.0) {
	  for (i = _minx; i <= _maxx; i++) {
		  double gradient_value = (gradientmax - gradientmin) * (i / double(maxx)) + gradientmin;
		  for (j = _miny; j <= _maxy; j++) {
			  write_pixel_nocheck(i, j, gradient_value);
		  }
	  }
  }

  // Compute where the samples should be taken.  These need to be taken
  // symmetrically around the pixel center and cover all samples within
  // the pixel exactly once.  First, compute the step size between the
  // samples, then the one with the smallest value that lies within a
  // half-pixel-width of the center of the pixel as the starting offset.
  // Proceeed until we exceed a half-pixel-radius on the high side of
  // the pixel.

  double step = 1.0 / _oversample;
  double start;
  if (_oversample % 2 == 1) { // Odd number of steps
	  start = - step * floor( _oversample / 2.0 );
  } else {    // Even number of steps
	  start = - step * ( (_oversample / 2 - 1) + 0.5 );
  }
  
  // visualize all the beads in the spot vector, s
  for(std::vector<Spot>::size_type spot = 0; spot < s.size(); spot++){

	  // Find out where bead should be
	  double x = s[spot].x, y = s[spot].y;
	  // Subtracting 1 from the cos() value so that we start at the
      // correct location, not one radius away from it.
      x = s[spot].x + motion_rad * (cos(frame * motion_spd * M_PI/180) - 1);
      y = s[spot].y + motion_rad * sin(frame * motion_spd * M_PI/180);

	  // Flip the image in Y because the write function is going to flip it again
	  // for us before saving the image.
      y = _maxx - y;

	  switch(s[spot].spot_type) {
	  case DISC:
		  {
		  // Add in 0.5 pixel to the X and Y positions
		  // so that we center the disc on the middle of the pixel rather than
		  // having it centered between pixels.
		  // x += 0.5;
		  // y += 0.5;

		  // Fill in the spot intensity (the part of it that is within the image).
		  // Oversample the image by the factor specified in _oversample, averaging
		  // all results within the pixel.  
#ifdef	DEBUG
		  printf("multi_spot_image::multi_spot_image(): Making disk of radius %lg\n", s[spot].r);
#endif
		  double disk2 = s[spot].r * s[spot].r;
		  for (i = (int)floor(x - s[spot].r); i <= (int)ceil(x + s[spot].r); i++) {
			  for (j = (int)floor(y - s[spot].r); j <= (int)ceil(y + s[spot].r); j++) {
				  if ( disk2 >= (i - x)*(i - x) + (j - y)*(j - y) ) {
					  if (find_index(i,j,index)) {
						  _image[index] = 0;
						  for (oi = 0; oi < _oversample; oi++) {
							  for (oj = 0; oj < _oversample; oj++) {
								  double x = i + oi/_oversample;
								  double y = j + oj/_oversample;
								  _image[index] += s[spot].intensity;
							  } 

						  }
					  }
					  _image[index] /= (_oversample * _oversample);
				  }
			  }
		  }
		 break;
		  }

	  case CONE:
		  {
#ifdef	DEBUG
		  printf("multi_spot_image::multi_spot_image(): Making cone of radius %lg\n", s[spot].r));
#endif
		  for (i = (int)floor(x - s[spot].r); i <= (int)ceil(x + s[spot].r); i++) {
			  for (j = (int)floor(y - s[spot].r); j <= (int)ceil(y + s[spot].r); j++) {
				  double dist = sqrt((i-x)*(i-x) + (j-y)*(j-y));
				  if ( s[spot].r >= dist ) {
					  if (find_index(i,j,index)) {
						  _image[index] = 0;
						  for (oi = 0; oi < _oversample; oi++) {
							  for (oj = 0; oj < _oversample; oj++) {
								  double x = i + start + oi*step;
								  double y = j + start + oj*step;

								  double frac = dist / s[spot].r;
								  double intensity = frac * background + (1 - frac) * s[spot].intensity;
								  _image[index] += intensity;
							  } 
						  }
					  }
					  _image[index] /= (_oversample * _oversample);
				  }
			  }
		  }
		  break;
		  }

	  case GAUSSIAN:
		  {
#ifdef	DEBUG
		  printf("multi_spot_image::multi_spot_image(): Making Gaussian of standard deviation %lg, background %lg, volume %lg\n",
			  s[spot].r, background, s[spot].intensity * s[spot].r*s[spot].r * 2 * M_PI);
#endif
		  for (i = (int)floor(x - (3.5*s[spot].r)); i <= (int)ceil(x + (3.5*s[spot].r)); i++) {
			  for (j = (int)floor(y - (3.5*s[spot].r)); j <= (int)ceil(y + (3.5*s[spot].r)); j++) {
				  double x0 = (i - x) - 0.5;    // The left edge of the pixel in Gaussian space
				  double x1 = x0 + 1;                   // The right edge of the pixel in Gaussian space
				  double y0 = (j - y) - 0.5;    // The bottom edge of the pixel in Gaussian space
				  double y1 = y0 + 1;                   // The top edge of the pixel in Gaussian space
				  if (find_index(i,j,index)) {
					  // For this call to ComputeGaussianVolume, we assume 1-meter pixels.
					  // This makes std dev and other be in pixel units without conversion.
					  summedvolume = s[spot].intensity * s[spot].r*s[spot].r * 2 * M_PI;
					  _image[index] += ComputeGaussianVolume(summedvolume, s[spot].r, x0,x1, y0,y1, _oversample); 
				  }
			  }
		  }
		  break;
		  }
	  }
  }
}

int main(int argc, char *argv[])
{
  std::vector<Spot> spots(0);         // Holds the spots to be added to the image
  double background = 0.0;			  // Value in the background of the image
  double gradient_max = 0.0;			  // Maximum pixel value of brightness gradient
  double gradient_min = 0.0;			  // Minimum pixel value of brightness gradient
  unsigned oversampling = 100;        // How much to oversample in X and Y within each pixel
  unsigned width = 101;               // Width of the image
  unsigned height= 101;               // Height of the image
  double start_x = 75;                // Where to put the bead
  double start_y = 50;                // Where to put the bead
  const char *basename = "multi_spot";
  double intensity = 65535;			  // pixel value within a disc, maximum pixel value for a cone or Gaussian
  double radius_std = 10;             // radius of a disc or cone, standard deviation for a Gaussian 
  double motion_rad = 0;
  double motion_spd = 0;
  unsigned frames = 1;
  bool verbose = false;               // Print out info along the way?

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
	} else if (!strncmp(argv[i], "-gradient", strlen("-gradient"))) {
          if (++i > argc) { Usage(argv[0]); }
	  gradient_min = 65535 * atof(argv[i]);
          if (++i > argc) { Usage(argv[0]); }
	  gradient_max = 65535 * atof(argv[i]);
    } else if (!strncmp(argv[i], "-disc", strlen("-disc"))) {
          if (++i > argc) { Usage(argv[0]); }
	  intensity = atof(argv[i]);   // Scale so 1 becomes maximum pixel value
          if (++i > argc) { Usage(argv[0]); }
	  radius_std = atof(argv[i]);
		  if (++i > argc) { Usage(argv[0]); }
	  start_x = atof(argv[i]);
          if (++i > argc) { Usage(argv[0]); }
	  start_y = atof(argv[i]);
          Spot spot(DISC, start_x, start_y, intensity, radius_std);
		  spots.push_back(spot);
    } else if (!strncmp(argv[i], "-cone", strlen("-cone"))) {
          if (++i > argc) { Usage(argv[0]); }
	  intensity = atof(argv[i]);   // Scale 1 becomes maximum pixel value
          if (++i > argc) { Usage(argv[0]); }
	  radius_std = atof(argv[i]);
	      if (++i > argc) { Usage(argv[0]); }
	  start_x = atof(argv[i]);
          if (++i > argc) { Usage(argv[0]); }
	  start_y = atof(argv[i]);
          Spot spot(CONE, start_x, start_y, intensity, radius_std);
		  spots.push_back(spot);
    } else if (!strncmp(argv[i], "-gaussian", strlen("-gaussian"))) {
          if (++i > argc) { Usage(argv[0]); }
	  intensity = atof(argv[i]);   // Scale 1 becomes maximum pixel value
          if (++i > argc) { Usage(argv[0]); }
	  radius_std = atof(argv[i]);
	      if (++i > argc) { Usage(argv[0]); }
	  start_x = atof(argv[i]);
          if (++i > argc) { Usage(argv[0]); }
	  start_y = atof(argv[i]);
          Spot spot(GAUSSIAN, start_x, start_y, intensity, radius_std);
		  spots.push_back(spot);
	} else if (!strncmp(argv[i], "-frames", strlen("-frames"))) {
          if (++i > argc) { Usage(argv[0]); }
	  frames = atoi(argv[i]);
	} else if (!strncmp(argv[i], "-motion", strlen("-motion"))) {
          if (++i > argc) { Usage(argv[0]); }
	  motion_rad = atof(argv[i]);
          if (++i > argc) { Usage(argv[0]); }
	  motion_spd = atof(argv[i]);
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
    // Make the appropriate image using spot vector.
    image_wrapper *beads;
    beads = new multi_spot_image(spots, 0, width-1, 0, height-1, background, 0.0, oversampling, 
		gradient_min, gradient_max, frame, motion_rad, motion_spd); 

    // Write the image to file whose name is the base name with
    // the frame number and .tif added.
    sprintf(filename, "%s.%04d.tif", basename, frame);
    if (verbose) { printf("Writing %s:\n", filename);}
    beads->write_to_grayscale_tiff_file(filename, 0, 1.0, 0.0, true);

	// Find out where the first bead should be
	double x = spots[0].x, y = spots[0].y;
	// Subtracting 1 from the cos() value so that we start at the
    // correct location, not one radius away from it.
    x = spots[0].x + motion_rad * (cos(frame * motion_spd * M_PI/180) - 1);
    y = spots[0].y + motion_rad * sin(frame * motion_spd * M_PI/180);

    // Write the information about the first bead to the CSV file, in a format
    // that matches the header description.  We DO NOT flip y in this report
    fprintf(g_csv_file, "%d,0,%lg,%lg,0,%lg,%lg\n", frame, x, y, radius_std, 0);

    // Clean up things allocated for this frame.
    delete beads;
  }

  // Clean up things allocated for the whole sequence
  if (g_csv_file) { fclose(g_csv_file); g_csv_file = NULL; };
  return 0;
}
