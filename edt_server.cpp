#include "edt_server.h"

const unsigned PULNIX_X_SIZE = 648;
const unsigned PULNIX_Y_SIZE = 484;

edt_pulnix_raw_file_server::edt_pulnix_raw_file_server(const char *filename) :
d_buffer(NULL),
d_infile(NULL)
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
    sprintf(msg, "edt_pulnix_raw_file_server::edt_pulnix_raw_file_server: Could not open file %s", filename);
    perror(msg);
  }

  // Set the mode to pause.
  d_mode = PAUSE;

  // Allocate space to read a frame from the file
  if ( (d_buffer = new vrpn_uint8[PULNIX_X_SIZE * PULNIX_Y_SIZE]) == NULL) {
    fprintf(stderr,"edt_pulnix_raw_file_server::edt_pulnix_raw_file_server: Out of memory\n");
    return;
  }

  // Read one frame when we start
  d_mode = SINGLE;

  // Everything opened okay.
  _minX = _minY = 0;
  _maxX = PULNIX_X_SIZE-1;
  _maxY = PULNIX_Y_SIZE-1;
  _num_columns = PULNIX_X_SIZE;
  _num_rows = PULNIX_Y_SIZE;
  _binning = 1;
  _status = true;
}

edt_pulnix_raw_file_server::~edt_pulnix_raw_file_server(void)
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

void  edt_pulnix_raw_file_server::play()
{
  d_mode = PLAY;
}

void  edt_pulnix_raw_file_server::pause()
{
  d_mode = PAUSE;
}

void  edt_pulnix_raw_file_server::rewind()
{
  // Seek to the beginning of the file.
  if (d_infile != NULL) {
    fseek(d_infile, 0, SEEK_SET);
  }

  // Read one frame when we start
  d_mode = SINGLE;
}

void  edt_pulnix_raw_file_server::single_step()
{
  d_mode = SINGLE;
}

bool  edt_pulnix_raw_file_server::read_image_to_memory(unsigned minX, unsigned maxX,
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
  if (fread(d_buffer, PULNIX_X_SIZE * PULNIX_Y_SIZE, 1, d_infile) != 1) {
    d_mode = PAUSE;
    return false;
  }

  // Okay, we got a new frame!
  return true;
}

/// Get pixels out of the memory buffer
bool  edt_pulnix_raw_file_server::get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint8 &val, int /* ignore color */) const
{
  // Make sure we are within the range of allowed pixels
  if ( (X < _minX) || (Y < _minY) || (X > _maxX) || (Y > _maxY) ) {
    return false;
  }

  // Fill in the pixel value, assuming pixels vary in X fastest in the file.
  // Invert Y so that the image shown in the spot tracker program matches the
  // images shown in the capture program.
  val = d_buffer[ X + (_maxY - Y) * PULNIX_X_SIZE ];
  return true;
}

bool  edt_pulnix_raw_file_server::get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint16 &val, int /* ignore color */) const
{
  // Make sure we are within the range of allowed pixels
  if ( (X < _minX) || (Y < _minY) || (X > _maxX) || (Y > _maxY) ) {
    return false;
  }

  // Fill in the pixel value, assuming pixels vary in X fastest in the file.
  val = d_buffer[ X + (_maxY - Y) * PULNIX_X_SIZE ];
  return true;
}

/// Store the memory image to a PPM file.
bool  edt_pulnix_raw_file_server::write_memory_to_ppm_file(const char *filename, int gain, bool sixteen_bits) const
{
  //XXX;
  fprintf(stderr,"edt_pulnix_raw_file_server::write_memory_to_ppm_file(): Not yet implemented\n");
  return false;

  return true;
}

/// Send whole image over a vrpn connection
bool  edt_pulnix_raw_file_server::send_vrpn_image(vrpn_TempImager_Server* svr,vrpn_Synchronized_Connection* svrcon,double g_exposure,int svrchan)
{
  ///XXX;
  fprintf(stderr,"edt_pulnix_raw_file_server::send_vrpn_image(): Not yet implemented\n");
  return false;

  return true;
}
