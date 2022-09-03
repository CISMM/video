#ifndef	VRPN_IMAGER_CAMERA_SERVER_H
#define	VRPN_IMAGER_CAMERA_SERVER_H

#include <vrpn_Imager.h>
#include <vrpn_FileConnection.h>
#include "base_camera_server.h"

class VRPN_Imager_camera_server : public base_camera_server {
public:
  // If clear_new_frames = true, each new image buffer is filled with
  // zeroes before being written to.  If it is false, the previous image
  // (if there is one) is copied into the buffer.  This allows continuous-
  // looking frames when only partial frames are sent by the server.
  VRPN_Imager_camera_server(const char *name, bool clear_new_frames = false);

  virtual ~VRPN_Imager_camera_server(void);

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
			     double exposure_time_millisecs = 250.0);

  /// Get pixels out of the memory buffer
  virtual bool	get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint8 &val, int RGB = 0) const;
  virtual bool	get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint16 &val, int RGB = 0) const;

  /// How many colors are in the image.
  virtual unsigned  get_num_colors() const { return 1; }

  /// Store the memory image to a PPM file.
  virtual bool  write_memory_to_ppm_file(const char *filename, int gain = 1, bool sixteen_bits = false) const;

  /// Send in-memory image over a vrpn connection
  virtual bool  send_vrpn_image(vrpn_Imager_Server* svr,vrpn_Connection* svrcon,double g_exposure,int svrchan, int num_chans = 1);

  // Write the texture, using a virtual method call appropriate to the particular
  // camera type.  NOTE: At least the first time this function is called,
  // we must write a complete texture, which may be larger than the actual bytes
  // allocated for the image.  After the first time, and if we don't change the
  // image size to be larger, we can use the subimage call to only write the
  // pixels we have.
  virtual bool write_to_opengl_texture(GLuint tex_id);

protected:
  vrpn_Imager_Remote  *_imager;           //< Imager to use
  vrpn_File_Connection *_fileCon;	  //< File connection, if we have one.
  bool	_gotResolution;			  //< Lets us know we've heard the resolution from the server
  int     _frameNum;			  //< How many frames we've read in.
  bool	  _justStepped;			  //< Just took a step to a new location, play out frame.
  bool    _pause_after_one_frame;         //< Flag to let the callback handler know to pause after we get one
  bool    _paused;                        //< Keeps track of whether we're paused or not.
  bool    _clear_new_frames;              //< Clear out each new frame, or copy from previous?

  // We use double-buffering to prevent half-updated frames.
  // Writes always happen to the back buffer, and reads from the front buffer.
  // The end-of-frame message swaps the buffers.
  vrpn_uint16 *_front, *_back; //< Pointer to the in-memory buffers read from the VRPN Imager
  vrpn_uint32 _buflen;  //< Length of that buffer

  virtual bool open_and_find_parameters(const char *name);

  // Callback handlers for the various message types
  static void VRPN_CALLBACK handle_end_frame_message(void * userdata, const vrpn_IMAGERENDFRAMECB info);
  static void VRPN_CALLBACK handle_description_message(void * userdata, const struct timeval msg_time);
  static void VRPN_CALLBACK handle_region_message(void * userdata, const vrpn_IMAGERREGIONCB info);

  // The min and max coordinates specified here should be without regard to
  // binning.  That is, they should be in the full-resolution device coordinates.
  virtual bool read_one_frame(unsigned short minX, unsigned short maxX, unsigned short minY, unsigned short maxY, unsigned exposure_time_millisecs);
};
#endif

