#include "base_camera_server.h"
#include <list>
#include <string>

class file_stack_server : public base_camera_server {
public:
  file_stack_server(const char *filename);
  virtual ~file_stack_server(void);

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

  /// Store the memory image to a PPM file.
  virtual bool  write_memory_to_ppm_file(const char *filename, int gain = 1, bool sixteen_bits = false) const;

  /// Send whole image over a vrpn connection
  virtual bool  send_vrpn_image(vrpn_TempImager_Server* svr,vrpn_Synchronized_Connection* svrcon,double g_exposure,int svrchan);

  /// Get a pointer to the raw memory holding the image.  Dangerous but fast.
  vrpn_uint16	*get_memory_pointer(void) const { return NULL; };

protected:
  vrpn_uint16		      *d_buffer;	  //< Holds one frame of data from the file
  enum {PAUSE, PLAY, SINGLE}  d_mode;		  //< What we're doing right now
  std::list <std::string>     d_fileNames;	  //< Sorted list of files that we are to use.
  std::list <std::string>::iterator d_whichFile;  //< Which file is next up to be read.
  unsigned		      d_xFileSize;	  //< Number of pixels in X in the files
  unsigned		      d_yFileSize;	  //< Number of pixels in Y in the files

  bool read_image_from_file(const std::string filename);

  static bool ds_majickInitialized;		  //< Has ImageMagick been initialized?
};