#include <stdlib.h>
#include <stdio.h>
#include "cooke_server.h"
#include <vrpn_BaseClass.h>

#ifdef __MINGW32__
#define sprintf_s(buffer, buffer_size, stringbuffer, args...) sprintf(buffer, stringbuffer, ## args)
#endif
#include <PCO_err.h>
#include <SC2_defs.h>

// for text error messages
#define PCO_ERRT_H_CREATE_OBJECT
#include <PCO_errt.h>

const unsigned g_verbosity = 10;

static	unsigned long	duration(struct timeval t1, struct timeval t2)
{
	return (t1.tv_usec - t2.tv_usec) +
	       1000000L * (t1.tv_sec - t2.tv_sec);
}

bool cooke_server::open_and_find_parameters( void )
{
  if (g_verbosity) { printf("cooke_server::open_and_find_parameters() entered\n"); };

  // open the PCO camera
  int success = PCO_OpenCamera( &d_camera, 0 /* camera 0 */ );
  if( success != PCO_NOERROR ) {
    PCO_GetErrorText( success, d_errorText, sizeof(d_errorText) );
    fprintf( stderr, "cooke_server::setupCamera:  Error opening "
	    "the Cooke camera (open).  Code:  0x%x (%s)\n", success, d_errorText );
    return false;
  }

  // make sure the camera isn't recording
  unsigned short recstate = 0;
  success = PCO_GetRecordingState( d_camera, &recstate );
  if( success == PCO_NOERROR && recstate != 0 ) {
    success = PCO_SetRecordingState( d_camera, 0 );
    if( success != PCO_NOERROR ) {
      PCO_GetErrorText( success, d_errorText, sizeof(d_errorText) );
      fprintf( stderr, "cooke_server::open_and_find_parameters:  Error telling"
	      "the camera to stop recording.  Code:  0x%x (%s)\n", success, d_errorText );
      return false;
    }
  }

  // reset the camera settings to default
  success = PCO_ResetSettingsToDefault( d_camera );
  if( success != PCO_NOERROR ) {
    PCO_GetErrorText( success, d_errorText, sizeof(d_errorText) );
    fprintf( stderr, "cooke_server::open_and_find_parameters:  Error setting "
	    "camera to default state.  Code:  0x%x (%s)\n", success, d_errorText );
    return false;
  }

  // validate settings
  success = PCO_ArmCamera( d_camera );
  if( success != PCO_NOERROR ) {
    PCO_GetErrorText( success, d_errorText, sizeof(d_errorText) );
    fprintf( stderr, "cooke_server::open_and_find_parameters:  Error "
	    "validating settings.  Code:  0x%x (%s)\n", success, d_errorText );
    return false;
  }

  // get the camera information
  success = PCO_GetGeneral( d_camera, &d_cameraGeneral );
  if( success != PCO_NOERROR ) {
    PCO_GetErrorText( success, d_errorText, sizeof(d_errorText) );
    fprintf( stderr, "cooke_server::open_and_find_parameters:  Error getting "
	    "camera general information.  Code:  0x%x (%s)\n", success, d_errorText );
    return false;
  }
  success = PCO_GetCameraType( d_camera, &d_cameraType );
  if( success != PCO_NOERROR ) {
    PCO_GetErrorText( success, d_errorText, sizeof(d_errorText) );
    fprintf( stderr, "cooke_server::open_and_find_parameters:  Error getting "
	    "camera type.  Code:  0x%x (%s)\n", success, d_errorText );
    return false;
  }
  switch( d_cameraType.wCamType ) {
    case CAMERATYPE_PCO1200HS:
		printf("PCO.Camera 1200 hs found \n");
		break;
    case CAMERATYPE_PCO1300:
		printf("PCO.Camera 1300 found \n");
		break;
    case CAMERATYPE_PCO1600:
		printf("PCO.Camera 1600 found \n");
		break;
    case CAMERATYPE_PCO2000:
		printf("PCO.Camera 2000 found \n");
		break;
    case CAMERATYPE_PCO4000:
		printf("PCO.Camera 4000 found \n");
		break;
    default:
		printf("PCO.Camera undefined type");
  }
	
  success = PCO_GetSensorStruct( d_camera, &d_cameraSensor );
  if( success != PCO_NOERROR ) {
    PCO_GetErrorText( success, d_errorText, sizeof(d_errorText) );
    fprintf( stderr, "cooke_server::open_and_find_parameters:  Error getting "
	    "camera sensor information.  Code:  0x%x (%s)\n", success, d_errorText );
    return false;
  }
  success = PCO_GetCameraDescription( d_camera, &d_cameraDescription );
  if( success != PCO_NOERROR ) {
    PCO_GetErrorText( success, d_errorText, sizeof(d_errorText) );
    fprintf( stderr, "cooke_server::open_and_find_parameters:  Error getting "
	    "camera description.  Code:  0x%x (%s)\n", success, d_errorText );
    return false;
  }
  d_standardMaxResX = d_cameraDescription.wMaxHorzResStdDESC;
  d_standardMaxResY = d_cameraDescription.wMaxVertResStdDESC;
  _num_columns = d_standardMaxResX;
  _num_rows = d_standardMaxResY;
  printf( "\tstandard resolution:  %d x %d\n", 
		  d_cameraDescription.wMaxHorzResStdDESC, d_cameraDescription.wMaxVertResStdDESC );
  printf( "\tmaximum binning:  %d x %d\n",
		  d_cameraDescription.wMaxBinHorzDESC, d_cameraDescription.wMaxBinVertDESC );
  printf( "\tbinning steps:  %s x %s\n",  d_cameraDescription.wBinHorzSteppingDESC == 1 ? "linear" : "exponential",
		  d_cameraDescription.wBinVertSteppingDESC == 1 ? "linear" : "exponential" );


  if (g_verbosity) { printf("cooke_server::open_and_find_parameters() looking for timing\n"); };
  success = PCO_GetTimingStruct( d_camera, &d_cameraTiming );
  if( success != PCO_NOERROR ) {
    PCO_GetErrorText( success, d_errorText, sizeof(d_errorText) );
    fprintf( stderr, "cooke_server::open_and_find_parameters:  Error getting "
	    "camera timing information.  Code:  0x%x (%s)\n", success, d_errorText );
    return false;
  }
  success = PCO_GetRecordingStruct( d_camera, &d_cameraRecording );
  if( success != PCO_NOERROR ) {
    PCO_GetErrorText( success, d_errorText, sizeof(d_errorText) );
    fprintf( stderr, "cooke_server::open_and_find_parameters:  Error getting "
	    "camera recording information.  Code:  0x%x (%s)\n", success, d_errorText );
    return false;
  }

  // see how many ADCs this is set up to use
  if (g_verbosity) { printf("cooke_server::open_and_find_parameters() looking for ADCs\n"); };
  WORD adcs = 0;
  success = PCO_GetADCOperation( d_camera, &adcs );
  if( success != PCO_NOERROR ) {
    PCO_GetErrorText( success, d_errorText, sizeof(d_errorText) );
    fprintf( stderr, "cooke_server::open_and_find_parameters:  Error getting "
	    "number of ADCs.  Code:  0x%x (%s)\n", success, d_errorText );
    return false;
  }
  printf( "\t using %d ADC(s).\n", adcs );

  // make sure double image mode if off
  // (per email from sufang zhao at cooke)
  if (g_verbosity) { printf("cooke_server::open_and_find_parameters() setting double image off\n"); };
  success = PCO_SetDoubleImageMode( d_camera, 0 );
  if( success != PCO_NOERROR ) {
    PCO_GetErrorText( success, d_errorText, sizeof(d_errorText) );
    fprintf( stderr, "cooke_server::open_and_find_parameters:  Error insuring "
	    "double-image mode is off.  Code:  0x%x (%s)\n", success, d_errorText );
    return false;
  }

  // set the sensor to use only standard pixels
  if (g_verbosity) { printf("cooke_server::open_and_find_parameters() setting sensor size\n"); };
  success = PCO_SetSensorFormat( d_camera, 0x0000 );
  if( success != PCO_NOERROR ) {
    PCO_GetErrorText( success, d_errorText, sizeof(d_errorText) );
    fprintf( stderr, "cooke_server::open_and_find_parameters:  Error setting "
	    "sensor format.  Code:  0x%x (%s)\n", success, d_errorText );
    return false;
  }

  // set binning size
  short binning = 1;
  if (g_verbosity) { printf("cooke_server::open_and_find_parameters() setting binning to %dx%d\n",binning,binning); };
  success = PCO_SetBinning( d_camera, binning, binning );
  if( success != PCO_NOERROR ) {
    PCO_GetErrorText( success, d_errorText, sizeof(d_errorText) );
    fprintf( stderr, "cooke_server::open_and_find_parameters:  Error setting "
	    "binning to %d.  Code:  0x%x (%s)\n", binning, success, d_errorText );
    return false;
  }
  _binning = binning;

  // set the capture area to be the whole imager area.
  if (g_verbosity) { printf("cooke_server::open_and_find_parameters() setting capture area\n"); };
  int maxX = d_standardMaxResX / binning;
  int maxY = d_standardMaxResY / binning;

  WORD left = 1;
  WORD right = maxX;
  WORD top = 1;
  WORD bottom = maxY;
  // WORD left = 1 + 156, right = res_x + 156, top = 1 + 100, bottom = res_y + 100;
  printf( "setting ROI to %d x %d:  (%d,%d) to (%d,%d).  Max is %d x %d.\n",
		  maxX, maxY, left, top, right, bottom, maxX, maxY );
  success = PCO_SetROI( d_camera, left, top, right, bottom );
  if( success != PCO_NOERROR ) {
    PCO_GetErrorText( success, d_errorText, sizeof(d_errorText) );
    fprintf( stderr, "cooke_server::open_and_find_parameters:  Error setting "
	    "ROI to %d x %d:  (%d,%d) to (%d,%d).  Code:  0x%x (%s)\n", 
	    maxX, maxY, left, top, right, bottom, success, d_errorText );
    return false;
  }
  success = PCO_CamLinkSetImageParameters( d_camera, maxX, maxY );
  if( success != PCO_NOERROR ) {
    PCO_GetErrorText( success, d_errorText, sizeof(d_errorText) );
    fprintf( stderr, "cooke_server::open_and_find_parameters:  Error setting "
	    "CamLink board for images (%d x %d).  Code:  0x%x (%s)\n", 
	    maxX, maxY, success, d_errorText );
    return false;
  }
  _minX = _minY = 0;
  _maxX = maxX - 1;
  _maxY = maxY - 1;
  _num_columns = d_standardMaxResX;
  _num_rows = d_standardMaxResY;

  // set trigger mode
  // from the example code -- do we care?
  success = PCO_SetTriggerMode( d_camera, 0 /* auto-trigger */ );
  if( success != PCO_NOERROR ) {
    PCO_GetErrorText( success, d_errorText, sizeof(d_errorText) );
    fprintf( stderr, "cooke_server::open_and_find_parameters:  Error "
	    "setting trigger mode.  Code:  0x%x (%s)\n", success, d_errorText );
    return false;
  }

  // validate settings
  success = PCO_ArmCamera( d_camera );
  if( success != PCO_NOERROR ) {
    PCO_GetErrorText( success, d_errorText, sizeof(d_errorText) );
    fprintf( stderr, "cooke_server::open_and_find_parameters:  Error "
	    "validating settings.  Code:  0x%x (%s)\n", success, d_errorText );
    return false;
  }

  // start recording
  success = PCO_SetRecordingState( d_camera, 1 );
  if( success != PCO_NOERROR ) {
    PCO_GetErrorText( success, d_errorText, sizeof(d_errorText) );
    fprintf( stderr, "cooke_server::acquireImage:  Error starting "
		    "recording.  Code:  0x%x (%s)\n", success, d_errorText );
    return false;
  }

  // ask for 8 bits per pixel

  // turn off auto-exposure
  // XXX We'll need to do this!

  // add our callback
  // Set the color to monochrome mode (turn off filter wheels)
  
  if (g_verbosity) { printf("cooke_server::open_and_find_parameters() exited\n"); };
  return true;
}

cooke_server::cooke_server(unsigned binning) :
  base_camera_server(binning),
  d_myImageBuffer(NULL),
  d_cameraImageBuffer(NULL),
  d_last_exposure_ms(-1)
{
  if (g_verbosity) { printf("cooke_server::cooke_server(bin %dx%d) entered\n", binning, binning); };
  // Not working yet...
  _status = false;

  //---------------------------------------------------------------------
  // set the 'size' paramater of the various Cooke structs to the expected size
  d_cameraGeneral.wSize = sizeof(d_cameraGeneral);
  d_cameraGeneral.strCamType.wSize = sizeof(d_cameraGeneral.strCamType);
  d_cameraType.wSize = sizeof(d_cameraType);
  d_cameraSensor.wSize = sizeof(d_cameraSensor);
  d_cameraSensor.strDescription.wSize = sizeof(d_cameraSensor.strDescription);
  d_cameraDescription.wSize = sizeof(d_cameraDescription);
  d_cameraTiming.wSize = sizeof(d_cameraTiming);
  d_cameraStorage.wSize = sizeof(d_cameraStorage);
  d_cameraRecording.wSize = sizeof(d_cameraRecording);

  // Cooke camera initialization
  if( !open_and_find_parameters( ) ) { return; }

  // create buffers in which to store data
  vrpn_int32 maxExtents[2]; // width then height
  maxExtents[0] = d_standardMaxResX;
  maxExtents[1] = d_standardMaxResY;
  d_maxBufferSize = maxExtents[0] * maxExtents[1];
  if( d_maxBufferSize % 0x1000 ) {  // firewire interface needs the buffer to be (multiple of 4096) + 8192
    d_maxBufferSize = d_maxBufferSize / 0x1000;
    d_maxBufferSize += 2;
    d_maxBufferSize *= 0x1000;
  } else {
    d_maxBufferSize += 0x1000;
  }
  if (g_verbosity) { printf("cooke_server::cooke_server() allocating %d buffers\n", d_maxBufferSize); };
  d_myImageBuffer = new vrpn_uint16[ d_maxBufferSize ];
  if ( (d_myImageBuffer == NULL) ) {
    fprintf(stderr,"cooke_server::cooke_server(): Out of memory\n");
    return;
  }
  d_cameraBufferNumber = -1;  // -1 to have PCO allocate a new buffer
  d_cameraEvent = 0;  // 0 to allocate a new handle

  int success = PCO_AllocateBuffer( d_camera, &d_cameraBufferNumber, d_maxBufferSize*sizeof(vrpn_uint16), 
								    (WORD**) &d_cameraImageBuffer, &d_cameraEvent );
  if( success != PCO_NOERROR ) {
    PCO_GetErrorText( success, d_errorText, sizeof(d_errorText) );
    fprintf( stderr, "nmm_Microscope_SEM_cooke constructor:  Error allocating "
		    "the image buffer.  Code:  0x%x (%s)\n", success, d_errorText );
    return;
  }

  // Set the binning to what was requested.
  set_current_camera_parameters( 0,0, (d_standardMaxResX-1)/_binning,(d_standardMaxResY-1)/_binning,
    binning, 10);

  if (g_verbosity) { printf("cooke_server::cooke_server() exited\n"); };
  _status = true;
}

//---------------------------------------------------------------------
// Close the camera and the system.  Free up memory.

cooke_server::~cooke_server(void)
{
  if (g_verbosity) { printf("cooke_server::~cooke_server() entered\n"); };
  // let the camera clean up after itself
  PCO_CancelImages( d_camera );
  PCO_SetRecordingState( d_camera, 0 );
  PCO_FreeBuffer( d_camera, d_cameraBufferNumber );
  PCO_CloseCamera( d_camera );

  if (d_myImageBuffer) { delete[] d_myImageBuffer; d_myImageBuffer = NULL; }
  if (g_verbosity) { printf("cooke_server::~cooke_server() exited\n"); };
}


//-----------------------------------------------------------------------

bool  read_continuous(HANDLE camera_handle,
		       unsigned short minX, unsigned short maxX,
		       unsigned short minY, unsigned short maxY,
                       unsigned binning,
		       const vrpn_uint32 exposure_time_millisecs)
{
  fprintf(stderr,"cooke_server::read_continuous(): Not yet implemented\n");
  return false;
}

//-----------------------------------------------------------------------

bool  cooke_server::set_current_camera_parameters(int newminX, int newminY,
                                             int newmaxX, int newmaxY,
                                             int newbinning,
                                             vrpn_uint32 newexposure_time_millisecs)
{
  if (g_verbosity) { printf("cooke_server::set_current_camera_parameters() entered\n"); };
  // If none of the parameters have changed, we exit without interrupting
  // the imaging process.
  if ( (newminX == _minX) && (newminY == _minY) &&
       (newmaxX == _maxX) && (newmaxY == _maxY) &&
       (newbinning == _binning) &&
       (static_cast<long>(newexposure_time_millisecs) == d_last_exposure_ms) ) {
    return true;
  }

  // make sure the camera isn't recording
  unsigned short recstate = 0;
  int success = PCO_GetRecordingState( d_camera, &recstate );
  if( success == PCO_NOERROR && recstate != 0 ) {
    success = PCO_SetRecordingState( d_camera, 0 );
    if( success != PCO_NOERROR ) {
      PCO_GetErrorText( success, d_errorText, sizeof(d_errorText) );
      fprintf( stderr, "cooke_server::set_current_camera_parameters():  Error telling"
	      "the camera to stop recording.  Code:  0x%x (%s)\n", success, d_errorText );
      return false;
    }
  }

  // If the requested binning is different, request the new binning value.
  if( static_cast<unsigned>(newbinning) != _binning ) {
    _binning = newbinning;
    if (g_verbosity) { printf("cooke_server::set_current_camera_parameters() set binning to %d\n", newbinning); };

    // check that the current resolution is reasonable with the new binning
    if( static_cast<unsigned>(newbinning) > _binning ) {
      double resFactor = (double) newbinning / (double) _binning;
      int resX = _minX, resY = _minY;
      int maxXres = d_standardMaxResX;
      int maxYres = d_standardMaxResY;
      if( resX * resFactor > maxXres || resY * resFactor > maxYres ) {
	      fprintf( stderr, "cooke_server::set_current_camera_parameters():  "
		      "required area too large for camera: (%d x %d) x %f needed.  "
		      "(%d x %d) available.\n", resX, resY, resFactor, maxXres, maxYres );
	      return false;
      }
    }

    short binAsShort = newbinning;
    success = PCO_SetBinning( d_camera, binAsShort, binAsShort );
    if( success != PCO_NOERROR ) {
    PCO_GetErrorText( success, d_errorText, sizeof(d_errorText) );
      _binning = 1;
      fprintf( stderr, "cooke_server::open_and_find_parameters:  Error setting "
	      "binning to %d.  Code:  0x%x (%s): setting it to %d\n", newbinning, success, d_errorText, _binning);
      if (PCO_SetBinning( d_camera, _binning, _binning ) != PCO_NOERROR) {
        fprintf(stderr,"cooke_server::open_and_find_parameters(): Failed to return binning\n");
        return false;
      }
    }

    // Make sure that we reset the ROI next by making it not match the
    // current one;
    _minX = _minY = 0;
    _maxX = _maxY = 0;
  }

  // If the requested resolution changed, set it to the new value.
  if( (newminX != _minX) || (newminY != _minY) ||
      (newmaxX != _maxX) || (newmaxY != _maxY) ) {

    if (g_verbosity) { printf("cooke_server::set_current_camera_parameters() set ROI to (%d,%d)-(%d,%d)\n", newminX,newminY, newmaxX, newmaxY); };

    // This is not really the resolution, but rather one past the
    // rightmost pixel value; it is not reduced by the mins.
    vrpn_int32 res_x = 0, res_y = 0;
    res_x = newmaxX/_binning + 1;
    res_y = newmaxY/_binning + 1;
    
    // get the max resolution
    int maxXres = (d_standardMaxResX / _binning) + 1;
    int maxYres = (d_standardMaxResY / _binning) + 1;

    // get the old ROI in case we need to revert
    WORD oldLeft, oldRight, oldTop, oldBottom;
    success = PCO_GetROI( d_camera, &oldLeft, &oldTop, &oldRight, &oldBottom );
    if( success != PCO_NOERROR ) {
      PCO_GetErrorText( success, d_errorText, sizeof(d_errorText) );
      fprintf( stderr, "cooke_server::set_current_camera_parameters():  Error "
	      "getting ROI.  Code:  0x%x (%s)\n", success, d_errorText );
      return false;
    }

    // check that the requested resolution isn't too big
    if( res_x > maxXres || res_y > maxYres ) {
      fprintf( stderr, "cooke_server::set_current_camera_parameters():  "
	      "resolution (%d x %d) x %d greater than max (%d x %d).\n",
	      res_x, res_y, _binning, maxXres, maxYres );
      return false;
    }
    
    // Request the new (sub)region.
    WORD left = newminX/_binning + 1;
    WORD right = newmaxX/_binning + 1;
    WORD top = newminY/_binning + 1;
    WORD bottom = newmaxY/_binning + 1;
    success = PCO_SetROI( d_camera, left, top, right, bottom );
    if( success != PCO_NOERROR ) {
      PCO_GetErrorText( success, d_errorText, sizeof(d_errorText) );
      fprintf( stderr, "cooke_server::set_current_camera_parameters():  Error setting "
	      "ROI to %d x %d:  (%d,%d) to (%d,%d).  Code:  0x%x (%s)\n", 
	      res_x, res_y, left, top, right, bottom, success, d_errorText );
      return false;
    }

    success = PCO_CamLinkSetImageParameters( d_camera, res_x, res_y );
    if( success != PCO_NOERROR ) {
      PCO_GetErrorText( success, d_errorText, sizeof(d_errorText) );
      fprintf( stderr, "cooke_server::set_current_camera_parameters():  Error setting "
	      "CamLink board for images (%d x %d).  Code:  0x%x (%s)\n", 
	      res_x, res_y, success, d_errorText );
      // try to go back to the old ROI
      PCO_SetROI( d_camera, oldLeft, oldTop, oldRight, oldBottom );
      return false;
    }

    _minX = newminX;
    _maxX = newmaxX;
    _minY = newminY;
    _maxY = newmaxY;
  }

  // If the exposure changed, try to set a new exposure.
  if (newexposure_time_millisecs != d_last_exposure_ms) {
    success = PCO_SetDelayExposureTime( d_camera, 
	    0, // delay
	    newexposure_time_millisecs, 
	    2, // Timebase: 0-ns; 1-us; 2-ms 
	    2 // Timebase: 0-ns; 1-us; 2-ms 
	    );
    if( success != PCO_NOERROR ) {
      fprintf( stderr, "cooke_server::set_current_camera_parameters():  Error setting "
	      "exposure.  Code:  0x%x\n", success );
      return false;
    }

    d_last_exposure_ms = newexposure_time_millisecs;
  }

  // validate settings
  success = PCO_ArmCamera( d_camera );
  if( success != PCO_NOERROR ) {
	  PCO_GetErrorText( success, d_errorText, sizeof(d_errorText) );
	  fprintf( stderr, "cooke_server::set_current_camera_parameters():  Error "
		  "validating settings (arm).  Code:  0x%x (%s)\n", success, d_errorText );
	  return false;
  }

  // start recording
  success = PCO_SetRecordingState( d_camera, 1 );
  if( success != PCO_NOERROR ) {
    PCO_GetErrorText( success, d_errorText, sizeof(d_errorText) );
    fprintf( stderr, "cooke_server::set_current_camera_parameters():  Error starting "
		    "recording.  Code:  0x%x (%s)\n", success, d_errorText );
    return false;
  }

  if (g_verbosity) { printf("cooke_server::set_current_camera_parameters() exited\n"); };
  return true;
}

//-----------------------------------------------------------------------

bool  cooke_server::read_one_frame(HANDLE camera_handle,
		       unsigned short minX, unsigned short maxX,
		       unsigned short minY, unsigned short maxY,
                       unsigned binning,
		       const vrpn_uint32 exposure_time_millisecs)
{
  if (g_verbosity > 9) { printf("  cooke_server::read_one_frame() entering\n"); };

  // Check for any changes in parameters compared to the last image.
  // If there are changes, apply them to the camera.
  if (!set_current_camera_parameters(minX, minY, maxX, maxY, binning, exposure_time_millisecs)) {
    fprintf(stderr,"cooke_server::read_one_frame(): Could not set parameters\n");
    return false;
  }

  // Read one frame into our memory buffer (d_myImageBuffer, a 16-bit unsigned-short buffer).

  // make sure the camera is recording
  PCO_ArmCamera( d_camera );
  PCO_SetRecordingState( d_camera, 1 );

  if (g_verbosity > 9) { printf("  cooke_server::read_one_frame() adding buffer %dx%d\n",
    get_num_columns(), get_num_rows()); };
  int success = PCO_AddBufferEx( d_camera, 0, 0, d_cameraBufferNumber, get_num_columns(), get_num_rows(), 16 );
  if( success != PCO_NOERROR ) {
    PCO_GetErrorText( success, d_errorText, sizeof(d_errorText) );
    fprintf( stderr, "cooke_server::read_one_frame:  "
            "Error adding the image buffer.  Code:  0x%x (%s)\n", success, d_errorText );
    return false;
  }
  
  if (g_verbosity > 9) { printf("  cooke_server::read_one_frame() waiting for image\n"); };
  success = WaitForSingleObject( d_cameraEvent, exposure_time_millisecs + 1000 ); // Wait until picture arrives
  ResetEvent( d_cameraEvent );
  if( success != WAIT_OBJECT_0 ) {
    fprintf( stderr, "cooke_server::read_one_frame:  Error waiting "
	    "for the image.  Code:  0x%x (%s)\n", success, d_errorText );
    return false;
  }

  // Copy the image from the camera image buffer into my image buffer.
  if (g_verbosity > 9) { printf("  cooke_server::read_one_frame() copying %d bytes\n", get_num_columns() * get_num_rows() * sizeof(vrpn_uint16)); };
  if (d_myImageBuffer == NULL) {
    fprintf(stderr,"cooke_server::read_one_frame(): Zero image buffer!\n");
    return false;
  }
  if (d_cameraImageBuffer == NULL) {
    fprintf(stderr,"cooke_server::read_one_frame(): Zero camera buffer!\n");
    return false;
  }
  memcpy(d_myImageBuffer, d_cameraImageBuffer, get_num_columns() * get_num_rows() * sizeof(vrpn_uint16) );

  if (g_verbosity > 9) { printf("  cooke_server::read_one_frame() exiting\n"); };
  return true;
}

bool  cooke_server::read_image_to_memory(unsigned minX, unsigned maxX, unsigned minY, unsigned maxY,
					 double exposure_time_millisecs)
{
  if (g_verbosity) {
    printf("cooke_server::read_image_to_memory() entered\n");
    printf("  minX %d, maxX %d, minY %d, maxY %d, exposure %lfms\n",minX, maxX, minY, maxY, exposure_time_millisecs);
  };
  //---------------------------------------------------------------------
  // In case we fail, clear these

  //---------------------------------------------------------------------
  // Set the size of the window to include all pixels if there were not
  // any binning.  This means adding all but 1 of the binning back at
  // the end to cover the pixels that are within that bin.
  _minX = minX * _binning;
  _maxX = maxX * _binning + (_binning-1);
  _minY = minY * _binning;
  _maxY = maxY * _binning + (_binning-1);

  //---------------------------------------------------------------------
  // If the maxes are greater than the mins, set them to the size of
  // the image.
  if (_maxX < _minX) {
    _minX = 0; _maxX = _num_columns - 1;
  }
  if (_maxY < _minY) {
    _minY = 0; _maxY = _num_rows - 1;
  }

  //---------------------------------------------------------------------
  // Clip collection range to the size of the sensor on the camera.
  if (_minX < 0) { _minX = 0; };
  if (_minY < 0) { _minY = 0; };
  if (_maxX >= _num_columns) { _maxX = _num_columns - 1; };
  if (_maxY >= _num_rows) { _maxY = _num_rows - 1; };

  return read_one_frame(d_camera, _minX, _maxX, _minY, _maxY, _binning, (int)exposure_time_millisecs);
}

bool cooke_server::read_continuous(HANDLE camera_handle,
		       unsigned short minX, unsigned short maxX,
		       unsigned short minY, unsigned short maxY,
                       unsigned binning,
		       const vrpn_uint32 exposure_time_millisecs)
{
  fprintf(stderr,"cooke_server::read_continuous(): Not yet implemented\n");
  return false;
}

static	vrpn_uint16 clamp_gain(vrpn_uint16 val, double gain, double clamp = 65535.0)
{
  double result = val * gain;
  if (result > clamp) { result = clamp; }
  return (vrpn_uint16)result;
}

bool  cooke_server::write_memory_to_ppm_file(const char *filename, int gain, bool sixteen_bits) const
{
  //---------------------------------------------------------------------
  // Make sure the region is non-zero (so we've read an image)
  if ( (_maxX <= _minX) || (_maxY <= _minY) ) {
    fprintf(stderr,"cooke_server::write_memory_to_ppm_file(): No image in memory\n");
    return false;
  }

  //---------------------------------------------------------------------
  // If we are not doing 16 bits, map the 12 bits to the range 0-255, and then write out an
  // uncompressed 8-bit grayscale PPM file with the values scaled to this range.
  if (!sixteen_bits) {
    vrpn_uint16	  *vals = (vrpn_uint16 *)d_myImageBuffer;
    unsigned char *pixels;
    // This buffer will be oversized if min and max don't span the whole window.
    if ( (pixels = new unsigned char[d_maxBufferSize]) == NULL) {
      fprintf(stderr, "Can't allocate memory for stored image\n");
      return false;
    }
    unsigned r,c;
    vrpn_uint16 minimum = clamp_gain(vals[0],gain);
    vrpn_uint16 maximum = clamp_gain(vals[0],gain);
    vrpn_uint16 cols = (_maxX - _minX)/_binning + 1;
    vrpn_uint16 rows = (_maxY - _minY)/_binning + 1;
    for (r = 0; r < rows; r++) {
      for (c = 0; c < cols; c++) {
	if (clamp_gain(vals[r*cols + c],gain) < minimum) { minimum = clamp_gain(vals[r*cols+c],gain, 4095); }
	if (clamp_gain(vals[r*cols + c],gain) > maximum) { maximum= clamp_gain(vals[r*cols+c],gain, 4095); }
      }
    }
    printf("Minimum = %d, maximum = %d\n", minimum, maximum);
    vrpn_uint16 offset = 0;
    double scale = gain;
    for (r = 0; r < rows; r++) {
      for (c = 0; c < cols; c++) {
	pixels[r*cols + c] = clamp_gain(vals[r*cols+c] - offset, scale, 4095) >> 4;
      }
    }
    FILE *of = fopen(filename, "wb");
    fprintf(of, "P5\n%d %d\n%d\n", cols, rows, 255);
    fwrite(pixels, 1, cols*rows, of);
    fclose(of);
    delete [] pixels;

  // If we are doing 16 bits, write out a 16-bit file.
  } else {
    vrpn_uint16 *vals = (vrpn_uint16 *)d_myImageBuffer;
    unsigned r,c;
    vrpn_uint16 *pixels;
    // This buffer will be oversized if min and max don't span the whole window.
    if ( (pixels = new vrpn_uint16[d_maxBufferSize]) == NULL) {
      fprintf(stderr, "Can't allocate memory for stored image\n");
      return false;
    }
    vrpn_uint16 minimum = clamp_gain(vals[0],gain);
    vrpn_uint16 maximum = clamp_gain(vals[0],gain);
    vrpn_uint16 cols = get_num_columns();
    vrpn_uint16 rows = get_num_rows();
    for (r = 0; r < rows; r++) {
      for (c = 0; c < cols; c++) {
	if (clamp_gain(vals[r*cols + c],gain) < minimum) { minimum = clamp_gain(vals[r*cols+c],gain); }
	if (clamp_gain(vals[r*cols + c],gain) > maximum) { maximum = clamp_gain(vals[r*cols+c],gain); }
      }
    }
    printf("Minimum = %d, maximum = %d\n", minimum, maximum);
    vrpn_uint16 offset = 0;
    double scale = gain;
    for (r = 0; r < rows; r++) {
      for (c = 0; c < cols; c++) {
	pixels[r*cols + c] = clamp_gain(vals[r*cols+c] - offset, scale);
      }
    }
    FILE *of = fopen(filename, "wb");
    fprintf(of, "P5\n%d %d\n%d\n", cols, rows, 4095);
    fwrite(pixels, sizeof(vrpn_uint16), cols*rows, of);
    fclose(of);
    delete [] pixels;
  }
  return true;
}

//---------------------------------------------------------------------
// Map the 16 bits to the range 0-255, and return the result
bool	cooke_server::get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint8 &val, int RGB) const
{
  if ( (_maxX <= _minX) || (_maxY <= _minY) ) {
    fprintf(stderr,"cooke_server::get_pixel_from_memory(): No image in memory\n");
    return false;
  }
  if (RGB != 0) {
    fprintf(stderr,"cooke_server::get_pixel_from_memory(): Can't select other than 0th color\n");
    return false;
  }
  if ( (X < _minX/_binning) || (X > _maxX/_binning) || (Y < _minY/_binning) || (Y > _maxY/_binning) ) {
    return false;
  }

  // The Cooke library sticks it into the full-sized buffer, but into a subset
  // of the buffer.  We read it out in-place as if the full buffer were being
  // used.
  vrpn_uint16	*vals = (vrpn_uint16 *)d_myImageBuffer;
  vrpn_uint16	cols = get_num_columns();
  val = (vrpn_uint8)(vals[Y*cols + X] >> 8);
  return true;
}

bool	cooke_server::get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint16 &val, int RGB) const
{
  if ( (_maxX <= _minX) || (_maxY <= _minY) ) {
    fprintf(stderr,"cooke_server::get_pixel_from_memory(): No image in memory\n");
    return false;
  }
  if (RGB != 0) {
    fprintf(stderr,"cooke_server::get_pixel_from_memory(): Can't select other than 0th color\n");
    return false;
  }
  if ( (X < _minX/_binning) || (X > _maxX/_binning) || (Y < _minY/_binning) || (Y > _maxY/_binning) ) {
    return false;
  }

  // The Cooke library sticks it into the full-sized buffer, but into a subset
  // of the buffer.  We read it out in-place as if the full buffer were being
  // used.
  vrpn_uint16	*vals = (vrpn_uint16 *)d_myImageBuffer;
  vrpn_uint16	cols = get_num_columns();
  val = vals[Y*cols + X];
  return true;
}

bool cooke_server::send_vrpn_image(vrpn_Imager_Server* svr,vrpn_Connection* svrcon,double g_exposure,int svrchan, int num_chans)
{
    // Send the current frame over to the client in chunks as big as possible (limited by vrpn_IMAGER_MAX_REGION)
    vrpn_uint16 cols = (_maxX - _minX)/_binning + 1;
    vrpn_uint16 rows = (_maxY - _minY)/_binning + 1;
    if (g_verbosity > 9) { printf("cooke_server::send_vrpn_image() sending %dx%d image\n", cols, rows); };
    unsigned  num_x = cols;
    unsigned  num_y = rows;
    //XXX Should be 16-bit version later
    int nRowsPerRegion = vrpn_IMAGER_MAX_REGIONu8/(num_x*sizeof(vrpn_uint8)) - 1;
    unsigned y;

    //XXX This is hacked to send the upper 8 bits of each value. Need to modify reader to handle 16-bit ints.
    // For these, stride will be 1 and offset will be 0, and the code will use memcpy() to copy the values.
    // Note that the offset depends on the endian-ness of the computer.
    const int stride = 2;
    const int offset = 1;
    svr->send_begin_frame(0, cols-1, 0, rows-1);
    for(y=0;y<num_y;y=min(num_y,y+nRowsPerRegion)) {
      svr->send_region_using_base_pointer(svrchan,0,num_x-1,y,min(num_y,y+nRowsPerRegion)-1,
	(vrpn_uint8 *)d_myImageBuffer + offset, stride, num_x * stride);
      svr->mainloop();
    }
    svr->send_end_frame(0, cols-1, 0, rows-1);
    svr->mainloop();

    // Mainloop the server connection (once per server mainloop, not once per object).
    svrcon->mainloop();
    return true;
}


// Write the texture, using a virtual method call appropriate to the particular
// camera type.  NOTE: At least the first time this function is called,
// we must write a complete texture, which may be larger than the actual bytes
// allocated for the image.  After the first time, and if we don't change the
// image size to be larger, we can use the subimage call to only write the
// pixels we have.
bool cooke_server::write_to_opengl_texture(GLuint tex_id)
{
  // Note: Check the GLubyte or GLushort or whatever in the temporary buffer!
  const GLint   NUM_COMPONENTS = 1;
  const GLenum  FORMAT = GL_LUMINANCE;
  const GLenum  TYPE = GL_UNSIGNED_SHORT;
  const void*   BASE_BUFFER = d_myImageBuffer;
  const void*   SUBSET_BUFFER = &d_myImageBuffer[NUM_COMPONENTS * ( _minX + get_num_columns()*_minY )];

  return write_to_opengl_texture_generic(tex_id, NUM_COMPONENTS, FORMAT, TYPE,
    BASE_BUFFER, SUBSET_BUFFER, _minX, _minY, _maxX, _maxY);
}
