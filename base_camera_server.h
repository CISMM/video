#ifndef	BASE_CAMERA_SERVER_H
#define	BASE_CAMERA_SERVER_H

#include  <vrpn_Types.h>
#include  <vrpn_Connection.h>
#include  <vrpn_TempImager.h>

class base_camera_server {
public:
  virtual ~base_camera_server(void) {};

  /// Is the camera working properly?
  bool working(void) const { return _status; };

  // These functions should be used to determine the stride in the
  // image when skipping lines.  They are in terms of the full-screen
  // number of pixels with the current binning level.
  unsigned  get_num_rows(void) const { return _num_rows / _binning; };
  unsigned  get_num_columns(void) const { return _num_columns / _binning; };

  /// Read an image to a memory buffer.  Max < min means "whole range".
  /// Setting binning > 1 packs more camera pixels into each image pixel, so
  /// the maximum number of pixels has to be divided by the binning constant
  /// (the effective number of pixels is lower and pixel coordinates are set
  /// in this reduced-count pixel space).  This routine returns false if a
  /// new image could not be read (for example, because of a timeout on
  /// the reading because we're at the end of a video stream).
  virtual bool	read_image_to_memory(unsigned minX = 255, unsigned maxX = 0,
			     unsigned minY = 255, unsigned maxY = 0,
			     double exposure_time = 250.0) { return false; };

  /// Get pixels out of the memory buffer, RGB indexes the colors
  virtual bool	get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint8 &val, int RGB = 0) const { return false; };
  virtual bool	get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint16 &val, int RGB = 0) const { return false; };

  /// Return the number of colors that the device has
  virtual unsigned  get_num_colors() const { return 0; };

  /// Store the memory image to a PPM file.
  virtual bool  write_memory_to_ppm_file(const char *filename, int gain = 1, bool sixteen_bits = false) const {return false;};

  /// Send whole image over a vrpn connection
  virtual bool  send_vrpn_image(vrpn_TempImager_Server* svr,vrpn_Synchronized_Connection* svrcon,double g_exposure,int svrchan) {return false;};

protected:
  bool	    _status;			//< True is working, false is not
  unsigned  _num_rows, _num_columns;    //< Size of the memory buffer
  unsigned  _minX, _minY, _maxX, _maxY; //< Region of the image in memory
  unsigned  _binning;			//< How many camera pixels compressed into image pixel

  virtual bool	open_and_find_parameters(void) {return false;};
  base_camera_server(unsigned binning = 1) {
    _binning = binning;
    if (_binning < 1) { _binning = 1; }
  };
};

#endif
