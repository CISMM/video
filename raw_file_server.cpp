#include "raw_file_server.h"

#ifndef min
#define min(a,b) ( (a)<(b)?(a):(b) )
#endif

raw_file_server::raw_file_server(const char *filename, unsigned numX, unsigned numY, unsigned bitdepth,
                  unsigned channels, unsigned headersize, unsigned frameheadersize) :
d_buffer(NULL),
d_bits(bitdepth),
d_header_size(headersize),
d_frame_header_size(frameheadersize),
d_channels(channels),
d_infile(NULL)
{
  // In case we fail somewhere along the way
  _status = false;

  // Set the mode to pause in case something goes wrong.
  d_mode = PAUSE;

  // Check to make sure that the parameters are valid
  if ( (numX == 0) || (numY) == 0) {
    fprintf(stderr, "raw_file_server::raw_file_server: Invalid image size (%d,%d)\n", numX, numY);
    return;
  }
  _num_columns = numX;
  _num_rows = numY;

  if (bitdepth != 8) {
    fprintf(stderr, "raw_file_server::raw_file_server: Bit depth %d not yet supported", bitdepth);
    return;
  }
  if (channels != 1) {
    fprintf(stderr, "raw_file_server::raw_file_server: %d channels not yet supported", channels);
    return;
  }

  // Open the file whose name is passed in for binary reading.
#ifdef	_WIN32
  d_infile = fopen(filename, "rb");
#else
  d_infile = fopen(filename, "r");
#endif
  if (d_infile == NULL) {
    char  msg[1024];
    sprintf(msg, "raw_file_server::raw_file_server: Could not open file %s", filename);
    perror(msg);
    return;
  }

  // Allocate space to read a frame from the file
  if ( (d_buffer = new vrpn_uint8[get_num_columns() * get_num_rows()]) == NULL) {
    fprintf(stderr,"raw_file_server::raw_file_server: Out of memory\n");
    return;
  }

  // Get us to the beginning of the file.
  rewind();

  // Set it so that it will one frame when we start
  d_mode = SINGLE;

  // Everything opened okay.
  _minX = _minY = 0;
  _maxX = get_num_columns() - 1;
  _maxY = get_num_rows() - 1;
  _binning = 1;
  _status = true;
}

raw_file_server::~raw_file_server(void)
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

void  raw_file_server::play()
{
  d_mode = PLAY;
}

void  raw_file_server::pause()
{
  d_mode = PAUSE;
}

void  raw_file_server::rewind()
{
  // Seek to the beginning of the file.
  if (d_infile != NULL) {
    fseek(d_infile, 0, SEEK_SET);
  }

  // If we've been asked to skip a header, do so now.
  if (d_header_size> 0) {
    if (fread(d_buffer, d_header_size, 1, d_infile) != 1) {
      char  msg[1024];
      sprintf(msg, "raw_file_server::rewind: Could not skip file header");
      perror(msg);
      return;
    }
  }

  // Read one frame when we start
  d_mode = SINGLE;
}

void  raw_file_server::single_step()
{
  d_mode = SINGLE;
}

bool  raw_file_server::read_image_to_memory(unsigned minX, unsigned maxX,
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
  if (d_frame_header_size > 0) { // Skip any extra padding before images
    if (fread(d_buffer, d_frame_header_size, 1, d_infile) != 1) {
      d_mode = PAUSE;
      return false;
    }
  }
  if (fread(d_buffer, get_num_columns() * get_num_rows(), 1, d_infile) != 1) {
    d_mode = PAUSE;
    return false;
  }

  // Okay, we got a new frame!
  return true;
}

/// Get pixels out of the memory buffer
bool  raw_file_server::get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint8 &val, int /* ignore color */) const
{
  // Make sure we are within the range of allowed pixels
  if ( (X < _minX) || (Y < _minY) || (X > _maxX) || (Y > _maxY) ) {
    return false;
  }

  // Fill in the pixel value, assuming pixels vary in X fastest in the file.
  // Invert Y so that the image shown in the spot tracker program matches the
  // images shown in the capture program.
  val = d_buffer[ X + (_maxY - Y) * get_num_columns() ];
  return true;
}

bool  raw_file_server::get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint16 &val, int /* ignore color */) const
{
  // Make sure we are within the range of allowed pixels
  if ( (X < _minX) || (Y < _minY) || (X > _maxX) || (Y > _maxY) ) {
    return false;
  }

  // Fill in the pixel value, assuming pixels vary in X fastest in the file.
  val = d_buffer[ X + (_maxY - Y) * get_num_columns() ];
  return true;
}

/// Store the memory image to a PPM file.
bool  raw_file_server::write_memory_to_ppm_file(const char *filename, int gain, bool sixteen_bits) const
{
  //XXX;
  fprintf(stderr,"raw_file_server::write_memory_to_ppm_file(): Not yet implemented\n");
  return false;

  return true;
}

/// Send whole image over a vrpn connection
bool  raw_file_server::send_vrpn_image(vrpn_Imager_Server* svr,vrpn_Connection* svrcon,double g_exposure,int svrchan, int)
{
    // Send the current frame over to the client in chunks as big as possible (limited by vrpn_IMAGER_MAX_REGION)
    unsigned  num_x = get_num_columns();
    unsigned  num_y = get_num_rows();
    int nRowsPerRegion=vrpn_IMAGER_MAX_REGIONu8/num_x;
    unsigned y;
    svr->send_begin_frame(0, num_x-1, 0, num_y-1);
    for(y=0;y<num_y;y=min(num_y,y+nRowsPerRegion)) {
      svr->send_region_using_base_pointer(svrchan,0,num_x-1,y,min(num_y,y+nRowsPerRegion)-1,
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
bool raw_file_server::write_to_opengl_texture(GLuint tex_id)
{
  const GLint   NUM_COMPONENTS = 1;
  const GLenum  FORMAT = GL_LUMINANCE;
  const GLenum  TYPE = GL_UNSIGNED_BYTE;
  const void*   BASE_BUFFER = d_buffer;
  const void*   SUBSET_BUFFER = &d_buffer[NUM_COMPONENTS * ( _minX + get_num_columns()*_minY )];
  return write_to_opengl_texture_generic(tex_id, NUM_COMPONENTS, FORMAT, TYPE,
    BASE_BUFFER, SUBSET_BUFFER, _minX, _minY, _maxX, _maxY);
}

bool raw_file_server::write_opengl_texture_to_quad(double xfrac, double yfrac)
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
