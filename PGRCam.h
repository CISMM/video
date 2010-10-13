#include "PGRFlyCapture.h"
#include "PGRFlyCapturePlus.h"

class PGRCam {
public:
	PGRCam(double framerate = -1, double msExposure = -1, int binning = 1, bool trigger = 0, int camera = 0);
	~PGRCam();

	bool GetNewImage();

	unsigned char* GetImagePointer();

	int rows;
	int cols;

	FlyCaptureImage image;
	FlyCaptureImage imageConverted;

	bool connected;

private:
	FlyCaptureContext context;
	FlyCaptureInfoEx info;
	FlyCaptureError err;

	unsigned char* testImage;

	unsigned char* imgPtr;

	int counter;
	unsigned long xmitErrorCountLast;
};