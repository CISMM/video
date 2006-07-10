// To be able to read 16-bit images, we need to use a version of ImageMagick
// that is compiled with -DQuantumDepth=16.  Then, we need to #define QuantumLeap
// before includeing magick/api.h.
// Also, notice that we need to define _LIB in order to use the statically-
// linked version of ImageMagick.

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#define _LIB
#define QuantumLeap
#include <magick/api.h>

static	char  magickfilesdir[] = "C:/nsrg/external/pc_win32/bin/ImageMagick-5.5.7-Q16/MAGIC_DIR_PATH";

void Usage (const char * s)
{
  fprintf(stderr,"Usage: %s seed uniform_noise intensity_noise input_file output_file\n",s);
  fprintf(stderr,"     seed : (int) Random-number seed to use to generate noise\n");
  fprintf(stderr,"     uniform_noise : (real) specifies half-range of interval within\n");
  fprintf(stderr,"        which a uniform variable will select noise to\n");
  fprintf(stderr,"        be added to the image.  Each pixel will have a\n");
  fprintf(stderr,"        random value from [-uniform_noise..uniform_noise]\n");
  fprintf(stderr,"        added to its value.  This plus the intensity noise will\n");
  fprintf(stderr,"        be added as real numbers, added to the value, and then\n");
  fprintf(stderr,"        rounded and clamped to the available pixel range [0..255] or [0..65535]\n");
  fprintf(stderr,"        The same uniform noise is added to red, green, and blue channels.\n");
  fprintf(stderr,"     intensity_noise : (real) specifies fraction of the intensity found at\n");
  fprintf(stderr,"        a pixel that will be used to determine the half-range\n");
  fprintf(stderr,"        of an interval for noise.  Each pixel will have a\n");
  fprintf(stderr,"        random value from [-val*intensity_noise..val*intensity_noise]\n");
  fprintf(stderr,"        added to its value.  This plus the uniform noise will\n");
  fprintf(stderr,"        be added as real numbers, added to the value, and then\n");
  fprintf(stderr,"        rounded and clamped to the available pixel range [0..255] or [0..65535]\n");
  fprintf(stderr,"        The same random number is used to determine red, green, and blue noise.\n");
  fprintf(stderr,"     input_file : (string) Name of the input file to read from\n");
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
  double  uniform_noise, intensity_noise;
  char	*input_file_name = NULL, *output_file_name = NULL;
  int	seed;
  int	realparams = 0;
  int	i;
  i = 1;
  while (i < argc) {
    if (argv[i][0] == '-') {	// Unknown flag
	  Usage(argv[0]);
    } else switch (realparams) {		// Non-flag parameters
      case 0:
	  seed = atoi(argv[i]);
	  srand(seed);
	  realparams++;
	  break;
      case 1:
	  uniform_noise = atof(argv[i]);
	  realparams++;
	  break;
      case 2:
	  intensity_noise = atof(argv[i]);
	  realparams++;
	  break;
      case 3:
	  input_file_name = argv[i];
	  realparams++;
	  break;
      case 4:
	  output_file_name = argv[i];
	  realparams++;
	  break;
      default:
	  Usage(argv[0]);
    }
    i++;
  }
  if (realparams != 5) {
    Usage(argv[0]);
  }

  //------------------------------------------------------------------------
  // Initialize the ImageMagick software with a pointer to the place it can
  // find the files it needs to parse different image formats.
#ifdef	_WIN32
  InitializeMagick(magickfilesdir);
#endif

  //------------------------------------------------------------------------
  // Read the image from the input file whose name is passed, or from
  // stdin if no name is given.
  ExceptionInfo	  exception;
  Image		  *in_image, *out_image;
  ImageInfo       *in_image_info, *out_image_info;

  //Initialize the image info structure and read an image.
  GetExceptionInfo(&exception);
  in_image_info=CloneImageInfo((ImageInfo *) NULL);
  strcpy(in_image_info->filename,input_file_name);
  in_image=ReadImage(in_image_info,&exception);
  if (in_image == (Image *) NULL) {
      // print out something to let us know we are missing the 
      // delegates.mgk or whatever if that is the problem instead of just
      // saying the file can't be loaded later
      fprintf(stderr, "nmb_ImgMagic: %s: %s\n",
             exception.reason,exception.description);
      return -1;
  }

  //------------------------------------------------------------------------
  // Create an output image of the same size and bit depth as the image we
  // just read.
  out_image = CloneImage(in_image, 0, 0, MagickTrue, &exception);
  if (out_image == NULL) {
      // print out something to let us know we are missing the 
      // delegates.mgk or whatever if that is the problem instead of just
      // saying the file can't be loaded later
      fprintf(stderr, "nmb_ImgMagic: %s: %s\n",
             exception.reason,exception.description);
      return -1;
  }
  out_image_info=CloneImageInfo(in_image_info);
  strcpy(out_image_info->filename,output_file_name);
  strcpy(out_image->filename, output_file_name);

  //------------------------------------------------------------------------
  // Add noise to each pixel proportional to the uniform noise and to the
  // scaled intensity noise.  Round to an integer, clamp to the range of pixels
  // available in the image.
  unsigned row,col;
  PixelPacket	  *read_pixels, *write_pixels;
  double uniform, intensity;
  double r,g,b;
  read_pixels = GetImagePixels(out_image, 0,0, out_image->columns, out_image->rows);
  unsigned depth = out_image->depth;
  unsigned minval = 0;
  unsigned maxval = (1 << depth) - 1;
  if (read_pixels == NULL) {
      // print out something to let us know we are missing the 
      // delegates.mgk or whatever if that is the problem instead of just
      // saying the file can't be loaded later
      fprintf(stderr, "nmb_ImgMagic: %s: %s\n",
             exception.reason,exception.description);
      return -1;
  }
  write_pixels = SetImagePixels(out_image, 0,0, out_image->columns, out_image->rows);
  if (write_pixels == NULL) {
      // print out something to let us know we are missing the 
      // delegates.mgk or whatever if that is the problem instead of just
      // saying the file can't be loaded later
      fprintf(stderr, "nmb_ImgMagic: %s: %s\n",
             exception.reason,exception.description);
      return -1;
  }
  for (row = 0; row < out_image->rows; row++) {
    for (col = 0; col < out_image->columns; col++) {
      uniform = uniform_noise * unit_random();
      intensity = intensity_noise * unit_random();

      r = read_pixels[col + out_image->columns*row].red >> (16 - depth);
      g = read_pixels[col + out_image->columns*row].green >> (16 - depth);
      b = read_pixels[col + out_image->columns*row].blue >> (16 - depth);

      r += r*intensity + uniform;
      g += g*intensity + uniform;
      b += b*intensity + uniform;

      r = floor(r+0.5);
      g = floor(g+0.5);
      b = floor(b+0.5);

      if (r < minval) { r = minval; }
      if (r > maxval) { r = maxval; }
      if (g < minval) { g = minval; }
      if (g > maxval) { g = maxval; }
      if (b < minval) { b = minval; }
      if (b > maxval) { b = maxval; }

      write_pixels[col + out_image->columns*row].red = ((Quantum)r) << (16 - depth);
      write_pixels[col + out_image->columns*row].green = ((Quantum)g) << (16 - depth);
      write_pixels[col + out_image->columns*row].blue = ((Quantum)b) << (16 - depth);
    }
  }
  SyncImagePixels(out_image);

  //------------------------------------------------------------------------
  // Write the output image, either to the filename or to stdout.
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
  DestroyImageInfo(in_image_info);
  DestroyImage(in_image);
  DestroyImageInfo(out_image_info);
  DestroyImage(out_image);

  return 0;
}