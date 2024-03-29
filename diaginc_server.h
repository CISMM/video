#ifndef	DIAGINC_SERVER_H
#define	DIAGINC_SERVER_H

#ifdef _WIN32
#include "base_camera_server.h"


class diaginc_server : public base_camera_server {
public:
  diaginc_server(unsigned binning = 1);

  virtual ~diaginc_server(void);

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
  unsigned short _binMin, _binMax;//< Range of values that binning can be set to
  unsigned short _bitDepth;	  //< Bit depth that is set
  bool	      _circbuffer_on;     //< Can it do circular buffers?
  unsigned    _last_exposure;	  //< Stores the exposure time for last circular-buffer region
  unsigned short    _last_minX, _last_maxX, _last_minY, _last_maxY;
  bool	      _circbuffer_run;    //< Is the circular buffer running now?
  unsigned    _circbuffer_len;	  //< Length of the circular buffer memory
  unsigned    _circbuffer_num;	  //< Number of images in the circular buffer
  vrpn_uint16 *_circbuffer;	  //< Buffer to hold the images from the camera

  // Global Memory Pointers used to get non-virtual memory
  HGLOBAL     _buffer;  //< Global memory-locked buffer
  vrpn_uint32 _buflen;  //< Length of that buffer
  void	      *_memory; //< Pointer to either the global buffer (for single-frame) or the circular buffer

  virtual bool open_and_find_parameters(void);

  // The min and max coordinates specified here should be without regard to
  // binning.  That is, they should be in the full-resolution device coordinates.
  virtual bool read_one_frame(unsigned short minX, unsigned short maxX, unsigned short minY, unsigned short maxY, unsigned exposure_time_millisecs);
//  virtual bool read_continuous(int minX, int maxX, int minY, int maxY, unsigned	exposure_time_millisecs);

};
#endif

#endif
