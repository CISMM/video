// To be able to read 16-bit images, we need to use a version of ImageMagick
// that is compiled with -DQuantumDepth=16.  Then, we need to #define QuantumLeap
// before includeing magick/api.h.

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#define QuantumLeap
#include <magick/api.h>
#include "image_wrapper.h"

static	char  magickfilesdir[] = "C:/nsrg/external/pc_win32/bin/ImageMagick-5.5.7-Q16/MAGIC_DIR_PATH";

void Usage (const char * s)
{
  fprintf(stderr,"Usage: %s [-s nx ny] [-i background spot] [-t TYPE] [-os oversampling] center_x center_y radius output_file\n",s);
  fprintf(stderr,"     -s : Size of the image in x and y in pixels (default 256 256)\n");
  fprintf(stderr,"     -i : Intensity of the background and of the spot (default 0 255)\n");
  fprintf(stderr,"     -t : Type of the spot: disc, cone (default disc)\n");
  fprintf(stderr,"     -os : Amount of oversampling to do at each pixel (default 100, min 4)\n");
  fprintf(stderr,"     center_x : X location of the center of the spot\n");
  fprintf(stderr,"     center_y : Y location of the center of the spot\n");
  fprintf(stderr,"     radius : Radius of the spot in pixels\n");
  fprintf(stderr,"     output_file : (string) Name of the output file to write to\n");
  exit(0);
}

// Return a random double in the range [-1..1]
double	unit_random(void)
{
  double val = rand();
  return -1 + 2 * (val / RAND_MAX);
}

main (int argc, char * argv[])
{
  //------------------------------------------------------------------------
  // Parse the command line
  char	*output_file_name = NULL;
  unsigned rows = 256, cols = 256;
  double background = 0, foreground = 255;
  double center_x, center_y;
  double radius;
  enum {disc, cone} type = disc;
  unsigned oversampling = 100;
  image_wrapper  *image = NULL;
  int	realparams = 0;
  int	i;
  i = 1;
  while (i < argc) {
    if (strcmp(argv[i], "-s") == 0) {
        if (++i >= argc) Usage(argv[0]);
        cols = atoi(argv[i]);
        if (++i >= argc) Usage(argv[0]);
        rows = atoi(argv[i]);
    } else if (strcmp(argv[i], "-i") == 0) {
        if (++i >= argc) Usage(argv[0]);
        background = atof(argv[i]);
        if (++i >= argc) Usage(argv[0]);
        foreground = atof(argv[i]);
    } else if (strcmp(argv[i], "-t") == 0) {
        if (++i >= argc) Usage(argv[0]);
	if (strcmp(argv[i], "disc") == 0) {
	  type = disc;
	} else if (strcmp(argv[i], "cone") == 0) {
	  type = cone;
	} else {
	  Usage(argv[0]);
	}
    } else if (strcmp(argv[i], "-os") == 0) {
        if (++i >= argc) Usage(argv[0]);
        oversampling = atoi(argv[i]);
	if (oversampling <= 3) { Usage(argv[0]); }
    } else if (argv[i][0] == '-') {	// Unknown flag
	Usage(argv[0]);
    } else switch (realparams) {		// Non-flag parameters
      case 0:
	  center_x = atof(argv[i]);
	  realparams++;
	  break;
      case 1:
	  center_y = atof(argv[i]);
	  realparams++;
	  break;
      case 2:
	  radius = atof(argv[i]);
	  realparams++;
	  break;
      case 3:
	  output_file_name = argv[i];
	  realparams++;
	  break;
      default:
	  Usage(argv[0]);
    }
    i++;
  }
  if (realparams != 4) {
    Usage(argv[0]);
  }

  //------------------------------------------------------------------------
  // Make the image from which to sample.
  switch (type) {
  case disc:
    image = new disc_image(0,cols-1, 0,rows-1, background, 0.0, center_x, center_y, radius, foreground, oversampling);
    break;
  case cone:
    image = new cone_image(0,cols-1, 0,rows-1, background, 0.0, center_x, center_y, radius, foreground, oversampling);
    break;
  default:
    Usage(argv[0]);
  }
  if (image == NULL) {
    fprintf(stderr, "Out of memory when trying to create image\n");
    exit(1);
  }

  //------------------------------------------------------------------------
  // Add noise to each pixel proportional to the uniform noise and to the
  // scaled intensity noise.  Round to an integer, clamp to the range of pixels
  // available in the image.
  unsigned row,col;
  Quantum *pixels = new Quantum[rows*cols*3];
  if (pixels == NULL) {
    fprintf(stderr, "Out of memory allocating pixels to write to output image\n");
    return -1;
  }
  double val;
  unsigned depth = QuantumDepth;
  unsigned minval = 0;
  unsigned maxval = MaxRGB;
  for (row = 0; row < rows; row++) {
    for (col = 0; col < cols; col++) {

      // Find the value for this pixel and round it to the nearest integer
      // value.  Then clip to the possible values given the quantum and store
      // in the output array.
      val = image->read_pixel_nocheck(row, col);
      val = floor(val+0.5);
      if (val < minval) { val = minval; }
      if (val > maxval) { val = maxval; }
      pixels[(col + cols*row)*3 + 0] =
	pixels[(col + cols*row)*3 + 1] =
	pixels[(col + cols*row)*3 + 2] = ((Quantum)val) << (16 - depth);
    }
  }

  //------------------------------------------------------------------------
  // Initialize the ImageMagick software with a pointer to the place it can
  // find the files it needs to parse different image formats.
#ifdef	_WIN32
  InitializeMagick(magickfilesdir);
#endif

  //------------------------------------------------------------------------
  // Create the image structure to hold the pixels that we have computed above.
  ExceptionInfo	  exception;
  Image		  *out_image;
  ImageInfo       *out_image_info;

  //Initialize the image info structure.
  GetExceptionInfo(&exception);
  out_image=ConstituteImage(cols, rows, "RGB", ShortPixel, pixels, &exception);
  if (out_image == (Image *) NULL) {
      fprintf(stderr, "writeFileMagick: Can't create image.\n");
      return -1;
  }
  out_image_info=CloneImageInfo((ImageInfo *) NULL);
  out_image_info->compression=NoCompression;
  strcpy(out_image_info->filename,output_file_name);
  strcpy(out_image->filename, output_file_name);

  //------------------------------------------------------------------------
  // Write the output image.
  if (WriteImage(out_image_info, out_image) == 0) {
      // print out something to let us know we are missing the 
      // delegates.mgk or whatever if that is the problem instead of just
      // saying the file can't be loaded later
      fprintf(stderr, "nmb_ImgMagic: %s: %s\n",
             exception.reason,exception.description);
      return -1;
  }

  //------------------------------------------------------------------------
  // Close the things that we opened and exit.
  DestroyImageInfo(out_image_info);
  DestroyImage(out_image);
  delete [] pixels;

  return 0;
}