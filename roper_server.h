#ifndef	ROPER_SERVER_H
#define	ROPER_SERVER_H

#ifdef _WIN32
#include <windows.h>
#include "base_camera_server.h"

// Include files for Roper PPK
#include <master.h>
#include <pvcam.h>

class roper_server : public base_camera_server {
public:
  roper_server(unsigned binning = 1);
  virtual ~roper_server(void);

  /// Read an image to a memory buffer.  Exposure time is in milliseconds
  virtual bool	read_image_to_memory(unsigned minX = 0, unsigned maxX = 0,
			     unsigned minY = 0, unsigned maxY = 0,
			     double exposure_time_millisecs = 250.0);

  /// Get pixels out of the memory buffer
  virtual bool	get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint8 &val, int RGB = 0) const;
  virtual bool	get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint16 &val, int RGB = 0) const;

  /// Store the memory image to a PPM file.
  virtual bool  write_memory_to_ppm_file(const char *filename, int gain = 1, bool sixteen_bits = false) const;

  /// How many colors are in the image.
  virtual unsigned  get_num_colors() const { return 1; }

  /// Send in-memory image over a vrpn connection
  virtual bool  send_vrpn_image(vrpn_Imager_Server* svr,vrpn_Connection* svrcon,double g_exposure,int svrchan, int num_chans = 1);

protected:
  int16	  _camera_handle;     //< Used to access the camera
  bool	  _circbuffer_on;     //< Can it do circular buffers?
  rgn_type  _last_region;     //< Stores the values for last circular-buffer region
  unsigned _last_exposure;    //< Stores the exposure time for last circular-buffer region
  bool	  _circbuffer_run;    //< Is the circular buffer running now?
  unsigned  _circbuffer_len;  //< Length of the circular buffer memory
  unsigned  _circbuffer_num;  //< Number of images in the circular buffer
  uns16	    *_circbuffer;     //< Buffer to hold the images from the camera
  uns32     _last_buffer_cnt; //< How many buffers read continuously before?
  uns16		_bit_depth;		  //< Number of valid bits in the 16-bit values from camera

  // Buffers and pointers to the image data.
  uns16     *_buffer;  //< Global memory-locked buffer
  uns32	    _buflen;  //< Length of that buffer
  void	    *_memory; //< Pointer to either the global buffer (for single-frame) or the circular buffer

  virtual bool open_and_find_parameters(void);
  virtual bool read_one_frame(const int16 camera_handle,
		       const rgn_type &region_description,
		       const uns32 exposure_time_millisecs);
  virtual bool read_continuous(const int16 camera_handle,
		       const rgn_type &region_description,
		       const uns32 exposure_time_millisecs);

  // Write the texture, using a virtual method call appropriate to the particular
  // camera type.  NOTE: At least the first time this function is called,
  // we must write a complete texture, which may be larger than the actual bytes
  // allocated for the image.  After the first time, and if we don't change the
  // image size to be larger, we can use the subimage call to only write the
  // pixels we have.
  virtual bool write_to_opengl_texture(GLuint tex_id);
};

// The following class does not require the Roper header files.  It is
// completely self-contained, and based on a document that describes the
// Roper SPE file format.
class spe_file_server : public base_camera_server {
public:
  spe_file_server(const char *filename);
  virtual ~spe_file_server(void);

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
  unsigned  d_numFrames;	  //< Number of frames in the file
  vrpn_uint16  *d_buffer;	  //< Holds one frame of data from the file
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

#endif
