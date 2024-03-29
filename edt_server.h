#ifndef	EDT_SERVER_H
#define	EDT_SERVER_H

// On 5/20/2011, built against version 4.1.9.6 of the library.  It linked fine.

#include "base_camera_server.h"
#include "raw_file_server.h"

#ifdef _WIN32
class edt_server : public base_camera_server {
public:
  // Version 3303 of the library would flip every other line
  // improperly on our Pulnix cameras, so I had this swap_lines
  // entry, and it defaulted to true.  Version 3335 of the
  // drivers fixes this, so the default is now false.
  edt_server(bool swap_lines = false, unsigned num_buffers = 10);
  virtual ~edt_server(void);

  /// Read an image to a memory buffer.  Exposure time is in milliseconds.
  virtual bool	read_image_to_memory(unsigned minX = 0, unsigned maxX = 0,
			     unsigned minY = 0, unsigned maxY = 0,
			     double exposure_time_millisecs = 0.0);

  /// Get pixels out of the memory buffer
  virtual bool	get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint8 &val, int RGB = 0) const;
  virtual bool	get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint16 &val, int RGB = 0) const;

  /// How many colors are in the image.
  virtual unsigned  get_num_colors() const { return 1; }

  /// Send in-memory image over a vrpn connection
  virtual bool  send_vrpn_image(vrpn_Imager_Server* svr,
    vrpn_Connection* svrcon,double g_exposure,int svrchan, int num_chans = 1);

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
  virtual bool write_opengl_texture_to_quad();

protected:
  vrpn_uint8  *d_buffer;	  //< Points to current frame of data
  vrpn_uint8  *d_swap_buffer;	  //< Holds a line at a time during swapping, if we swap lines
  void	      *d_pdv_p;		  //< Holds a pointer to the video device.
  bool	      d_swap_lines;	  //< Do we swap every other line in video (workaround for EDT bug in our system)
  unsigned    d_num_buffers;	  //< How many EDT-managed buffers to allocate in ring?
  unsigned    d_started;	  //< How many acquisitions started?

  unsigned    d_last_timeouts;	  //< How many timeouts last time we read?
  unsigned    d_first_timeouts;	  //< How many timeouts when we opened the device?
  unsigned    d_unreported_timeouts;  //< How many timeouts do we have that VRPN doesn't know about?
  bool        d_missed_some_images; //< We've missed an unknown number of images due to buffers filling up

  struct timeval d_pc_time_first_image;   //< PC clock's time at first image acquisition
  struct timeval d_edt_time_first_image;  //< EDT time of first image acquisition
  struct timeval d_timestamp;     //< When (in PC clock) the most recent image got to DMA

  virtual bool open_and_find_parameters(void);
};
#endif

class edt_pulnix_raw_file_server : public raw_file_server {
public:
  edt_pulnix_raw_file_server(const char *filename) : raw_file_server(filename, 648, 484, 8, 1, 0, 0) {};

protected:
};
#endif
