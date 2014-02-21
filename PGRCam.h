#include "FlyCapture2.h"

class PGRCam {
public:
	PGRCam(double framerate = -1, double msExposure = -1, int binning = 1, bool trigger = 0, float gain = 0, int camera = 0);
	~PGRCam();

	bool GetNewImage();

	unsigned char* GetImagePointer();

	int rows;
	int cols;

	bool connected;

private:
  FlyCapture2::Image image;

  FlyCapture2::Camera cam;
  FlyCapture2::CameraInfo camInfo;
  FlyCapture2::Error error;

  bool triggered;

  void configure_triggering_and_timeout(bool is_triggered, unsigned timeout_ms);
  void grab_and_toss_initial_image();

  unsigned char* imgPtr;
};
