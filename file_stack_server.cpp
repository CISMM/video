// To be able to read 16-bit images, we need to use a version of ImageMagick
// that is compiled with -DQuantumDepth=16.  Then, we need to #define QuantumLeap
// before includeing magick/api.h.
// We don't use ImageMagick at all if we're using the MinGW compiler set, because
// we can't link against its X11 library.

#include "file_stack_server.h"
#include "file_list.h"

#if !defined(VIDEO_NO_IMAGEMAGICK)
  #define QuantumLeap
  // When statically linking with ImageMagick, you need to define _LIB here or
  // in the preprocessor so it knows to link correctly.
  // Then you also need to link with wsock32.lib on Windows.
  #define _LIB
  #include <magick/api.h>
#endif

#include <list>
using namespace std;

#ifdef	_WIN32
  const char READSTRING[] = "rb";
#else
  const char READSTRING[] = "r";
#endif

// This is to ensure that we only call InitializeMagick once.
bool file_stack_server::ds_majickInitialized = false;

// This routine will open, read, and discard the image from the file
// whose name is passed in.  Its purpose is to pull the file into the
// operating system's memory cache so that when it is actually needed
// it will be immediately available.
// This routine operates in a separate thread and in a non-blocking
// manner so that it does not slow down the main process (unless it
// competes for disk bandwidth).
// To make this thread prefetch a file, change the string pointed to
// in its userdata to a new name.
static void prefetch_thread_func(vrpn_ThreadData &threadData)
{
  string *fileName = static_cast<string *>(threadData.pvUD);

  // Record the original file name.
  string oldFileName = *fileName;

  // Whenever the new name becomes not the same as the last name,
  // prefetch the file.  Sleep a little between tests to avoid
  // eating an entire CPU in busy-wait.
  do {
    if (oldFileName != *fileName) {
      oldFileName = *fileName;
#if !defined(VIDEO_NO_IMAGEMAGICK)
      ExceptionInfo   exception;
      Image           *image;
      ImageInfo       *image_info;

      //Initialize the image info structure and read an image.
      GetExceptionInfo(&exception);
      image_info=CloneImageInfo((ImageInfo *) NULL);
      (void) strcpy(image_info->filename,oldFileName.c_str());

      image=ReadImage(image_info,&exception);
      if (image != (Image *) NULL) {
        DestroyImageInfo(image_info);
        DestroyImageList(image);
      }

#endif
    } else {
      vrpn_SleepMsecs(1);
    }
  } while (true);
}

file_stack_server::file_stack_server(const char *filename, const char *magickfilesdir) :
d_buffer(NULL),
d_mode(SINGLE),
d_xFileSize(0),
d_yFileSize(0),
d_listOfImages(NULL)
{
  // In case we fail somewhere along the way
  _status = false;

  // If we've not called MagickIncarnate(), do so now
  if (!ds_majickInitialized) {
      ds_majickInitialized = true;
#if !defined(VIDEO_NO_IMAGEMAGICK)
#ifdef	_WIN32
      InitializeMagick(magickfilesdir);
#endif
#endif
  }

  // Get a list of the files in the same directory as the file we
  // just opened.  If it is empty, then we've got a problem and should
  // bail.
  if (!file_list(filename, d_fileNames)) {
    fprintf(stderr,"file_stack_server::file_stack_server(): Could not get file name list\n");
    return;
  }

  // Open the first file and find out how big it is.
  // This routine has some side effects: It allocates the buffer and it fills in
  // the d_xFileSize and d_yFileSize data members.
  if (!read_image_from_file(*d_fileNames.begin())) {
    fprintf(stderr,"file_stack_server::file_stack_server(): Can't read %s\n", d_fileNames.begin()->c_str());
    return;
  }

  // Start the prefetch thread
  vrpn_ThreadData td;
  td.pvUD = &d_prefetchName;
  d_prefetchThread = new vrpn_Thread(prefetch_thread_func, td);
  if (d_prefetchThread == NULL) {
    fprintf(stderr,"file_stack_server::file_stack_server(): Can't create prefetch thread\n");
    return;
  }
  if (!d_prefetchThread->go()) {
    fprintf(stderr,"file_stack_server::file_stack_server(): Can't run prefetch thread\n");
    return;
  }

  // Everything opened okay.
  _minX = _minY = 0;
  _maxX = d_xFileSize-1;
  _maxY = d_yFileSize-1;
  _num_columns = d_xFileSize;
  _num_rows = d_yFileSize;
  _binning = 1;
  d_whichFile = d_fileNames.begin();
  _status = true;
}

file_stack_server::~file_stack_server(void)
{
  // Stop the prefetch thread, if there is one
  if (d_prefetchThread) {
    d_prefetchThread->kill();
  }

  // Free the space taken by the in-memory image copy (if allocated)
  if (d_buffer) {
    delete [] d_buffer;
    d_buffer = NULL;
  }

#if !defined(VIDEO_NO_IMAGEMAGICK)
  // If we have a multi-layer image open, then destroy it.
  Image *image = static_cast<Image *>(d_listOfImages);
  if (image != NULL) {
    DestroyImageList(image);
    d_listOfImages = NULL;
  }
#endif
}

void  file_stack_server::play()
{
  d_mode = PLAY;
}

void  file_stack_server::pause()
{
  d_mode = PAUSE;
}

void  file_stack_server::rewind()
{
#if !defined(VIDEO_NO_IMAGEMAGICK)

  // If we have a multi-layer image open, then destroy it
  // and set our open-image pointer to NULL to indicate that
  // we don't have one open.  This will cause it to be
  // opened the next time.
  Image *image = static_cast<Image *>(d_listOfImages);
  if (image != NULL) {
    DestroyImageList(image);
    d_listOfImages = NULL;
  }
#endif

  // Seek back to the first file
  d_whichFile = d_fileNames.begin();

  // Read one frame when we start
  d_mode = SINGLE;
}

void  file_stack_server::single_step()
{
  d_mode = SINGLE;
}

// This routine has some side effects: It allocates the buffer and it fills in
// the d_xFileSize and d_yFileSize data members.
//*** Note: This routine is complicated by the fact that some images (TIFF files
// in particular) can have more than one actual image in them.  In this case, there
// is an additional side effect that the d_listOfImages variable holds a pointer
// to an already-opened image when we're not at the end of the stack in that
// one image.  In this case, we step forward in the stack for that image rather
// than going to the next image in the file list.
bool  file_stack_server::read_image_from_file(const string filename)
{
#if !defined(VIDEO_NO_IMAGEMAGICK)

  ExceptionInfo	  exception;
  Image		  *image;
  ImageInfo       *image_info;
  PixelPacket	  *pixels;
#if (MagickLibVersion <= 0x649)
  ViewInfo	*vinfo;
#else
  CacheView	*vinfo;
#endif

  //Initialize the image info structure and read an image.
  GetExceptionInfo(&exception);
  image_info=CloneImageInfo((ImageInfo *) NULL);
  (void) strcpy(image_info->filename,filename.c_str());

  // If we have a multi-layer image in memory from last time, we
  // use it.  Otherwise, go ahead and open a new image based on the
  // file name.

  if (d_listOfImages != NULL) {
    image = static_cast<Image *>(d_listOfImages);
  } else {
    image=ReadImage(image_info,&exception);
    if (image == (Image *) NULL) {
      // print out something to let us know we are missing the 
      // delegates.mgk or whatever if that is the problem instead of just
      // saying the file can't be loaded later
      fprintf(stderr, "file_stack_server::read_image_from_file(): ReadImage() failed: %s: %s\n",
             exception.reason,exception.description);
      //MagickError(exception.severity,exception.reason,exception.description);
      // Get here if we can't decipher the file, let caller handle it. 
      return false;
    }
  }

  // Ensure that we don't get images of different sizes.  If this image is the
  // wrong size, then return false.  If we've never read any images, then the
  // sizes will be 0.
  if (d_xFileSize == 0) {
    d_xFileSize = image->columns;
    d_yFileSize = image->rows;
  } else if ( (d_xFileSize != image->columns) || (d_yFileSize != image->rows) ) {
    fprintf(stderr,"file_stack_server::read_image_from_file(): Image size differs\n");
    return false;
  }

  // Allocate space to read a frame from the file, if it has not yet been
  // allocated
  if (d_buffer == NULL) {
    if ( (d_buffer = new vrpn_uint16[d_xFileSize * d_yFileSize * 3]) == NULL) {
      fprintf(stderr,"file_stack_server::read_image_from_file: Out of memory\n");
      _status = false;
      d_mode = PAUSE;
      return false;
    }
  }

  // This is the method for reading pixels that compiles and works, 
  // as opposed to GetImagePixels or GetOnePixel, which wouldn't compile. 
  vinfo = AcquireCacheView(image);
  pixels = GetCacheViewPixels(vinfo, 0,0,image->columns,image->rows);
  if(!pixels) {
      fprintf(stderr, "file_stack_server::read_image_from_file: unable to get pixel cache.\n"); 
      return false;
  }

  // Copy the pixels from the image into the buffer. Flip the image over in Y
  // to match the orientation we expect.  Note that if we have an 8-bit image,
  // ImagemMagick will have shifted it left to the most-significant-byte.
  unsigned x, y, flip_y;
  for (y = 0; y < d_yFileSize; y++) {
    flip_y = (d_yFileSize - 1) - y;
    for (x = 0; x < d_xFileSize; x++) {
      d_buffer[ (x + flip_y * d_xFileSize) * 3 + 0 ] = pixels[x + image->columns*y].red;
      d_buffer[ (x + flip_y * d_xFileSize) * 3 + 1 ] = pixels[x + image->columns*y].green;
      d_buffer[ (x + flip_y * d_xFileSize) * 3 + 2 ] = pixels[x + image->columns*y].blue;
    }
  }

  DestroyCacheView(vinfo);
  DestroyImageInfo(image_info);

  // If we just looked at the last image in this file, destroy the image.
  // Otherwise, keep a pointer to the next image around so that we can look
  // at it in the next frame.  In this case, we advance the d_listOfImages
  // to the next image in the file.
  Image *next = GetNextImageInList(image);
  if (next == NULL) {
    DestroyImageList(image);
    d_listOfImages = NULL;
  } else {
    d_listOfImages = next;
  }
  return true;
#else
  fprintf(stderr, "file_stack_server::read_image_from_file(): ImageMagick not compiled in\n");
#endif
}

// Tell the prefetch thread that it is time to grab another image.
// If told the same image twice, won't re-read.
void  file_stack_server::prefetch_image_from_file(const string filename)
{
#if !defined(VIDEO_NO_IMAGEMAGICK)
  d_prefetchName = filename;
#else
  fprintf(stderr, "file_stack_server::prefetch_image_from_file(): ImageMagick not compiled in\n");
#endif
}

//*** Note: This routine is complicated by the fact that some images (TIFF files
// in particular) can have more than one actual image in them.  In this case, there
// is an additional side effect that the d_listOfImages variable holds a pointer
// to an already-opened image when we're not at the end of the stack in that
// one image.  In this case, we step forward in the stack for that image rather
// than going to the next image in the file list.
// It is made a little more complicated because we attempt to prefetch the next
// file in the series each time, so it will be in an in-memory buffer when it
// is needed.
bool  file_stack_server::read_image_to_memory(unsigned minX, unsigned maxX,
							unsigned minY, unsigned maxY,
							double exposure_time_millisecs)
{
  // If we're paused, then return without an image and try not to eat the whole CPU
  if (d_mode == PAUSE) {
    vrpn_SleepMsecs(10);
    return false;
  }

  // If we're doing single-frame, then set the mode to pause for next time so that we
  // won't keep trying to read frames.
  if (d_mode == SINGLE) {
    d_mode = PAUSE;
  }

  // If we've gone past the end of the list, then set the mode to pause and return
  if (d_whichFile == d_fileNames.end()) {
    d_mode = PAUSE;
    return false;
  }

  //---------------------------------------------------------------------
  // Set the size of the window to include all pixels the user requested.
  _minX = minX;
  _maxX = maxX;
  _minY = minY;
  _maxY = maxY;

  //---------------------------------------------------------------------
  // If the maxes are greater than the mins, set them to the size of
  // the image.
  if (_maxX < _minX) {
    _minX = 0; _maxX = _num_columns - 1;
  }
  if (_maxY < _minY) {
    _minY = 0; _maxY = _num_rows - 1;
  }

  //---------------------------------------------------------------------
  // Clip collection range to the size of the sensor on the camera.
  if (_minX < 0) { _minX = 0; };
  if (_minY < 0) { _minY = 0; };
  if (_maxX >= _num_columns) { _maxX = _num_columns - 1; };
  if (_maxY >= _num_rows) { _maxY = _num_rows - 1; };

  // Try to read the current file.  If we fail in the read, set the mode to paused so we don't keep trying.
  // If we succeed, then set up to read the next file.
  if (!read_image_from_file(*d_whichFile)) {
    fprintf(stderr,"file_stack_server::read_image_to_memory(): Could not read file\n");
    return false;
  }
  // If we have a multi-image image file in memory, then don't advance to the
  // next file name.  Otherwise, go to the next file name.
  if (d_listOfImages == NULL) {
    d_whichFile++;
  }

  // Try and prefetch the new file
  if (d_whichFile != d_fileNames.end()) {
    prefetch_image_from_file(*d_whichFile);
  }

  // Okay, we got a new frame!
  return true;
}

/// Get pixels out of the memory buffer.
bool  file_stack_server::get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint8 &val, int color) const
{
  // Make sure we are within the range of allowed pixels
  if ( (X < _minX) || (Y < _minY) || (X > _maxX) || (Y > _maxY) ) {
    return false;
  }

  // Fill in the pixel value, assuming pixels vary in X fastest in the file.
  // We don't shift right here; that was already done if we read an 8-bit image.
  // If we get a 16-bit image, we'll just send the lower-order bits.
  // XXX Endian-ness assumption here.
  val = (vrpn_uint8)(d_buffer[ (X + Y * d_xFileSize) * 3 + color ]);

  return true;
}

/// Get pixels out of the memory buffer.
bool  file_stack_server::get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint16 &val, int color) const
{
  // Make sure we are within the range of allowed pixels
  if ( (X < _minX) || (Y < _minY) || (X > _maxX) || (Y > _maxY) ) {
    return false;
  }

  // Fill in the pixel value, assuming pixels vary in X fastest in the file.
  val = d_buffer[ (X + Y * d_xFileSize) * 3 + color ];
  return true;
}

/// Store the memory image to a PPM file.
bool  file_stack_server::write_memory_to_ppm_file(const char *filename, int gain, bool sixteen_bits) const
{
  //XXX;
  fprintf(stderr,"file_stack_server::write_memory_to_ppm_file(): Not yet implemented\n");
  return false;

  return true;
}

/// Send in-memory image over a vrpn connection
bool  file_stack_server::send_vrpn_image(vrpn_Imager_Server* svr,vrpn_Connection* svrcon,double g_exposure,int svrchan, int)
{
  ///XXX;
  fprintf(stderr,"file_stack_server::send_vrpn_image(): Not yet implemented\n");
  return false;

  return true;
}

// Write the texture, using a virtual method call appropriate to the particular
// camera type.
bool file_stack_server::write_to_opengl_texture(GLuint tex_id)
{
  const GLint   NUM_COMPONENTS = 3;
  const GLenum  FORMAT = GL_RGB;
  const GLenum  TYPE = GL_UNSIGNED_SHORT;
  const void*   BASE_BUFFER = d_buffer;
  const void*   SUBSET_BUFFER = &d_buffer[NUM_COMPONENTS * ( _minX + get_num_columns()*_minY )];
  return write_to_opengl_texture_generic(tex_id, NUM_COMPONENTS, FORMAT, TYPE,
    BASE_BUFFER, SUBSET_BUFFER, _minX, _minY, _maxX, _maxY);
}


#ifdef	USE_METAMORPH
#include "tiffio.h"

// Metamorph custom tags
const uint32  UIC2Tag_ID = 33629; //< Rational tag containing the number of data planes

Metamorph_stack_server::Metamorph_stack_server(const char *filename) :
d_buffer(NULL),
d_mode(SINGLE),
d_xFileSize(0),
d_yFileSize(0)
{
  // In case we fail somewhere along the way
  _status = false;

  // Open the file, which will load the first directory; find out how big it is.
  // Experimentation shows that there is only one directory in the file even
  // when there are multiple images in the stack.
  d_tiff = TIFFOpen(filename, "r");
  if (d_tiff == NULL) {
    fprintf(stderr,"Metamorph_stack_server::Metamorph_stack_server(): Can't open %s\n", filename);
    return;
  }

  // This routine has some side effects: It allocates the buffer and it fills in
  // the d_xFileSize and d_yFileSize data members.
  if (!read_image_from_file()) {
    fprintf(stderr,"Metamorph_stack_server::Metamorph_stack_server(): Can't read %s\n", filename);
    return;
  }

  // Everything opened okay.
  _minX = _minY = 0;
  _maxX = d_xFileSize-1;
  _maxY = d_yFileSize-1;
  _num_columns = d_xFileSize;
  _num_rows = d_yFileSize;
  _binning = 1;
  _status = true;
}

Metamorph_stack_server::~Metamorph_stack_server(void)
{
  // Free the space taken by the in-memory image (if allocated)
  if (d_buffer) {
    delete [] d_buffer;
  }

  // Close the TIFF file (if it was opened)
  if (d_tiff != NULL) {
    TIFFClose((TIFF*)d_tiff);
  }
}

void  Metamorph_stack_server::play()
{
  d_mode = PLAY;
}

void  Metamorph_stack_server::pause()
{
  d_mode = PAUSE;
}

void  Metamorph_stack_server::rewind()
{
  // Seek back to the first plane in the file
  if (d_tiff != NULL) {
    //XXX
  }

  // Read one frame when we start
  d_mode = SINGLE;
}

void  Metamorph_stack_server::single_step()
{
  d_mode = SINGLE;
}


// This routine has some side effects: It allocates the buffer and it fills in
// the d_xFileSize and d_yFileSize data members.
bool  Metamorph_stack_server::read_image_from_file(void)
{

  //Initialize the image info structure and read an image.

  // Ensure that we don't get images of different sizes.  If this image is the
  // wrong size, then return false.  If we've never read any images, then the
  // sizes will be 0.
  uint32 w, h;
  if (TIFFGetField((TIFF*)d_tiff, TIFFTAG_IMAGEWIDTH, &w) != 1) {
    fprintf(stderr,"Metamorph_stack_server::read_image_from_file: Can't get image width\n");
    return false;
  }
  if (TIFFGetField((TIFF*)d_tiff, TIFFTAG_IMAGELENGTH, &h) != 1) {
    fprintf(stderr,"Metamorph_stack_server::read_image_from_file: Can't get height\n");
    return false;
  }
  if (d_xFileSize == 0) {
    d_xFileSize = w;
    d_yFileSize = h;
  } else if ( (d_xFileSize != w) || (d_yFileSize != h) ) {
    fprintf(stderr,"Metamorph_stack_server::read_image_from_file(): Image size differs\n");
    return false;
  }

  // Read the values required to look up a row in the image.  We'll be reading
  // them sequentially, except when we go back to the beginning.
  uint32 rows_per_strip;
  if (TIFFGetField((TIFF*)d_tiff, TIFFTAG_ROWSPERSTRIP, &rows_per_strip) != 1) {
    fprintf(stderr,"Metamorph_stack_server::read_image_from_file: Can't get rows per strip\n");
    return false;
  }
  uint32 strips_per_image = h / rows_per_strip;
  printf("XXX %lu rows per strip (%lu strips per image)\n", rows_per_strip, strips_per_image);
  uint32 *strip_offsets;      //< Receives a pointer to an array of strip offsets
  uint32 *strip_byte_counts;  //< Receives a pointer to an array of strip byte counts
  if (TIFFGetField((TIFF*)d_tiff, TIFFTAG_STRIPOFFSETS, &strip_offsets) != 1) {
    fprintf(stderr,"Metamorph_stack_server::read_image_from_file: Can't get strip offsets\n");
    return false;
  }
  if (TIFFGetField((TIFF*)d_tiff, TIFFTAG_STRIPBYTECOUNTS, &strip_byte_counts) != 1) {
    fprintf(stderr,"Metamorph_stack_server::read_image_from_file: Can't get strip byte counts\n");
    return false;
  }

  // Get the custom Metamorph tags from the file.
  /*XXX This code causes a seg fault down inside the TIFF library
  const TIFFFieldInfo *UIC2TagInfo = TIFFFindFieldInfo((TIFF *)d_tiff, UIC2Tag_ID, TIFF_RATIONAL);
  if (UIC2TagInfo == NULL) {
    fprintf(stderr,"Metamorph_stack_server::read_image_from_file: Can't find UIC2 tag\n");
    return false;
  }
  */
  void	*UIC2Tag;
  if (TIFFGetField((TIFF*)d_tiff, UIC2Tag_ID, &UIC2Tag) != 1) {
    fprintf(stderr,"Metamorph_stack_server::read_image_from_file: Can't get UIC2 tag\n");
    return false;
  }

  // Allocate space to read a frame from the file, if it has not yet been
  // allocated
  if (d_buffer == NULL) {
    if ( (d_buffer = new vrpn_uint16[d_xFileSize * d_yFileSize * 3]) == NULL) {
      fprintf(stderr,"Metamorph_stack_server::read_image_from_file: Out of memory\n");
      _status = false;
      d_mode = PAUSE;
      return false;
    }
  }
  
	uint16 config;
	uint16 datatype;
	uint16 bitspersample;
	uint16 samplesperpixel;
	uint16 compression;
	uint16 predictor;
	uint32 d;

	if (TIFFGetField((TIFF *)d_tiff, TIFFTAG_PLANARCONFIG, &config) != 1) {
	  fprintf(stderr,"Metamorph_stack_server::read_image_from_file: Can't get planar config\n");
	} else {
	  if (config == PLANARCONFIG_CONTIG) {
	    printf("XXX Contiguous\n");
	  } else if (config == PLANARCONFIG_SEPARATE) {
	    printf("XXX Separate\n");
	  } else {
	    printf("XXX Unknown (%d)\n", config);
	  }
	}
	if (TIFFGetField((TIFF *)d_tiff, TIFFTAG_BITSPERSAMPLE, &bitspersample) != 1) {
	  fprintf(stderr,"Metamorph_stack_server::read_image_from_file: Can't get bits per sample\n");
	} else {
	  printf("XXX Bits per sample: %u\n", bitspersample);
	}
	if (TIFFGetField((TIFF *)d_tiff, TIFFTAG_COMPRESSION, &compression) != 1) {
	  fprintf(stderr,"Metamorph_stack_server::read_image_from_file: Can't get compression\n");
	} else {
	  printf("XXX Compression %u\n", compression);
	}
	if (TIFFGetField((TIFF *)d_tiff, TIFFTAG_PREDICTOR, &predictor) != 1) {
	  fprintf(stderr,"Metamorph_stack_server::read_image_from_file: Can't get predictor\n");
	} else {
	  printf("XXX Predictor %u\n", predictor);
	}
/*XXX
	if (TIFFGetField((TIFF *)d_tiff, TIFFTAG_DATATYPE, &datatype) != 1) {
	  fprintf(stderr,"Metamorph_stack_server::read_image_from_file: Can't get data type\n");
	} else {
	  printf("XXX Datatype %u\n", datatype);
	}
	if (TIFFGetField((TIFF *)d_tiff, TIFFTAG_SAMPLESPERPIXEL, &samplesperpixel) != 1) {
	  fprintf(stderr,"Metamorph_stack_server::read_image_from_file: Can't get samples per pixel\n");
	} else {
	  printf("XXX Samples per pixel: %u\n", samplesperpixel);
	}
	if (TIFFGetField((TIFF*)d_tiff, TIFFTAG_IMAGEDEPTH, &d) != 1) {
	  fprintf(stderr,"Metamorph_stack_server::read_image_from_file: Can't get depth\n");
	} else {
	  printf("XXX Depth: %l\n", d);
	}
*/

  // Metamorph's documentation on the STK format specifies the following
  // method to determine the start of a given image plane:
  //  StripOffsets
  //  The strips for all the planes of the stack are stored contiguously
  //  at this location. The following pseudocode fragment shows how to
  //  find the offset of a specified plane planeNum.
  //	LONG	planeOffset = planeNum *
  //		(stripOffsets[stripsPerImage - 1] +
  //		stripByteCounts[stripsPerImage - 1] - stripOffsets[0]);
  //  Note that the planeOffset must be added to the stripOffset[0] to
  //  find the image data for the specific plane in the file.

  // Copy the pixels from the image into the buffer. Flip the image over in Y
  // to match the orientation we expect.
/*XXX
  unsigned x, y, flip_y;
  for (y = 0; y < d_yFileSize; y++) {
    flip_y = (d_yFileSize - 1) - y;
    for (x = 0; x < d_xFileSize; x++) {
      d_buffer[ (x + flip_y * d_xFileSize) * 3 + 0 ] = pixels[x + image->columns*y].red;
      d_buffer[ (x + flip_y * d_xFileSize) * 3 + 1 ] = pixels[x + image->columns*y].green;
      d_buffer[ (x + flip_y * d_xFileSize) * 3 + 2 ] = pixels[x + image->columns*y].blue;
    }
  }
*/

  return true;
}

bool  Metamorph_stack_server::read_image_to_memory(unsigned minX, unsigned maxX,
							unsigned minY, unsigned maxY,
							double exposure_time_millisecs)
{
  // If we're paused, then return without an image and try not to eat the whole CPU
  if (d_mode == PAUSE) {
    vrpn_SleepMsecs(10);
    return false;
  }

  // If we're doing single-frame, then set the mode to pause for next time so that we
  // won't keep trying to read frames.
  if (d_mode == SINGLE) {
    d_mode = PAUSE;
  }

  // If we've gone past the end of the list, then set the mode to pause and return
  if (true /*XXX*/) {
    d_mode = PAUSE;
    return false;
  }

  // Try to read the current file.  If we fail in the read, set the mode to paused so we don't keep trying.
  // If we succeed, then set up to read the next file.
  if (!read_image_from_file()) {
    fprintf(stderr,"Metamorph_stack_server::read_image_to_memory(): Could not read file\n");
    return false;
  }

  // Okay, we got a new frame!
  return true;
}

/// Get pixels out of the memory buffer.
bool  Metamorph_stack_server::get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint8 &val, int color) const
{
  // Make sure we are within the range of allowed pixels
  if ( (X < _minX) || (Y < _minY) || (X > _maxX) || (Y > _maxY) ) {
    return false;
  }

  // Fill in the pixel value, assuming pixels vary in X fastest in the file.
  // We don't shift right here; that was already done if we read an 8-bit image.
  // If we get a 16-bit image, we'll just send the lower-order bits.
  //XXX Endianness assumption here.
  val = (vrpn_uint8)(d_buffer[ (X + Y * d_xFileSize) * 3 + color ]);

  return true;
}

/// Get pixels out of the memory buffer.
bool  Metamorph_stack_server::get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint16 &val, int color) const
{
  // Make sure we are within the range of allowed pixels
  if ( (X < _minX) || (Y < _minY) || (X > _maxX) || (Y > _maxY) ) {
    return false;
  }

  // Fill in the pixel value, assuming pixels vary in X fastest in the file.
  val = d_buffer[ (X + Y * d_xFileSize) * 3 + color ];
  return true;
}

/// Store the memory image to a PPM file.
bool  Metamorph_stack_server::write_memory_to_ppm_file(const char *filename, int gain, bool sixteen_bits) const
{
  //XXX;
  fprintf(stderr,"Metamorph_stack_server::write_memory_to_ppm_file(): Not yet implemented\n");
  return false;

  return true;
}

/// Send in-memory image over a vrpn connection
bool  Metamorph_stack_server::send_vrpn_image(vrpn_Imager_Server* svr,vrpn_Synchronized_Connection* svrcon,double g_exposure,int svrchan, int)
{
  ///XXX;
  fprintf(stderr,"Metamorph_stack_server::send_vrpn_image(): Not yet implemented\n");
  return false;

  return true;
}

#endif
