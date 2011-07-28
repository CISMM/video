#include  <math.h>
#include  <time.h>
#include  <stdio.h>
#include  <stdlib.h>
#include  <iostream>
#include  "spot_tracker.h"

using namespace std;

const char *version = "01.02";

//------------------------------------------------------------------------------------
// Simulation Parameters.
//
// The physical dimensions of the well, not the FOV. I.e. the total space in which beads are randomly generated. 
#define WELL_WIDTH 2000
#define WELL_HEIGHT 2000
// The maximum speed of the stage motor, just a parameter for now. Assume the same limits on both x and y axes.
// Set this to 500 for both COM and RECT and to motion.random_step for PMIN.
#define MAX_SPEED 500
// The mass of an individual bead. Assume unit mass for all beads.
#define BEAD_MASS 1
// The maximum step size of the beads, i.e. motion.random_step
#define BEAD_STEP 5
// The algorithm to use when computing the new FOV: 0 = COM, 1 = RECT, 2 = PMIN.
#define FOV_ALG 0
// The seed value for rand(). Used for testing and evaluation.
#define RAND_SEED 4
//------------------------------------------------------------------------------------

typedef enum { DISC, CONE, GAUSSIAN, ROD
    } SPOT_TYPE;
typedef enum { LINE, CIRCLE, SPIRAL, RANDOM
    } MOTION_TYPE;

void Usage (const char * s)
{
  fprintf(stderr,"Usage: %s [-v] [-background BACK] [oversampling OVER]\n", s);
  fprintf(stderr,"       [-imagesize WIDTH HEIGHT]\n");
  fprintf(stderr,"       [-start X Y] [-frames FRAMES] [-beads NUM R_MIN R_MAX I_MIN I_MAX SPOT_TYPE(disc/cone/gaussian)]\n");
  fprintf(stderr,"       [-disc VALUE RADIUS | cone VALUE RADIUS | -gaussian CENTER_VALUE STD_DEV |\n");
  fprintf(stderr,"       -rod VALUE RADIUS LENGTH]\n");
  fprintf(stderr,"       [-line DX DY | -circle RAD SPEED | -spiral SIZE SPEED | -random SEED MAX_STEP MAX_ANGLE]\n");
  fprintf(stderr,"       [BASENAME]\n");
  fprintf(stderr,"     -v: Verbose mode\n");
  fprintf(stderr,"     -background: Specify the background brightness, infraction of total brightness (default 0)\n");
  fprintf(stderr,"     -oversampling: How many times oversampling in X and Y (default 100)\n");
  fprintf(stderr,"     -imagesize: Specify the image size in X and Y (default 101 101)\n");
  fprintf(stderr,"     -start: Starting location for the bead in pixels (default 75 50)\n");
  fprintf(stderr,"     -frames: The number of output images (default 120)\n");
  fprintf(stderr,"     -beads: The number and characteristics (range of radii, intensities, and stop type) of the randomly placed beads\n");
  fprintf(stderr,"     -disc: Draw a disc-shaped bead of specified value (fraction of total) and radius in pixels (default 1.0 10)\n");
  fprintf(stderr,"     -cone: Draw a cone-shaped bead of specified maximum value (fraction of total) and radius in pixels\n");
  fprintf(stderr,"     -gaussian: Draw a Gaussian-shaped bead of specified maximum fractional brightness and standard deviation in pixels\n");
  fprintf(stderr,"     -rod: Draw a rod shape of specified value (fraction of total) and radius in pixels; angle is in degrees\n");
  fprintf(stderr,"     -line: Move the bead along a specified vector, <x, y>. (default 1 1)\n");
  fprintf(stderr,"     -circle: Move the bead in a circle of specified radius and speed (degrees/frame) (default 50 3)\n");
  fprintf(stderr,"     -spiral: Move the bead in a spiral of specified size and speed (degrees/frame) (try 1 5)\n");
  fprintf(stderr,"     -random: Move the bead randomly with maximum step size in pixels and angle in degrees\n");
  fprintf(stderr,"     BASENAME: Base name for the output file (default ./moving_spot if not specified)\n");
  exit(0);
}

// Return a random double in the range [-1..1]
double	unit_random(void)
{
  double val = rand();
  //cout << "unit_random = " << val << "  ";
  //printf("%f ", -1 + 2 * (val / RAND_MAX));
  return -1 + 2 * (val / RAND_MAX);
}

// Returns a random double between minimum and maximum
double GetRandomNumber(double minimum, double maximum)
{
  double val = rand();
  //printf("min = %f, max = %f\n", minimum, maximum);
  return (val / RAND_MAX) * (maximum - minimum) + minimum;
}

// Returns 1 if position is inside the fov, otherwise returns 0.
int inside_fov(position pos, position fov, int width, int height)
{ 
  if((fov.x-(width/2) < pos.x) && (pos.x < fov.x+(width/2))) {
    if((fov.y-(height/2) < pos.y) && (pos.y < fov.y+(height/2)))
      return 1;
  }
  
  return 0;
} 

// Returns a vector of beads within the current FOV.
std::vector<bead> keep_fov_beads(position fov, int width, int height, std::vector<bead>& b_well)
{ 
  position b_pos;
  std::vector<bead> b_fov;
  
  for(std::vector<bead>::size_type b = 0; b < b_well.size(); b++) {
    // Position of the current bead.
    b_pos.x = b_well[b].x;
    b_pos.y = b_well[b].y;
    
    // If the bead is within the current FOV, record it.
    if(inside_fov(b_pos, fov, width, height) == 1)
      b_fov.push_back(b_well[b]);
  } 
  
  return b_fov;
} 

// Returns the position of the COM in the current frame.
position get_COM_position(std::vector<bead>& b_fov)
{ 
  double sum_x = 0;
  double sum_y = 0;
  position COM_position;
  
  for(std::vector<bead>::size_type b = 0; b < b_fov.size(); b++) {
    sum_x = sum_x + BEAD_MASS*b_fov[b].x;
    sum_y = sum_y + BEAD_MASS*b_fov[b].y;
  }
  
  // Compute the x and y position of the COM
  double M = BEAD_MASS*b_fov.size(); // sum of bead masses
  COM_position.x = sum_x / M;
  COM_position.y = sum_y / M;  
  
  return COM_position;
}

// Find the smallest recorded x and y positions
position find_min_coord(std::vector<bead>& b_fov)
{ 
  double min_x = b_fov[0].x;
  double min_y = b_fov[0].y;
  
  for(std::vector<bead>::size_type b = 1; b < b_fov.size(); b++) {
    if(b_fov[b].x < min_x)
      min_x = b_fov[b].x;
    if(b_fov[b].y < min_y)
      min_y = b_fov[b].y;
  }
  
  position min;
  min.x = min_x;
  min.y = min_y;
  
  return min;
} 

// Find the largest recorded x and y positions
position find_max_coord(std::vector<bead>& b_fov)
{ 
  double max_x = b_fov[0].x;
  double max_y = b_fov[0].y;
  
  for(std::vector<bead>::size_type b = 1; b < b_fov.size(); b++) {
    if(b_fov[b].x > max_x)
      max_x = b_fov[b].x;
    if(b_fov[b].y > max_y)
      max_y = b_fov[b].y;
  }
  
  position max;
  max.x = max_x;
  max.y = max_y;
  
  return max;
}

// Returns the center of the smallest rectangle enclosing the beads.
position get_rect_position(std::vector<bead>& b_fov)
{ 
  // Find the diagonal corners of the rectangle.
  position min, max;
  min = find_min_coord(b_fov);
  max = find_max_coord(b_fov);
  
  // Find the center of the rectangle.
  position rect_position;
  rect_position.x = (min.x + max.x) / 2;
  rect_position.y = (min.y + max.y) / 2;
  
  return rect_position;
}  

// Returns the probability of a bead exiting across the lower edge of the fov (left of bottom). 
double edge_exit_prob(double left_edge, double bead_pos, double step)
{
  if(bead_pos < left_edge-step)
    return 1;
  else if(bead_pos < left_edge+step)
    return (left_edge+step-bead_pos) / (2*step);
  else
    return 0;
}

// Returns the probability of a single bead exiting the given fov in one move.
double prob_single_bead_exit(bead b, position fov, double width, double height)
{
  double step = b.motion.random_step;
  double exit_horizontally = edge_exit_prob(fov.x-width/2, b.x, step)  + edge_exit_prob(-(fov.x+width/2) , -b.x, step);
  double exit_vertically   = edge_exit_prob(fov.y-height/2, b.y, step) + edge_exit_prob(-(fov.y+height/2), -b.y, step);
  
  return 1-(1-exit_horizontally)*(1-exit_vertically);
}

// Returns the overall probability of any beads exiting the given fov.
double prob_bead_list_exit(std::vector<bead>& b_fov, position fov, int width, int height)
{ 
  double prob = 1;
  
  // Probability of all beads staying within the fov.
  for(std::vector<bead>::size_type b = 0; b < b_fov.size(); b++)
    prob *= 1 - prob_single_bead_exit(b_fov[b], fov, width, height);
  
  // Probabilily of any beads exiting the fov.
  return 1 - prob;
} 

// Returns the fov position that will minimize the overall probability of any beads exiting the frame.
position get_minp_position(position fov, int width, int height, std::vector<bead>& b_fov, double step)
{ 
  position current_fov, minp_fov;
  double prob_current, prob_min;
  
  // Initialize the minp fov and probability to the initial fov.
  minp_fov.x = fov.x;
  minp_fov.y = fov.y;
  prob_min = prob_bead_list_exit(b_fov, fov, width, height);
  cout << "before search loop : prob_min = " << prob_min << endl;
  
  // If the initial fov already has a probability of zero, we are done.
  if(prob_min == 0) {
    cout << "return initial fov" << endl;
    return fov;
  }
  
  // Search to find the best fov.
  for(current_fov.y = fov.y - MAX_SPEED; current_fov.y < fov.y + MAX_SPEED; current_fov.y += step) {
    for(current_fov.x = fov.x - MAX_SPEED; current_fov.x < fov.x + MAX_SPEED; current_fov.x += step) {
      prob_current = prob_bead_list_exit(b_fov, current_fov, width, height);
      if(prob_current < prob_min){
        prob_min = prob_current;
        cout << "inside search loop : prob_min = " << prob_min << endl;
        minp_fov = current_fov;
      }
      // Once a fov with probability zero has been found, we are done.
      if(prob_min == 0)
        return minp_fov;
    }
  }
  
  return minp_fov;
} 

// Limits the motion of the fov in one time step to MAX_SPEED of the stage motors.
position keep_speed_under(position current_fov, position new_fov)
{ 
  // limit x axis motion
  if(abs(current_fov.x - new_fov.x) > MAX_SPEED) {
    if(current_fov.x < new_fov.x) // moving right
      new_fov.x = current_fov.x + MAX_SPEED;
    else // moving left
      new_fov.x = current_fov.x - MAX_SPEED;
  }
  
  // limit y axis motion
  if(abs(current_fov.y - new_fov.y) > MAX_SPEED) {
    if(current_fov.y < new_fov.y) // moving up
      new_fov.y = current_fov.y + MAX_SPEED;
    else // moving down
      new_fov.y = current_fov.y - MAX_SPEED;
  }
  
  return new_fov;
}

// Limits the motion of the fov to the physical size of the well.
position keep_fov_within(position fov, int width, int height)
{ 
  // limit x FOV position
  if((fov.x-(width/2)) < 0)
    fov.x = (width/2)-1;
  if((fov.x+(width/2)) > WELL_WIDTH)
    fov.x = WELL_WIDTH-(width/2)-1;
  
  // limit y FOV position
  if((fov.y-(height/2)) < 0)
    fov.y = (height/2)-1;
  if((fov.y+(height/2)) > WELL_HEIGHT)
    fov.y = WELL_HEIGHT-(height/2)-1;
  
  return fov;
}

// Returns a new FOV position based on the current FOV and the beads within it. 
position get_new_fov(position current_fov, int width, int height, std::vector<bead>& b_fov)
{ 
  position new_fov;
  
  // Which algorithm to use.
  switch (FOV_ALG) {
    case 0: // Compute bead COM
      new_fov = get_COM_position(b_fov);
      cout << "COM : x = " << new_fov.x << ",  y = " << new_fov.y << endl;
      break;
    
    case 1: // Compute bead rectangle
      new_fov = get_rect_position(b_fov);
      cout << "RECT : x = " << new_fov.x << ",  y = " << new_fov.y << endl;  
      break;
    
    case 2: // Compute minimum probability
      new_fov = get_minp_position(current_fov, width, height, b_fov, 0.5); // step = 0.5 works well.
      cout << "PMIN : x = " << new_fov.x << ", y = " << new_fov.y << endl;
      break;
  }

  // MAX_SPEED of the stage can not be exceeded
  new_fov = keep_speed_under(current_fov, new_fov);
  cout << "MAX_SPEED : x = " << new_fov.x << ",  y = " << new_fov.y << endl;   
  
  // FOV must stay within the bounds of the well
  new_fov = keep_fov_within(new_fov, width, height);
  cout << "Well bounds : x = " << new_fov.x << ",  y = " << new_fov.y << endl << endl;   
  
  return new_fov;
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
  int num_beads = 1;                  // How many random beads to draw
  double r_min = 4.0;                 // Minimum radius for randomly drawn beads
  double r_max = 9.0;                 // Maximum radius for randomly drawn beads
  double intensity_min = 0.7;         // Minimum intensity for randomly drawn beads
  double intensity_max = 1.0;         // Maximum intensty for randomly drawn beads
  SPOT_TYPE spot_type = DISC;         // Type of spot to use
  double disc_value = 65535;          // Pixel value within a disc
  double disc_radius = 10;            // Radius of a disc in pixels
  double cone_value = 65535;          // Maximum pixel value within a cone
  double cone_radius = 10;            // Radius of a cone in pixels
  double gaussian_value;              // Maximum pixel value obtained by the Gaussian
  double gaussian_std;                // Standard deviation of a Gaussian
  double rod_value = 65535;           // Pixel value within a rod
  double rod_radius = 65535;          // Raidus of a rod in pixels
  double rod_length = 65535;          // Length of a rod in pixels
  MOTION_TYPE motion_type = CIRCLE;
  double line_x = 1;                  // The x component of the linear motion vector
  double line_y = 1;                  // The y component of the linear motion vector
  double circle_radius = 25;          // Radius of circular motion
  double circle_speed = 3;            // Speed of moving around the circle in degrees/second
  double spiral_size = 1;             // Larger numbers make a looser spiral
  double spiral_speed = 1;            // Speed of moving around the spiral
  int random_seed = 10;               // Seed for random number generator
  double random_step = 1;             // Maximum magnitude of noise step
  double random_angle = 1;            // Maximum degrees of angle change
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
    } else if (!strncmp(argv[i], "-start", strlen("-start"))) {
          if (++i > argc) { Usage(argv[0]); }
	  start_x = atof(argv[i]);
          if (++i > argc) { Usage(argv[0]); }
	  start_y = atof(argv[i]);
    } else if (!strncmp(argv[i], "-frames", strlen("-frames"))) {
          if (++i > argc) { Usage(argv[0]); }
	  frames = atoi(argv[i]);
    } else if (!strncmp(argv[i], "-beads", strlen("-beads"))) {
          if (++i > argc) { Usage(argv[0]); }
	  num_beads = atoi(argv[i]);
          if (++i > argc) { Usage(argv[0]); }
	  r_min = atof(argv[i]);
          if (++i > argc) { Usage(argv[0]); }
	  r_max = atof(argv[i]);
          if (++i > argc) { Usage(argv[0]); }
	  intensity_min = atof(argv[i]);
          if (++i > argc) { Usage(argv[0]); }
	  intensity_max = atof(argv[i]);
          if (++i > argc) { Usage(argv[0]); }
      if (!strncmp(argv[i], "disc", strlen("disc")))
	    spot_type = DISC;
      else if (!strncmp(argv[i], "cone", strlen("cone")))
        spot_type = CONE;
      else
        spot_type = GAUSSIAN;
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
    } else if (!strncmp(argv[i], "-rod", strlen("-rod"))) {
          if (++i > argc) { Usage(argv[0]); }
	  rod_value = 65535 * atof(argv[i]);   // Scale 1 becomes maximum pixel value
          if (++i > argc) { Usage(argv[0]); }
	  rod_radius = atof(argv[i]);
          if (++i > argc) { Usage(argv[0]); }
	  rod_length = atof(argv[i]);
          spot_type = ROD;
    } else if (!strncmp(argv[i], "-line", strlen("-line"))) {
          if (++i > argc) { Usage(argv[0]); }
	  line_x = atof(argv[i]);   
          if (++i > argc) { Usage(argv[0]); }
	  line_y = atof(argv[i]);
          motion_type = LINE;
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
    } else if (!strncmp(argv[i], "-v", strlen("-v"))) {
          verbose = true;
    } else if (argv[i][0] == '-') {	// Unknown flag
	  Usage(argv[0]);
    } else switch (realparams) { // Non-flag parameters
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

  if (verbose) { printf("\nVersion %s\n", version); }

  const unsigned MAX_NAME = 2048;
  char filename[MAX_NAME];
  if (strlen(basename) > MAX_NAME - strlen(".0000.tif")) {
    fprintf(stderr,"Base file name too long\n");
    return -1;
  }

  //------------------------------------------------------------------------------------
  // Open the CSV file that will record the motion and put the header information into it.
  FILE  *g_csv_file = NULL;		  // File to save data in with .csv extension
  char *csvname = new char[strlen(basename)+5];	// Remember the closing '\0'
  if (csvname == NULL) {
    fprintf(stderr, "Out of memory when allocating CSV file name\n");
    return -1;
  }
  sprintf(csvname, "%s.csv", basename);
  if ( NULL == (g_csv_file = fopen(csvname, "w")) ) {
    fprintf(stderr,"Cannot open CSV file for writing: %s\n", csvname);
  } else {
    fprintf(g_csv_file, "FrameNumber,");
    for(int i = 0; i < num_beads; i++) {
      fprintf(g_csv_file, "Spot ID,X,Y,Z,Radius,Intensity,");
    }
    fprintf(g_csv_file, "\n");
  }
  delete [] csvname;
  
  // Initialize the FOV position. For now, just put the initial FOV directly in the center of the well.
  position fov_center;
  fov_center.x =  WELL_WIDTH / 2;
  fov_center.y = WELL_HEIGHT / 2;
  
  //------------------------------------------------------------------------------------
  // Generate the bead vector, containing the desired number of randomly placed beads.
  std::vector<bead> beads; // list of all beads in the well
  std::vector<bead> fov_beads; // list of all beads in the current FOV
  std::vector<std::vector<bead> > all_fovs_beads; //list of all beads in the FOV for all frames  
  bead random_bead; // an individual randomly placed bead
  
  // a black point in the current frame denoting the next location of the fov_center
  position test_position;
  bead test_bead; 
  test_bead.r = 2;
  test_bead.intensity = 1.00;
  
  //srand( time(NULL) );
  srand(RAND_SEED);
  
  //cout << "Initial Bead Positions :" << endl;
  for(int i = 0; i < num_beads; i++) {
    // generate beads in the entire well
    random_bead.id = i;
    random_bead.x = GetRandomNumber(0.0, WELL_WIDTH  - 1);
    random_bead.y = GetRandomNumber(0.0, WELL_HEIGHT - 1);
    //cout << "random_bead.x = " << random_bead.x << "  random_bead.y = " << random_bead.y << endl;
    // bead radius and intensity
    random_bead.r = GetRandomNumber(r_min, r_max);
    random_bead.intensity = 65535 * GetRandomNumber(intensity_min, intensity_max);
    // bead motion type and parameters
    random_bead.motion.type = RANDOM; // later, only a portion of the beads will move randomly
    random_bead.motion.random_step = BEAD_STEP; // assume that all beads have the same max step size
    //random_bead.motion.type = LINE;
    //random_bead.motion.line_x = 5; 
    //random_bead.motion.line_y = 0;
    
    beads.push_back(random_bead);
  }

  //------------------------------------------------------------------------------------
  // Generate images matching the requested motion and disc types.
  unsigned frame;
  for (frame = 0; frame < frames; frame++) {
    // Save the frame number to the CSV file.
    fprintf(g_csv_file, "%d,", frame);
    
    // Update all bead positions for the current frame.
    for (std::vector<bead>::size_type b = 0; b < beads.size(); b++) {
      if (frame == 0){ // if this is the very first frame, just output the original info
        fprintf(g_csv_file, "%d,%lg,%lg,0,%lg,%lg,", b, beads[b].x, beads[b].y, beads[b].r, beads[b].intensity);
        continue;
      }
      
      // Record the current positions as the previous time step positions.
      beads[b].old_x = beads[b].x;
      beads[b].old_y = beads[b].y;
           
      // Find out where this bead should be based on its motion type.
      switch (beads[b].motion.type) {
        case LINE:
          beads[b].x = beads[b].old_x + beads[b].motion.line_x;
          beads[b].y = beads[b].old_y + beads[b].motion.line_y;
          break;
        case RANDOM:
          beads[b].x = beads[b].old_x + beads[b].motion.random_step * unit_random();
          beads[b].y = beads[b].old_y + beads[b].motion.random_step * unit_random();
          //cout << "tmp_x = " << tmp_x << "  tmp_y = " << tmp_y;
          //cout << "  beads[b].old_x = " << beads[b].old_x << "  beads[b].old_y = " << beads[b].old_y;
          //cout << "  beads[b].motion.random_step = " << beads[b].motion.random_step;
          //cout << "  beads[b].x = " << beads[b].x << "  beads[b].y = " << beads[b].y << endl;
          break;
      }
      
      // Write the information about the current bead to the CSV file, in a format
      // that matches the header description. We DO NOT flip y in this report.
      
      // The x and y positions are recorded relative to the lower left hand corner
      // of the image (the entire well), which denotes position (x = 0, y = 0).
      fprintf(g_csv_file, "%d,%lg,%lg,0,%lg,%lg,", b, beads[b].x, beads[b].y, beads[b].r, beads[b].intensity);
    }
    // Start a new row for the next frame in the CSV file.
    fprintf(g_csv_file, "\n");
    
    // Filter the beads vector to the current FOV.
    fov_beads = keep_fov_beads(fov_center, width, height, beads);
    // Save the list of beads within the FOV.
    all_fovs_beads.push_back(fov_beads);
    
    // Determine the test point position, i.e. the next location of the fov_center
    cout << "Frame #" << frame <<endl;
    test_position = get_new_fov(fov_center, width, height, fov_beads);
    test_bead.x   = test_position.x;
    test_bead.y   = test_position.y;
    fov_beads.push_back(test_bead);
    
    //------------------------------------------------------------------------------------
    // Visualize only the beads within the current FOV.     
    image_wrapper *bead_image; // pointer to the image_wrapper class 
    // Make the appropriate image based on the bead type.
    switch (spot_type) {
      case DISC:
        bead_image = new multi_disc_image(fov_beads, 
                                          (int) fov_center.x-(width/2) , (int) fov_center.x+(width/2)-1,  // left and right edge
                                          (int) fov_center.y-(height/2), (int)fov_center.y+(height/2)-1, // bottom and top edge
                                          background, 0.0,       
                                          oversampling);
        break;
      
      case CONE:
        bead_image = new multi_cone_image(fov_beads,
                                          (int) fov_center.x-(width/2) , (int) fov_center.x+(width/2)-1,  // left and right edge
                                          (int) fov_center.y-(height/2), (int) fov_center.y+(height/2)-1, // bottom and top edge
                                          background, 0.0,
                                          oversampling);
        break;
      
      case GAUSSIAN:
        // Figure out the volume that will make the Gaussian have a maximum value
        // equal to gaussian_value.  The volume under the curve is equal to
        // std^2 * 2 * PI; this is divided by to make it unit volume, so we need
        // to multiply by this to get back to unit height and then multiply again
        // by the value we want.
        bead_image = new Integrated_multi_Gaussian_image(fov_beads,
                                          (int) fov_center.x-(width/2) , (int) fov_center.x+(width/2)-1,  // left and right edge
                                          (int) fov_center.y-(height/2), (int) fov_center.y+(height/2)-1, // bottom and top edge
                                          background, 0.0,
                                          oversampling);
        break;
    }
    
    // Write the image to file whose name is the base name with
    // the frame number and .tif added.
    sprintf(filename, "%s.%04d.tif", basename, frame);
    if (verbose) { printf("Writing %s:\n", filename); }
    // comment the next line when generating a giant .csv file with all the beads
    bead_image->write_to_grayscale_tiff_file(
                  filename,
                  0,
                  1.0,
                  0.0,
                  false, 
                  "C:/NSRG/external/pc_win32/ImageMagick/MAGIC_DIR_PATH",
                  true);
    
    // Clean up things allocated for this frame.
    delete bead_image;

    //------------------------------------------------------------------------------------
    // Get a new FOV. Use only the beads within the curent FOV to decide where to move next.
    fov_beads.pop_back(); // drop the test point from the list
    fov_center = get_new_fov(fov_center, width, height, fov_beads);
  }

  // Print the beads in the initial FOV.
  std::vector<bead> fov_print = all_fovs_beads.front();
  cout << "Beads in initial FOV: ";
  for(std::vector<bead>::size_type i = 0; i < fov_print.size(); i++) {
    if(i == fov_print.size()-1)
      cout << fov_print[i].id << endl << endl;
    else
      cout << fov_print[i].id << ", "; 
  }
  
  // Print the beads in the final FOV.
  fov_print = all_fovs_beads.back();
  cout << "Beads in final FOV: ";
  for(std::vector<bead>::size_type i = 0; i < fov_print.size(); i++) {
    if(i == fov_print.size()-1)
      cout << fov_print[i].id << endl << endl;
    else
      cout << fov_print[i].id << ", "; 
  }
  
  // Print the percentage of initial beads retained.
  //cout << "Percent beads retained: " << 

  // Print the number of beads visible in each of the frames.
  cout << "Number of beads in each FOV: ";
  for(std::vector<std::vector<bead> >::size_type i = 0; i < all_fovs_beads.size(); i++) {
    if(i == all_fovs_beads.size()-1)
      cout << all_fovs_beads[i].size();
    else
      cout << all_fovs_beads[i].size() << ", ";
  }
  
  // Clean up things allocated for the whole sequence
  if (g_csv_file) { fclose(g_csv_file); g_csv_file = NULL; };
  return 0;
}
