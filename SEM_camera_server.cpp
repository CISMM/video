/* XXX This implementation is only for reading from stream-files that are written
 * using the SEM server.  It does not yet have the functions needed to actually
 * set scan sizes and so forth that are required to connect to a live instrument.
 */

#include <stdlib.h>
#include <stdio.h>
#include "SEM_camera_server.h"
#include <vrpn_BaseClass.h>

//#define	DEBUG

static	unsigned long	duration(struct timeval t1, struct timeval t2)
{
	return (t1.tv_usec - t2.tv_usec) +
	       1000000L * (t1.tv_sec - t2.tv_sec);
}

// Callback handler that is called by the SEM remote object when it
// gets new data from the server.  The first parameter points to the
// SEM_camera_server that requested the callback.
void SEM_camera_server::handle_SEM_update(void *ud, const nmm_Microscope_SEM_ChangeHandlerData &info)
{
  SEM_camera_server *me = (SEM_camera_server *)ud;
  vrpn_int32 res_x, res_y;
  void *scanlineData;
  vrpn_int32 start_x, start_y, dx,dy, line_length, num_fields, num_lines;
  nmb_PixelType pix_type;
  int i,j;

  // See what kind of message this is from the SEM.  For some reason, rather
  // than sending different types of messages they are all packed into one
  // callback that then switches based on an internally-stored type.  Not the
  // normal VRPN way to do things, but that's how it goes.
  switch(info.msg_type) {
    case nmm_Microscope_SEM::REPORT_RESOLUTION:
        info.sem->getResolution(res_x, res_y);
	me->_num_rows = res_y;
	me->_num_columns = res_x;
	me->_minX = 0;
	me->_maxX = me->_num_columns - 1;
	me->_minY = 0;
	me->_maxY = me->_num_rows - 1;

	//---------------------------------------------------------------------
	// Delete the old buffer, if there was one
	if (me->_memory != NULL) { delete [] me->_memory; }

	//---------------------------------------------------------------------
	// Allocate a buffer that is large enough to read the maximum-sized
	// image with no binning.
	me->_buflen = (vrpn_uint32)(me->_num_rows* me->_num_columns);	// Two bytes per pixel, but we're allocating 16-bit values
	if ( (me->_memory = new vrpn_uint16[me->_buflen]) == NULL) {
	  fprintf(stderr, "SEM_camera_server::handle_SEM_update(): Cannot allocate memory buffer\n");
	  me->_status = false;
	  return;
	}

	// We've now heard the resolution from the server.
	me->_gotResolution = true;
	break;

    case nmm_Microscope_SEM::SCANLINE_DATA:
        info.sem->getScanlineData(start_x, start_y, dx, dy, line_length,
                                  num_fields, num_lines, 
                                  pix_type, &scanlineData);

	// Make sure the resolution matches what we expect.
        info.sem->getResolution(res_x, res_y);
        if (start_y + num_lines*dy > res_y || line_length*dx != res_x) {
	   fprintf(stderr, "SEM_camera_server: Scanline data dimensions unexpected\n");
	   fprintf(stderr, "  got (%d,[%d-%d]), expected (%d,%d)\n",
                              line_length*dx, start_y, start_y + dy*(num_lines),
				res_x, res_y);
	   me->_status = false;
	   break;
        }

	// Copy the pixels into the camera's image buffer if we've heard the resolution
	// already.
        if (me->_gotResolution) {
          int x, y;
          x = start_x;
          y = start_y;
          vrpn_uint8 *uint8_data = (vrpn_uint8 *)scanlineData;
          vrpn_uint16 *uint16_data = (vrpn_uint16 *)scanlineData;
          vrpn_float32 *float32_data = (vrpn_float32 *)scanlineData;
          switch(pix_type) {
            case NMB_UINT16:  // Straight copy into the 16-bit buffer
              for (i = 0; i < num_lines; i++) {
                x = start_x;
                for (j = 0; j < line_length; j++) {
		  me->_memory[x + y *line_length] = uint16_data[(i*line_length+j)*num_fields];
                   x += dx;
                }
                y += dy;
              }
              break;
            case NMB_UINT8: // Store the 8-bit values into the 16-bit fields, shifted left 8
              for (i = 0; i < num_lines; i++) {
                x = start_x;
                for (j = 0; j < line_length; j++) {
		  me->_memory[x + y *line_length] = uint8_data[(i*line_length+j)*num_fields] << 8;
                   x += dx;
                }
                y += dy;
              }
              break;
            case NMB_FLOAT32:
	      // XXX Not sure how to scale these values so that they will stay in range.
	      // For now, just truncate them.
              for (i = 0; i < num_lines; i++) {
                x = start_x;
                for (j = 0; j < line_length; j++) {
		  me->_memory[x + y *line_length] = (vrpn_uint16)(float32_data[(i*line_length+j)*num_fields]);
                   x += dx;
                }
                y += dy;
              }
              break;
          }         

          // when we get the end of an image, increment the frame number
          if (info.sem->lastScanMessageCompletesImage()) {
	    me->_frameNum++;
            if (me->_pause_after_one_frame) { 
              // To avoid skipping past the message we just read, find out the current
              // message time and set the time on the file to it.  This should stop us
              // "on a dime".  Then pause to set the replay rate to zero so we stay
              // put.
              me->_fileCon->jump_to_filetime(info.msg_time);
              me->pause();
            }
          }
        }

	// Ask the SEM server for a new image if we just got a full
	// one.
        if (info.sem->lastScanMessageCompletesImage()) {
          info.sem->requestScan(1);
	}
	break;

    // We don't care about these message types.
    case nmm_Microscope_SEM::REPORT_PIXEL_INTEGRATION_TIME:
    case nmm_Microscope_SEM::REPORT_INTERPIXEL_DELAY_TIME:
    case nmm_Microscope_SEM::REPORT_EXPOSURE_STATUS:
    case nmm_Microscope_SEM::REPORT_MAGNIFICATION:
    case nmm_Microscope_SEM::REPORT_TIMING_STATUS:
    case nmm_Microscope_SEM::POINT_DWELL_TIME:
    case nmm_Microscope_SEM::BEAM_BLANK_ENABLE:
    case nmm_Microscope_SEM::MAX_SCAN_SPAN:
    case nmm_Microscope_SEM::RETRACE_DELAYS:
    case nmm_Microscope_SEM::DAC_PARAMS:
    case nmm_Microscope_SEM::EXTERNAL_SCAN_CONTROL_ENABLE:
        break;

    default:
        fprintf(stderr, "SEM_camera_server: Warning, unknown message type: %d\n", info.msg_type);
	break;
  }

}

//-----------------------------------------------------------------------
// The min and max coordinates specified here should be without regard to
// binning.  That is, they should be in the full-resolution device coordinates.

bool  SEM_camera_server::read_one_frame(unsigned short minX, unsigned short maxX,
				     unsigned short minY, unsigned short maxY,
				     unsigned exposure_time_millisecs)
{
  // Make sure the frame we've been asked to get is within the bounds of
  // what we are going to get, then go get a full-screen image.
  if ( (minX < 0) || (minY < 0) || (maxX > _num_columns-1) || (maxY > _num_rows-1) ) {
    fprintf(stderr, "SEM_camera_server::read_one_frame(): Invalid frame size\n");
    return false;
  }
  _minX = 0;
  _maxX = _num_columns - 1;
  _minY = 0;
  _maxY = _num_rows - 1;

  // If we've just been stepped to a new location in the video, then
  // go ahead and play out the current frame -- it has not been seen yet.
  if (_justStepped) {
    _justStepped = false;
    return true;
  }

  // If we're paused, then go ahead and bail because we're not going to be
  // getting any images.
  if (_paused) {
    return false;
  }

  // Wait until we either get a new complete scan or else time out waiting for one.
  // For the SEM server, timeout after two seconds rather than a tenth of a second.
  // This is because we're also now spoofing an SEM with our optical cameras, which
  // have much longer integration times.
  struct timeval start, now;
  gettimeofday(&start, NULL);
  int lastFrameNum = _frameNum;
  while (lastFrameNum == _frameNum) {
    _myScope->mainloop();
    gettimeofday(&now, NULL);
    if (duration(now, start) > 2000000L) {
      return false;
    }
    // If we do the following, then it doesn't go fast enough on some machines because
    // it sleeps in between each message (the fileconnection is now set to only do one
    // message per mainloop).
    //vrpn_SleepMsecs(0);	// Avoid eating the whole CPU
  }
  int frames_moved = _frameNum - lastFrameNum;
  if (frames_moved != 1) {
    fprintf(stderr, "SEM_camera_server::read_one_frame(): Skipped %d frames\n", frames_moved-1);
  }
  return true;
}

//---------------------------------------------------------------------
// Open the camera and determine its available features and parameters.
// This requires waiting until a message has been received from the
// server so that we know what the parameters are for the "camera."

bool SEM_camera_server::open_and_find_parameters(const char *name)
{
  int i;

  _myScope = new nmm_Microscope_SEM_Remote(name);

  // Register the change handler and call mainloop() until we hear
  // from the server or we timeout.
  _myScope->registerChangeHandler(this, handle_SEM_update);
  _gotResolution = false;
  struct timeval start, now;
  _status = true;
  _frameNum = -1;
  _myScope->mainloop();	  // Do one mainloop to pull in the file from disk
  gettimeofday(&start, NULL);
  while (!_gotResolution) {
    _myScope->mainloop();
    gettimeofday(&now, NULL);
    if (duration(now, start) > 10000000L) {
      fprintf(stderr,"SEM_camera_server::open_and_find_parameters(): Timeout trying to read resolution\n");
      return false;
    }
  }

  // Try to get the file interface from the connection for this device.  If
  // we can, then we can do play, rewind, pause, and other operations on it.
  // We would normally get the connection pointer directly from the object,
  // but the nmm_Microscope_SEM_Remote doesn't seem to derive from the base
  // object.
  vrpn_Connection *con = vrpn_get_connection_by_name(name);
  _fileCon = con->get_File_Connection();

  // Set the file connection to only play back one message for each call to
  // mainloop(), so that we don't skip past frames.
  if (_fileCon) {
    _fileCon->limit_messages_played_back(1);
  }

  // Try to read the first frame from the SEM.  If we have a file controller,
  // then speed up the replay rate until we get done with the first frame.
  // If the reading times out, try again a bunch of times.
  if (_fileCon) {
    _fileCon->set_replay_rate(100.0);
    _paused = false;
    _pause_after_one_frame = true;
  }
  for (i = 0; i < 1000; i++) {
    if (read_one_frame(0, _num_columns-1, 0, _num_rows-1, 0)) { break; }
  }
  if (_fileCon) {
    _fileCon->set_replay_rate(0.0);
    _pause_after_one_frame = false;
  }

  return _status;
}

SEM_camera_server::SEM_camera_server(const char *name) :
  base_camera_server(1),
  _fileCon(NULL),
  _bitDepth(0),
  _myScope(NULL),
  _memory(NULL),
  _frameNum(-1),
  _justStepped(false),
  _pause_after_one_frame(false),
  _paused(false)
{
  //---------------------------------------------------------------------
  // Openthe SEM and find out what its capabilities are.
  if (!open_and_find_parameters(name)) {
    fprintf(stderr, "SEM_camera_server::SEM_camera_server(): Cannot open SEM\n");
    _status = false;
    return;
  }

  //---------------------------------------------------------------------
  // No image in memory yet.
  _minX = _minY = _maxX = _maxY = 0;

  if (_myScope) {
    _status = true;
  } else {
    _status = false;
  }
}

//---------------------------------------------------------------------
// Close the SEM.  Free up memory.

SEM_camera_server::~SEM_camera_server(void)
{
  delete [] _memory;
  if (_myScope != NULL) { delete _myScope; }
}

// The min/max coordinates here are in post-binned space.  That is, the maximum should be
// the edge of the image produced, taking into account the binning of the full resolution.

bool  SEM_camera_server::read_image_to_memory(unsigned minX, unsigned maxX, unsigned minY, unsigned maxY,
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
  printf("SEM_camera_server::read_image_to_memory(): Setting window from (%d,%d) to (%d,%d)\n",
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
    fprintf(stderr,"SEM_camera_server::read_image_to_memory(): Clipping maxX to %d from %d\n",_num_columns, _maxX);
    _maxX = _num_columns - 1;
  };
  if (_maxY >= _num_rows) {
    fprintf(stderr,"SEM_camera_server::read_image_to_memory(): Clipping maxY\n");
    _maxY = _num_rows - 1;
  };

  //---------------------------------------------------------------------
  // Go get the frame.
  if (!read_one_frame(_minX, _maxX, _minY, _maxY, (unsigned long)exposure_time_millisecs)) {
    // Don't report a timeout here; we may be at the end of the file, or paused.
    return false;
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
bool  SEM_camera_server::write_memory_to_ppm_file(const char *filename, int gain, bool sixteen_bits) const
{
  //---------------------------------------------------------------------
  // Make sure the region is non-zero (so we've read an image)
  if ( (_maxX <= _minX) || (_maxY <= _minY) ) {
    fprintf(stderr,"SEM_camera_server::write_memory_to_ppm_file(): No image in memory\n");
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
      fprintf(stderr, "SEM_camera_server::write_memory_to_ppm_file(): Can't allocate memory for stored image\n");
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
      fprintf(stderr, "SEM_camera_server::write_memory_to_ppm_file(): Can't allocate memory for stored image\n");
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
bool	SEM_camera_server::get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint8 &val, int RGB) const
{
  if ( (_maxX <= _minX) || (_maxY <= _minY) ) {
    fprintf(stderr,"SEM_camera_server::get_pixel_from_memory(): No image in memory\n");
    return false;
  }
  if (RGB != 0) {
    fprintf(stderr,"SEM_camera_server::get_pixel_from_memory(): Can't select other than 0th color\n");
    return false;
  }
  if ( (X < _minX/_binning) || (X > _maxX/_binning) || (Y < _minY/_binning) || (Y > _maxY/_binning) ) {
    return false;
  }
  vrpn_uint16	*vals = (vrpn_uint16 *)_memory;
  vrpn_uint16	cols = (_maxX - _minX + 1)/_binning;  // Don't use num_columns here; depends on the region size.
  vrpn_uint32	index = (Y-_minY/_binning)*cols + (X-_minX/_binning);
  val = (vrpn_uint8)(vals[index] >> 4);
  return true;
}

bool	SEM_camera_server::get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint16 &val, int RGB) const
{
  if ( (_maxX <= _minX) || (_maxY <= _minY) ) {
    fprintf(stderr,"SEM_camera_server::get_pixel_from_memory(): No image in memory\n");
    return false;
  }
  if (RGB != 0) {
    fprintf(stderr,"SEM_camera_server::get_pixel_from_memory(): Can't select other than 0th color\n");
    return false;
  }
  if ( (X < _minX/_binning) || (X > _maxX/_binning) || (Y < _minY/_binning) || (Y > _maxY/_binning) ) {
    return false;
  }
  vrpn_uint16	*vals = (vrpn_uint16 *)_memory;
  vrpn_uint16	cols = (_maxX - _minX + 1)/_binning;  // Don't use num_columns here; depends on the region size.
  vrpn_uint32	index = (Y-_minY/_binning)*cols + (X-_minX/_binning);
  val = vals[index];
  return true;
}

// XXX This routine needs to be tested.
bool SEM_camera_server::send_vrpn_image(vrpn_Imager_Server* svr,vrpn_Connection* svrcon,double g_exposure,int svrchan, int)
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

void  SEM_camera_server::play()
{
  if (_fileCon) {
    _fileCon->set_replay_rate(1.0);
    _paused = false;
  } else {
    fprintf(stderr, "SEM_camera_server: Cannot play a non-file connection.\n");
  }
}

void  SEM_camera_server::pause()
{
  if (_fileCon) {
    _fileCon->set_replay_rate(0.0);
    _paused = true;
  } else {
    fprintf(stderr, "SEM_camera_server: Cannot pause a non-file connection.\n");
  }
}

void  SEM_camera_server::rewind()
{
  int i;
  if (_fileCon) {
    pause();
    _fileCon->reset();

    // Try to read the first frame from the SEM.  First,
    // speed up the replay rate until we get done with the first frame.
    // If the reading times out, then retry the read several times.
    _fileCon->set_replay_rate(100.0);
    _paused = false;
    _pause_after_one_frame = true;
    for (i = 0; i < 3; i++) {
      if (read_one_frame(0, _num_columns-1, 0, _num_rows-1, 0)) { break; }
    }
    _pause_after_one_frame = false;
    pause();
    _justStepped = true;

  } else {
    fprintf(stderr, "SEM_camera_server: Cannot rewind a non-file connection.\n");
  }
}

void  SEM_camera_server::single_step()
{
  if (_fileCon) {
    // Try to read the next frame from the SEM.  First,
    // speed up the replay rate until we get done with the first frame.
    // If the reading times out, then we get no new image.
    _fileCon->set_replay_rate(100.0);
    _paused = false;
    _pause_after_one_frame = true;
    if (read_one_frame(0, _num_columns-1, 0, _num_rows-1, 0)) {
      _justStepped = true;
    }
    _pause_after_one_frame = false;
    pause();
  } else {
    fprintf(stderr, "SEM_camera_server: Cannot single-step a non-file connection.\n");
  }
}

// Write the texture, using a virtual method call appropriate to the particular
// camera type.  NOTE: At least the first time this function is called,
// we must write a complete texture, which may be larger than the actual bytes
// allocated for the image.  After the first time, and if we don't change the
// image size to be larger, we can use the subimage call to only write the
// pixels we have.
bool SEM_camera_server::write_to_opengl_texture(GLuint tex_id)
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
      fprintf(stderr,"SEM_camera_server::write_to_opengl_texture(): Out of memory allocating temporary buffer\n");
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
    FORMAT, TYPE, &((vrpn_uint16 *)_memory)[NUM_COMPONENTS * ( _minX + get_num_columns()*_minY )]);
  return true;
}
