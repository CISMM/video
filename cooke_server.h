#include <windows.h>
#include "base_camera_server.h"

// Include files for Cooke PPK
#include <SC2_SDKStructures.h> // needs to be before SC2_CamExport.h
#include <SC2_CamExport.h>

class cooke_server : public base_camera_server {
public:
  cooke_server(unsigned binning = 1);
  virtual ~cooke_server(void);

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

  /// Send whole image over a vrpn connection
  virtual bool  send_vrpn_image(vrpn_Imager_Server* svr,vrpn_Connection* svrcon,double g_exposure,int svrchan, int num_chans = 1) const;

protected:
  vrpn_bool       d_image_mode_settings_changed;
  vrpn_bool       d_shared_settings_changed;
  vrpn_bool       d_scan_enabled;
  vrpn_int32      d_lines_per_message;

  // Cooke-specific parameters
  HANDLE          d_camera;
  int             d_boardNumber;
  PCO_General     d_cameraGeneral;
  PCO_CameraType  d_cameraType;
  PCO_Sensor      d_cameraSensor;
  PCO_Description d_cameraDescription;
  PCO_Timing      d_cameraTiming;
  PCO_Storage     d_cameraStorage;
  PCO_Recording   d_cameraRecording;
  HANDLE          d_cameraEvent;

  // these are the actual number of "standard"
  // pixels on camera sensor.  the language 
  // follows cooke's usage.  It is also possible
  // to do a larger-sized image, which has a strange
  // number of pixels.
  int d_standardMaxResX, d_standardMaxResY;

  char            d_errorText[256];
  
  SHORT           d_cameraBufferNumber;
  int             d_maxBufferSize;
  vrpn_uint16*    d_myImageBuffer;
  vrpn_uint16*    d_cameraImageBuffer;

  // Settings for the camera as of the last image taken.
  // (Note: min and max x and y and binning are stored in the base class)
  int             d_last_exposure_ms;

  void printCookeValues( );
  vrpn_int32 initializeParameterDefaults(void);

  virtual bool open_and_find_parameters(void);
  virtual bool set_current_camera_parameters(int minX, int minY,
                                             int maxX, int maxY,
                                             int binning,
                                             vrpn_uint32 exposure_time_millisecs);
  virtual bool read_one_frame(HANDLE camera_handle,
		       unsigned short minX, unsigned short maxX,
		       unsigned short minY, unsigned short maxY,
                       unsigned binning,
		       const vrpn_uint32 exposure_time_millisecs);
  virtual bool read_continuous(HANDLE camera_handle,
		       unsigned short minX, unsigned short maxX,
		       unsigned short minY, unsigned short maxY,
                       unsigned binning,
		       const vrpn_uint32 exposure_time_millisecs);

  // Write the texture, using a virtual method call appropriate to the particular
  // camera type.  NOTE: At least the first time this function is called,
  // we must write a complete texture, which may be larger than the actual bytes
  // allocated for the image.  After the first time, and if we don't change the
  // image size to be larger, we can use the subimage call to only write the
  // pixels we have.
  virtual bool write_to_opengl_texture(GLuint tex_id);
};
