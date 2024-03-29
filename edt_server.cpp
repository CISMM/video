#include "edt_server.h"

#ifndef min
#define min(a,b) ( (a)<(b)?(a):(b) )
#endif

#ifdef _WIN32
#include "edtinc.h"

edt_server::edt_server(bool swap_lines, unsigned num_buffers) :
d_buffer(NULL),
d_swap_buffer(NULL),
d_pdv_p(NULL),
d_swap_lines(swap_lines),
d_num_buffers(num_buffers),
d_unreported_timeouts(0),
d_missed_some_images(false)
{
  // In case we fail somewhere along the way
  _status = false;

  // No first image time yet
  d_pc_time_first_image.tv_sec = 0;
  d_pc_time_first_image.tv_usec = 0;

  // Open the device and find out what we need to know.
  _status = open_and_find_parameters();
}

edt_server::~edt_server()
{
  if (d_pdv_p) {
    pdv_close((PdvDev*)d_pdv_p);
    d_pdv_p = NULL;
  }
  if (d_swap_buffer) {
    delete [] d_swap_buffer;
    d_swap_buffer = NULL;
  }
}

bool edt_server::open_and_find_parameters(void)
{
  char    errstr[256];
  char    *devname = EDT_INTERFACE;
  int	  unit = 0;
  int	  channel = 0;

  // Try to open the default device name and unit
  d_pdv_p = pdv_open_channel(devname, unit, channel);
  if (d_pdv_p == NULL) {
    fprintf(stderr,"edt_server::open_and_find_parameters: Could not open camera\n");
    sprintf(errstr, "pdv_open_channel(%s%d_%d)", devname, unit, channel);
    pdv_perror(errstr);
    return false;
  }

  // Check the parameters.
  if (pdv_get_depth((PdvDev*)d_pdv_p) != 8) {
    fprintf(stderr,"edt_server::open_and_find_parameters: Can only handle 8-bit cameras\n");
    pdv_close((PdvDev*)d_pdv_p);
    d_pdv_p = NULL;
    return false;
  }
  _num_columns = pdv_get_width((PdvDev*)d_pdv_p);
  _num_rows = pdv_get_height((PdvDev*)d_pdv_p);
  _minX = _minY = 0;
  _maxX = _num_columns-1;
  _maxY = _num_rows-1;
  _binning = 1;

  // Allocate space to swap a line from the buffer
  if ( (d_swap_buffer = new vrpn_uint8[_num_columns]) == NULL) {
    fprintf(stderr,"edt_server::open_and_find_parameters: Out of memory\n");
    pdv_close((PdvDev*)d_pdv_p);
    d_pdv_p = NULL;
    return false;
  }

  /*
   * allocate buffers for optimal pdv ring buffer pipeline (reduce if
   * memory is at a premium)
   */
  pdv_multibuf((PdvDev*)d_pdv_p, d_num_buffers);

  // Clear the timeouts value.
  d_first_timeouts = d_last_timeouts = pdv_timeouts((PdvDev*)d_pdv_p);

  /*
   * prestart the first image or images outside the loop to get the
   * pipeline going. Start multiple images unless force_single set in
   * config file, since some cameras (e.g. ones that need a gap between
   * images or that take a serial command to start every image) don't
   * tolerate queueing of multiple images
   */
  if (((PdvDev*)d_pdv_p)->dd_p->force_single) {
      d_started = 1;
      pdv_start_image((PdvDev*)d_pdv_p);
  } else {
      // Request as many images as buffers, so we are always trying to
      // fill all of them.  If we have gotten all we asked for, we
      // may have missed some.
      d_started = d_num_buffers;
      pdv_start_images((PdvDev*)d_pdv_p, d_started);
  }

  // Read one frame when we start.
  _status = true;
  if (read_image_to_memory()) {
    return true;
  } else {
    pdv_close((PdvDev*)d_pdv_p);
    d_pdv_p = NULL;
    fprintf(stderr, "edt_server::open_and_find_parameters: could not read image to memory\n");
    return false;
  }
}

bool  edt_server::read_image_to_memory(unsigned minX, unsigned maxX,
							unsigned minY, unsigned maxY,
							double exposure_time_millisecs)
{
  u_char *image_p;
  struct timeval now;

  if (!_status) { return false; }

  //---------------------------------------------------------------------
  // XXX Set the exposure time.

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

  // If the in-memory buffers have all been filled up, assume
  // that we have missed some unknown number of images.  Save this
  // so that it can be reported if we're sending VRPN messages.
  // Actually, we report this if we drop down to 1 buffer because
  // this was where the version running on one of our computers
  // hit the floor at and started losing frames.
  unsigned outstanding = edt_get_todo((PdvDev*)d_pdv_p) -
                         edt_done_count((PdvDev*)d_pdv_p);
  if ( outstanding <= 1 ) {
    d_missed_some_images = true;
  }

  // Once a second, tell how many buffers have been filled and are waiting
  // for us to process.
  static struct timeval last = { 0, 0 };
  vrpn_gettimeofday(&now, NULL);
  if (now.tv_sec > last.tv_sec) {
    last = now;
    //printf("XXX EDT: %d outstanding buffers\n", outstanding );
  }

  /*
   * get the image and immediately start the next one. Processing
   * can then occur in parallel with the next acquisition
   */
  unsigned int sec_usec[2];
  image_p = pdv_wait_image_timed((PdvDev*)d_pdv_p, sec_usec);
  if (image_p == NULL) {
    fprintf(stderr,"edt_server::read_image_to_memory(): Failed to read image\n");
    pdv_close((PdvDev*)d_pdv_p);
    d_pdv_p = NULL;
    _status = false;
    return false;
  }
  pdv_start_image((PdvDev*)d_pdv_p);
  /*XXX
  if (edt_reg_read((PdvDev*)d_pdv_p, PDV_STAT) & PDV_OVERRUN) {
    XXX_overrun_check; // http://www.edt.com/api/simple__take_8c-source.html
  }
  */

  // XXX We might think that we wanted to use the wait_images functions
  // rather than wait_image, but we do seem to fill up all of the buffers
  // when we can't send data out fast enough.  But it may be worth trying
  // to use it and ask for one at a time, then only start more reads when
  // half of them have been read or something?  And do this using a commend
  // to start multiple reads?  Does it re-start every time we call start?
  // If so, what's the point of multiple buffers in the simple_take.c file?
  // Google search for pdv_wait_image_timed turned up pointers to their docs.

  //---------------------------------------------------------------------
  // Time handling: We let the EDT board tell us what time each image
  // was put into the DMA buffer.  We don't know how this compares to
  // the absolute PC clock time, so we record the offset from the first
  // time we got an image (clock reading and EDT reading), so that the
  // time will be reported relative to the computer's clock.
  // If we don't have a nonzero PC time, this is the first time through,
  // so get both initial times.
  struct timeval edt_now = { sec_usec[0], sec_usec[1] };
  if ( d_pc_time_first_image.tv_sec == 0 ) {
    vrpn_gettimeofday(&d_pc_time_first_image, NULL);
    d_edt_time_first_image = edt_now;
  }
  struct timeval time_offset = vrpn_TimevalDiff(d_pc_time_first_image, d_edt_time_first_image);
  d_timestamp = vrpn_TimevalSum( edt_now, time_offset );

  // XXX The EDT-board time handling seems to be broken, so for now we just
  // put on the current time for the frame.
  vrpn_gettimeofday(&now, NULL);
  d_timestamp = now;

  // Check for timeouts in image transer from the camera into a memory
  // buffer.  This does NOT tell us when we ask for more images than will
  // fit into buffers, but rather when there was a communication error or
  // the system bus was too slow to pull the image off the camera.  We'll
  // need to look at timestamps to determine whether we missed any images
  // due to not having room in the buffers we allocated.

  unsigned timeouts = pdv_timeouts((PdvDev*)d_pdv_p);
  if (timeouts > d_last_timeouts)
  {
      /*
       * pdv_timeout_cleanup helps recover gracefully after a timeout,
       * particularly if multiple buffers were prestarted
       */
      if (d_num_buffers > 1)
      {
	  int     pending = pdv_timeout_cleanup((PdvDev*)d_pdv_p);
	  pdv_start_images((PdvDev*)d_pdv_p, pending);
      }
      d_unreported_timeouts += (timeouts - d_last_timeouts);
      d_last_timeouts = timeouts;
  }

  // If we're supposed to swap every other line, do that here.
  // The image lines are swapped in place.
  if (d_swap_lines) {
    unsigned j;    // Indexes into the lines, skipping every other.

    for (j = 0; j < _num_rows; j += 2) {
      memcpy(d_swap_buffer, image_p + j*_num_columns, _num_columns);
      memcpy(image_p + (j+1)*_num_columns, image_p + j*_num_columns, _num_columns);
      memcpy(image_p + (j+1)*_num_columns, d_swap_buffer, _num_columns);
    }
  }

  // Point to the image in memory.
  d_buffer = image_p;

  return true;
}

/// Get pixels out of the memory buffer
bool  edt_server::get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint8 &val, int /* ignore color */) const
{
  // Make sure we have a valid, open device
  if (!_status) { return false; };

  // Make sure we are within the range of allowed pixels
  if ( (X < _minX) || (Y < _minY) || (X > _maxX) || (Y > _maxY) ) {
    return false;
  }

  // Fill in the pixel value, assuming pixels vary in X fastest in the file.
  // Invert Y so that the image shown in the spot tracker program matches the
  // images shown in the capture program.
  val = d_buffer[ X + ((_num_rows-1) - Y) * _num_columns ];
  return true;
}

bool  edt_server::get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint16 &val, int /* ignore color */) const
{
  // Make sure we have a valid, open device
  if (!_status) { return false; };

  // Make sure we are within the range of allowed pixels
  if ( (X < _minX) || (Y < _minY) || (X > _maxX) || (Y > _maxY) ) {
    return false;
  }

  // Fill in the pixel value, assuming pixels vary in X fastest in the file.
  val = d_buffer[ X + ((_num_rows-1) - Y) * _num_columns ];
  return true;
}

/// Send in-memory image over a vrpn connection
bool  edt_server::send_vrpn_image(vrpn_Imager_Server* svr,vrpn_Connection* svrcon,double g_exposure,int svrchan, int)
{
  // Make sure we have a valid, open device
  if (!_status) { return false; };

  unsigned y;

  // If we have unreported timeouts, send the discarded_frame message
  if (d_unreported_timeouts) {
    svr->send_discarded_frames(d_unreported_timeouts);
    d_unreported_timeouts = 0;
  }

  // If we have missed some images, send the discarded_frame message
  // with a count of zero to indicate we don't know how many we missed.
  if (d_missed_some_images) {
    svr->send_discarded_frames(0);
    d_missed_some_images = false;
  }

  // Send the current frame over to the client in chunks as big as possible (limited by vrpn_IMAGER_MAX_REGION).
  int nRowsPerRegion=vrpn_IMAGER_MAX_REGIONu8/_num_columns;
  svr->send_begin_frame(0, _num_columns-1, 0, _num_rows-1, 0, 0, &d_timestamp);
  for(y=0; y<_num_rows; y+=nRowsPerRegion) {
    svr->send_region_using_base_pointer(svrchan,0,_num_columns-1,y,min(_num_rows,y+nRowsPerRegion)-1,
      d_buffer, 1, _num_columns, _num_rows, true, 0, 0, 0, &d_timestamp);
    svr->mainloop();
  }
  svr->send_end_frame(0, _num_columns-1, 0, _num_rows-1, 0, 0, &d_timestamp);
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
bool edt_server::write_to_opengl_texture(GLuint tex_id)
{
  // Note: Check the GLubyte or GLushort or whatever in the temporary buffer!
  const GLint   NUM_COMPONENTS = 1;
  const GLenum  FORMAT = GL_LUMINANCE;
  const GLenum  TYPE = GL_UNSIGNED_BYTE;
  const void*   BASE_BUFFER = d_buffer;
  const void*   SUBSET_BUFFER = &d_buffer[NUM_COMPONENTS * ( _minX + get_num_columns()*_minY )];

  return write_to_opengl_texture_generic(tex_id, NUM_COMPONENTS, FORMAT, TYPE,
    BASE_BUFFER, SUBSET_BUFFER, _minX, _minY, _maxX, _maxY);
}

bool edt_server::write_opengl_texture_to_quad()
{
  double xfrac = static_cast<double>(get_num_columns()) / _opengl_texture_size_x;
  double yfrac = static_cast<double>(get_num_rows()) / _opengl_texture_size_y;

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
#endif
