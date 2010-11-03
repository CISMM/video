#include <stdio.h>
#include "PGRCam.h"

using namespace FlyCapture2;

#define PGR_WIDTH 640
#define PGR_HEIGHT 480

void PrintCameraInfo( CameraInfo* pCamInfo )
{
    printf(
        "\n*** CAMERA INFORMATION ***\n"
        "Serial number - %u\n"
        "Camera model - %s\n"
        "Camera vendor - %s\n"
        "Sensor - %s\n"
        "Resolution - %s\n"
        "Firmware version - %s\n"
        "Firmware build time - %s\n\n",
        pCamInfo->serialNumber,
        pCamInfo->modelName,
        pCamInfo->vendorName,
        pCamInfo->sensorInfo,
        pCamInfo->sensorResolution,
        pCamInfo->firmwareVersion,
        pCamInfo->firmwareBuildTime );
}

//
// Helper code to handle a FlyCapture error.
//
void PrintError( Error error , const char *function_name)
{
  fprintf(stderr,"Error in function: %s ", function_name);
  error.PrintErrorTrace();
}

PGRCam::PGRCam(double framerate, double msExposure, int binning, bool trigger, float gain, int camera)
        : triggered(trigger)
{
  connected = false;

  BusManager busMgr;
  unsigned int numCameras;
  error = busMgr.GetNumOfCameras(&numCameras);
  if (error != PGRERROR_OK)
  {
      PrintError( error , "GetNumOfCameras" );
      return;
  }
  if (numCameras < 1) {
    printf("*** Warning: no PGR cameras dectected! ***\n");
    return;
  }

  PGRGuid guid;
  error = busMgr.GetCameraFromIndex(camera, &guid);
  if (error != PGRERROR_OK)
  {
      PrintError( error, "GetCameraFromIndex");
      return;
  }

  // Connect to a camera
  error = cam.Connect(&guid);
  if (error != PGRERROR_OK)
  {
      PrintError( error, "PGR camera CONNECT" );
      return;
  }

  // Get the camera information
  error = cam.GetCameraInfo(&camInfo);
  if (error != PGRERROR_OK)
  {
      PrintError( error, "GetCameraInfo");
      return;
  }

  PrintCameraInfo(&camInfo);

  if (binning != 1) {
    fprintf(stderr, "XXX Binning not yet implemented in new PGR interface\n");
    return;
  } 
  cols = PGR_WIDTH / binning;   // XXX Horrible hack!  Fix this to look it up.
  rows = PGR_HEIGHT / binning;

        /* XXX Fix this later so that it works.
	int mode = 0; // mode 0 = no binning
	if (binning == 2) {
		mode = 1; // mode 1 = 2x2 binning
	}
	err = flycaptureStartCustomImage(context, mode, 0, 0, PGR_WIDTH / binning, PGR_HEIGHT / binning, 100, FLYCAPTURE_MONO8);	
        */

  bool framerateAuto = (framerate == -1);
  if (framerateAuto) {
    framerate = 1000.0f / msExposure;
  }

  bool shutterAuto = (msExposure == -1);
  if (shutterAuto) {
    msExposure = 1000.0f / framerate;
  }

  if (trigger) {
    // Get current trigger settings
    TriggerMode triggerMode;
    error = cam.GetTriggerMode( &triggerMode );
    if (error != PGRERROR_OK)
    {
        PrintError( error, "GetTriggerMode" );
        return;
    }

    // Set camera to trigger mode 0
    triggerMode.onOff = true;
    triggerMode.mode = 1;
    triggerMode.parameter = 0;

    // Triggering the camera externally using source 0.
    triggerMode.source = 0;

    error = cam.SetTriggerMode( &triggerMode );
    if (error != PGRERROR_OK)
    {
        PrintError( error, "SetTriggerMode" );
        return;
    }

  } else {
    fprintf(stderr, "XXX PGR non-triggered mode not yet implemented in new interface\n");
    return;
/*    
    //creating the property struct and setting the type to be GAIN
    Property prop;
    prop.type = GAIN;

    //Reading the property from the camera first, not necessary to do this step
    error = cam.GetProperty(&prop);
    if (error != PGRERROR_OK){
	PrintError( error, "GetProperty" );
	return;
    }

    //setting to absolute value mode
    prop.absControl = true;
    //setting to manual mode
    prop.autoManualMode = false;
    //making sure the property is enabled
    prop.onOff = true;
    //making sure the OnePush feature is disabled
    prop.onePush = false;
    //setting to 0dB
    prop.absValue = gain;

    printf("GAIN = %f\n", gain);

    //setting the property to the camera
    error = cam.SetProperty(&prop);
    if (error != PGRERROR_OK){
	PrintError( error, "SetProperty" );
	return;
    }
*/
  /* XXX Fix this so it works in new interface.
	err = flycaptureSetCameraAbsPropertyEx(context, FLYCAPTURE_FRAME_RATE, false, true, framerateAuto, framerate);
	_HANDLE_ERROR(err, "setting FLYCAPTURE_FRAME_RATE (ex)");
	
	// verify that the exposure is possible given the specified framerate
	if (!framerateAuto) {
		if (msExposure > (1000.0f / framerate)) {
			msExposure = (1000.0f / framerate);
			printf("Exposure limited to %f by specified framerate!\n", msExposure);
		}
	}
	err = flycaptureSetCameraAbsPropertyEx(context, FLYCAPTURE_SHUTTER, false, true, shutterAuto, msExposure);
	_HANDLE_ERROR(err, "setting FLYCAPTURE_SHUTTER");


	err = flycaptureSetColorProcessingMethod(context, FLYCAPTURE_NEAREST_NEIGHBOR_FAST);
	_HANDLE_ERROR(err, "flycaptureSetColorProcessingMethod()");


	err = flycaptureSetCameraRegister(context, 0x12fc, 0x80000000);
	_HANDLE_ERROR(err, "flycaptureSetCameraRegister()");
*/
  }

  // XXX Do we have to poll for trigger ready here?  This happened in the
  // example AsyncTriggerEx.cpp code.

  // Get the camera configuration
  FC2Config config;
  error = cam.GetConfiguration( &config );
  if (error != PGRERROR_OK)
  {
      PrintError( error , "GetConfiguration" );
      return;
  } 
  
  // Otherwise, set the grab timeout to 5 seconds
  // If we're in trigger mode, set the capture timeout to 100 milliseconds,
  // so we won't wait forever for an image.  This will cause the higher-level
  // code to get control again at a reasonable rate.
  if (trigger) {
    config.grabTimeout = 100;
  } else {
    config.grabTimeout = 5000;
  }

  // Set the camera configuration
  error = cam.SetConfiguration( &config );
  if (error != PGRERROR_OK)
  {
      PrintError( error, "SetConfiguration" );
      return;
  }

    //creating the property struct and setting the type to be GAIN
    Property prop;
    prop.type = GAIN;

    //Reading the property from the camera first, not necessary to do this step
    error = cam.GetProperty(&prop);
    if (error != PGRERROR_OK){
	PrintError( error, "GetProperty" );
	return;
    }

    //setting to absolute value mode
    prop.absControl = true;
    //setting to manual mode
    prop.autoManualMode = false;
    //making sure the property is enabled
    prop.onOff = true;
    //making sure the OnePush feature is disabled
    prop.onePush = false;
    //setting to 0dB
    prop.absValue = gain;

    printf("GAIN = %f\n", gain);

    //setting the property to the camera
    error = cam.SetProperty(&prop);
    if (error != PGRERROR_OK){
	PrintError( error, "SetProperty" );
	return;
    }

  // Camera is ready, start capturing images
  error = cam.StartCapture();
  if (error != PGRERROR_OK)
  {
      PrintError( error, "StartCapture" );
      return;
  }   

  connected = true;
}

PGRCam::~PGRCam() {
  // Stop capturing images
  error = cam.StopCapture();
  if (error != PGRERROR_OK)
  {
      PrintError( error, "StopCapture" );
      return;
  }      

  // Turn off trigger mode
  TriggerMode triggerMode;
  error = cam.GetTriggerMode( &triggerMode );
  if (error != PGRERROR_OK)
  {
      PrintError( error, "GetTriggerMode" );
      return;
  }
  triggerMode.onOff = false;
  error = cam.SetTriggerMode( &triggerMode );
  if (error != PGRERROR_OK)
  {
      PrintError( error, "SetTriggerMode" );
      return;
  }    

  // Disconnect the camera
  error = cam.Disconnect();
  if (error != PGRERROR_OK)
  {
      PrintError( error, "Disconnect" );
      return;
  }
}

bool PGRCam::GetNewImage() {
  if (!connected) {
    return false;
  }

  error = cam.RetrieveBuffer( &image );
  if ( (error != PGRERROR_OK) && (error != PGRERROR_TIMEOUT) )
  {
      PrintError( error, "RetrieveBuffer" );
      return false;
  }

  // If we timed out, go ahead and return false.
  if (error == PGRERROR_TIMEOUT) {
    return false;
  }

  // update iRows and iCols
  rows = image.GetRows();
  cols = image.GetCols();

  imgPtr = image.GetData();

  return (error == PGRERROR_OK);
}

unsigned char* PGRCam::GetImagePointer() {
	
	return imgPtr;
}