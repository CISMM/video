#include <stdio.h>
#include "PGRCam.h"

using namespace FlyCapture2;

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

void PrintFormat7Capabilities( Format7Info fmt7Info )
{
    printf(
		"Camera Initialized into FORMAT_7 ...\n\n"
		"*** FORMAT_7 CAPABILITIES ***\n"
        "Max image pixels: (%u, %u)\n"
        "Image Unit size: (%u, %u)\n"
        "Offset Unit size: (%u, %u)\n"
        "Pixel format bitfield: 0x%08x\n\n",
        fmt7Info.maxWidth,
        fmt7Info.maxHeight,
        fmt7Info.imageHStepSize,
        fmt7Info.imageVStepSize,
        fmt7Info.offsetHStepSize,
        fmt7Info.offsetVStepSize,
        fmt7Info.pixelFormatBitField );
}

// Helper code to handle a FlyCapture error.
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
  if(error != PGRERROR_OK){
    PrintError(error, "GetNumOfCameras");
    return;
  }
  
  if(numCameras<1){
    printf("*** Warning: no PGR cameras dectected! ***\n");
    return;
  }
  
  PGRGuid guid;
  error = busMgr.GetCameraFromIndex(camera, &guid);
  if(error != PGRERROR_OK){
    PrintError(error, "GetCameraFromIndex");
    return;
  }
  
  // Connect to a camera
  error = cam.Connect(&guid);
  if(error != PGRERROR_OK){
    PrintError(error, "PGR camera CONNECT");
    return;
  }
  
  // Get the camera information
  error = cam.GetCameraInfo(&camInfo);
  if(error != PGRERROR_OK){
    PrintError(error, "GetCameraInfo");
    return;
  }
  
  // Print the camera information
  PrintCameraInfo(&camInfo);

  // Initialize the camera into Format7 custom imaging mode
  const PixelFormat k_fmt7PixFmt = PIXEL_FORMAT_MONO8;
  
  Mode k_fmt7Mode;
  if(binning == 1)
    k_fmt7Mode = MODE_0; // MODE_0 = no binning
  else
    k_fmt7Mode = MODE_1; // MODE_1 = 2x2 binning

  // Query for available Format 7 modes
  Format7Info fmt7Info;
  bool supported;
  fmt7Info.mode = k_fmt7Mode;
  error = cam.GetFormat7Info(&fmt7Info, &supported);
  if(error != PGRERROR_OK){
    PrintError(error, "GetFormat7Info");
    return;
  }

  PrintFormat7Capabilities(fmt7Info);

  if((k_fmt7PixFmt & fmt7Info.pixelFormatBitField)==0){
    // Pixel format not supported!
	printf("Pixel format is not supported\n");
    return;
  }
    
  Format7ImageSettings fmt7ImageSettings;
  fmt7ImageSettings.mode = k_fmt7Mode;
  fmt7ImageSettings.offsetX = 0;
  fmt7ImageSettings.offsetY = 0;
  fmt7ImageSettings.width = fmt7Info.maxWidth;
  fmt7ImageSettings.height = fmt7Info.maxHeight;
  fmt7ImageSettings.pixelFormat = k_fmt7PixFmt;

  bool valid;
  Format7PacketInfo fmt7PacketInfo;

  // Validate the settings to make sure that they are valid
  error = cam.ValidateFormat7Settings(
    &fmt7ImageSettings,
    &valid,
    &fmt7PacketInfo);
  if(error!=PGRERROR_OK){
    PrintError(error, "ValidateFormat7Settings");
    return;
  }

  if(!valid){
    // Settings are not valid
    printf("Format7 settings are not valid\n");
    return;
  }

  // Set the settings to the camera
  error = cam.SetFormat7Configuration(
    &fmt7ImageSettings,
	fmt7PacketInfo.maxBytesPerPacket);
  if(error!=PGRERROR_OK){
    PrintError(error, "SetFormat7Configuration");
    return;
  }

  // Get the camera configuration to configure the timeout setting
  FC2Config config;
  error = cam.GetConfiguration(&config);
  if(error!=PGRERROR_OK){
    PrintError(error, "GetConfiguration");
    return;
  } 

  // Configure the camera triggered mode
  if(trigger){
    TriggerMode triggerMode;
    error = cam.GetTriggerMode(&triggerMode);
    if(error!=PGRERROR_OK){
      PrintError(error, "GetTriggerMode");
      return;
    }
    
    // Set camera to trigger mode 0
    triggerMode.onOff = true;
    triggerMode.mode = 1;
    triggerMode.parameter = 0;
    
    // Triggering the camera externally using source 0.
    triggerMode.source = 0;
    
    error = cam.SetTriggerMode(&triggerMode);
    if(error!=PGRERROR_OK){
      PrintError(error, "SetTriggerMode");
      return;
    }

	// If we're in trigger mode, set the capture timeout to 100 milliseconds,
    // so we won't wait forever for an image. This will cause the higher-level
    // code to get control again at a reasonable rate. Otherwise, set the grab 
    // timeout to 5 seconds.
    config.grabTimeout = 100;
  }
  // Configure the camera non-triggered mode
  else{
    // Set the grab timeout to 5 seconds
    config.grabTimeout = 5000;

    //printf("XXX PGR non-triggered mode not yet implemented in new interface\n");
    //return;
  }

  // Set the camera configuration to configure the timeout setting
  error = cam.SetConfiguration(&config);
  if(error!=PGRERROR_OK){
    PrintError(error, "SetConfiguration");
    return;
  }

  // Configure the camera GAIN setting
  Property prop;
  prop.type = GAIN;

  // Reading the GAIN value from the camera
  error = cam.GetProperty(&prop);
  if(error!=PGRERROR_OK){
    PrintError(error, "GetProperty GAIN");
    return;
  }
  
  // Setting to absolute value mode
  prop.absControl = true;
  // Setting to manual mode
  prop.autoManualMode = false;
  // Making sure the property is enabled
  prop.onOff = true;
  // Making sure the OnePush feature is disabled
  prop.onePush = false;
  // Setting to 0dB
  prop.absValue = gain;

  printf("GAIN = %.2f\n\n", gain);

  // Setting the property to the camera
  error = cam.SetProperty(&prop);
  if (error!=PGRERROR_OK){
  	PrintError(error, "SetProperty GAIN");
  	return;
  }

  // Camera is ready, start capturing images 
  cols = fmt7Info.maxWidth;
  rows = fmt7Info.maxHeight;

  error = cam.StartCapture();
  if(error != PGRERROR_OK){
    PrintError(error, "StartCapture");
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