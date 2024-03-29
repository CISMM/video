#include <stdlib.h>
#include <stdio.h>
#include "nikon_server.h"
#include <vrpn_BaseClass.h>

static	unsigned long	duration(struct timeval t1, struct timeval t2)
{
	return (t1.tv_usec - t2.tv_usec) +
	       1000000L * (t1.tv_sec - t2.tv_sec);
}

//-----------------------------------------------------------------------

bool  nikon_server::read_continuous(const int16 camera_handle,
		       const rgn_type &region_description,
		       const uns32 exposure_time_millisecs)
{
  static uns32 last_buffer_cnt = 0; //< How many buffers have been read?
  int16	progress;	//< Progress made in getting the current frame
  uns32	buffer_size;	//< Size required to hold the data

  //--------------------------------------------------------------------
  // See if the parameters have changed or if the circular buffer has not
  // yet been set up to run..  If so, then we need to stop any previous
  // acquisition and then restart a new set of acquisitions.  Otherwise,
  // we must have already acquired a buffer so we want to free it up
  // to be re-filled next time.
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
      if (!(retval1 = pl_exp_stop_cont(_camera_handle, CCS_HALT))) {
	fprintf(stderr,"roper_server::read_continuous(): Cannot stop acquisition\n");
      }
      if (_circbuffer) {
	delete [] _circbuffer;
	_circbuffer_len = 0;
      }
      if (retval1) { return false; }
    }

    // Allocate a new circular buffer that is large enough to hold the number
    // of buffers we want.
    _circbuffer_len = (uns32)(_circbuffer_num * 
      (_maxX - _minX + 1) * (_maxY - _minY + 1) / (_binning * _binning) );
    if ( (_circbuffer = new uns16[_circbuffer_len]) == NULL) {
      fprintf(stderr,"roper_server::read_continuous(): Out of memory\n");
      _circbuffer_len = 0;
      return false;
    }

    // Setup and start a continuous acquisition.
    PL_CHECK_RETURN(pl_exp_setup_cont(camera_handle, 1, &region_description,
      TIMED_MODE, exposure_time_millisecs, &buffer_size, CIRC_NO_OVERWRITE),
      "pl_exp_setup_cont");
    // The factor of 2 here is converting bytes to pixels (16 bits)
    if (buffer_size / 2 != _circbuffer_len / _circbuffer_num) {
      fprintf(stderr,"roper_server::read_continuous(): Unexpected buffer size (got %u, expected %u)\n",
	buffer_size, _circbuffer_len / _circbuffer_num * 2);
      return false;
    }
    PL_CHECK_RETURN(pl_exp_start_cont(camera_handle, _circbuffer, _circbuffer_len),
      "pl_exp_start_cont");

    // No buffers have been read yet.
    last_buffer_cnt = 0;
  } else {
    // Free up the buffer used last time
    pl_exp_unlock_oldest_frame(camera_handle);
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

     // See how many buffers have been filled (XXX times the buffer has been filled?).
     // Each time we get another one, we go ahead and read it (set progress to READOUT_COMPLETE)
     if (buffer_cnt > last_buffer_cnt) {
       progress = READOUT_COMPLETE;
       last_buffer_cnt ++;
     }

     // Check for a timeout; if we haven't heard anything for a second longer than exposure,
     // abort the read and restart it.
     gettimeofday(&now, NULL);
     if (duration(now, start) / 1.0e6 > READ_TIMEOUT) {
//XXX       send_text_message("Timeout when reading, retrying", vrpn_TEXT_WARNING, 0);
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

  return TRUE;
}


//-----------------------------------------------------------------------

bool  nikon_server::read_one_frame(const int16 camera_handle,
		       const rgn_type &region_description,
		       const uns32 exposure_time_millisecs) {
  int16	progress = EXPOSURE_IN_PROGRESS;
  uns32	trash;
  uns32	buffer_size;	// Size required to hold the data

  // Initialize the sequence and set the parameters.  Also
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
//XXX       send_text_message("Timeout when reading, retrying", vrpn_TEXT_WARNING, 0);
       // XXX Should return an error and let the above code decide to retry
       fprintf(stderr,"roper_server::read_one_frame(): Timeout, retrying\n");
       PL_CHECK_WARN(pl_exp_abort(camera_handle, CCS_HALT), "pl_exp_abort");
       PL_CHECK_RETURN(pl_exp_start_seq(camera_handle, _buffer), "pl_exp_start_seq");
       gettimeofday(&start, NULL);
     }
     // Check at 1ms intervals to avoid eating the whole CPU
     vrpn_SleepMsecs(1);

  } while (progress != READOUT_COMPLETE);

  return TRUE;
}

//---------------------------------------------------------------------
// Open the camera and determine its available features and parameters.

bool nikon_server::open_and_find_parameters(void)
{
  // Find out how many cameras are on the system, then get the
  // name of the first cam`era, open it, and make sure that it has
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
  PL_CHECK_EXIT(pl_cam_get_name(0, camera_name), "pl_cam_get_name");
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
  //XXX Circular-buffer code times out in acquisition mode, also in sample code from
  // Roper. Have asked them how to fix this.
  _circbuffer_on = false;
  if (!_circbuffer_on) {
    fprintf(stderr,"roper_server::open_and_find_parameters(): Does not support circular buffers (using single-capture mode)\n");
  }

  // XXX Find the list of available exposure settings..

  return true;
}

nikon_server::roper_server(unsigned binning, unsigned which_camera) :
  base_camera_server(binning)
{
  //---------------------------------------------------------------------
  // Initialize the Nikon software, open the camera, and find out what its
  // capabilities are.
  XXX;

  //---------------------------------------------------------------------
  // Allocate a buffer that is large enough to read the maximum-sized
  // image with no binning.
  XXX;

  //---------------------------------------------------------------------
  // No image in memory yet.
  _minX = _minY = _maxX = _maxY = 0;

  _status = true;
}

//---------------------------------------------------------------------
// Close the camera and the system.  Free up memory.

nikon_server::~nikon_server(void)
{
  //---------------------------------------------------------------------
  // Shut down the Nikon software and free the global memory buffer.
  XXX;
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
  // If we are not doing 16 bits, map the 12 bits to the range 0-255, and then write out an
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
	if (clamp_gain(vals[r*cols + c],gain) < minimum) { minimum = clamp_gain(vals[r*cols+c],gain, 4095); }
	if (clamp_gain(vals[r*cols + c],gain) > maximum) { maximum= clamp_gain(vals[r*cols+c],gain, 4095); }
      }
    }
    printf("Minimum = %d, maximum = %d\n", minimum, maximum);
    uns16 offset = 0;
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
// Map the 12 bits to the range 0-255, and return the result
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
  val = (vrpn_uint8)(vals[(Y-_minY/_binning)*cols + (X-_minX/_binning)] >> 4);
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
    unsigned  num_x = get_num_columns();
    unsigned  num_y = get_num_rows();
    //XXX Should be 16-bit version later
    int nRowsPerRegion = vrpn_IMAGER_MAX_REGIONu8/(num_x*sizeof(vrpn_uint8)) - 1;
    unsigned y;

    //XXX This is hacked to optimize the value in the upper 8 bits to be the most-significant bits
    // of the 12 bits.  I thought this should depend on binning, but it seems like it does not.
    // Strange.
    unsigned loop;
    for (loop = 0; loop < num_x*num_y; loop++) {
	((vrpn_uint16*)_memory)[loop] = ((vrpn_uint16*)_memory)[loop] << 4;
    }

    //XXX This is hacked to send the upper 8 bits of each value. Need to modify reader to handle 16-bit ints.
    // For these, stride will be 1 and offset will be 0, and the code will use memcpy() to copy the values.
    const int stride = 2;
    const int offset = 1;
    svr->send_begin_frame(0, _num_columns-1, 0, _num_rows-1);
    for(y=0;y<num_y;y=__min(num_y,y+nRowsPerRegion)) {
      svr->send_region_using_base_pointer(svrchan,0,num_x-1,y,__min(num_y,y+nRowsPerRegion)-1,
	(uns8 *)_memory + offset, stride, num_x * stride);
      svr->mainloop();
    }
    svr->send_end_frame(0, _num_columns-1, 0, _num_rows-1);
    svr->mainloop();

    // Mainloop the server connection (once per server mainloop, not once per object).
    svrcon->mainloop();
    return true;
}
