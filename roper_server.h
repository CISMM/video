#include <windows.h>
#include "base_camera_server.h"

// Include files for Roper PPK
#include <master.h>
#include <pvcam.h>

class roper_server : public base_camera_server {
public:
  roper_server(void);
  virtual ~roper_server(void);

  /// Read an image to a memory buffer.  Exposure time is in milliseconds
  bool	read_image_to_memory(int minX = 0, int maxX = -1,
			     int minY = 0, int maxY = -1,
			     double exposure_time_millisecs = 250.0);

  /// Get pixels out of the memory buffer
  bool	get_pixel_from_memory(int X, int Y, vrpn_uint8 &val, int RGB = 0) const;
  bool	get_pixel_from_memory(int X, int Y, vrpn_uint16 &val, int RGB = 0) const;

  /// Store the memory image to a PPM file.
  bool  write_memory_to_ppm_file(const char *filename, bool sixteen_bits = false) const;

protected:
  bool	  _status;		      // True is working, false is not
  int16	  _camera_handle;
  uns16	  _num_rows, _num_columns;    // Size of the memory buffer
  int	  _minX, _minY, _maxX, _maxY; // Region of the image in memory

  // Global Memory Pointers used to get non-virtual memory
  HGLOBAL _buffer;   // Global memory-locked buffer
  void	  *_memory;  // Pointer to that buffer
  uns32	  _buflen;   // Length of that buffer

  virtual bool open_and_find_parameters(void);
  virtual bool read_one_frame(const int16 camera_handle,
		       const rgn_type &region_description,
		       const uns32 exposure_time_millisecs);
};