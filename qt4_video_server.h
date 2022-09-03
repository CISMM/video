#pragma once

#include "base_camera_server.h"

// XXXX Problem: the current version of Phonon does not allow access to
// individual video frames.  Once it does, we can implement this class.
// It currently only allows the display of frames, not grabbing them.
// XXX Can I implement a Sink based on their source code that will do
// this, or is that all down inside the platform-specific stuff?  It is
// surely in the platform stuff -- that's why you can't know whether you
// can play a stream because you don't know what CODECs are available.
// XXX Hold on -- there is a snapshot() function in the video widget that
// returns a QImage (but a comment in the code makes it seem like it may
// not be implemented).  There was some sort of seek-to function somewhere
// else.
#include <Phonon/MediaObject>

class qt4_video_server : public base_camera_server {
public:
  qt4_video_server(const char *filename);
  virtual ~qt4_video_server(void);
  virtual void close_device(void);

  /// Return the number of colors that the device has
  virtual unsigned  get_num_colors() const { return 3; };

  /// Read an image to a memory buffer. Max < min means "whole range"
  virtual bool	read_image_to_memory(unsigned minX = 0, unsigned maxX = 0,
			              unsigned minY = 0, unsigned maxY = 0,
			              double exposure_time_millisecs = 0.0);

  /// Get pixels out of the memory buffer
  virtual bool	get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint8 &val, int RGB = 0) const;
  virtual bool	get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint16 &val, int RGB = 0) const;

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

  /// Start the stored video playing.
  virtual void play(void);

  /// Pause the stored video
  virtual void pause(void);

  /// Rewind the stored video to the beginning (also pauses).
  virtual void rewind(void);

  /// Single-step the stored video for one frame.
  virtual void single_step();

protected:
	// XXX Include Qt stuff that we need here.
	Phonon::MediaObject	*m_media;
  
  struct timeval m_timestamp;     // timestamp of our most recent image

};

