// To be able to read 16-bit images, we need to use a version of ImageMagick
// that is compiled with -DQuantumDepth=16.  Then, we need to #define QuantumLeap
// before includeing magick/api.h.
// Also, notice that we need to define _LIB in order to use the statically-
// linked version of ImageMagick.

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#ifdef  _WIN32
#include <process.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

#define _LIB
#define QuantumLeap
#include <magick/api.h>

//--------------------------------------------------------------------------
#include <time.h>

static	char  magickfilesdir[] = "C:/nsrg/external/pc_win32/bin/ImageMagick-5.5.7-Q16/MAGIC_DIR_PATH";

void Usage (const char * s)
{
  fprintf(stderr,"Usage: %s input_file output_file\n", s);
  fprintf(stderr,"     input_file : (string) Name of the input file to read from\n");
  fprintf(stderr,"     output_file : (string) Name of the output file to write to\n");
  exit(0);
}

int main (int argc, char * argv[])
{
  //------------------------------------------------------------------------
  // Parse the command line
  char	*input_file_name = NULL, *output_file_name = NULL;
  int	realparams = 0;
  int	i;
  i = 1;
  while (i < argc) {
    if (argv[i][0] == '-') {	// Unknown flag
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

      if (r < minval) { r = minval; }
      if (r > maxval) { r = maxval; }
      if (g < minval) { g = minval; }
      if (g > maxval) { g = maxval; }
      if (b < minval) { b = minval; }
      if (b > maxval) { b = maxval; }

	  // Invert brightness values
	  r = maxval - r;
	  g = maxval - g;
	  b = maxval - b;

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

