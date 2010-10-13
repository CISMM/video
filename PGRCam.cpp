#include <stdio.h>
#include "PGRCam.h"


#define PGR_WIDTH 640
#define PGR_HEIGHT 480


void reportCameraInfo( const FlyCaptureInfoEx* pinfo )
{
   //
   // Print out camera information. This can be obtained by calling
   // flycaptureGetCameraInfo() anytime after the camera has been initialized.
   //
   printf( "Serial number: %d\n", pinfo->SerialNumber );
   printf( "Camera model: %s\n", pinfo->pszModelName );
   printf( "Camera vendor: %s\n", pinfo->pszVendorName );
   printf( "Sensor: %s\n", pinfo->pszSensorInfo );
   printf( "DCAM compliance: %1.2f\n", (float)pinfo->iDCAMVer / 100.0 );
   printf( "Bus position: (%d,%d).\n", pinfo->iBusNum, pinfo->iNodeNum );
}

//
// Helper code to handle a FlyCapture error.
//
#define _HANDLE_ERROR( error, function ) \
   if( (error != FLYCAPTURE_OK) && (error != FLYCAPTURE_TIMEOUT) ) \
   { \
      printf( "%s: %s\n", function, flycaptureErrorToString( error ) ); \
   } \

PGRCam::PGRCam(double framerate, double msExposure, int binning, bool trigger, int camera)
        : triggered(trigger)
{
	connected = false;
	imageConverted.pData = NULL;

	const int maxCameras = 32;
	unsigned int numFound = maxCameras;
	FlyCaptureInfoEx infos[maxCameras];

	err = flycaptureBusEnumerateCamerasEx(infos, &numFound);
	_HANDLE_ERROR(err, "flycaptureBusEnumerateCamerasEx()");

	if (numFound < 1)
		printf("*** Warning: no PGR cameras dectected! ***\n");
/*
	for (int i = 0; i < numFound; ++i) {
		printf("Camera %i:\n", i);
		reportCameraInfo(&(infos[i]));
	}
*/
	err = flycaptureCreateContext(&context);
	_HANDLE_ERROR(err, "flycaptureCreateContext()");
	
	//err = flycaptureInitializePlus(context, camera, 32, NULL);
	//_HANDLE_ERROR(err, "flycaptureInitialize()");
	err = flycaptureInitialize(context, camera);
	_HANDLE_ERROR(err, "flycaptureInitialize()");

	if( err == FLYCAPTURE_OK ) {
		connected = true;
	}
	else {
		return;
	}

	// set camera reset
	err = flycaptureSetCameraRegister(context, 0, 0x80000000);
	_HANDLE_ERROR(err, "flycaptureSetCameraRegister()");


	err = flycaptureGetCameraInfo(context, &info);
	_HANDLE_ERROR(err, "flycaptureGetCameraInfo()");

	printf( "Camera info:\n" );
	reportCameraInfo(&info);

	int mode = 0; // mode 0 = no binning
	if (binning == 2) {
		mode = 1; // mode 1 = 2x2 binning
	}

	//err = flycaptureStart(context, FLYCAPTURE_VIDEOMODE_ANY, FLYCAPTURE_FRAMERATE_ANY);

	/* the following call initializes the camera into Format 7, custom image format. see flycaptureStartCustomImage() on 
	   line 1288 of pgrflycapture.h for more details. this is where you will set the left/top and width/height of your
	   desired ROI, as well as the maximum packet size to send. however, you will still need to set the frame rate 
	   elsewhere, unless the camera is being triggered externally that is, in which case the frame rate setting is 
	   irrelevant.
	*/ 
	err = flycaptureStartCustomImage(context, mode, 0, 0, PGR_WIDTH / binning, PGR_HEIGHT / binning, 100, FLYCAPTURE_MONO8);	
	//err = flycaptureStart(context, FLYCAPTURE_VIDEOMODE_1024x768Y8, FLYCAPTURE_FRAMERATE_30);
	_HANDLE_ERROR(err, "flycaptureStart()");
	cols = PGR_WIDTH / binning;
	rows = PGR_HEIGHT / binning;

	bool framerateAuto = (framerate == -1);
	if (framerateAuto)
		framerate = 1000.0f / msExposure;

	bool shutterAuto = (msExposure == -1);
	if (shutterAuto)
		msExposure = 1000.0f / framerate;
/*
	printf("framerateAuto = ");
	if (framerateAuto)
		printf("true.\n");
	else
		printf("false.\n");
	printf("framerate = %f\n", framerate);
*/

	err = flycaptureSetCameraAbsPropertyEx(context, FLYCAPTURE_FRAME_RATE, false, true, framerateAuto, framerate);
	_HANDLE_ERROR(err, "setting FLYCAPTURE_FRAME_RATE (ex)");
	
	// verify that the exposure is possible given the specified framerate
	if (!framerateAuto) {
		if (msExposure > (1000.0f / framerate)) {
			msExposure = (1000.0f / framerate);
			printf("Exposure limited to %f by specified framerate!\n", msExposure);
		}
	}
/*
	printf("shutterAuto = ");
	if (shutterAuto)
		printf("true.\n");
	else
		printf("false.\n");
	printf("msExposure = %f\n", msExposure);
*/
	err = flycaptureSetCameraAbsPropertyEx(context, FLYCAPTURE_SHUTTER, false, true, shutterAuto, msExposure);
	_HANDLE_ERROR(err, "setting FLYCAPTURE_SHUTTER");


	err = flycaptureSetColorProcessingMethod(context, FLYCAPTURE_NEAREST_NEIGHBOR_FAST);
	_HANDLE_ERROR(err, "flycaptureSetColorProcessingMethod()");


	err = flycaptureSetCameraRegister(context, 0x12fc, 0x80000000);
	_HANDLE_ERROR(err, "flycaptureSetCameraRegister()");

	/*
	float shutter = -1, framerate = -1;
	err = flycaptureGetCameraAbsProperty(context, FLYCAPTURE_SHUTTER, &shutter);
	_HANDLE_ERROR(err, "getting FLYCAPTURE_SHUTTER");
	err = flycaptureGetCameraAbsProperty(context, FLYCAPTURE_FRAME_RATE, &framerate);
	_HANDLE_ERROR(err, "getting FLYCAPTURE_FRAME_RATE");

	printf("Camera set to:\n");
	printf("\tshutter speed = \t%f ms\n", shutter);
	printf("\tframerate = \t%f fps\n", framerate);
	*/
	
	/* the following call puts the camera into external triggering mode. see flycaptureSetTrigger() on 
	   line 2376 of pgrflycapture.h for more details. 
	*/ 
	if( trigger ){
		err = flycaptureSetTrigger(context, true, 0, 0, 1, 0);
		_HANDLE_ERROR(err, "flycaptureSetTrigger()");

                // If we're in trigger mode, set the capture timeout to 100 milliseconds,
                // so we won't wait forever for an image.
	        err = flycaptureSetGrabTimeoutEx(context, 100);	
	        _HANDLE_ERROR(err, "flycaptureSetGrabTimeoutEx()");
	}

	/* if you set a grab timeout using flycaptureSetGrabTimeoutEx(), this timeout will be used in asynchronous 
	   trigger mode as well: flycaptureGrabImage*() will return with the image when you either trigger the camera,
	   or the timeout value expires. because we do not want the camera to return anything unless we actually
	   trigger it, set this value only when the camera is *not* initialized in external triggering mode.
	*/
	else{
		err = flycaptureSetGrabTimeoutEx(context, 5000);
		_HANDLE_ERROR(err, "flycaptureSetGrabTimeoutEx()");
	}
	
	testImage = NULL;
	/*
	const int w = 256;
	const int h = 256;

	rows = h;
	cols = w;

	testImage = new unsigned char[w * h * 3];
	int i = 0;
	for (int x = 0; x < w; ++x) {
		for (int y = 0; y < h; ++y) {
			testImage[3 * (x + w * y) + 0] = i % 255;
			testImage[3 * (x + w * y) + 1] = i % 255;
			testImage[3 * (x + w * y) + 2] = i % 255;
			++i;
		}
	}
	*/
	
	counter = 0;
	xmitErrorCountLast = 0;
}

PGRCam::~PGRCam() {
	err = flycaptureStop(context);
	_HANDLE_ERROR(err, "flycaptureStop()");
	if (imageConverted.pData != NULL)
		delete[] imageConverted.pData;
	if (testImage != NULL)
		delete[] testImage;
}

bool PGRCam::GetNewImage() {
	if (!connected)
		return false;
	//*
/*
	printf("1");

	if (context == NULL)
		printf("NULLiFied biatch!\n");

	printf("image.iCols = %i\n", image.iCols);

	getchar();
	printf("2");*/
	err = flycaptureGrabImage2(context, &image);
	_HANDLE_ERROR(err, "flycaptureGrabImage2()");

        // If we timed out, go ahead and return false.
        if (err == FLYCAPTURE_TIMEOUT) {
          return false;
        }
	//*/
	/*
	err = flycaptureLockLatest(context, &image);
	_HANDLE_ERROR(err, "flycaptureLockLatest()");
	//*/

	// update iRows and iCols if we're NOT using a test image
	if (testImage == NULL) {
		rows = image.iRows;
		cols = image.iCols;
	}

	imgPtr = image.pData;

	/*
	if (imageConverted.pData == NULL)
		imageConverted.pData = new unsigned char[ image.iCols * image.iRows * 4 ];

	imageConverted.pixelFormat = FLYCAPTURE_BGRU;
	imgPtr = NULL;
	err = flycaptureConvertImage( context, &image, &imageConverted );
	imgPtr = imageConverted.pData;
	_HANDLE_ERROR(err, "flycaptureConvertImage()");
	*/

	/*
	unsigned long xmitErrorCount = 0;
	err = flycaptureGetCameraRegister(context, 0x12fc, &xmitErrorCount);
	_HANDLE_ERROR(err, "flycaptureGetCameraRegister()");

	
	if (xmitErrorCountLast != xmitErrorCount)
		printf("xmitErrorCount = %i\n", xmitErrorCount);

	xmitErrorCountLast = xmitErrorCount;
	//*/

	/*
	char path[256];
	sprintf_s(path, 256, "D:\\tmpshr\\adesk\\PGRWrapper\\capture\\image_%i.pgm", counter);
	err = flycaptureSaveImage(context, &image, path, FLYCAPTURE_FILEFORMAT_PGM);
	_HANDLE_ERROR(err, "flycaptureSaveImage()");

	sprintf_s(path, 256, "D:\\tmpshr\\adesk\\PGRWrapper\\capture\\cimage_%i.jpg", counter++);
	err = flycaptureSaveImage(context, &imageConverted, path, FLYCAPTURE_FILEFORMAT_JPG);
	_HANDLE_ERROR(err, "flycaptureSaveImage()");
	//*/

	/*
	err = flycaptureUnlock(context, image.uiBufferIndex);
	_HANDLE_ERROR(err, "flycaptureUnlock()");
	//*/

	return (err == 0);
}

unsigned char* PGRCam::GetImagePointer() {
	//printf( "Converting image.\n" );
	
	if (testImage != NULL)
		return testImage;

	//return imageConverted.pData;
	return imgPtr;
}