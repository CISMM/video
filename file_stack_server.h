#ifndef	FILE_STACK_SERVER_H
#define	FILE_STACK_SERVER_H

#include "base_camera_server.h"
#pragma warning( disable : 4786 )
#include <vector>
#include <string>

class file_stack_server : public base_camera_server {
public:
  file_stack_server(const char *filename, const char *magickFilesDir = "");
  virtual ~file_stack_server(void);

  /// Start the stored video playing.
  // This will place the next image into memory each time read_image_to_memory()
  // is called until the last image is read.
  virtual void play(void);

  /// Pause the stored video.
  // This causes the current image to be kept in memory when read_image_to_memory() is called.
  virtual void pause(void);

  /// Rewind the stored video to the beginning (also pauses).
  // This points the reading code back to the first image in the stack, so that it will
  // be read into memory at the next call to read_image_to_memory(), then it pauses at
  // that image.
  virtual void rewind(void);

  /// Single-step the stored video for one frame.
  // This causes a new image to be read (unless we're at the last image) the next
  // time read_image_to_memory() is called, and then pauses at that image.
  virtual void single_step();

  /// Read an image to a memory buffer.  Exposure time is in milliseconds is ignored.
  virtual bool	read_image_to_memory(unsigned minX = 0, unsigned maxX = 0,
			     unsigned minY = 0, unsigned maxY = 0,
			     double exposure_time_millisecs = 0.0);

  /// Get pixels out of the memory buffer in the requested format.
  virtual bool	get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint8 &val, int RGB = 0) const;
  virtual bool	get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint16 &val, int RGB = 0) const;

  /// How many colors are in the image.
  virtual unsigned  get_num_colors() const { return 3; }

  /// Store the memory image to a PPM file.
  virtual bool  write_memory_to_ppm_file(const char *filename, int gain = 1, bool sixteen_bits = false) const;

  /// Send in-memory image over a vrpn connection.
  virtual bool  send_vrpn_image(vrpn_Imager_Server* svr,vrpn_Connection* svrcon,double g_exposure,int svrchan, int num_chans = 1);

  // Write the texture, using a virtual method call appropriate to the particular
  // camera type.  NOTE: At least the first time this function is called,
  // we must write a complete texture, which may be larger than the actual bytes
  // allocated for the image.  After the first time, and if we don't change the
  // image size to be larger, we can use the subimage call to only write the
  // pixels we have.
  virtual bool write_to_opengl_texture(GLuint tex_id);

protected:
  vrpn_uint16		      *d_buffer;	  //< Holds one frame of data from the file
  enum {PAUSE, PLAY, SINGLE}  d_mode;		  //< What we're doing right now
  std::vector <std::string>     d_fileNames;	  //< Sorted list of files that we are to use.
  std::vector <std::string>::iterator d_whichFile;  //< Which file is next up to be read.
  void                        *d_listOfImages;    //< Non-NULL if we have a list of images in memory (multi-layer image loaded).

  unsigned		      d_xFileSize;	  //< Number of pixels in X in the files
  unsigned		      d_yFileSize;	  //< Number of pixels in Y in the files

  bool read_image_from_file(const std::string filename);
  void prefetch_image_from_file(const std::string filename);
  std::string                 d_prefetchName;     //< Name of the next file to prefetch
  vrpn_Thread                 *d_prefetchThread;  //< Thread that prefetches files from disk
  
  static bool ds_majickInitialized;		  //< Has ImageMagick been initialized?
};

// Metamorph reader is not complete.  It uses the TIFF library, which does not have
// a way for the app to handle unknown tags (you can do this, but you have to
// rebuild the TIFF library from source code).
//#define	USE_METAMORPH
#ifdef	USE_METAMORPH

class Metamorph_stack_server : public base_camera_server {
public:
  Metamorph_stack_server(const char *filename);
  virtual ~Metamorph_stack_server(void);

  /// Start the stored video playing.
  // This will place the next image into memory each time read_image_to_memory()
  // is called until the last image is read.
  virtual void play(void);

  /// Pause the stored video.
  // This causes the current image to be kept in memory when read_image_to_memory() is called.
  virtual void pause(void);

  /// Rewind the stored video to the beginning (also pauses).
  // This points the reading code back to the first image in the stack, so that it will
  // be read into memory at the next call to read_image_to_memory(), then it pauses at
  // that image.
  virtual void rewind(void);

  /// Single-step the stored video for one frame.
  // This causes a new image to be read (unless we're at the last image) the next
  // time read_image_to_memory() is called, and then pauses at that image.
  virtual void single_step();

  /// Read an image to a memory buffer.  Exposure time is in milliseconds is ignored.
  virtual bool	read_image_to_memory(unsigned minX = 0, unsigned maxX = 0,
			     unsigned minY = 0, unsigned maxY = 0,
			     double exposure_time_millisecs = 0.0);

  /// Get pixels out of the memory buffer in the requested format.
  virtual bool	get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint8 &val, int RGB = 0) const;
  virtual bool	get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint16 &val, int RGB = 0) const;

  /// Store the memory image to a PPM file.
  virtual bool  write_memory_to_ppm_file(const char *filename, int gain = 1, bool sixteen_bits = false) const;

  /// Send in-memory image over a vrpn connection.
  virtual bool  send_vrpn_image(vrpn_Imager_Server* svr,vrpn_Synchronized_Connection* svrcon,double g_exposure,int svrchan, int num_chans = 1);

protected:
  vrpn_uint16		      *d_buffer;	  //< Holds one frame of data from the file
  enum {PAUSE, PLAY, SINGLE}  d_mode;		  //< What we're doing right now
  std::string		      d_fileName;	  //< File to open
  void			      *d_tiff;		  //< TIFF file handle for the stack file (hidden in void* to avoid having to #include TIFF stuff in header
  unsigned		      d_xFileSize;	  //< Number of pixels in X in the files
  unsigned		      d_yFileSize;	  //< Number of pixels in Y in the files

  bool read_image_from_file(void);
};

#endif

#endif
