#ifndef	DIRECTX_CAMERA_SERVER_H
#define	DIRECTX_CAMERA_SERVER_H

#include <windows.h>
#include "base_camera_server.h"

// Include files for DirectShow video input
#include <dshow.h>
#include <qedit.h>


class directx_camera_server : public base_camera_server {
public:
  /// Open the nth available camera
  directx_camera_server(int which);
  virtual ~directx_camera_server(void);
  virtual void close_device(void);

  /// Return the number of colors that the device has
  virtual unsigned  get_num_colors() const { return 3; };

  /// Read an image to a memory buffer.  Max < min means "whole range"
  virtual bool	read_image_to_memory(unsigned minX = 255, unsigned maxX = 0,
			     unsigned minY = 255, unsigned maxY = 0,
			     double exposure_millisecs = 250.0);

  /// Get pixels out of the memory buffer, RGB indexes the colors
  virtual bool	get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint8 &val, int RGB = 0) const;
  virtual bool	get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint16 &val, int RGB = 0) const;

  /// Store the memory image to a PPM file.
  virtual bool  write_memory_to_ppm_file(const char *filename, bool sixteen_bits = false) const;

  /// Send whole image over a vrpn connection
  virtual bool  send_vrpn_image(vrpn_TempImager_Server* svr,vrpn_Synchronized_Connection* svrcon,double g_exposure,int svrchan);

protected:
  /// Construct but do not open camera (used by derived classes)
  directx_camera_server();

  // Objects needed for DirectShow video input.  Described in the help
  // menus for the DirectX API
  IGraphBuilder *_pGraph;	    // Constructs a DirectShow filter graph
  ICaptureGraphBuilder2 *_pBuilder; // Filter graph builder
  IMediaControl *_pMediaControl;    // Handles media streaming in the filter graph
  IMediaEvent   *_pEvent;	    // Handles filter graph events
  IBaseFilter *_pSampleGrabberFilter; // Grabs samples from the media stream
  ISampleGrabber *_pGrabber;	    // Interface for the sample grabber filter

  // Memory pointers used to get non-virtual memory
  char	    *_buffer;	  //< Buffer for what comes from camera
  size_t    _buflen;	  //< Length of that buffer
  bool	    _invert_y;	  //< Do we need to invert the Y axis?
  bool	    _started_graph; //< Did we start the filter graph running?
  unsigned  _mode;	  //< Mode 0 = running, Mode 1 = paused.

  virtual bool	open_and_find_parameters(const int which);\
  virtual bool	read_one_frame(unsigned minX, unsigned maxX,
			unsigned minY, unsigned maxY,
			unsigned exposure_millisecs);
  virtual bool	invert_memory_image_in_y(void);
};

#endif