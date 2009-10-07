#pragma once

#include "base_camera_server.h"

#include "PGRCam.h"

class point_grey_server : public base_camera_server {
public:
  point_grey_server(double framerate = -1, double msExposure = -1);
  virtual ~point_grey_server(void);

  /// Read an image to a memory buffer.
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


protected:
  PGRCam* m_cam;
  

  struct timeval m_timestamp;     // timestamp of our most recent image

  // Write the texture, using a virtual method call appropriate to the particular
  // camera type.  NOTE: At least the first time this function is called,
  // we must write a complete texture, which may be larger than the actual bytes
  // allocated for the image.  After the first time, and if we don't change the
  // image size to be larger, we can use the subimage call to only write the
  // pixels we have.
  virtual bool write_to_opengl_texture(GLuint tex_id);
/*
  // Write from the texture to a quad.  Write only the actually-filled
  // portion of the texture (parameters passed in).  This version does not
  // flip the quad over.
  virtual bool write_opengl_texture_to_quad(double xfrac, double yfrac);
*/

};

