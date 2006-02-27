/*XXX
1) There is a subtraction of 1 from the number of columns in the get_pixel_from_memory() routines for the spot tracker that seems to work for
  binning of 2,3,4.  I don't know why it needs to be there, which bothers me.  The whole image does seem to show up at each level of binning.
  When I printed the step size, it was indeed one smaller than expected (657).  The region asked for is the maximum size (and if we ask for one
  more in Y, then the request fails).  Even when binned 1x1, we get a correct-looking image.  It goes to all corners of the display.  (It looks
  like maybe the pixels are scanned from top to bottom, given some streaking behavior.)  The number of pixels skipped is still one less than
  expected, but the number of pixels read is the full number.  I'll be the pixel stride is wrong in the display program!  Nope -- it is copying
  the expected pixel range and then drawing the expected size...  Pixel (0,2) is indexed by the read pixel routine as 1314, which is 2 less
  than the 1316 (twice 658) that would be expected to be correct.  The two edges of the screen do not seem to be the same, which seems to
  be impossible given that the indexes should match.  Check the texture coordinates in the texture writes.

2) The image is upside-down somewhere.  If the image is left at full-screen, then the display works properly and the tracking works properly.
  If it is subset, then it is clear that the camera is reading flipped in Y from the way things are drawn on the screen; the other part of the
  image shows up in the green square, updated.  Tracking doesn't like it much when this happens.
*/

#include <stdlib.h>
#include <stdio.h>
#include "diaginc_server.h"
#include <vrpn_BaseClass.h>

// Include files for Diagnostic Instruments, Inc.
#include  "spotCam.h"

#define	DEBUG

static	unsigned long	duration(struct timeval t1, struct timeval t2)
{
	return (t1.tv_usec - t2.tv_usec) +
	       1000000L * (t1.tv_sec - t2.tv_sec);
}

//-----------------------------------------------------------------------
// These definitions provide standard error checking and printing for
// the routines from the PVCAM library.  The first parameter (x) is the
// entire function call.  The second is the string to be printed in case
// of an error.  The _EXIT routine exits with an error code after printing
// the message, while the _WARN function just prints the warning and
// returns.  The _RETURN version returns from the function passing FALSE
// if there is an error.

#define	SP_CHECK_EXIT(x, y) { int retval; \
  if ( (retval = x) != SPOT_SUCCESS ) { \
    fprintf(stderr, "%s failed with code %d\n", y, retval); \
    exit(-1); \
  } \
}

#define	SP_CHECK_RETURN(x, y) { int retval; \
  if ( (retval = x) != SPOT_SUCCESS ) { \
    fprintf(stderr, "%s failed with code %d\n", y, retval); \
    return false; \
  } \
}

#define	SP_CHECK_WARN(x, y) { int retval; \
  if ( (retval = x) != SPOT_SUCCESS ) { \
    fprintf(stderr, "%s failed with code %d\n", y, retval); \
  } \
}

//-----------------------------------------------------------------------
// The min and max coordinates specified here should be without regard to
// binning.  That is, they should be in the full-resolution device coordinates.

bool  diaginc_server::read_one_frame(unsigned short minX, unsigned short maxX,
				     unsigned short minY, unsigned short maxY,
				     unsigned exposure_time_millisecs)
{
  // If the image size or exposure time have changed since the
  // last time, set new ones.  Also, set it to not do auto-exposure.
  if (exposure_time_millisecs != _last_exposure) {
    BOOL  autoexpose = false;
    SPOT_EXPOSURE_STRUCT2 exposure;
    SP_CHECK_RETURN(SpotSetValue(SPOT_AUTOEXPOSE, &autoexpose), "SpotSetValue(Autoexpose)");
    exposure.nGain = 1;	// XXX Later, let this be set.
    exposure.dwExpDur = exposure_time_millisecs * 2000;	  // Convert to half-microseconds
    exposure.dwClearExpDur = exposure_time_millisecs * 2000;	  // Convert to half-microseconds
    exposure.dwRedExpDur = exposure_time_millisecs * 2000;
    exposure.dwGreenExpDur = exposure_time_millisecs * 2000;
    exposure.dwBlueExpDur = exposure_time_millisecs * 2000;
    SP_CHECK_RETURN(SpotSetValue(SPOT_EXPOSURE2, &exposure), "SpotSetValue(Exposure2)");
  }
  if ( (_last_minX != minX) || (_last_maxX != maxX) ||
       (_last_minY != minY) || (_last_maxY != minY) ) {
    RECT  image_region;
    image_region.left = minX;
    image_region.right = maxX;
    image_region.bottom = maxY;	  // Bottom is at highest index
    image_region.top = minY;	  // Top is at index zero
    SP_CHECK_RETURN(SpotSetValue(SPOT_IMAGERECT, &image_region), "SpotSetValue(Region)");
    //XXX For some reason, this gets done each time.  It does do the whole region, though.
  }
  _last_exposure = exposure_time_millisecs;
  _last_minX = minX; _last_maxX = maxX; _last_minY = minY; _last_maxY = maxY;

  // Get the picture into the memory buffer
  // XXX There seems to be an off-by-one problem somewhere when reading this.
  SP_CHECK_RETURN(SpotGetImage(_bitDepth, false, 0, _buffer, NULL, NULL, NULL),"SpotGetImage()");

  return true;
}

//---------------------------------------------------------------------
// Open the camera and determine its available features and parameters.

bool diaginc_server::open_and_find_parameters(void)
{
  // Find out how many cameras are on the system, then get the
  // name of the first camera, open it, and make sure that it has
  // no critical failures.
  int	  num_cameras;
  SPOT_INTF_CARD_STRUCT	cameras[SPOT_MAX_INTF_CARDS];
  short	  camera_to_use = 0;
  short	  bitdepth = 0;
  short	  binRange[2];
  RECT	  image_region;

  // Find the list of cameras available, select the
  // first one, and then initialize the library.
  SpotFindInterfaceCards(cameras, &num_cameras);
  if (num_cameras <= 0) {
    fprintf(stderr,"SPOT driver: Cannot find any connected cameras\n");
    return false;
  }
  SP_CHECK_RETURN(SpotSetValue(SPOT_DRIVERDEVICENUMBER, &camera_to_use), "SpotSetValue(Device)");
  SP_CHECK_WARN(SpotInit(), "SpotInit");

  // Set the color to monochrome mode (turn off filter wheels) and ask it for 12 bits.
  SPOT_COLOR_ENABLE_STRUCT2 colorstruct;
  colorstruct.bEnableRed = false;
  colorstruct.bEnableGreen = false;
  colorstruct.bEnableBlue = false;
  colorstruct.bEnableClear = false;
  SP_CHECK_RETURN(SpotSetValue(SPOT_COLORENABLE2, &colorstruct), "SpotSetValue(ColorEnable)");
  bitdepth = 12;
  SP_CHECK_RETURN(SpotSetValue(SPOT_BITDEPTH, &bitdepth), "SpotGetvalue(Bitdepth)");

  // Find out the parameters available on this camera
  SP_CHECK_RETURN(SpotGetValue(SPOT_BITDEPTH, &bitdepth), "SpotGetvalue(Bitdepth)");
  if (bitdepth != 12) {
    fprintf(stderr,"diagnc_server::open_and_find_parameters(): Got depth %d, wanted 12\n", bitdepth);
    return false;
  }
  _bitDepth = (unsigned short) bitdepth;
  SP_CHECK_RETURN(SpotGetValue(SPOT_IMAGERECT, &image_region), "SpotGetValue(Region)");
  _minX = image_region.left;
  _maxX = image_region.right;
  _minY = image_region.top;	    // This is zero
  _maxY = image_region.bottom;	    // This is the largest Y value
  _num_rows = _maxY - _minY + 1;
  _num_columns = _maxX - _minX + 1;
#ifdef	DEBUG
  printf("diaginc_server::open_and_find_parameters(): Region (%d,%d) to (%d,%d), size (%d,%d)\n",
    _minX,_minY, _maxX,_maxY, _num_columns,_num_rows);
#endif
  _circbuffer_on = false;
  SP_CHECK_RETURN(SpotGetValue(SPOT_BINSIZELIMITS, &(binRange[0])), "SpotGetValue(BinLimits)");
  _binMin = binRange[0];
  _binMax = binRange[1];

  // XXX Find the list of available exposure settings..

  return true;
}

diaginc_server::diaginc_server(unsigned binning) :
  base_camera_server(binning),
  _last_exposure(0),
  _binMin(0),
  _binMax(0),
  _bitDepth(0),
  _last_minX(0),
  _last_maxX(0),
  _last_minY(0),
  _last_maxY(0),
  _circbuffer_run(false),
  _circbuffer(NULL),
  _circbuffer_len(0),
  _circbuffer_num(3)	  //< Use this many buffers in the circular buffer
{
  //---------------------------------------------------------------------
  // Initialize the SPOT software, open the camera, and find out what its
  // capabilities are.
  if (!open_and_find_parameters()) {
    fprintf(stderr, "diaginc_server::diaginc_server(): Cannot open camera\n");
    _status = false;
    return;
  }

  //---------------------------------------------------------------------
  // Make sure the binning value requested is within range
  if ( (binning < _binMin) || (binning > _binMax) ) {
    fprintf(stderr,"diaginc_server::diaginc_server(): Binning (%d) out of range (%d - %d)\n",
      binning, _binMin, _binMax);
    _status = false;
    return;
  }

  //---------------------------------------------------------------------
  // Allocate a buffer that is large enough to read the maximum-sized
  // image with no binning.
  _buflen = (vrpn_uint32)(_num_rows* _num_columns * 2);	// Two bytes per pixel
  if ( (_buffer = GlobalAlloc( GMEM_ZEROINIT, _buflen)) == NULL) {
    fprintf(stderr,"diaginc_server::diaginc_server(): Cannot allocate enough locked memory for buffer (%dx%d)\n",
      _num_columns, _num_rows);
    _status = false;
    return;
  }
  if ( (_memory = GlobalLock(_buffer)) == NULL) {
    fprintf(stderr, "diaginc_server::diaginc_server(): Cannot lock memory buffer\n");
    _status = false;
    return;
  }

  //---------------------------------------------------------------------
  // Set the binning to the value requested.
  short binVal = (short)binning;
  if (SPOT_SUCCESS != SpotSetValue(SPOT_BINSIZE, &binVal)) {
    fprintf(stderr, "diaginc_server::diaginc_server(): Can't set binning\n");
    _status = false;
    return;
  }

  //---------------------------------------------------------------------
  // No image in memory yet.
  _minX = _minY = _maxX = _maxY = 0;

  _status = true;
}

//---------------------------------------------------------------------
// Close the camera and the system.  Free up memory.

diaginc_server::~diaginc_server(void)
{
  //---------------------------------------------------------------------
  // If the circular-buffer mode is supported, shut that down here.
  if (_circbuffer_run) {
    //XXX;
    if (_circbuffer) {
      delete [] _circbuffer;
      _circbuffer_len = 0;
    }
  }

  //---------------------------------------------------------------------
  // Shut down the SPOT software and free the global memory buffer.
  SP_CHECK_WARN(SpotExit(), "SpotExit");
  GlobalUnlock(_buffer );
  GlobalFree(_buffer );
}

// The min/max coordinates here are in post-binned space.  That is, the maximum should be
// the edge of the image produced, taking into account the binning of the full resolution.

bool  diaginc_server::read_image_to_memory(unsigned minX, unsigned maxX, unsigned minY, unsigned maxY,
					 double exposure_time_millisecs)
{
  //---------------------------------------------------------------------
  // Set the size of the window to include all pixels if there were not
  // any binning.  This means adding all but 1 of the binning back at
  // the end to cover the pixels that are within that bin.
  _minX = minX * _binning;
  _maxX = maxX * _binning + (_binning-1);
  _minY = minY * _binning;
  _maxY = maxY * _binning + (_binning-1);
#ifdef	DEBUG
  printf("diaginc_server::read_image_to_memory(): Setting window from (%d,%d) to (%d,%d)\n",
    _minX, _minY, _maxX, _maxY);
#endif

  //---------------------------------------------------------------------
  // If the maxes are greater than the mins, set them to the size of
  // the image.
  if (_maxX < _minX) {
    _minX = 0; _maxX = _num_columns - 1;  // Uses _num_columns rather than get_num_columns() because it is in pre-binning space.
  }
  if (_maxY < _minY) {
    _minY = 0; _maxY = _num_rows - 1;
  }

  //---------------------------------------------------------------------
  // Clip collection range to the size of the sensor on the camera.
  if (_minX < 0) { _minX = 0; };
  if (_minY < 0) { _minY = 0; };
  if (_maxX >= _num_columns) {	  // Uses _num_columns rather than get_num_columns() because it is in pre-binning space.
    fprintf(stderr,"diaginc_server::read_image_to_memory(): Clipping maxX\n");
    _maxX = _num_columns - 1;
  };
  if (_maxY >= _num_rows) {
    fprintf(stderr,"diaginc_server::read_image_to_memory(): Clipping maxY\n");
    _maxY = _num_rows - 1;
  };

  //---------------------------------------------------------------------
  // Set up and read one frame at a time if the circular-buffer code is
  // not available.  Otherwise, the opening code will have set up
  // the system to run continuously, so we get the next frame from the
  // buffers.
  if (_circbuffer_on) {
    fprintf(stderr,"diaginc_server::read_image_to_memory(): Circular buffer not implemented\n");
    return false;
  } else {
    if (!read_one_frame(_minX, _maxX, _minY, _maxY, (unsigned long)exposure_time_millisecs)) {
      fprintf(stderr, "diagnc_server::read_image_to_memory(): Could not read frame\n");
      return false;
    }
  }

  return true;
}

static	vrpn_uint16 clamp_gain(vrpn_uint16 val, double gain, double clamp = 65535.0)
{
  double result = val * gain;
  if (result > clamp) { result = clamp; }
  return (vrpn_uint16)result;
}

//XXX This routine needs to be tested.
bool  diaginc_server::write_memory_to_ppm_file(const char *filename, int gain, bool sixteen_bits) const
{
  //---------------------------------------------------------------------
  // Make sure the region is non-zero (so we've read an image)
  if ( (_maxX <= _minX) || (_maxY <= _minY) ) {
    fprintf(stderr,"diaginc_server::write_memory_to_ppm_file(): No image in memory\n");
    return false;
  }

  //---------------------------------------------------------------------
  // If we are not doing 16 bits, map the 12 bits to the range 0-255, and then write out an
  // uncompressed 8-bit grayscale PPM file with the values scaled to this range.
  if (!sixteen_bits) {
    vrpn_uint16	  *vals = (vrpn_uint16 *)_memory;
    unsigned char *pixels;
    // This buffer will be oversized if min and max don't span the whole window.
    if ( (pixels = new unsigned char[_buflen/2]) == NULL) {
      fprintf(stderr, "diaginc_server::write_memory_to_ppm_file(): Can't allocate memory for stored image\n");
      return false;
    }
    unsigned r,c;
    vrpn_uint16 minimum = clamp_gain(vals[0],gain);
    vrpn_uint16 maximum = clamp_gain(vals[0],gain);
    vrpn_uint16 cols = (_maxX - _minX)/_binning + 1;
    vrpn_uint16 rows = (_maxY - _minY)/_binning + 1;
    for (r = 0; r < rows; r++) {
      for (c = 0; c < cols; c++) {
	if (clamp_gain(vals[r*cols + c],gain) < minimum) { minimum = clamp_gain(vals[r*cols+c],gain, 4095); }
	if (clamp_gain(vals[r*cols + c],gain) > maximum) { maximum= clamp_gain(vals[r*cols+c],gain, 4095); }
      }
    }
    printf("Minimum = %d, maximum = %d\n", minimum, maximum);
    vrpn_uint16 offset = 0;
    double scale = gain;
    for (r = 0; r < rows; r++) {
      for (c = 0; c < cols; c++) {
	pixels[r*cols + c] = clamp_gain(vals[r*cols+c] - offset, scale, 4095) >> 4;
      }
    }
    FILE *of = fopen(filename, "wb");
    fprintf(of, "P5\n%d %d\n%d\n", cols, rows, 255);
    fwrite(pixels, 1, cols*rows, of);
    fclose(of);
    delete [] pixels;

  // If we are doing 16 bits, write out a 16-bit file.
  } else {
    vrpn_uint16 *vals = (vrpn_uint16 *)_memory;
    unsigned r,c;
    vrpn_uint16 *pixels;
    // This buffer will be oversized if min and max don't span the whole window.
    if ( (pixels = new vrpn_uint16[_buflen]) == NULL) {
      fprintf(stderr, "diaginc_server::write_memory_to_ppm_file(): Can't allocate memory for stored image\n");
      return false;
    }
    vrpn_uint16 minimum = clamp_gain(vals[0],gain);
    vrpn_uint16 maximum = clamp_gain(vals[0],gain);
    vrpn_uint16 cols = (_maxX - _minX)/_binning + 1;
    vrpn_uint16 rows = (_maxY - _minY)/_binning + 1;
    for (r = 0; r < rows; r++) {
      for (c = 0; c < cols; c++) {
	if (clamp_gain(vals[r*cols + c],gain) < minimum) { minimum = clamp_gain(vals[r*cols+c],gain); }
	if (clamp_gain(vals[r*cols + c],gain) > maximum) { maximum = clamp_gain(vals[r*cols+c],gain); }
      }
    }
    printf("Minimum = %d, maximum = %d\n", minimum, maximum);
    vrpn_uint16 offset = 0;
    double scale = gain;
    for (r = 0; r < rows; r++) {
      for (c = 0; c < cols; c++) {
	pixels[r*cols + c] = clamp_gain(vals[r*cols+c] - offset, scale);
      }
    }
    FILE *of = fopen(filename, "wb");
    fprintf(of, "P5\n%d %d\n%d\n", cols, rows, 4095);
    fwrite(pixels, sizeof(vrpn_uint16), cols*rows, of);
    fclose(of);
    delete [] pixels;
  }
  return true;
}

//---------------------------------------------------------------------
// Map the 12 bits to the range 0-255, and return the result
bool	diaginc_server::get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint8 &val, int RGB) const
{
  if ( (_maxX <= _minX) || (_maxY <= _minY) ) {
    fprintf(stderr,"diaginc_server::get_pixel_from_memory(): No image in memory\n");
    return false;
  }
  if (RGB != 0) {
    fprintf(stderr,"diaginc_server::get_pixel_from_memory(): Can't select other than 0th color\n");
    return false;
  }
  if ( (X < _minX/_binning) || (X > _maxX/_binning) || (Y < _minY/_binning) || (Y > _maxY/_binning) ) {
    return false;
  }
  vrpn_uint16	*vals = (vrpn_uint16 *)_memory;
  //XXX Why -1?
  vrpn_uint16	cols = (_maxX - _minX + 1)/_binning - 1;  // Don't use num_columns here; depends on the region size.
  vrpn_uint32	index = (Y-_minY/_binning)*cols + (X-_minX/_binning);
#ifdef DEBUG
  if ( (X == 0) && (Y == 2)) { printf("XXX for pixel (%d,%d), indexing as %ld\n", X, Y, index); }
#endif
  val = (vrpn_uint8)(vals[index] >> 4);
  return true;
}

bool	diaginc_server::get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint16 &val, int RGB) const
{
  if ( (_maxX <= _minX) || (_maxY <= _minY) ) {
    fprintf(stderr,"diaginc_server::get_pixel_from_memory(): No image in memory\n");
    return false;
  }
  if (RGB != 0) {
    fprintf(stderr,"diaginc_server::get_pixel_from_memory(): Can't select other than 0th color\n");
    return false;
  }
  if ( (X < _minX/_binning) || (X > _maxX/_binning) || (Y < _minY/_binning) || (Y > _maxY/_binning) ) {
    return false;
  }
  vrpn_uint16	*vals = (vrpn_uint16 *)_memory;
  //XXX Why -1?
  vrpn_uint16	cols = (_maxX - _minX + 1)/_binning - 1;  // Don't use num_columns here; depends on the region size.
  vrpn_uint32	index = (Y-_minY/_binning)*cols + (X-_minX/_binning);
#ifdef DEBUG
  if ( (X == 0) && (Y == 2)) { printf("XXX for pixel (%d,%d), indexing as %ld\n", X, Y, index); }
#endif
  val = vals[index];
  return true;
}

// XXX This routine needs to be tested.
bool diaginc_server::send_vrpn_image(vrpn_Imager_Server* svr,vrpn_Connection* svrcon,double g_exposure,int svrchan, int) const
{
    // Send the current frame over to the client in chunks as big as possible (limited by vrpn_IMAGER_MAX_REGION)
    unsigned  num_x = get_num_columns();
    unsigned  num_y = get_num_rows();
    int nRowsPerRegion=vrpn_IMAGER_MAX_REGIONu16/num_x;
    unsigned y;
    svr->send_begin_frame(0, num_x-1, 0, num_y-1);
    for(y=0;y<num_y;y=__min(num_y,y+nRowsPerRegion)) {
      svr->send_region_using_base_pointer(svrchan,0,num_x-1,y,__min(num_y,y+nRowsPerRegion)-1,
	(vrpn_uint16 *)_memory, 1, get_num_columns());
      svr->mainloop();
    }
    svr->send_end_frame(0, num_x-1, 0, num_y-1);
    svr->mainloop();

    // Mainloop the server connection (once per server mainloop, not once per object).
    svrcon->mainloop();
    return true;
}


// Write the texture, using a virtual method call appropriate to the particular
// camera type.  NOTE: At least the first time this function is called,
// we must write a complete texture, which may be larger than the actual bytes
// allocated for the image.  After the first time, and if we don't change the
// image size to be larger, we can use the subimage call to only write the
// pixels we have.
bool diaginc_server::write_to_opengl_texture(GLuint tex_id)
{
  // Note: Check the GLubyte or GLushort or whatever in the temporary buffer!
  const GLint   NUM_COMPONENTS = 1;
  const GLenum  FORMAT = GL_LUMINANCE;
  const GLenum  TYPE = GL_UNSIGNED_SHORT;

  // We need to write an image to the texture at least once that includes all of
  // the pixels, before we can call the subimage write method below.  We need to
  // allocate a buffer large enough to send, and of the appropriate type, for this.
  if (!_opengl_texture_have_written) {
    GLushort *tempimage = new GLushort[NUM_COMPONENTS * _opengl_texture_size_x * _opengl_texture_size_y];
    if (tempimage == NULL) {
      fprintf(stderr,"roper_server::write_to_opengl_texture(): Out of memory allocating temporary buffer\n");
      return false;
    }
    memset(tempimage, 0, _opengl_texture_size_x * _opengl_texture_size_y);

    // Set the pixel storage parameters and store the total blank image.
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, _opengl_texture_size_x);
    glTexImage2D(GL_TEXTURE_2D, 0, NUM_COMPONENTS, _opengl_texture_size_x, _opengl_texture_size_y,
      0, FORMAT, TYPE, tempimage);

    delete [] tempimage;
    _opengl_texture_have_written = true;
  }

  // Set the offset and scale parameters for the transfer.  Set all of the RGB ones to
  // the requested one even for GL_LUMINANCE textures, because they get used.  Set the
  // Alpha ones to pass through unmodified because they eventually get used in the
  // pixel math for some cases.  We multiply the existing scales and offsets by
  // 2^4 = 16 because we encode a 12-bit value in a 16-bit variable.  We read the
  // red scale that was set before and set them all to 16 times that.
  double scale, offset;
  glGetDoublev(GL_RED_SCALE, &scale);
  glGetDoublev(GL_RED_BIAS, &offset);
  scale *= 16;
  offset *= 16;
  glPixelTransferf(GL_RED_SCALE, scale);
  glPixelTransferf(GL_GREEN_SCALE, scale);
  glPixelTransferf(GL_BLUE_SCALE, scale);
  glPixelTransferf(GL_ALPHA_SCALE, 1.0);
  glPixelTransferf(GL_RED_BIAS, offset);
  glPixelTransferf(GL_GREEN_BIAS, offset);
  glPixelTransferf(GL_BLUE_BIAS, offset);
  glPixelTransferf(GL_ALPHA_BIAS, 0.0);

  // Set the pixel storage parameters.
  // In this case, we need to invert the image in Y to make the display match
  // that of the capture program.  We are not allowed a negative row length,
  // so we have to find another trick.
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, get_num_columns());

  // Send the subset of the image that we actually have active to OpenGL.
  // For the EDT_Pulnix, we use an 8-bit unsigned, GL_LUMINANCE texture.
  glTexSubImage2D(GL_TEXTURE_2D, 0,
    _minX,_minY, _maxX-_minX+1,_maxY-_minY+1,
    FORMAT, TYPE, &((vrpn_uint16 *)_memory)[NUM_COMPONENTS * ( _minX + get_num_columns()*_minY )]);
  return true;
}
