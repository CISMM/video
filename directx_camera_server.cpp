#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "directx_camera_server.h"
#include <vrpn_BaseClass.h>

// XXX See info on source and target rectangles for how to subset, if wanted.

//#define HACK_TO_REOPEN
//#define	DEBUG

//-----------------------------------------------------------------------
// Helper functions for editing the filter graph:

static HRESULT GetPin(IBaseFilter *pFilter, PIN_DIRECTION PinDir, IPin **ppPin)
{
    IEnumPins  *pEnum;
    IPin       *pPin;
    pFilter->EnumPins(&pEnum);
    while(pEnum->Next(1, &pPin, 0) == S_OK)
    {
        PIN_DIRECTION PinDirThis;
        pPin->QueryDirection(&PinDirThis);
        if (PinDir == PinDirThis)
        {
            pEnum->Release();
            *ppPin = pPin;
            return S_OK;
        }
        pPin->Release();
    }
    pEnum->Release();
    return E_FAIL;  
}

static HRESULT ConnectTwoFilters(IGraphBuilder *pGraph, IBaseFilter *pFirst, IBaseFilter *pSecond)
{
    IPin *pOut = NULL, *pIn = NULL;
    HRESULT hr = GetPin(pFirst, PINDIR_OUTPUT, &pOut);
    if (FAILED(hr)) return hr;
    hr = GetPin(pSecond, PINDIR_INPUT, &pIn);
    if (FAILED(hr)) 
    {
        pOut->Release();
        return E_FAIL;
     }
    hr = pGraph->Connect(pOut, pIn);
    pIn->Release();
    pOut->Release();
    return hr;
}


//-----------------------------------------------------------------------

/** The first time we are called, start the filter graph running in continuous
    mode and grab the first image that comes out.  Later times, grab each new
    image as it comes.  The "mode" parameter tells what mode we are in:
      Mode 0 = Run()
      Mode 1 = Pause()
    */
bool  directx_camera_server::read_one_frame(unsigned minX, unsigned maxX,
			      unsigned minY, unsigned maxY,
			      unsigned exposure_millisecs)
{
  long	scrap;	//< Parameter we don't need to know the value of
  HRESULT hr;

  if (!_status) { return false; };

#ifdef	HACK_TO_REOPEN
  open_and_find_parameters();
  _started_graph = false;
#endif

  // If we have not yet started the media graph running, set it up to do
  // multiple-frame capture and then set it running.
#if 0
  if (!_started_graph) {
    // Set the grabber do not do one-shot mode and buffering.
    // One-shot mode means it will stop the graph from
    // running after a single frame capture; the setting we
    // are using means it will run continuously.
    _pGrabber->SetOneShot(FALSE);
    _pGrabber->SetBufferSamples(TRUE);

    // Run the graph and wait until it captures a frame into its buffer
    //XXX do we need this?pMediaFilter->SetSyncSource(NULL); // Turn off the reference clock.
    // This version sets it without having to query a new interface.  It doesn't seem to make
    // any difference with the Hauppauge card (still only grabs first frame).
    //_pSampleGrabberFilter->SetSyncSource(NULL);

    hr = _pMediaControl->Run();
    if ( (hr != S_OK) && (hr != S_FALSE) ){
      fprintf(stderr,"directx_camera_server::read_one_frame(): Can't run filter graph\n");
      _status = false;
      return false;
    }
    _started_graph = true;
  }
#else
  // Set the grabber one-shot mode and buffering.
  // One-shot mode means it will stop the graph from
  // running after a single frame capture.
  _pGrabber->SetOneShot(TRUE);
  _pGrabber->SetBufferSamples(TRUE);

  // Run the graph and wait until it captures a frame into its buffer
  //XXX do we need this?pMediaFilter->SetSyncSource(NULL); // Turn off the reference clock.
  // This version sets it without having to query a new interface.  It doesn't seem to make
  // any difference with the Hauppauge card (still only grabs first frame).  Also doesn't make
  // any difference with the movies that don't play through.
  //_pSampleGrabberFilter->SetSyncSource(NULL);

  switch (_mode) {
    case 0: // Case 0 = run
      hr = _pMediaControl->Run();
      break;
    case 1: // Case 1 = paused
      hr = _pMediaControl->Pause();
      break;
    default:
      fprintf(stderr, "directx_camera_server::read_one_frame(): Unknown mode (%d)\n", _mode);
      _status = false;
      return false;
  }
  if ( (hr != S_OK) && (hr != S_FALSE) ){
    fprintf(stderr,"directx_camera_server::read_one_frame(): Can't run filter graph\n");
    _status = false;
    return false;
  }
#endif

  // Wait until done or 1 second has passed (timeout).
  // XXX We seem to be okay if the wait for completion returns okay or
  // returns "Wrong state" -- which I think may mean that it has already
  // finished.  Check to see if we are losing frames when this happens.
  // This seems to work okay on Logitech cameras, but does not work properly
  // on the Hauppauge (sp?) WinTV card -- it captures the first frame after
  // the card is opened and then repeatedly reads the same frame out of the
  // card (it thinks it is getting new frames, but they are all the first
  // frame over and over again).  Also, we get the first frame over and over
  // again for some videofile clips (BigBeadSmallMovie, LongOrbit, PerfectOrbit).
  hr = _pEvent->WaitForCompletion(1000, &scrap);
  if ( (hr != S_OK) && (hr != VFW_E_WRONG_STATE) ) {
    if (hr == E_ABORT) {
      fprintf(stderr,"directx_camera_server::read_one_frame(): Timeout\n");
    } else if (hr == VFW_E_WRONG_STATE) {
      fprintf(stderr,"directx_camera_server::read_one_frame(): Filter not running\n");
    } else {
      fprintf(stderr,"directx_camera_server::read_one_frame(): Unknown failure to read\n");
    }
    _status = false;
    return false;
  }

  // Check the size of the required buffer by calling the get routine
  // with a NULL pointer.  Make sure our buffer is large enough to handle
  // the image, then read it in.
  long vidbuflen = 0;
  _pGrabber->GetCurrentBuffer(&vidbuflen, NULL); //< Call with NULL to get length
  if (vidbuflen != (int)_buflen) {
    fprintf(stderr,"directx_camera_server::read_one_frame(): Unexpected buffer size!\n");
    fprintf(stderr,"   (expected %u, got %ld)\n", _buflen, vidbuflen);
    _status = false;
    return false;
  }
  if (S_OK != _pGrabber->GetCurrentBuffer(&vidbuflen, reinterpret_cast<long*>(_buffer))) {
    fprintf(stderr,"directx_camera_server::read_one_frame(): Can't get the buffer!\n");
    _status = false;
    return false;
  }

#ifdef	HACK_TO_REOPEN
  close_device();
#endif

  // Capture timing information and print out how many frames per second
  // are being received.

  { static struct timeval last_print_time;
    struct timeval now;
    static bool first_time = true;
    static int frame_count = 0;

    if (first_time) {
      gettimeofday(&last_print_time, NULL);
      first_time = false;
    } else {
      static	unsigned  last_r = 10000;
      frame_count++;
      gettimeofday(&now, NULL);
      double timesecs = 0.001 * vrpn_TimevalMsecs(vrpn_TimevalDiff(now, last_print_time));
      if (timesecs >= 5) {
	double frames_per_sec = frame_count / timesecs;
	frame_count = 0;
	printf("Received frames per second = %lg\n", frames_per_sec);
	last_print_time = now;
      }
    }
  }

  return true;
}

//---------------------------------------------------------------------
// Open the camera specified and determine its available features and parameters.

bool directx_camera_server::open_and_find_parameters(const int which, unsigned width, unsigned height)
{
  //-------------------------------------------------------------------
  // Create COM and DirectX objects needed to access a video stream.

  // Initialize COM.  This must have a matching uninitialize in
  // the destructor.
#ifdef	DEBUG
  printf("directx_camera_server::open_and_find_parameters(): Before CoInitialize\n");
#endif
  CoInitialize(NULL);

  // Create the filter graph manager
#ifdef	DEBUG
  printf("directx_camera_server::open_and_find_parameters(): Before CoCreateInstance FilterGraph\n");
#endif
  CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, 
		      IID_IGraphBuilder, (void **)&_pGraph);
  if (_pGraph == NULL) {
    fprintf(stderr, "directx_camera_server::open_and_find_parameters(): Can't create graph manager\n");
    return false;
  }
  _pGraph->QueryInterface(IID_IMediaControl, (void **)&_pMediaControl);
  _pGraph->QueryInterface(IID_IMediaEvent, (void **)&_pEvent);

  // Create the Capture Graph Builder.
#ifdef	DEBUG
  printf("directx_camera_server::open_and_find_parameters(): Before CoCreateInstance CaptureGraphBuilder2\n");
#endif
  CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC, 
      IID_ICaptureGraphBuilder2, (void **)&_pBuilder);
  if (_pBuilder == NULL) {
    fprintf(stderr, "directx_camera_server::open_and_find_parameters(): Can't create graph builder\n");
    return false;
  }

  // Associate the graph with the builder.
#ifdef	DEBUG
  printf("directx_camera_server::open_and_find_parameters(): Before SetFilterGraph\n");
#endif
  _pBuilder->SetFiltergraph(_pGraph);

  //-------------------------------------------------------------------
  // Go find a video device to use: in this case, we are using the first
  // one we find.

  // Create the system device enumerator.
#ifdef	DEBUG
  printf("directx_camera_server::open_and_find_parameters(): Before CoCreateInstance SystemDeviceEnum\n");
#endif
  ICreateDevEnum *pDevEnum = NULL;
  CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC, 
      IID_ICreateDevEnum, (void **)&pDevEnum);
  if (pDevEnum == NULL) {
    fprintf(stderr, "directx_camera_server::open_and_find_parameters(): Can't create device enumerator\n");
    return false;
  }

  // Create an enumerator for video capture devices.
#ifdef	DEBUG
  printf("directx_camera_server::open_and_find_parameters(): Before CreateClassEnumerator\n");
#endif
  IEnumMoniker *pClassEnum = NULL;
  pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pClassEnum, 0);
  if (pClassEnum == NULL) {
    fprintf(stderr, "directx_camera_server::open_and_find_parameters(): Can't create video enumerator (no cameras?)\n");
    pDevEnum->Release();
    return false;
  }

#ifdef	DEBUG
  printf("directx_camera_server::open_and_find_parameters(): Before Loop over enumerators\n");
#endif
  ULONG cFetched;
  IMoniker *pMoniker = NULL;
  IBaseFilter *pSrc = NULL;
  // Skip (which - 1) cameras
  int i;
  for (i = 0; i < which-1 ; i++) {
    if (pClassEnum->Next(1, &pMoniker, &cFetched) != S_OK) {
      fprintf(stderr,"directx_camera_server::open_and_find_parameters(): Can't open camera (not enough cameras)\n");
      pMoniker->Release();
      return false;
    }
  }
  // Take the next camera and bind it
  if (pClassEnum->Next(1, &pMoniker, &cFetched) == S_OK) {
    // Bind the first moniker to a filter object.
    pMoniker->BindToObject(0, 0, IID_IBaseFilter, (void**)&pSrc);
    pMoniker->Release();
  } else {
    fprintf(stderr,"directx_camera_server::open_and_find_parameters(): Can't open camera (not enough cameras)\n");
    pMoniker->Release();
    return false;
  }

  pClassEnum->Release();
  pDevEnum->Release();

  //-------------------------------------------------------------------
  // Construct the sample grabber that will be used to snatch images from
  // the video stream as they go by.

  // Create the Sample Grabber.
#ifdef	DEBUG
  printf("directx_camera_server::open_and_find_parameters(): Before CoCreateInstance SampleGrabber\n");
#endif
  CoCreateInstance(CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER,
      IID_IBaseFilter, reinterpret_cast<void**>(&_pSampleGrabberFilter));
  if (_pSampleGrabberFilter == NULL) {
    fprintf(stderr,"directx_camera_server::open_and_find_parameters(): Can't get SampleGrabber filter (not DirectX 8.1?)\n");
    return false;
  }
#ifdef	DEBUG
  printf("directx_camera_server::open_and_find_parameters(): Before QueryInterface\n");
#endif
  _pSampleGrabberFilter->QueryInterface(IID_ISampleGrabber,
      reinterpret_cast<void**>(&_pGrabber));

  // Set the media type to video
#ifdef	DEBUG
  printf("directx_camera_server::open_and_find_parameters(): Before SetMediaType\n");
#endif
  AM_MEDIA_TYPE mt;
  // Ask for video media producers that produce 8-bit RGB
  ZeroMemory(&mt, sizeof(AM_MEDIA_TYPE));
  mt.majortype = MEDIATYPE_Video;	  // Ask for video media producers
  mt.subtype = MEDIASUBTYPE_RGB24;	  // Ask for 8 bit RGB
  _pGrabber->SetMediaType(&mt);

  //-------------------------------------------------------------------
  // Ask for the video resolution that has been passed in.
  // This code is based on the AMCap sample application, and
  // intuiting that we need to use the SetFormat call on the IAMStreamConfig
  // interface; this interface is described in the help pages.
  _pBuilder->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, pSrc,
                            IID_IAMStreamConfig, (void **)&_pStreamConfig);
  if (_pStreamConfig == NULL) {
    fprintf(stderr,"directx_camera_server::open_and_find_parameters(): Can't get StreamConfig interface\n");
    return false;
  }

  mt.pbFormat = (BYTE*)CoTaskMemAlloc(sizeof(VIDEOINFOHEADER));
  VIDEOINFOHEADER *pVideoHeader = (VIDEOINFOHEADER*)mt.pbFormat;
  ZeroMemory(pVideoHeader, sizeof(VIDEOINFOHEADER));
  pVideoHeader->bmiHeader.biBitCount = 24;
  pVideoHeader->bmiHeader.biWidth = width;
  pVideoHeader->bmiHeader.biHeight = height;
  pVideoHeader->bmiHeader.biPlanes = 1;
  pVideoHeader->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  pVideoHeader->bmiHeader.biSizeImage = DIBSIZE(pVideoHeader->bmiHeader);

  // Set the format type and size.
  mt.formattype = FORMAT_VideoInfo;
  mt.cbFormat = sizeof(VIDEOINFOHEADER);

  // Set the sample size.
  mt.bFixedSizeSamples = TRUE;
  mt.lSampleSize = DIBSIZE(pVideoHeader->bmiHeader);

  // Make the call to actually set the video type to what we want.
  if (_pStreamConfig->SetFormat(&mt) != S_OK) {
    fprintf(stderr,"directx_camera_server::open_and_find_parameters(): Can't set resolution to %dx%d\n",
      pVideoHeader->bmiHeader.biWidth, pVideoHeader->bmiHeader.biHeight);
    return false;
  }

  // Clean up the pbFormat header memory we allocated above.
  CoTaskMemFree(mt.pbFormat);

  //-------------------------------------------------------------------
  // Create a NULL renderer that will be used to discard the video frames
  // on the output pin of the sample grabber

#ifdef	DEBUG
  printf("directx_camera_server::open_and_find_parameters(): Before CoCreateInstance NullRenderer\n");
#endif
  IBaseFilter *pNull = NULL;
  CoCreateInstance(CLSID_NullRenderer, NULL, CLSCTX_INPROC_SERVER,
      IID_IBaseFilter, reinterpret_cast<void**>(&pNull));

  //-------------------------------------------------------------------
  // Build the filter graph.  First add the filters and then connect them.

  // pSrc is the capture filter for the video device we found above.
  _pGraph->AddFilter(pSrc, L"Video Capture");

  // Add the sample grabber filter
  _pGraph->AddFilter(_pSampleGrabberFilter, L"SampleGrabber");

  // Add the null renderer filter
  _pGraph->AddFilter(pNull, L"NullRenderer");

  // Connect the output of the video reader to the sample grabber input
  ConnectTwoFilters(_pGraph, pSrc, _pSampleGrabberFilter);

  // Connect the output of the sample grabber to the NULL renderer input
  ConnectTwoFilters(_pGraph, _pSampleGrabberFilter, pNull);
  
  //-------------------------------------------------------------------
  // Find _num_rows and _num_columns, which is the maximum size.
  _pGrabber->GetConnectedMediaType(&mt);
  VIDEOINFOHEADER *pVih;
  if (mt.formattype == FORMAT_VideoInfo) {
      pVih = reinterpret_cast<VIDEOINFOHEADER*>(mt.pbFormat);
  } else {
    fprintf(stderr,"directx_camera_server::open_and_find_parameters(): Can't get video header type\n");
    return false;
  }

  // XXX This is the default number of rows and columns, but we'd like to know the
  // maximum number...

  // Number of rows and columns
  _num_columns = pVih->bmiHeader.biWidth;
  // A negative height indicates that the images are stored non-inverted in Y
  _num_rows = abs(pVih->bmiHeader.biHeight);
  if (_num_rows < 0) {
    _invert_y = false;
  } else {
    _invert_y = true;
  }
  printf("Got %dx%d video\n", _num_columns, _num_rows);

  // Make sure that the image is not compressed and that we have 8 bits
  // per pixel.
  if (pVih->bmiHeader.biCompression != BI_RGB) {
    fprintf(stderr,"directx_camera_server::open_and_find_parameters(): Compression not RGB\n");
    switch (pVih->bmiHeader.biCompression) {
      case BI_RLE8:
	fprintf(stderr,"  (It is BI_RLE8)\n");
	break;
      case BI_RLE4:
	fprintf(stderr,"  (It is BI_RLE4)\n");
      case BI_BITFIELDS:
	fprintf(stderr,"  (It is BI_BITFIELDS)\n");
	break;
      default:
	fprintf(stderr,"  (Unknown compression type)\n");
    }
    return false;
  }
  if (pVih->bmiHeader.biBitCount != 24) {
    fprintf(stderr,"directx_camera_server::open_and_find_parameters(): Not 24 bits (%d)\n",
      pVih->bmiHeader.biBitCount);
    return false;
  }

  //-------------------------------------------------------------------
  // Release resources that won't be used later and return
  pSrc->Release();
  pNull->Release();
  return true;
}

/// Construct but do not open a camera
directx_camera_server::directx_camera_server() :
  _pGraph(NULL),
  _pBuilder(NULL),
  _pMediaControl(NULL),
  _pEvent(NULL),
  _pSampleGrabberFilter(NULL),
  _pGrabber(NULL),
  _pStreamConfig(NULL),
  _started_graph(false)
{
  // No image in memory yet.
  _minX = _maxX = _minY = _maxY = 0;
  _vrpn_buffer=NULL;
  _vrpn_buffer_size=0;
}

/// Open nth available camera with specified resolution.
directx_camera_server::directx_camera_server(int which, unsigned width, unsigned height) :
  _pGraph(NULL),
  _pBuilder(NULL),
  _pMediaControl(NULL),
  _pEvent(NULL),
  _pSampleGrabberFilter(NULL),
  _pGrabber(NULL),
  _pStreamConfig(NULL),
  _started_graph(false),
  _mode(0)
{
  //---------------------------------------------------------------------
  if (!open_and_find_parameters(which, width, height)) {
    fprintf(stderr, "directx_camera_server::directx_camera_server(): Cannot open camera\n");
    _status = false;
    return;
  }

  //---------------------------------------------------------------------
  // Allocate a buffer that is large enough to read the maximum-sized
  // image with no binning.
  _buflen = (unsigned)(_num_rows * _num_columns * 3);	// Expect B,G,R; 8-bits each.
  if ( (_buffer = new unsigned char[_buflen]) == NULL) {
    fprintf(stderr,"directx_camera_server::directx_camera_server(): Out of memory for buffer\n");
    _status = false;
    return;
  }

  // No image in memory yet.
  _minX = _maxX = _minY = _maxY = 0;

#ifdef	HACK_TO_REOPEN
  close_device();
#endif

  _status = true;
  _vrpn_buffer=NULL;
  _vrpn_buffer_size=0;
}

//---------------------------------------------------------------------
// Close the camera and the system.  Free up memory.

void  directx_camera_server::close_device(void)
{
  // Clean up.
  if (_pGrabber) { _pGrabber->Release(); };
  if (_pSampleGrabberFilter) { _pSampleGrabberFilter->Release(); };
  if (_pStreamConfig) { _pStreamConfig->Release(); };
  if (_pEvent) { _pEvent->Release(); };
  if (_pMediaControl) { _pMediaControl->Release(); };
  if (_pBuilder) { _pBuilder->Release(); };
  if (_pGraph) { _pGraph->Release(); };
  CoUninitialize();
}
  
directx_camera_server::~directx_camera_server(void)
{
  close_device();
  if (_vrpn_buffer!=NULL) {
    free(_vrpn_buffer);
    _vrpn_buffer_size=0;
  }
}

bool  directx_camera_server::read_image_to_memory(unsigned minX, unsigned maxX, unsigned minY, unsigned maxY,
					 double exposure_millisecs)
{
  if (!_status) { return false; };

  //---------------------------------------------------------------------
  // In case we fail, clear these
  _minX = _minY = _maxX = _maxY = 0;

  //---------------------------------------------------------------------
  // If the maxes are greater than the mins, set them to the size of
  // the image.
  if (maxX <= minX) {
    minX = 0; maxX = _num_columns - 1;
  }
  if (maxY <= minY) {
    minY = 0; maxY = _num_rows - 1;
  }

  //---------------------------------------------------------------------
  // Clip collection range to the size of the sensor on the camera.
  if (minX < 0) { minX = 0; };
  if (minY < 0) { minY = 0; };
  if (maxX >= _num_columns) { maxX = _num_columns - 1; };
  if (maxY >= _num_rows) { maxY = _num_rows - 1; };

  //---------------------------------------------------------------------
  // Store image size for later use
  _minX = minX; _minY = minY; _maxX = maxX; _maxY = maxY;

  //---------------------------------------------------------------------
  // Set up and read one frame.
  if (!read_one_frame(_minX, _maxX, _minY, _maxY, (int)exposure_millisecs)) {
    fprintf(stderr, "directx_camera_server::read_image_to_memory(): Can't read image\n");
    return false;
  }

  //---------------------------------------------------------------------
  // Invert the image in place if we need to.
  if (_invert_y) { invert_memory_image_in_y(); }

  return true;
}

bool  directx_camera_server::write_memory_to_ppm_file(const char *filename, bool sixteen_bits) const
{
  // XXX This will depend on what kind of pixels we have!

  if (!_status) { return false; };

  //---------------------------------------------------------------------
  // Make sure the region is non-zero (so we've read an image)
  if ( (_maxX <= _minX) || (_maxY <= _minY) ) {
    fprintf(stderr,"directx_camera_server::write_memory_to_ppm_file(): No image in memory\n");
    return false;
  }

  //---------------------------------------------------------------------
  // If we've collected only a subset of the image, this version of the code
  // won't work.
  if ( (_minX != 0) || (_maxX != _num_columns-1) || (_minY != 0) || (_maxY != _num_rows-1) ) {
    fprintf(stderr, "directx_camera_server::write_memory_to_ppm_file(): Can't write subset (not implemented)\n");
    return false;
  }

  //---------------------------------------------------------------------
  // Swap the red and blue values for each triplet in the buffer, since
  // the DirectShow order is BGR and the PPM order is RGB.
  unsigned  char *bufrgb = new unsigned char[_buflen];
  if (bufrgb == NULL) {
    fprintf(stderr,"directx_camera_server::write_memory_to_ppm_file(): Out of memory\n");
    return false;
  }
  unsigned  X,Y;
  unsigned  cols = (_maxX - _minX) + 1;
  for (Y = _minY; Y < _maxY; Y++) {
    for (X = _minX; X < _maxX; X++) {
      bufrgb[ (Y*cols + X) * 3 + 0 ] = _buffer[ (Y*cols + X) * 3 + 2 ];
      bufrgb[ (Y*cols + X) * 3 + 1 ] = _buffer[ (Y*cols + X) * 3 + 1 ];
      bufrgb[ (Y*cols + X) * 3 + 2 ] = _buffer[ (Y*cols + X) * 3 + 0 ];
    }
  }

  //---------------------------------------------------------------------
  // Write out an uncompressed RGB PPM file.  We ignore the "16bits" request
  // because we don't have 16 bits to put in there!
  if (sixteen_bits) {
    fprintf(stderr, "directx_camera_server::write_memory_to_ppm_file(): 16 bits requested, ignoring\n");
  }
  FILE *of = fopen(filename, "wb");
  if (of == NULL) {
    fprintf(stderr, "directx_camera_server::write_memory_to_ppm_file(): Can't open %s for write\n", filename);
    return false;
  }
  if (fprintf(of, "P6\n%d %d\n%d\n", _num_columns, _num_rows, 255) < 0) {
    fprintf(stderr, "directx_camera_server::write_memory_to_ppm_file(): Can't write header to %s\n", filename);
    return false;
  }
  if (fwrite(bufrgb, 1, _buflen, of) != _buflen) {
    fprintf(stderr, "directx_camera_server::write_memory_to_ppm_file(): Can't write data to %s\n", filename);
    return false;
  }
  fclose(of);
  delete [] bufrgb;

  return true;
}

bool	directx_camera_server::get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint8 &val, int RGB) const
{
  if (!_status) { return false; };

  // Switch red and blue, since they are backwards in the record (blue is first).
  RGB = 2 - RGB;

  // XXX This will depend on what kind of pixels we have!
  if ( (_maxX <= _minX) || (_maxY <= _minY) ) {
    fprintf(stderr,"directx_camera_server::get_pixel_from_memory(): No image in memory\n");
    return false;
  }
  if ( (X < _minX) || (X > _maxX) || (Y < _minY) || (Y > _maxY) ) {
    return false;
  }
  unsigned  cols = (_maxX - _minX) + 1;
  val = _buffer[ (Y*cols + X) * 3 + RGB ];
  return true;
}

bool	directx_camera_server::get_pixel_from_memory(unsigned X, unsigned Y, vrpn_uint16 &val, int RGB) const
{
  if (!_status) { return false; };

  // Switch red and blue, since they are backwards in the record (blue is first).
  RGB = 2 - RGB;

  // XXX This will depend on what kind of pixels we have!
  if ( (_maxX <= _minX) || (_maxY <= _minY) ) {
    fprintf(stderr,"directx_camera_server::get_pixel_from_memory(): No image in memory\n");
    return false;
  }
  if ( (X < _minX) || (X > _maxX) || (Y < _minY) || (Y > _maxY) ) {
    return false;
  }
  unsigned  cols = (_maxX - _minX) + 1;
  val = _buffer[ (Y*cols + X) * 3 + RGB ];
  return true;
}

/// Invert the memory that is in the buffer in Y.  This points buffer to the new image.
bool  directx_camera_server::invert_memory_image_in_y(void)
{
  // XXX This will depend on what kind of pixels we have!

  unsigned char	*newbuf;    //< Holds the inverted image
  unsigned y;

  if ( (newbuf = new unsigned char[_buflen]) == NULL) {
    fprintf(stderr,"directx_camera_server::invert_memory_image_in_y(): Out of memory\n");
    return false;
  }
  for (y = 0; y < _num_rows; y++) {
    memcpy(&newbuf[ y * _num_columns * 3], &_buffer[ (_num_rows - 1 - y) * _num_columns * 3], _num_columns * 3);
  }

  // Out with the old and in with the new!
  delete [] _buffer;
  _buffer = newbuf;
  return true;
}

bool directx_camera_server::send_vrpn_image(vrpn_TempImager_Server* svr,vrpn_Synchronized_Connection* svrcon,double g_exposure,int svrchan)
{
    _minX=_minY=0;
    _maxX=_num_columns - 1;
    _maxY=_num_rows - 1;
    read_one_frame(_minX, _maxX, _minY, _maxY, (int)g_exposure);
    unsigned  x,y;
    unsigned  num_x = get_num_columns();
    unsigned  num_y = get_num_rows();
    if (_vrpn_buffer==NULL) {
		_vrpn_buffer_size=sizeof(vrpn_uint16)*num_x*num_y;
		_vrpn_buffer=malloc(_vrpn_buffer_size);
    }
    vrpn_uint16 *data = (vrpn_uint16*) _vrpn_buffer;

    if (data == NULL) {
	    fprintf(stderr, "mainloop_server_code(): Out of memory\n");
	    return false;
    }

    if (!_status) { return false; }
    // Switch red and blue, since they are backwards in the record (blue is first).
    int RGB = 2;
    if ( (_maxX <= _minX) || (_maxY <= _minY) ) {
		fprintf(stderr,"directx_camera_server::get_pixel_from_memory(): No image in memory\n");
		return false;
    }
    unsigned  cols = (_maxX - _minX) + 1;
    unsigned i,j;
    // Loop through each line, fill them in, and send them to the client.
    for (y = __max(0,_minX),i=num_x*(__min(num_y,_maxY)-1),j=0; y < __min(num_y,_maxY); y++,i-=num_x,j+=cols) {
      for (x = __max(0,_minX); x < __min(num_x,_maxX); x++){
	data[i + x] = _buffer[ (j + x) * 3 + RGB ];
      }
    }
    // Send the current frame over to the client in chunks as big as possible (limited by vrpn_IMAGER_MAX_REGION)
    int nRowsPerRegion=vrpn_IMAGER_MAX_REGION/num_x;
    for(y=0;y<num_y;y=__min(num_y,y+nRowsPerRegion)) {
		svr->fill_region(svrchan,0,num_x-1,y,__min(num_y,y+nRowsPerRegion)-1,data+y*num_x);
		svr->send_region();
		svr->mainloop();
    }

    // Mainloop the server connection (once per server mainloop, not once per object).
    svrcon->mainloop();
    return true;
}