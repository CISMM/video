#include <stdlib.h>
#include <stdio.h>
#include "roper_server.h"
#include <vrpn_BaseClass.h>

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

#define	PL_CHECK_EXIT(x, y) { int retval; \
  if ( (retval = x) == PV_FAIL ) { \
    char  msg[1024]; \
    pl_error_message(pl_error_code(), msg); \
    fprintf(stderr, "%s failed due to %s\n", y, msg); \
    exit(-1); \
  } \
}

#define	PL_CHECK_RETURN(x, y) { int retval; \
  if ( (retval = x) == PV_FAIL ) { \
    char  msg[1024]; \
    pl_error_message(pl_error_code(), msg); \
    fprintf(stderr, "%s failed due to %s\n", y, msg); \
    return FALSE; \
  } \
}

#define	PL_CHECK_WARN(x, y) { int retval; \
  if ( (retval = x) == PV_FAIL ) { \
    char  msg[1024]; \
    pl_error_message(pl_error_code(), msg); \
    fprintf(stderr, "%s failed due to %s\n", y, msg); \
  } \
}

//-----------------------------------------------------------------------

bool  roper_server::read_continuous(const int16 camera_handle,
		       const rgn_type &region_description,
		       const uns32 exposure_time_millisecs)
{
  int16	progress;	//< Progress made in getting the current frame
  uns32	buffer_size;	//< Size required to hold the data

  //--------------------------------------------------------------------
  // See if the parameters have changed or if the circular buffer has not
  // yet been set up to run..  If so, then we need to stop any previous
  // acquisition and then restart a new set of acquisitions.
  if ( !_circbuffer_run || (exposure_time_millisecs != _last_exposure) ||
       (region_description.s1 != _last_region.s1) ||
       (region_description.s2 != _last_region.s2) ||
       (region_description.p1 != _last_region.p1) ||
       (region_description.p2 != _last_region.p2) ||
       (region_description.sbin != _last_region.sbin) ||
       (region_description.pbin != _last_region.pbin) ) {

    // Stop any currently-running acquisition and free the memory used
    // by the circular buffer.
    if (_circbuffer_run) {
      int retval1;
      _circbuffer_run = false;
      if (FALSE == (retval1 = pl_exp_stop_cont(_camera_handle, CCS_HALT))) {
	fprintf(stderr,"roper_server::read_continuous(): Cannot stop acquisition\n");
      }
      if (_circbuffer) {
	delete [] _circbuffer;
        _circbuffer = NULL;
	_circbuffer_len = 0;
      }
      if (retval1 == FALSE) { return FALSE; }
    }

    // Allocate a new circular buffer that is large enough to hold the number
    // of buffers we want.
    _circbuffer_len = (uns32)(_circbuffer_num * 
      (_maxX - _minX + 1) * (_maxY - _minY + 1) / (_binning * _binning) );
    if ( (_circbuffer = new uns16[_circbuffer_len]) == NULL) {
      fprintf(stderr,"roper_server::read_continuous(): Out of memory\n");
      _circbuffer_len = 0;
      return FALSE;
    }

    // Setup and start a continuous acquisition.
    PL_CHECK_RETURN(pl_exp_setup_cont(camera_handle, 1, &region_description,
      TIMED_MODE, exposure_time_millisecs, &buffer_size, CIRC_NO_OVERWRITE),
      "pl_exp_setup_cont");
    // The factor of 2 here is converting bytes to pixels (16 bits)
    if (buffer_size / 2 != _circbuffer_len / _circbuffer_num) {
      fprintf(stderr,"roper_server::read_continuous(): Unexpected buffer size (got %u, expected %u)\n",
	buffer_size, _circbuffer_len / _circbuffer_num * 2);
      return FALSE;
    }
    PL_CHECK_RETURN(pl_exp_start_cont(camera_handle, _circbuffer, _circbuffer_len),
      "pl_exp_start_cont");

    // No buffers have been read yet.
    _last_buffer_cnt = 0;
  }
  _last_exposure = exposure_time_millisecs;
  _last_region = region_description;
  _circbuffer_run = true;

  // Wait for it to be done reading; timeout in 1 second longer than exposure should be
  const double READ_TIMEOUT = exposure_time_millisecs/1000 + 1;
  struct  timeval start, now;
  gettimeofday(&start, NULL);
  do {
     uns32 byte_cnt, buffer_cnt;
     PL_CHECK_RETURN(pl_exp_check_cont_status(camera_handle, &progress, &byte_cnt,&buffer_cnt),
       "pl_exp_check_cont_status");
     if (progress == READOUT_FAILED) {
       fprintf(stderr,"read_continuous(): Readout failed\n");
       return FALSE;
     }

     // See how many buffers have been filled.
     // Each time we get another one, we go ahead and read it (set progress to READOUT_COMPLETE)
     if (buffer_cnt > _last_buffer_cnt) {
       progress = READOUT_COMPLETE;
       _last_buffer_cnt += (buffer_cnt - _last_buffer_cnt);
     }

     // Check for a timeout; if we haven't heard anything for a second longer than exposure,
     // abort the read and restart it.
     gettimeofday(&now, NULL);
     if (duration(now, start) / 1.0e6 > READ_TIMEOUT) {
       // XXX Should return an error and let the above code decide to retry
       fprintf(stderr,"roper_server::read_continuous(): Timeout, retrying\n");
       switch (progress) {
       case READOUT_NOT_ACTIVE:
	 fprintf(stderr, "  Status: Readout Not Active\n");
	 break;
       case EXPOSURE_IN_PROGRESS:
	 fprintf(stderr, "  Status: Exposure In Progress\n");
	 break;
       case READOUT_IN_PROGRESS:
	 fprintf(stderr, "  Status: Readout In Progress\n");
	 break;
       case ACQUISITION_IN_PROGRESS:
	 fprintf(stderr, "  Status: Acquisition In Progress\n");
	 break;
       case READOUT_COMPLETE:
	 fprintf(stderr, "  Status: Readout Complete\n");
	 break;
       case READOUT_FAILED:
	 fprintf(stderr, "  Status: Readout Failed\n");
	 break;
       default:
	 fprintf(stderr, "  Unrecognized status (%d) at timeout (valid ones %d through %d)\n",
	   progress, READOUT_NOT_ACTIVE, ACQUISITION_IN_PROGRESS);
       }
       gettimeofday(&start, NULL);
     }
     // Check at 1ms intervals to avoid eating the whole CPU
     vrpn_SleepMsecs(1);
  } while (progress != READOUT_COMPLETE);

  // Get a pointer to the oldest valid frame data
  PL_CHECK_RETURN(pl_exp_get_oldest_frame(camera_handle, &_memory),
    "pl_exp_get_latest_frame");

  // Free up the buffer used last time
  PL_CHECK_RETURN(pl_exp_unlock_oldest_frame(camera_handle), "pl_exp_unlock_oldest_frame");

  return true;
}


//-----------------------------------------------------------------------

bool  roper_server::read_one_frame(const int16 camera_handle,
		       const rgn_type &region_description,
		       const uns32 exposure_time_millisecs)
{
  int16	progress = EXPOSURE_IN_PROGRESS;
  uns32	trash;
  uns32	buffer_size;	// Size required to hold the data

  // Set up the sequence and set the parameters.  Also
  // check to make sure that the buffer passed in is large
  // enough to hold the image.  Only do this if the parameters
  // have changed since the last time through.
  if ( (exposure_time_millisecs != _last_exposure) ||
       (region_description.s1 != _last_region.s1) ||
       (region_description.s2 != _last_region.s2) ||
       (region_description.p1 != _last_region.p1) ||
       (region_description.p2 != _last_region.p2) ||
       (region_description.sbin != _last_region.sbin) ||
       (region_description.pbin != _last_region.pbin) ) {
    PL_CHECK_RETURN(pl_exp_setup_seq(camera_handle, 1, 1,
      &region_description, TIMED_MODE, exposure_time_millisecs, &buffer_size), "pl_exp_setup_seq");
    if (buffer_size > _buflen) {
      fprintf(stderr,"read_one_frame: Buffer passed in too small\n");
      return FALSE;
    }
  }
  _last_exposure = exposure_time_millisecs;
  _last_region = region_description;

  // Start it reading one exposure
  PL_CHECK_RETURN(pl_exp_start_seq(camera_handle, _buffer), "pl_exp_start_seq");

  // Wait for it to be done reading; timeout in 1 second longer than exposure should be
  const double READ_TIMEOUT = exposure_time_millisecs*1000 + 1;
  struct  timeval start, now;
  gettimeofday(&start, NULL);
  do {
     PL_CHECK_RETURN(pl_exp_check_status(camera_handle, &progress, &trash),
       "pl_exp_check_status");
     if (progress == READOUT_FAILED) {
       fprintf(stderr,"read_one_frame(): Readout failed\n");
       return FALSE;
     }

     // Check for a timeout; if we haven't heard anything for a second,
     // abort the read and restart it.
     gettimeofday(&now, NULL);
     if (duration(now, start) / 1.0e6 > READ_TIMEOUT) {
       // XXX Should return an error and let the above code decide to retry
       fprintf(stderr,"roper_server::read_one_frame(): Timeout, retrying\n");
       PL_CHECK_WARN(pl_exp_abort(camera_handle, CCS_HALT), "pl_exp_abort");
       PL_CHECK_RETURN(pl_exp_start_seq(camera_handle, _buffer), "pl_exp_start_seq");
       gettimeofday(&start, NULL);
     }
     // Check at 1ms intervals to avoid eating the whole CPU
     vrpn_SleepMsecs(1);

  } while (progress != READOUT_COMPLETE);

  PL_CHECK_RETURN(pl_exp_finish_seq(camera_handle, _buffer, 0), "pl_exp_finish_seq");

  return TRUE;
}

//---------------------------------------------------------------------
// Open the camera and determine its available features and parameters.

bool roper_server::open_and_find_parameters(void)
{
  // Find out how many cameras are on the system, then get the
  // name of the LAST camera, open it, and make sure that it has
  // no critical failures.
  int16	  num_cameras;
  int16	  num_rows, num_columns;
  bool	  avail_flag;
  char	  camera_name[CAM_NAME_LEN];
  PL_CHECK_EXIT(pl_cam_get_total(&num_cameras), "pl_cam_get_total");
  if (num_cameras <= 0) {
    fprintf(stderr, "No cameras found\n");
    return false;
  }
  PL_CHECK_EXIT(pl_cam_get_name(num_cameras-1, camera_name), "pl_cam_get_name");
  PL_CHECK_EXIT(pl_cam_open(camera_name, &_camera_handle, OPEN_EXCLUSIVE),
    "pl_cam_open");
  PL_CHECK_EXIT(pl_cam_get_diags(_camera_handle), "pl_cam_get_diags");

  // Find out the parameters available on this camera
  PL_CHECK_EXIT(pl_get_param(_camera_handle, PARAM_PAR_SIZE, ATTR_CURRENT, &num_rows),
    "PARAM_PAR_SIZE");
  PL_CHECK_EXIT(pl_get_param(_camera_handle, PARAM_SER_SIZE, ATTR_CURRENT, &num_columns),
    "PARAM_SER_SIZE");
  PL_CHECK_EXIT(pl_get_param(_camera_handle, PARAM_CIRC_BUFFER, ATTR_AVAIL, &avail_flag),
    "PARAM_SER_CIRCBUFFER");
  _num_rows = num_rows;
  _num_columns = num_columns;
  _circbuffer_on = avail_flag;
  //XXX If we don't have anywhere to store the image, the continuous-read code fails.
  // Need to make that a soft-failure that reports lost frames.  For now, turn off
  // circular buffering.
  _circbuffer_on = false;
  if (!_circbuffer_on) {
    fprintf(stderr,"roper_server::open_and_find_parameters(): Does not support circular buffers (using single-capture mode)\n");
  }

  // XXX Find the list of available exposure settings..

  return true;
}

roper_server::roper_server(unsigned binning) :
  base_camera_server(binning),
  _last_exposure(0),
  _circbuffer_run(false),
  _circbuffer(NULL),
  _circbuffer_len(0),
  _circbuffer_num(3),	  //< Use this many buffers in the circular buffer
  _last_buffer_cnt(0)
{
  //---------------------------------------------------------------------
  // Initialize the PVCAM software, open the camera, and find out what its
  // capabilities are.
  PL_CHECK_WARN(pl_pvcam_init(), "pl_pvcam_init");

  if (!open_and_find_parameters()) {
    fprintf(stderr, "roper_server::roper_server(): Cannot open camera\n");
    _status = false;
    return;
  }

  //---------------------------------------------------------------------
  // Allocate a buffer that is large enough to read the maximum-sized
  // image with no binning.
  _buflen = (uns32)(_num_rows * _num_columns * 2);	// Two bytes per pixel

  if ( (_buffer = new uns16[_buflen]) == NULL) {
    fprintf(stderr,"roper_server::roper_server(): Cannot allocate enough locked memory for buffer\n");
    _status = false;
    return;
  }
  _memory = _buffer;

  //---------------------------------------------------------------------
  // Initialize the acquisition sequence.
  PL_CHECK_WARN(pl_exp_init_seq(), "pl_exp_init_seq");

  //---------------------------------------------------------------------
  // No image in memory yet.
  _minX = _minY = _maxX = _maxY = 0;

  _status = true;
}

//---------------------------------------------------------------------
// Close the camera and the system.  Free up memory.

roper_server::~roper_server(void)
{
  //XXX This sometimes leaves the program hung open until IPlab or some
  // other program opens and closes the camera.

  //---------------------------------------------------------------------
  // If the circular-buffer mode is supported, shut that down here.
  if (_circbuffer_run) {
    if (!pl_exp_stop_cont(_camera_handle, CCS_HALT)) {
      fprintf(stderr,"roper_server::~roper_server(): Cannot stop acquisition\n");
    }
    if (!pl_exp_finish_seq(_camera_handle, _buffer, 0)) {
      fprintf(stderr,"roper_server::~roper_server(): Cannot finish sequence\n");
    }
  }

  // Uninitialize the acquisition sequence
  PL_CHECK_WARN(pl_exp_uninit_seq(), "pl_exp_uninit_seq");

  //---------------------------------------------------------------------
  // Shut down the PVCAM software.
  PL_CHECK_WARN(pl_cam_close(_camera_handle), "pl_pvcam_close");
  PL_CHECK_WARN(pl_pvcam_uninit(), "pl_pvcam_uninit");

  if (_circbuffer) {
    delete [] _circbuffer;
    _circbuffer = NULL;
    _circbuffer_len = 0;
  }
  if (_buffer) {
    delete [] _buffer;
    _buffer = NULL;
  }
}

bool  roper_server::read_image_to_memory(unsigned minX, unsigned maxX, unsigned minY, unsigned maxY,
					 double exposure_time_millisecs)
{
  //---------------------------------------------------------------------
  // In case we fail, clear these

  //---------------------------------------------------------------------
  // Set the size of the window to include all pixels if there were not
  // any binning.  This means adding all but 1 of the binning back at
  // the end to cover the pixels that are within that bin.
  _minX = minX * _binning;
  _maxX = maxX * _binning + (_binning-1);
  _minY = minY * _binning;
  _maxY = maxY * _binning + (_binning-1);

  //---------------------------------------------------------------------
  // If the maxes are less than the mins, set them to the size of
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

  //---------------------------------------------------------------------
  // Set up and read one frame at a time if the circular-buffer code is
  // not available.  Otherwise, the opening code will have set up
  // the system to run continuously, so we get the next frame from the
  // buffers.
  rgn_type  region_description;
  region_description.s1 = _minX;
  region_description.s2 = _maxX;
  region_description.p1 = _minY;
  region_description.p2 = _maxY;
  region_description.sbin = _binning;
  region_description.pbin = _binning;
  if (_circbuffer_on) {
    PL_CHECK_RETURN(read_continuous(_camera_handle, region_description, (int)exposure_time_millisecs),
      "read_continuous");
  } else {
    PL_CHECK_RETURN(read_one_frame(_camera_handle, region_description, (int)exposure_time_millisecs),
      "read_one_frame");
  }

  return true;
}

static	uns16 clamp_gain(uns16 val, double gain, double clamp = 65535.0)
{
  double result = val * gain;
  if (result > clamp) { result = clamp; }
  return (uns16)result;
}

bool  roper_server::write_memory_to_ppm_file(const char *filename, int gain, bool sixteen_bits) const
{
  //---------------------------------------------------------------------
  // Make sure the region is non-zero (so we've read an image)
  if ( (_maxX <= _minX) || (_maxY <= _minY) ) {
    fprintf(stderr,"roper_server::write_memory_to_ppm_file(): No image in memory\n");
    return false;
  }

  //---------------------------------------------------------------------
  // If we are not doing 16 bits, map the 16 bits to the range 0-255, and then write out an
  // uncompressed 8-bit grayscale PPM file with the values scaled to this range.
  if (!sixteen_bits) {
    uns16	  *vals = (uns16 *)_memory;
    unsigned char *pixels;
    // This buffer will be oversized if min and max don't span the whole window.
    if ( (pixels = new unsigned char[_buflen/2]) == NULL) {
      fprintf(stderr, "Can't allocate memory for stored image\n");
      return false;
    }
    unsigned r,c;
    uns16 minimum = clamp_gain(vals[0],gain);
    uns16 maximum = clamp_gain(vals[0],gain);
    uns16 cols = (_maxX - _minX)/_binning + 1;
    uns16 rows = (_maxY - _minY)/_binning + 1;
    for (r = 0; r < rows; r++) {
      for (c = 0; c < cols; c++) {
	if (clamp_gain(vals[r*cols + c],gain) < minimum) { minimum = clamp_gain(vals[r*cols+c],gain, 65535); }
	if (clamp_gain(vals[r*cols + c],gain) > maximum) { maximum= clamp_gain(vals[r*cols+c],gain, 65535); }
      }
    }
    printf("Minimum = %d, maximum = %d\n", minimum, maximum);
    uns16 offset = 0;
    double scale = gain;
    for (r = 0; r < rows; r++) {
      for (c = 0; c < cols; c++) {
	pixels[r*cols + c] = clamp_gain(vals[r*cols+c] - offset, scale, 65535) >> 8;
      }
    }
    FILE *of = fopen(filename, "wb");
    fprintf(of, "P5\n%d %d\n%d\n", cols, rows, 255);
    fwrite(pixels, 1, cols*rows, of);
    fclose(of);
    delete [] pixels;

  // If we are doing 16 bits, write out a 16-bit file.
  } else {
    uns16 *vals = (uns16 *)_memory;
    unsigned r,c;
    vrpn_uint16 *pixels;
    // This buffer will be oversized if min and max don't span the whole window.
    if ( (pixels = new vrpn_uint16[_buflen]) == NULL) {
      fprintf(stderr, "Can't allocate memory for stored image\n");
      return false;
    }
    uns16 minimum = clamp_gain(vals[0],gain);
    uns16 maximum = clamp_gain(vals[0],gain);
    uns16 cols = (_maxX - _minX)/_binning + 1;
    uns16 rows = (_maxY - _minY)/_binning + 1;
    for (r = 0; r < rows; r++) {
      for (c = 0; c < cols; c++) {
	if (clamp_gain(vals[r*cols + c],gain) < minimum) { minimum = clamp_gain(vals[r*cols+c],gain); }
	if (clamp_gain(vals[r*cols + c],gain) > maximum) { maximum = clamp_gain(vals[r*cols+c],gain); }
      }
    }
    printf("Minimum = %d, maximum = %d\n", minimum, maximum);
    uns16 offset = 0;
    double scale = gain;
    for (r = 0; r < rows; r++) {
      for (c = 0; c < cols; c++) {
	pixels[r*cols + c] = clamp_gain(vals[r*cols+c] - offset, scale);
      }
    }
    FILE *of = fopen(filename, "wb");
    fprintf(of, "P5\n%d %d\n%d\n", cols, rows, 4095);
    fwrite(pixels, sizeof(uns16), cols*rows, of);
    fclose(of);
    delete [] pixels;
  }
  return true;
}

//---------------------------------------------------------------------
// Map the 16 bits to the range 0-255, and return the result
bool	roper_server::get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint8 &val, int RGB) const
{
  if ( (_maxX <= _minX) || (_maxY <= _minY) ) {
    fprintf(stderr,"roper_server::get_pixel_from_memory(): No image in memory\n");
    return false;
  }
  if (RGB != 0) {
    fprintf(stderr,"roper_server::get_pixel_from_memory(): Can't select other than 0th color\n");
    return false;
  }
  if ( (X < _minX/_binning) || (X > _maxX/_binning) || (Y < _minY/_binning) || (Y > _maxY/_binning) ) {
    return false;
  }
  uns16	*vals = (uns16 *)_memory;
  uns16	cols = (_maxX - _minX + 1)/_binning;
  val = (vrpn_uint8)(vals[(Y-_minY/_binning)*cols + (X-_minX/_binning)] >> 8);
  return true;
}

bool	roper_server::get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint16 &val, int RGB) const
{
  if ( (_maxX <= _minX) || (_maxY <= _minY) ) {
    fprintf(stderr,"roper_server::get_pixel_from_memory(): No image in memory\n");
    return false;
  }
  if (RGB != 0) {
    fprintf(stderr,"roper_server::get_pixel_from_memory(): Can't select other than 0th color\n");
    return false;
  }
  if ( (X < _minX/_binning) || (X > _maxX/_binning) || (Y < _minY/_binning) || (Y > _maxY/_binning) ) {
    return false;
  }
  uns16	*vals = (uns16 *)_memory;
  uns16	cols = (_maxX - _minX + 1)/_binning;
  val = vals[(Y-_minY/_binning)*cols + (X-_minX/_binning)];
  return true;
}

bool roper_server::send_vrpn_image(vrpn_Imager_Server* svr,vrpn_Connection* svrcon,double g_exposure,int svrchan, int num_chans)
{
    // Send the current frame over to the client in chunks as big as possible (limited by vrpn_IMAGER_MAX_REGION)
    uns16 cols = (_maxX - _minX)/_binning + 1;
    uns16 rows = (_maxY - _minY)/_binning + 1;
    unsigned  num_x = cols;
    unsigned  num_y = rows;
    //XXX Should be 16-bit version later
    int nRowsPerRegion = vrpn_IMAGER_MAX_REGIONu8/(num_x*sizeof(vrpn_uint8)) - 1;
    unsigned y;

    //XXX Check for lost frames in continuous mode and report them if they are found.

    //XXX This is hacked to send the upper 8 bits of each value. Need to modify reader to handle 16-bit ints.
    // For these, stride will be 1 and offset will be 0, and the code will use memcpy() to copy the values.
    // Later, it should send 16 bits.
    const int stride = 2;
    const int offset = 1;
    svr->send_begin_frame(0, cols-1, 0, rows-1);
    for(y=0;y<num_y;y=__min(num_y,y+nRowsPerRegion)) {
      svr->send_region_using_base_pointer(svrchan,0,num_x-1,y,__min(num_y,y+nRowsPerRegion)-1, reinterpret_cast<vrpn_uint8 *>(_memory) + offset, stride, num_x * stride);
      svr->mainloop();
    }
    svr->send_end_frame(0, cols-1, 0, rows-1);
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
bool roper_server::write_to_opengl_texture(GLuint tex_id)
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
    FORMAT, TYPE, &((uns16 *)_memory)[NUM_COMPONENTS * ( _minX + get_num_columns()*_minY )]);
  return true;
}

static const unsigned SPE_HEADER_SIZE = 4100;
static const unsigned SPE_XDIM_OFFSET = 42;
static const unsigned SPE_YDIM_OFFSET = 656;
static const unsigned SPE_NUMFRAMES_OFFSET = 1446;

spe_file_server::spe_file_server(const char *filename) :
d_buffer(NULL),
d_infile(NULL),
d_numFrames(0)
{
  // In case we fail somewhere along the way
  _status = false;

  // Open the file whose name is passed in for binary reading.
#ifdef	_WIN32
  d_infile = fopen(filename, "rb");
#else
  d_infile = fopen(filename, "r");
#endif
  if (d_infile == NULL) {
    char  msg[1024];
    sprintf(msg, "spe_file_server::spe_file_server: Could not open file %s", filename);
    perror(msg);
  }

  // Set the mode to pause.
  d_mode = PAUSE;

  // Read the header information from the file to determine the number of samples
  // in X, Y, and Z. (columns, rows, and frames).
  vrpn_uint8  header[SPE_HEADER_SIZE];
  if (fread(header, SPE_HEADER_SIZE, 1, d_infile) != 1) {
    fprintf(stderr,"spe_file_server::spe_file_server: Could not read header\n");
    return;
  }
  _num_columns = *reinterpret_cast<vrpn_uint16 *>(&header[SPE_XDIM_OFFSET]);
  _num_rows = *reinterpret_cast<vrpn_uint16 *>(&header[SPE_YDIM_OFFSET]);
  d_numFrames = *reinterpret_cast<vrpn_uint32 *>(&header[SPE_NUMFRAMES_OFFSET]);

  // Allocate space to read a frame from the file
  if ( (d_buffer = new vrpn_uint16[_num_columns * _num_rows]) == NULL) {
    fprintf(stderr,"spe_file_server::spe_file_server: Out of memory\n");
    return;
  }

  // Read one frame when we start
  d_mode = SINGLE;

  // Everything opened okay.
  _minX = _minY = 0;
  _maxX = _num_columns-1;
  _maxY = _num_rows-1;
  _binning = 1;
  _status = true;
}

spe_file_server::~spe_file_server(void)
{
  // Close the file
  if (d_infile != NULL) {
    fclose(d_infile);
  }

  // Free the space taken by the in-memory image (if allocated)
  if (d_buffer) {
    delete [] d_buffer;
  }
}

void  spe_file_server::play()
{
  d_mode = PLAY;
}

void  spe_file_server::pause()
{
  d_mode = PAUSE;
}

void  spe_file_server::rewind()
{
  // Seek to the beginning of the file (after the header).
  if (d_infile != NULL) {
    fseek(d_infile, SPE_HEADER_SIZE, SEEK_SET);
  }

  // Read one frame when we start
  d_mode = SINGLE;
}

void  spe_file_server::single_step()
{
  d_mode = SINGLE;
}

bool  spe_file_server::read_image_to_memory(unsigned minX, unsigned maxX,
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

  // Make sure we have both a file and a buffer pointer that are valid.
  if ( (d_infile == NULL) || (d_buffer == NULL)) {
    return false;
  }

  // Try to read one frame from the current location in the file.  If we fail in the read,
  // set the mode to paused so we don't keep trying.
  if (fread(d_buffer, sizeof(d_buffer[0]), _num_columns * _num_rows, d_infile) != _num_columns * _num_rows) {
    d_mode = PAUSE;
    return false;
  }

  // Okay, we got a new frame!
  return true;
}

/// Get pixels out of the memory buffer
bool  spe_file_server::get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint8 &val, int /* ignore color */) const
{
  // Make sure we are within the range of allowed pixels
  if ( (X < _minX) || (Y < _minY) || (X > _maxX) || (Y > _maxY) ) {
    return false;
  }

  // Fill in the pixel value, assuming pixels vary in X fastest in the file.
  // Invert Y so that the image shown in the spot tracker program matches the
  // images shown in the capture program.  This stores the high-order 8 bits into
  // the buffer.
  val = (d_buffer[ X + (_maxY - Y) * _num_columns ] >> 8);
  return true;
}

bool  spe_file_server::get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint16 &val, int /* ignore color */) const
{
  // Make sure we are within the range of allowed pixels
  if ( (X < _minX) || (Y < _minY) || (X > _maxX) || (Y > _maxY) ) {
    return false;
  }

  // Fill in the pixel value, assuming pixels vary in X fastest in the file.
  val = d_buffer[ X + (_maxY - Y) * _num_columns ];
  return true;
}

/// Store the memory image to a PPM file.
bool  spe_file_server::write_memory_to_ppm_file(const char *filename, int gain, bool sixteen_bits) const
{
  //XXX;
  fprintf(stderr,"spe_file_server::write_memory_to_ppm_file(): Not yet implemented\n");
  return false;

  return true;
}

/// Send in-memory image over a vrpn connection
// XXX This needs to be tested
bool  spe_file_server::send_vrpn_image(vrpn_Imager_Server* svr,vrpn_Connection* svrcon,double g_exposure,int svrchan, int)
{
    // Send the current frame over to the client in chunks as big as possible (limited by vrpn_IMAGER_MAX_REGION)
    unsigned  num_x = get_num_columns();
    unsigned  num_y = get_num_rows();
    int nRowsPerRegion=vrpn_IMAGER_MAX_REGIONu16/num_x;
    unsigned y;
    svr->send_begin_frame(0, num_x-1, 0, num_y-1);
    for(y=0;y<num_y;y=__min(num_y,y+nRowsPerRegion)) {
      svr->send_region_using_base_pointer(svrchan,0,num_x-1,y,__min(num_y,y+nRowsPerRegion)-1,
	(vrpn_uint8 *)d_buffer, 1, get_num_columns());
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
bool spe_file_server::write_to_opengl_texture(GLuint tex_id)
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
      fprintf(stderr,"spe_file_server::write_to_opengl_texture(): Out of memory allocating temporary buffer\n");
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
    FORMAT, TYPE, &d_buffer[NUM_COMPONENTS * ( _minX + get_num_columns()*_minY )]);
  return true;
}

bool spe_file_server::write_opengl_texture_to_quad(double xfrac, double yfrac)
{
  // Flip over while writing.
  // Set the texture and vertex coordinates and write the quad to OpenGL.
  glBegin(GL_QUADS);
    glTexCoord2d(0.0, yfrac);
    glVertex2d(-1.0, -1.0);

    glTexCoord2d(xfrac, yfrac);
    glVertex2d(1.0, -1.0);

    glTexCoord2d(xfrac, 0.0);
    glVertex2d(1.0, 1.0);

    glTexCoord2d(0.0, 0.0);
    glVertex2d(-1.0, 1.0);
  glEnd();

  return true;
}
