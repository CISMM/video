#ifndef	RAW_FILE_SERVER_H
#define	RAW_FILE_SERVER_H

#include "base_camera_server.h"

class raw_file_server : public base_camera_server {
public:
  // (numX, numY) = image size in pixels.
  // bitdepth is the number of bits per pixel (8 for 1-byte images, 16 for 2-byte images)
  // channels = number of color channels (currently, only "1" is supported).
  // headersize = the number of bytes to skip at the beginning of the file
  // frameheadersize = the number of bytes to skip at the beginning of each image frame
  // EDT/Pulnix has numX = 648, numY = 484, bitdepth = 8, headersize = 0, frameheadersize = 0;
  // New Pulnix camera as numX = 640, numY = 480, bitdepth = 8, headersize = 0, frameheadersize = 0;
  // Point Grey raw format has numX = 1024, numY = 768, bitdepth = 8, headersize = 0, frameheadersize = 112;
  raw_file_server(const char *filename, unsigned numX = 648, unsigned numY = 484,
                  unsigned bitdepth = 8, unsigned channels = 1,
                  unsigned headersize = 0, unsigned frameheadersize = 0);
  virtual ~raw_file_server(void);

  /// Start the stored video playing.
  virtual void play(void);

  /// Pause the stored video
  virtual void pause(void);

  /// Rewind the stored video to the beginning (also pauses).
  virtual void rewind(void);

  /// Single-step the stored video for one frame.
  virtual void single_step();

  /// Read an image to a memory buffer.  Exposure time is in milliseconds
  virtual bool	read_image_to_memory(unsigned minX = 0, unsigned maxX = 0,
			     unsigned minY = 0, unsigned maxY = 0,
			     double exposure_time_millisecs = 0.0);

  /// Get pixels out of the memory buffer
  virtual bool	get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint8 &val, int RGB = 0) const;
  virtual bool	get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint16 &val, int RGB = 0) const;

  /// How many colors are in the image.
  virtual unsigned  get_num_colors() const { return 1; }

  /// Store the memory image to a PPM file.
  virtual bool  write_memory_to_ppm_file(const char *filename, int gain = 1, bool sixteen_bits = false) const;

  /// Send in-memory image over a vrpn connection
  virtual bool  send_vrpn_image(vrpn_Imager_Server* svr,vrpn_Connection* svrcon,double g_exposure,int svrchan, int num_chans = 1);

protected:
  FILE *d_infile;		  //< File to read from
  // _num_rows and _num_columns come from base_camera_server base class
  unsigned  d_header_size;        //< Number of bytes to skip at file head
  unsigned  d_frame_header_size;  //< Number of bytes to skip at the start of each frame
  unsigned  d_bits;               //< Number of bits per pixel
  unsigned  d_channels;           //< Number of channels
  vrpn_uint8  *d_buffer;	  //< Holds one frame of data from the file
  enum {PAUSE, PLAY, SINGLE} d_mode;	  //< What we're doing right now

  // Write the texture, using a virtual method call appropriate to the particular
  // camera type.  NOTE: At least the first time this function is called,
  // we must write a complete texture, which may be larger than the actual bytes
  // allocated for the image.  After the first time, and if we don't change the
  // image size to be larger, we can use the subimage call to only write the
  // pixels we have.
  virtual bool write_to_opengl_texture(GLuint tex_id);

  // Write from the texture to a quad.  Write only the actually-filled
  // portion of the texture (parameters passed in).  This version does not
  // flip the quad over.  The EDT version flips the image over, so we
  // don't use the base-class method.
  virtual bool write_opengl_texture_to_quad(double xfrac, double yfrac);
};
#endif
