#ifndef	BASE_CAMERA_SERVER_H
#define	BASE_CAMERA_SERVER_H

#include <vrpn_Types.h>

//XXX Include continuous-acquisition function

class base_camera_server {
public:
  /// Is the camera working properly?
  bool working(void) const { return _status; };
  unsigned  get_num_rows(void) { return _num_rows; };
  unsigned  get_num_columns(void) { return _num_columns; };

  /// Read an image to a memory buffer.  Max < min means "whole range"
  virtual bool	read_image_to_memory(unsigned minX = 255, unsigned maxX = 0,
			     unsigned minY = 255, unsigned maxY = 0,
			     double exposure_time = 250.0) { return false; };

  /// Get pixels out of the memory buffer, RGB indexes the colors
  virtual bool	get_pixel_from_memory(int X, int Y, vrpn_uint8 &val, int RGB = 0) const { return false; };
  virtual bool	get_pixel_from_memory(int X, int Y, vrpn_uint16 &val, int RGB = 0) const { return false; };

  /// Return the number of colors that the device has
  virtual unsigned  get_num_colors() const { return 0; };

  /// Store the memory image to a PPM file.
  virtual bool  write_memory_to_ppm_file(const char *filename) const {return false;};

protected:
  bool	    _status;			// True is working, false is not
  unsigned  _num_rows, _num_columns;    // Size of the memory buffer
  int	    _minX, _minY, _maxX, _maxY; // Region of the image in memory

  virtual bool	open_and_find_parameters(void) {return false;};
  virtual bool	read_one_frame(unsigned minX, unsigned maxX,
			unsigned minY, unsigned maxY,
			unsigned exposure_time) {return false;};
};

#endif
