#include <windows.h>
#include "base_camera_server.h"

// Include files for DirectShow video input
#include <dshow.h>
#include <qedit.h>


class directx_camera_server : public base_camera_server {
public:
  directx_camera_server(void);
  ~directx_camera_server(void);

  /// Return the number of colors that the device has
  virtual unsigned  get_num_colors() const { return 3; };

  /// Read an image to a memory buffer.  Max < min means "whole range"
  virtual bool	read_image_to_memory(unsigned minX = 255, unsigned maxX = 0,
			     unsigned minY = 255, unsigned maxY = 0,
			     double exposure_time = 250.0);

  /// Get pixels out of the memory buffer, RGB indexes the colors
  virtual bool	get_pixel_from_memory(int X, int Y, vrpn_uint8 &val, int RGB = 0) const;

  /// Store the memory image to a PPM file.
  virtual bool  write_memory_to_ppm_file(const char *filename) const;

protected:
  // Objects needed for DirectShow video input.  Described in the help
  // menus for the DirectX API
  IGraphBuilder *_pGraph;	    // Constructs a DirectShow filter graph
  ICaptureGraphBuilder2 *_pBuilder; // Filter graph builder
  IMediaControl *_pMediaControl;    // Handles media streaming in the filter graph
  IMediaEvent   *_pEvent;	    // Handles filter graph events
  IBaseFilter *_pSampleGrabberFilter; // Grabs samples from the media stream
  ISampleGrabber *_pGrabber;	    // Interface for the sample grabber filter

  // Memory pointers used to get non-virtual memory
  char	    *_buffer;	  // Global memory-locked buffer
  size_t    _buflen;	  // Length of that buffer
  bool	    _invert_y;	  // Do we need to invert the Y axis?

  virtual bool	open_and_find_parameters(void);
  virtual bool	read_one_frame(unsigned minX, unsigned maxX,
			unsigned minY, unsigned maxY,
			unsigned exposure_time);
  virtual bool	invert_memory_image_in_y(void);
};