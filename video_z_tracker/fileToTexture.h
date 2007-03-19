#pragma once

#ifdef _WIN32
//#define	VZT_USE_ROPER
//#define	VZT_USE_COOKE
//#define	VZT_USE_EDT
//#define	VZT_USE_DIAGINC
//#define	VZT_USE_SEM
#define	VZT_USE_DIRECTX
//#define USE_METAMORPH	    // Metamorph reader not completed.
#endif

#ifdef	VZT_USE_ROPER
#pragma comment(lib,"C:\\Program Files\\Roper Scientific\\PVCAM\\pvcam32.lib")
#endif

#ifdef	VZT_USE_ROPER
#include "roper_server.h"
#endif
#ifdef	VZT_USE_COOKE
#include "cooke_server.h"
#endif
#include "directx_camera_server.h"
#include "directx_videofile_server.h"

#include "diaginc_server.h"
#include "edt_server.h"
//#include "SEM_camera_server.h"

#include "file_stack_server.h"
#include "image_wrapper.h"

class Controllable_Video {
public:
  /// Start the stored video playing.
  virtual void play(void) = 0;

  /// Pause the stored video
  virtual void pause(void) = 0;

  /// Rewind the stored video to the beginning (also pauses).
  virtual void rewind(void) = 0;

  /// Single-step the stored video for one frame.
  virtual void single_step() = 0;

  /// Gets max frame number or returns -1 if it doesn't know.
  virtual int get_num_frames() {	return -1;	}

  /// Jumps to frame number specified, if possible.
  virtual bool jump_to_frame(int frame) {	return false;	/* by default, we fail */	}
};


#ifdef	VZT_USE_DIRECTX
class Directx_Controllable_Video : public Controllable_Video , public directx_videofile_server {
public:
  Directx_Controllable_Video(const char *filename) : directx_videofile_server(filename) {};
  virtual ~Directx_Controllable_Video() {};
  void play(void) { directx_videofile_server::play(); }
  void pause(void) { directx_videofile_server::pause(); }
  void rewind(void) { pause(); directx_videofile_server::rewind(); }
  void single_step(void) { directx_videofile_server::single_step(); }
};
#endif

#ifdef	VZT_USE_ROPER
class SPE_Controllable_Video : public Controllable_Video, public spe_file_server {
public:
  SPE_Controllable_Video(const char *filename) : spe_file_server(filename) {};
  virtual ~SPE_Controllable_Video() {};
  void play(void) { spe_file_server::play(); }
  void pause(void) { spe_file_server::pause(); }
  void rewind(void) { pause(); spe_file_server::rewind(); }
  void single_step(void) { spe_file_server::single_step(); }
};
#endif

#ifdef	VZT_USE_SEM
class SEM_Controllable_Video : public Controllable_Video, public SEM_camera_server {
public:
  SEM_Controllable_Video(const char *filename) : SEM_camera_server(filename) {};
  virtual ~SEM_Controllable_Video() {};
  void play(void) { SEM_camera_server::play(); }
  void pause(void) { SEM_camera_server::pause(); }
  void rewind(void) { pause(); SEM_camera_server::rewind(); }
  void single_step(void) { SEM_camera_server::single_step(); }
};
#endif

class Pulnix_Controllable_Video : public Controllable_Video, public edt_pulnix_raw_file_server {
public:
  Pulnix_Controllable_Video(const char *filename) : edt_pulnix_raw_file_server(filename) {};
  virtual ~Pulnix_Controllable_Video() {};
  void play(void) { edt_pulnix_raw_file_server::play(); }
  void pause(void) { edt_pulnix_raw_file_server::pause(); }
  void rewind(void) { pause(); edt_pulnix_raw_file_server::rewind(); }
  void single_step(void) { edt_pulnix_raw_file_server::single_step(); }
};

class FileStack_Controllable_Video : public Controllable_Video, public file_stack_server {
public:
  FileStack_Controllable_Video(const char *filename) : file_stack_server(filename) {};
  virtual ~FileStack_Controllable_Video() {};
  void play(void) { file_stack_server::play(); }
  void pause(void) { file_stack_server::pause(); }
  void rewind(void) { pause(); file_stack_server::rewind(); }
  void single_step(void) { file_stack_server::single_step(); }

  // Jumps to specified frame number.  Does nothing if frame is not a valid frame number.
	bool jump_to_frame(int num)
	{
		if (num < 0 || num > d_fileNames.size())
		{
			// Frame number out of bounds.  Do nothing.
			return false;
		}
		else
		{
			d_whichFile = d_fileNames.begin() + num;
			single_step();
			return true;
		}
	}

	int get_num_frames() {	return d_fileNames.size();	}

};
