#include <windows.h>
#include "base_camera_server.h"

// Include files for Nikon SDK

class nikon_server : public base_camera_server {
public:
  nikon_server(unsigned binning = 1, unsigned which_camera = 0);
  virtual ~nikon_server(void);

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
  virtual bool  send_vrpn_image(vrpn_Imager_Server* svr,vrpn_Connection* svrcon,double g_exposure,int svrchan, int num_chans = 1) const;

protected:

  virtual bool open_and_find_parameters(void);
  virtual bool read_one_frame(const int16 camera_handle,
		       const rgn_type &region_description,
		       const uns32 exposure_time_millisecs);
  virtual bool read_continuous(const int16 camera_handle,
		       const rgn_type &region_description,
		       const uns32 exposure_time_millisecs);
};
