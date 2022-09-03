#ifndef	SEM_CAMERA_SERVER_H
#define	SEM_CAMERA_SERVER_H

// This can be added to Linux later if needed.
#ifdef _WIN32
#include <vrpn_FileConnection.h>
#include "base_camera_server.h"

// Include files for the SEM remote object used to read from the SEM or stored file.
#include  "nmm_Microscope_SEM_Remote.h"

class SEM_camera_server : public base_camera_server {
public:
  SEM_camera_server(const char *name);

  virtual ~SEM_camera_server(void);

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
  nmm_Microscope_SEM_Remote *_myScope;	  //< Scope that we're using
  vrpn_File_Connection *_fileCon;	  //< File connection, if we have one.
  bool	_gotResolution;			  //< Lets us know we've heard the resolution from the server
  unsigned short _bitDepth;		  //< Bit depth that is set
  int     _frameNum;			  //< How many frames we've read in.
  bool	  _justStepped;			  //< Just took a step to a new location, play out frame.
  bool    _pause_after_one_frame;         //< Flag to let the callback handler know to pause after we get one
  bool    _paused;                        //< Keeps track of whether we're paused or not.

  // Global Memory Pointers used to get non-virtual memory
  vrpn_uint16 *_memory; //< Pointer to the in-memory buffer read from the SEM
  vrpn_uint32 _buflen;  //< Length of that buffer

  virtual bool open_and_find_parameters(const char *name);

  // The min and max coordinates specified here should be without regard to
  // binning.  That is, they should be in the full-resolution device coordinates.
  virtual bool read_one_frame(unsigned short minX, unsigned short maxX, unsigned short minY, unsigned short maxY, unsigned exposure_time_millisecs);
  static void handle_SEM_update(void *ud, const nmm_Microscope_SEM_ChangeHandlerData &info);
};
#endif

#endif
