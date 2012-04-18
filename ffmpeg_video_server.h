#pragma once

#include "base_camera_server.h"

// Include the appropriate headers.
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libavfilter/avfilter.h>
#include <libavutil/mathematics.h>
#include <libswscale/swscale.h>
}

class ffmpeg_video_server : public base_camera_server {
public:
  ffmpeg_video_server(const char *filename);
  virtual ~ffmpeg_video_server(void);
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
  // AV format context needed for the file.
  struct AVFormatContext *m_pFormatCtx;
  AVCodecContext  *m_pCodecCtx;
  int             m_videoStream;
  AVFrame         *m_pFrame;
  AVFrame         *m_pFrameRGB;
  uint8_t         *m_buffer;

  enum {PAUSE, PLAY, SINGLE}  d_mode;		  //< What we're doing right now

  struct timeval m_timestamp;     // timestamp of our most recent image

};

