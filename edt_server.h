#include "base_camera_server.h"

class edt_server : public base_camera_server {
public:
  // Version 3303 of the library would flip every other line
  // improperly on our Pulnix cameras, so I had this swap_lines
  // entry, and it defaulted to true.  Version 3335 of the
  // drivers fixes this, so the default is now false.
  edt_server(bool swap_lines = false, unsigned num_buffers = 1);
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

  /// Send whole image over a vrpn connection
  virtual bool  send_vrpn_image(vrpn_Imager_Server* svr,
    vrpn_Synchronized_Connection* svrcon,double g_exposure,int svrchan, int num_chans = 1);

protected:
  vrpn_uint8  *d_buffer;	  //< Points to current frame of data
  vrpn_uint8  *d_swap_buffer;	  //< Holds a line at a time during swapping, if we swap lines
  void	      *d_pdv_p;		  //< Holds a pointer to the video device.
  bool	      d_swap_lines;	  //< Do we swap every other line in video (workaround for EDT bug in our system)
  unsigned    d_num_buffers;	  //< How many EDT-managed buffers to allocate in ring?
  unsigned    d_started;	  //< How many acquisitions started?
  unsigned    d_last_timeouts;	  //< How many timeouts last time we read?
  unsigned    d_first_timeouts;	  //< How many timeouts when we opened the device?

  virtual bool open_and_find_parameters(void);
};

class edt_pulnix_raw_file_server : public base_camera_server {
public:
  edt_pulnix_raw_file_server(const char *filename);
  virtual ~edt_pulnix_raw_file_server(void);

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

  /// Send whole image over a vrpn connection
  virtual bool  send_vrpn_image(vrpn_Imager_Server* svr,vrpn_Synchronized_Connection* svrcon,double g_exposure,int svrchan, int num_chans = 1);

protected:
  FILE *d_infile;		  //< File to read from
  vrpn_uint8  *d_buffer;	  //< Holds one frame of data from the file
  enum {PAUSE, PLAY, SINGLE} d_mode;	  //< What we're doing right now
};
