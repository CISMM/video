#ifndef	DIRECTX_VIDEOFILE_SERVER_H
#define	DIRECTX_VIDEOFILE_SERVER_H

#include <windows.h>
#include "directx_camera_server.h"

// Include files for DirectShow video input
#include <dshow.h>
#include <qedit.h>


/// Reads from a video file, acts like a camera video source.
//  Derives from camera class so it can re-use many methods.

class directx_videofile_server : public directx_camera_server {
public:
  directx_videofile_server(const char *filename);
  virtual ~directx_videofile_server(void);
  virtual void close_device(void);

protected:
  virtual bool	open_and_find_parameters(const char *filename);
};

#endif