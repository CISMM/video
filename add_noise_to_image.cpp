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
#ifdef  _WIN32
#include <process.h>
#define getpid() _getpid()
#else
#include <sys/types.h>
#include <unistd.h>
#endif

//--------------------------------------------------------------------------
#include <time.h>
#include "stocc.h"                     // define random library classes

static	char  magickfilesdir[] = "C:/nsrg/external/pc_win32/bin/ImageMagick-5.5.7-Q16/MAGIC_DIR_PATH";

void Usage (const char * s)
{
  fprintf(stderr,"Usage: %s [-uniform_noise R] [-intensity_noise R]\n", s);
  fprintf(stderr,"       [-poisson PHOTPERCOUNT DARKPHOT] [-gaussian mean var] [-offset COUNT]\n");
  fprintf(stderr,"       [-intensity_gaussian frac] [-seed SEED] input_file output_file\n");
  fprintf(stderr,"     -seed : (int) Random-number seed to use to generate noise.\n");
  fprintf(stderr,"        This should be used if you want the same answer each time the program\n");
  fprintf(stderr,"        is run on the same input image.  Do not use the same seed for a sequence\n");
  fprintf(stderr,"        of images, or it will produce noise that is correlated between images\n");
  fprintf(stderr,"     (One or more of the following noise parameters should be specified)\n");
  fprintf(stderr,"     -uniform_noise : (real) [Deprecated] specifies half-range of interval within\n");
  fprintf(stderr,"        which a uniform variable will select noise to\n");
  fprintf(stderr,"        be added to the image.  Each pixel will have a\n");
  fprintf(stderr,"        random value from [-uniform_noise..uniform_noise]\n");
  fprintf(stderr,"        added to its value.  This plus the intensity noise will\n");
  fprintf(stderr,"        be added as real numbers, added to the value, and then\n");
  fprintf(stderr,"        rounded and clamped to the available pixel range [0..255] or [0..65535]\n");
  fprintf(stderr,"        The same uniform noise is added to red, green, and blue channels.\n");
  fprintf(stderr,"     -intensity_noise : (real) [Deprecated] specifies fraction of the intensity found at\n");
  fprintf(stderr,"        a pixel that will be used to determine the half-range\n");
  fprintf(stderr,"        of an interval for noise.  Each pixel will have a\n");
  fprintf(stderr,"        random value from [-val*intensity_noise..val*intensity_noise]\n");
  fprintf(stderr,"        added to its value.  This plus the uniform noise will\n");
  fprintf(stderr,"        be added as real numbers, added to the value, and then\n");
  fprintf(stderr,"        rounded and clamped to the available pixel range [0..255] or [0..65535]\n");
  fprintf(stderr,"        The same random number is used to determine red, green, and blue noise.\n");
  fprintf(stderr,"     -poisson : Generate Poisson (shot) noise depending on the number of\n");
  fprintf(stderr,"        photons at each pixel and depending on the amount of dark current to add\n");
  fprintf(stderr,"        to the image.  PHOTPERCOUNT (real) specifies the number of photons per\n");
  fprintf(stderr,"        count in the image.  DARKPHOT (real) specifies the number of dark-current\n");
  fprintf(stderr,"        photons to add to each pixel (these will be sampled from a separate Poisson\n");
  fprintf(stderr,"        distribution and then added to the image count; this assumes that there\n");
  fprintf(stderr,"        is one electron per photon.\n");
  fprintf(stderr,"     -gaussian : Generate Gaussian noise with mean and variance specified\n");
  fprintf(stderr,"     -intensity_gaussian : Generate Gaussian noise that depends on the intensity of\n");
  fprintf(stderr,"                 the signal at a pixel: the variance of the\n");
  fprintf(stderr,"                 noise will be the specified fraction of intensity\n");
  fprintf(stderr,"     -offset : (int) Add the specified offset in counts to all pixels, post-noise\n");
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

int main (int argc, char * argv[])
{
  //------------------------------------------------------------------------
  // Parse the command line
  double  uniform_noise = 0, intensity_noise = 0;
  double  gaussian_mean = 0, gaussian_var = 0;
  double  intensity_gaussian_frac = 0;
  double  photons_per_count = 0, dark_photons = 0;
  double  offset = 0;
  char	*input_file_name = NULL, *output_file_name = NULL;
  int	seed = getpid();  // Random seed in case they don't specify one.
  int	realparams = 0;
  int	i;
  i = 1;
  while (i < argc) {
    if (!strncmp(argv[i], "-uniform_noise", strlen("-uniform_noise"))) {
          if (++i > argc) { Usage(argv[0]); }
	  uniform_noise = atof(argv[i]);
    } else if (!strncmp(argv[i], "-intensity_noise", strlen("-intensity_noise"))) {
          if (++i > argc) { Usage(argv[0]); }
	  intensity_noise = atof(argv[i]);
    } else if (!strncmp(argv[i], "-poisson", strlen("-poisson"))) {
          if (++i > argc) { Usage(argv[0]); }
	  photons_per_count = atof(argv[i]);
          if (++i > argc) { Usage(argv[0]); }
	  dark_photons = atof(argv[i]);
    } else if (!strncmp(argv[i], "-gaussian", strlen("-gaussian"))) {
          if (++i > argc) { Usage(argv[0]); }
	  gaussian_mean = atof(argv[i]);
          if (++i > argc) { Usage(argv[0]); }
	  gaussian_var = atof(argv[i]);
    } else if (!strncmp(argv[i], "-intensity_gaussian", strlen("-intensity_gaussian"))) {
          if (++i > argc) { Usage(argv[0]); }
	  intensity_gaussian_frac = atof(argv[i]);
    } else if (!strncmp(argv[i], "-offset", strlen("-offset"))) {
          if (++i > argc) { Usage(argv[0]); }
	  offset = atof(argv[i]);
    } else if (!strncmp(argv[i], "-seed", strlen("-seed"))) {
          if (++i > argc) { Usage(argv[0]); }
	  seed = atoi(argv[i]);
    } else if (argv[i][0] == '-') {	// Unknown flag
	  Usage(argv[0]);
    } else switch (realparams) {		// Non-flag parameters
      case 0:
	  input_file_name = argv[i];
	  realparams++;
	  break;
      case 1:
	  output_file_name = argv[i];
	  realparams++;
	  break;
      default:
	  Usage(argv[0]);
    }
    i++;
  }
  if (realparams != 2) {
    Usage(argv[0]);
  }

  //------------------------------------------------------------------------
  // Set seeds and make random library to use for Poisson distribution
  srand(seed);
  static StochasticLib1 sto(seed);            // make instance of random library
  double gaussian_std = sqrt(gaussian_var);

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

  unsigned row,col;
  PixelPacket	  *read_pixels, *write_pixels;
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

      r = read_pixels[col + out_image->columns*row].red >> (16 - depth);
      g = read_pixels[col + out_image->columns*row].green >> (16 - depth);
      b = read_pixels[col + out_image->columns*row].blue >> (16 - depth);

      //------------------------------------------------------------------------
      // Handle Poisson sampling for both dark current and shot noise if
      // we have nonzero values for photons_per_count or dark_photons.
      if (photons_per_count > 0) {
        // Convert from counts to photons.
        r *= photons_per_count;
        g *= photons_per_count;
        b *= photons_per_count;

        // Poisson noise on the original photons
        r = sto.Poisson(r);
        g = sto.Poisson(g);
        b = sto.Poisson(b);

        // Dark-noise photons added in, if there are any.
        // Add the same to each channel.
        if (dark_photons > 0) {
          double dark = sto.Poisson(dark_photons);
          r += dark;
          g += dark;
          b += dark;
        }

        // Convert from photons back to counts
        r /= photons_per_count;
        g /= photons_per_count;
        b /= photons_per_count;
      }

      //------------------------------------------------------------------------
      // Add Gaussian uniform and intensity-based noise if the mean and variance
      // are nonzero.
      if ( (gaussian_mean != 0) || (gaussian_std != 0) ) {
        double gaussian = sto.Normal(gaussian_mean, gaussian_std);
        r += gaussian;
        g += gaussian;
        b += gaussian;
      }
      if ( intensity_gaussian_frac != 0) {
        // Sample a unit Gaussian and then scale to suit
        // by the standard deviation (sqrt of variance).
        double unit_gaussian = sto.Normal(0, 1);
        r += unit_gaussian*sqrt(r*intensity_gaussian_frac);
        g += unit_gaussian*sqrt(g*intensity_gaussian_frac);
        b += unit_gaussian*sqrt(b*intensity_gaussian_frac);
      }

      //------------------------------------------------------------------------
      // Add noise to each pixel proportional to the uniform noise and to the
      // scaled intensity noise.  Round to an integer, clamp to the range of pixels
      // available in the image.  These types of noise are unrealistic and should
      // no longer be used.
      double uniform = uniform_noise * unit_random();
      double intensity = intensity_noise * unit_random();
      r += r*intensity + uniform;
      g += g*intensity + uniform;
      b += b*intensity + uniform;

      //------------------------------------------------------------------------
      // Add the camera offset to the pixels
      r += offset;
      g += offset;
      b += offset;

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

