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

#define	PL_CHECK_EXIT(x, y) { boolean retval; \
  if ( (retval = x) == PV_FAIL ) { \
    char  msg[1024]; \
    pl_error_message(pl_error_code(), msg); \
    fprintf(stderr, "%s failed due to %s\n", y, msg); \
    exit(-1); \
  } \
}

#define	PL_CHECK_RETURN(x, y) { boolean retval; \
  if ( (retval = x) == PV_FAIL ) { \
    char  msg[1024]; \
    pl_error_message(pl_error_code(), msg); \
    fprintf(stderr, "%s failed due to %s\n", y, msg); \
    return FALSE; \
  } \
}

#define	PL_CHECK_WARN(x, y) { boolean retval; \
  if ( (retval = x) == PV_FAIL ) { \
    char  msg[1024]; \
    pl_error_message(pl_error_code(), msg); \
    fprintf(stderr, "%s failed due to %s\n", y, msg); \
  } \
}

//-----------------------------------------------------------------------

bool  roper_server::read_one_frame(const int16 camera_handle,
		       const rgn_type &region_description,
		       const uns32 exposure_time) {
  int16	progress = EXPOSURE_IN_PROGRESS;
  uns32	trash;
  uns32	buffer_size;	// Size required to hold the data

  // Initialize the sequence and set the parameters.  Also
  // check to make sure that the buffer passed in is large
  // enough to hold the image.
  PL_CHECK_RETURN(pl_exp_init_seq(), "pl_exp_init_seq");
  PL_CHECK_RETURN(pl_exp_setup_seq(camera_handle, 1, 1,
    &region_description, TIMED_MODE, exposure_time, &buffer_size), "pl_exp_setup_seq");
  if (buffer_size > _buflen) {
    fprintf(stderr,"read_one_frame: Buffer passed in too small\n");
    return FALSE;
  }

  // Start it reading one exposure
  PL_CHECK_RETURN(pl_exp_start_seq(camera_handle, _buffer), "pl_exp_start_seq");

  // Wait for it to be done reading
  const double READ_TIMEOUT = 1.0;
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

  } while (progress != READOUT_COMPLETE);

  // Uninitialize the sequence, and return
  PL_CHECK_RETURN(pl_exp_uninit_seq(), "pl_exp_uninit_seq");
  return TRUE;
}

//---------------------------------------------------------------------
// Open the camera and determine its available features and parameters.

bool roper_server::open_and_find_parameters(void)
{
  // Find out how many cameras are on the system, then get the
  // name of the first camera, open it, and make sure that it has
  // no critical failures.
  int16	  num_cameras;
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
  PL_CHECK_EXIT(pl_get_param(_camera_handle, PARAM_PAR_SIZE, ATTR_CURRENT, &_num_rows),
    "PARAM_PAR_SIZE");
  PL_CHECK_EXIT(pl_get_param(_camera_handle, PARAM_SER_SIZE, ATTR_CURRENT, &_num_columns),
    "PARAM_SER_SIZE");

  // XXX Find the list of available exposure settings..

  return true;
}

roper_server::roper_server(void)
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
  if ( (_buffer = GlobalAlloc( GMEM_ZEROINIT, _buflen)) == NULL) {
    fprintf(stderr,"roper_server::roper_server(): Cannot allocate enough locked memory for buffer\n");
    _status = false;
    return;
  }
  if ( (_memory = GlobalLock(_buffer)) == NULL) {
    fprintf(stderr, "roper_server::roper_server(): Cannot lock memory buffer\n");
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

roper_server::~roper_server(void)
{
  PL_CHECK_WARN(pl_pvcam_uninit(), "pl_pvcam_uninit");
  GlobalUnlock(_buffer );
  GlobalFree(_buffer );
}

bool  roper_server::read_image_to_memory(int minX, int maxX, int minY, int maxY,
					 double exposure_time)
{
  //---------------------------------------------------------------------
  // In case we fail, clear these
  _minX = _minY = _maxX = _maxY = 0;

  //---------------------------------------------------------------------
  // If the maxes are greater than the mins, set them to the size of
  // the image.
  if (maxX <= minX) {
    minX = 0; maxX = _num_columns - 1;
  }
  if (maxY <= minY) {
    minY = 0; maxY = _num_rows - 1;
  }

  //---------------------------------------------------------------------
  // Clip collection range to the size of the sensor on the camera.
  if (minX < 0) { minX = 0; };
  if (minY < 0) { minY = 0; };
  if (maxX >= _num_columns) { maxX = _num_columns - 1; };
  if (maxY >= _num_rows) { maxY = _num_rows - 1; };

  //---------------------------------------------------------------------
  // Store image size for later use
  _minX = minX; _minY = minY; _maxX = maxX; _maxY = maxY;

  //---------------------------------------------------------------------
  // Set up and read one frame.
  rgn_type  region_description;
  region_description.s1 = _minX;
  region_description.s2 = _maxX;
  region_description.p1 = _minY;
  region_description.p2 = _maxY;
  region_description.sbin = 1;
  region_description.pbin = 1;
  PL_CHECK_RETURN(read_one_frame(_camera_handle, region_description, (int)exposure_time),
    "read_one_frame");

  return true;
}

bool  roper_server::write_memory_to_ppm_file(const char *filename) const
{
  //---------------------------------------------------------------------
  // Make sure the region is non-zero (so we've read an image)
  if ( (_maxX <= _minX) || (_maxY <= _minY) ) {
    fprintf(stderr,"roper_server::write_memory_to_ppm_file(): No image in memory\n");
    return false;
  }

  //---------------------------------------------------------------------
  // Map the 12 bits to the range 0-255, and then write out an
  // uncompressed grayscale PPM file with the values scaled to this range.
  uns16	   *vals = (uns16 *)_memory;
  unsigned char *pixels;
  // This buffer will be oversized if min and max don't span the whole window.
  if ( (pixels = new unsigned char[_buflen/2]) == NULL) {
    fprintf(stderr, "Can't allocate memory for stored image\n");
    return false;
  }
  unsigned r,c;
  uns16 minimum = vals[0];
  uns16 maximum = vals[0];
  uns16	cols = (_maxX - _minX) + 1;
  uns16	rows = (_maxY - _minY) + 1;
  for (r = 0; r < rows; r++) {
    for (c = 0; c < cols; c++) {
      if (vals[r*cols + c] < minimum) { minimum = vals[r*cols+c]; }
      if (vals[r*cols + c] > maximum) { maximum= vals[r*cols+c]; }
    }
  }
  printf("Minimum = %d, maximum = %d\n", minimum, maximum);
  uns16 offset = 0;
  double scale = 255.0 / 4095.0;
  for (r = 0; r < rows; r++) {
    for (c = 0; c < cols; c++) {
      pixels[r*cols + c] = (unsigned char)((vals[r*cols+c] - offset) * scale);
    }
  }
  FILE *of = fopen(filename, "wb");
  fprintf(of, "P5\n%d %d\n%d\n", cols, rows, 255);
  fwrite(pixels, 1, cols*rows, of);
  fclose(of);
  delete [] pixels;

  return true;
}

bool	roper_server::get_pixel_from_memory(int X, int Y, uns16 &val) const
{
  if ( (_maxX <= _minX) || (_maxY <= _minY) ) {
    fprintf(stderr,"roper_server::get_pixel_from_memory(): No image in memory\n");
    return false;
  }
  if ( (X < _minX) || (X > _maxX) || (Y < _minY) || (Y > _maxY) ) {
    return false;
  }
  uns16	*vals = (uns16 *)_memory;
  uns16	cols = (_maxX - _minX) + 1;
  val = vals[Y*cols + X];
  return true;
}
