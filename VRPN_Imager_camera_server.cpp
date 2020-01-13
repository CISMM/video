/* XXX It is monochrome on stored video, overwriting all with the blue channel */
/* XXX It seems to be herky-jerky on stored video */
/* XXX What do to about RGB? */
/* XXX We may we want to do source-quench on live cameras. */
/* XXX Does not send the commands to reduce the region of interest */

#include <stdlib.h>
#include <stdio.h>
#include "VRPN_Imager_camera_server.h"
#ifndef __min
#define __min(a,b)  (((a) < (b)) ? (a) : (b))
#endif

//#define	DEBUG

static	unsigned long	duration(struct timeval t1, struct timeval t2)
{
	return (t1.tv_usec - t2.tv_usec) +
	       1000000L * (t1.tv_sec - t2.tv_sec);
}

void VRPN_Imager_camera_server::handle_description_message(void * userdata, const struct timeval msg_time)
{
  VRPN_Imager_camera_server *me = static_cast<VRPN_Imager_camera_server *>(userdata);

  //---------------------------------------------------------------------
  // If we've already gotten a description message, and the resolution is
  // the same, then we don't do anything -- this keeps us from clearing the
  // back buffer when we get an new region description.
  if ( (me->_gotResolution) &&
	(me->_num_rows == me->_imager->nRows()) &&
	(me->_num_columns == me->_imager->nCols()) ) {
    return;
  }

  me->_num_rows = me->_imager->nRows();
  me->_num_columns = me->_imager->nCols();
  me->_minX = 0;
  me->_maxX = me->_num_columns - 1;
  me->_minY = 0;
  me->_maxY = me->_num_rows - 1;

  //---------------------------------------------------------------------
  // Delete the old buffer, if there was one
  if (me->_front != NULL) { delete [] me->_front; }
  if (me->_back != NULL) { delete [] me->_back; }

  //---------------------------------------------------------------------
  // Allocate a buffer that is large enough to read the maximum-sized
  // image with no binning.  Clear the back buffer.
  me->_buflen = (vrpn_uint32)(me->_num_rows* me->_num_columns);	// Two bytes per pixel, but we're allocating 16-bit values
  if ( (me->_front = new vrpn_uint16[me->_buflen]) == NULL) {
    fprintf(stderr, "VRPN_Imager_camera_server::handle_description_message(): Cannot allocate memory buffer\n");
    me->_status = false;
    return;
  }
  if ( (me->_back = new vrpn_uint16[me->_buflen]) == NULL) {
    fprintf(stderr, "VRPN_Imager_camera_server::handle_description_message(): Cannot allocate memory buffer\n");
    me->_status = false;
    return;
  }
  memset(me->_back, 0, me->_buflen * 2);

  // We've now heard the resolution from the server.
  me->_gotResolution = true;
}

void VRPN_Imager_camera_server::handle_region_message(void * userdata, const vrpn_IMAGERREGIONCB info)
{
  VRPN_Imager_camera_server *me = static_cast<VRPN_Imager_camera_server *>(userdata);

  if (!info.region->decode_unscaled_region_using_base_pointer(me->_back, 1, me->_num_columns,
       0, me->_num_rows, true)) {
    fprintf(stderr, "VRPN_Imager_camera_server::handle_region_message(): Cannot decode region\n");
  }
}

void VRPN_Imager_camera_server::handle_end_frame_message(void * userdata, const vrpn_IMAGERENDFRAMECB info)
{
  VRPN_Imager_camera_server *me = static_cast<VRPN_Imager_camera_server *>(userdata);

  // Swap front and back buffers and increment frame count.
  vrpn_uint16 *temp = me->_front;
  me->_front = me->_back;
  me->_back = temp;
  me->_frameNum++;

  // If we're clearing each new frame, clear the back buffer.  Otherwise, copy the front
  // buffer into the back buffer so that we are keeping all pixels the same that are
  // not overwritten.
  if (me->_clear_new_frames) {
    memset(me->_back, 0, me->_buflen * 2);
  } else {
    memcpy(me->_back, me->_front, me->_buflen * 2);
  }


  if (me->_pause_after_one_frame) { 
    // To avoid skipping past the message we just read, find out the current
    // message time and set the time on the file to it.  This should stop us
    // "on a dime".  Then pause to set the replay rate to zero so we stay
    // put.
    if (me->_fileCon) {
      me->_fileCon->jump_to_filetime(info.msg_time);
    }
    me->pause();
  }
}

//-----------------------------------------------------------------------
// The min and max coordinates specified here should be without regard to
// binning.  That is, they should be in the full-resolution device coordinates.

bool  VRPN_Imager_camera_server::read_one_frame(unsigned short minX, unsigned short maxX,
				     unsigned short minY, unsigned short maxY,
				     unsigned exposure_time_millisecs)
{
  // Make sure the frame we've been asked to get is within the bounds of
  // what we are going to get, then go get a full-screen image.
  if ( (maxX > _num_columns-1) || (maxY > _num_rows-1) ) {
    fprintf(stderr, "VRPN_Imager_camera_server::read_one_frame(): Invalid frame size\n");
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
  // XXX Modify for live camera feed.
  if (_paused) {
    return false;
  }

  // Wait until we either get a new complete image or else time out waiting for one.
  // For the Imager server, timeout after two seconds rather than a tenth of a second.
  // This is because some of our our optical cameras have much longer integration times.
  struct timeval start, now;
  vrpn_gettimeofday(&start, NULL);
  int lastFrameNum = _frameNum;
  while (lastFrameNum == _frameNum) {
    _imager->mainloop();
    vrpn_gettimeofday(&now, NULL);
    if (duration(now, start) > 2000000L) {
      return false;
    }
    // If we do the following, then it doesn't go fast enough on some machines because
    // it sleeps in between each message (the fileconnection is now set to only do one
    // message per mainloop).
    //vrpn_SleepMsecs(0);	// Avoid eating the whole CPU
  }
  int frames_moved = _frameNum - lastFrameNum;
  if ( _fileCon && (frames_moved != 1) ) {
    fprintf(stderr, "VRPN_Imager_camera_server::read_one_frame(): Skipped %d frames\n", frames_moved-1);
  }
  return true;
}

//---------------------------------------------------------------------
// Open the camera and determine its available features and parameters.
// This requires waiting until a message has been received from the
// server so that we know what the parameters are for the "camera."

bool VRPN_Imager_camera_server::open_and_find_parameters(const char *name)
{
  _imager = new vrpn_Imager_Remote(name);

  // Register the change handler and call mainloop() until we hear
  // from the server or we timeout.
  _imager->register_description_handler(this, handle_description_message);
  _imager->register_region_handler(this, handle_region_message);
  _imager->register_end_frame_handler(this, handle_end_frame_message);
  _gotResolution = false;
  struct timeval start, now;
  _status = true;
  _frameNum = -1;
  _imager->mainloop();	  // Do one mainloop to pull in the file from disk
  vrpn_gettimeofday(&start, NULL);
  while (!_gotResolution) {
    _imager->mainloop();
    vrpn_gettimeofday(&now, NULL);
    if (duration(now, start) > 10000000L) {
      fprintf(stderr,"VRPN_Imager_camera_server::open_and_find_parameters(): Timeout trying to read resolution\n");
      return false;
    }
  }

  // Try to get the file interface from the connection for this device.  If
  // we can, then we can do play, rewind, pause, and other operations on it.
  _fileCon = _imager->connectionPtr()->get_File_Connection();

  // Set the file connection to only play back one message for each call to
  // mainloop(), so that we don't skip past frames.
  if (_fileCon) {
    _fileCon->limit_messages_played_back(1);
  }

  // Try to read the first frame from the imager.  If we have a file controller,
  // then speed up the replay rate until we get done with the first frame.
  // If the reading times out, try again a bunch of times.  If we end up waiting
  // more than ten seconds total, then we're really timed out.
  if (_fileCon) {
    _fileCon->reset();  // Make sure we don't skip frames at the beginning.
    _fileCon->set_replay_rate(100.0);
    _paused = false;
    _pause_after_one_frame = true;
  }
  vrpn_gettimeofday(&start, NULL);
  while (true) {
    if (read_one_frame(0, _num_columns-1, 0, _num_rows-1, 0)) { break; }
    vrpn_gettimeofday(&now, NULL);
    if (duration(now, start) > 10000000L) {
      fprintf(stderr,"VRPN_Imager_camera_server::open_and_find_parameters(): Timeout trying to read first frame\n");
      return false;
    }
  }
  if (_fileCon) {
    _fileCon->set_replay_rate(0.0);
    _pause_after_one_frame = false;
  }

  return _status;
}

VRPN_Imager_camera_server::VRPN_Imager_camera_server(const char *name, bool clear_new_frames) :
  base_camera_server(1),
  _fileCon(NULL),
  _imager(NULL),
  _front(NULL),
  _back(NULL),
  _frameNum(-1),
  _justStepped(false),
  _pause_after_one_frame(false),
  _paused(false),
  _clear_new_frames(clear_new_frames)
{
  //---------------------------------------------------------------------
  // Open the Imager and find out what its capabilities are.
  if ( (_status = open_and_find_parameters(name)) == false) {
    fprintf(stderr, "VRPN_Imager_camera_server::VRPN_Imager_camera_server(): Cannot open imager\n");
    return;
  }

  //---------------------------------------------------------------------
  // The above had the side-effect of loading an image.  Record the
  // fact that we have an image so the read routine doesn't read
  // a second one.
  _justStepped = true;
}

//---------------------------------------------------------------------
// Close the imager.  Free up memory.

VRPN_Imager_camera_server::~VRPN_Imager_camera_server(void)
{
  if (_front) { delete [] _front; }
  if (_back) { delete [] _back; }
  if (_imager != NULL) { delete _imager; }
}

// The min/max coordinates here are in post-binned space.  That is, the maximum should be
// the edge of the image produced, taking into account the binning of the full resolution.

bool  VRPN_Imager_camera_server::read_image_to_memory(unsigned minX, unsigned maxX, unsigned minY, unsigned maxY,
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
  printf("VRPN_Imager_camera_server::read_image_to_memory(): Setting window from (%d,%d) to (%d,%d)\n",
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
    fprintf(stderr,"VRPN_Imager_camera_server::read_image_to_memory(): Clipping maxX to %d from %d\n",_num_columns, _maxX);
    _maxX = _num_columns - 1;
  };
  if (_maxY >= _num_rows) {
    fprintf(stderr,"VRPN_Imager_camera_server::read_image_to_memory(): Clipping maxY\n");
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
bool  VRPN_Imager_camera_server::write_memory_to_ppm_file(const char *filename, int gain, bool sixteen_bits) const
{
  //---------------------------------------------------------------------
  // Make sure the region is non-zero (so we've read an image)
  if ( (_maxX <= _minX) || (_maxY <= _minY) ) {
    fprintf(stderr,"VRPN_Imager_camera_server::write_memory_to_ppm_file(): No image in memory\n");
    return false;
  }

  //---------------------------------------------------------------------
  // If we are not doing 16 bits, map the 16 bits to the range 0-255, and then write out an
  // uncompressed 8-bit grayscale PPM file with the values scaled to this range.
  if (!sixteen_bits) {
    vrpn_uint16	  *vals = (vrpn_uint16 *)_front;
    unsigned char *pixels;
    // This buffer will be oversized if min and max don't span the whole window.
    if ( (pixels = new unsigned char[_buflen/2]) == NULL) {
      fprintf(stderr, "VRPN_Imager_camera_server::write_memory_to_ppm_file(): Can't allocate memory for stored image\n");
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
	pixels[r*cols + c] = clamp_gain(vals[r*cols+c] - offset, scale, 4095) >> 8;
      }
    }
    FILE *of = fopen(filename, "wb");
    fprintf(of, "P5\n%d %d\n%d\n", cols, rows, 255);
    fwrite(pixels, 1, cols*rows, of);
    fclose(of);
    delete [] pixels;

  // If we are doing 16 bits, write out a 16-bit file.
  } else {
    vrpn_uint16 *vals = (vrpn_uint16 *)_front;
    unsigned r,c;
    vrpn_uint16 *pixels;
    // This buffer will be oversized if min and max don't span the whole window.
    if ( (pixels = new vrpn_uint16[_buflen]) == NULL) {
      fprintf(stderr, "VRPN_Imager_camera_server::write_memory_to_ppm_file(): Can't allocate memory for stored image\n");
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
// Map the 16 bits to the range 0-255, and return the result
bool	VRPN_Imager_camera_server::get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint8 &val, int RGB) const
{
  if ( (_maxX <= _minX) || (_maxY <= _minY) ) {
    fprintf(stderr,"VRPN_Imager_camera_server::get_pixel_from_memory(): No image in memory\n");
    return false;
  }
  if (RGB != 0) {
    fprintf(stderr,"VRPN_Imager_camera_server::get_pixel_from_memory(): Can't select other than 0th color\n");
    return false;
  }
  if ( (X < _minX/_binning) || (X > _maxX/_binning) || (Y < _minY/_binning) || (Y > _maxY/_binning) ) {
    return false;
  }
  vrpn_uint16	*vals = (vrpn_uint16 *)_front;
  vrpn_uint16	cols = (_maxX - _minX + 1)/_binning;  // Don't use num_columns here; depends on the region size.
  vrpn_uint32	index = (Y-_minY/_binning)*cols + (X-_minX/_binning);
  val = (vrpn_uint8)(vals[index] >> 8);
  return true;
}

bool	VRPN_Imager_camera_server::get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint16 &val, int RGB) const
{
  if ( (_maxX <= _minX) || (_maxY <= _minY) ) {
    fprintf(stderr,"VRPN_Imager_camera_server::get_pixel_from_memory(): No image in memory\n");
    return false;
  }
  if (RGB != 0) {
    fprintf(stderr,"VRPN_Imager_camera_server::get_pixel_from_memory(): Can't select other than 0th color\n");
    return false;
  }
  if ( (X < _minX/_binning) || (X > _maxX/_binning) || (Y < _minY/_binning) || (Y > _maxY/_binning) ) {
    return false;
  }
  vrpn_uint16	*vals = (vrpn_uint16 *)_front;
  vrpn_uint16	cols = (_maxX - _minX + 1)/_binning;  // Don't use num_columns here; depends on the region size.
  vrpn_uint32	index = (Y-_minY/_binning)*cols + (X-_minX/_binning);
  val = vals[index];
  return true;
}

// XXX This routine needs to be tested.
bool VRPN_Imager_camera_server::send_vrpn_image(vrpn_Imager_Server* svr,vrpn_Connection* svrcon,double g_exposure,int svrchan, int)
{
    // Send the current frame over to the client in chunks as big as possible (limited by vrpn_IMAGER_MAX_REGION)
    unsigned  num_x = get_num_columns();
    unsigned  num_y = get_num_rows();
    int nRowsPerRegion=vrpn_IMAGER_MAX_REGIONu16/num_x;
    unsigned y;
    svr->send_begin_frame(0, num_x-1, 0, num_y-1);
    for(y=0;y<num_y;y=__min(num_y,y+nRowsPerRegion)) {
      svr->send_region_using_base_pointer(svrchan,0,num_x-1,y,__min(num_y,y+nRowsPerRegion)-1,
	(vrpn_uint16 *)_front, 1, get_num_columns());
      svr->mainloop();
    }
    svr->send_end_frame(0, num_x-1, 0, num_y-1);
    svr->mainloop();

    // Mainloop the server connection (once per server mainloop, not once per object).
    svrcon->mainloop();
    return true;
}

void  VRPN_Imager_camera_server::play()
{
  if (_fileCon) {
    _fileCon->set_replay_rate(100.0);   // Replay the video as quickly as possible.
    _paused = false;
  } else {
    fprintf(stderr, "VRPN_Imager_camera_server: Cannot play a non-file connection.\n");
  }
}

void  VRPN_Imager_camera_server::pause()
{
  if (_fileCon) {
    _fileCon->set_replay_rate(0.0);
    _paused = true;
  } else {
    fprintf(stderr, "VRPN_Imager_camera_server: Cannot pause a non-file connection.\n");
  }
}

void  VRPN_Imager_camera_server::rewind()
{
  int i;
  if (_fileCon) {
    pause();
    _fileCon->reset();

    // Try to read the first frame from the imager.  First,
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
    fprintf(stderr, "VRPN_Imager_camera_server: Cannot rewind a non-file connection.\n");
  }
}

void  VRPN_Imager_camera_server::single_step()
{
  if (_fileCon) {
    // Try to read the next frame from the imager.  First,
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
    fprintf(stderr, "VRPN_Imager_camera_server: Cannot single-step a non-file connection.\n");
  }
}

// Write the texture, using a virtual method call appropriate to the particular
// camera type.  NOTE: At least the first time this function is called,
// we must write a complete texture, which may be larger than the actual bytes
// allocated for the image.  After the first time, and if we don't change the
// image size to be larger, we can use the subimage call to only write the
// pixels we have.
bool VRPN_Imager_camera_server::write_to_opengl_texture(GLuint tex_id)
{
  // Note: Check the GLubyte or GLushort or whatever in the temporary buffer!
  const GLint   NUM_COMPONENTS = 1;
  const GLenum  FORMAT = GL_LUMINANCE;
  const GLenum  TYPE = GL_UNSIGNED_SHORT;
  const void*   BASE_BUFFER = _front;
  const void*   SUBSET_BUFFER = &((vrpn_uint16 *)_front)[NUM_COMPONENTS * ( _minX + get_num_columns()*_minY )];

  return write_to_opengl_texture_generic(tex_id, NUM_COMPONENTS, FORMAT, TYPE,
    BASE_BUFFER, SUBSET_BUFFER, _minX, _minY, _maxX, _maxY);
}
