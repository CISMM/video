#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "directx_camera_server.h"
#include <vrpn_BaseClass.h>


//-----------------------------------------------------------------------
// Helper functions for editing the filter graph:

HRESULT GetPin(IBaseFilter *pFilter, PIN_DIRECTION PinDir, IPin **ppPin)
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

HRESULT ConnectTwoFilters(IGraphBuilder *pGraph, IBaseFilter *pFirst, IBaseFilter *pSecond)
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

bool  directx_camera_server::read_one_frame(unsigned minX, unsigned maxX,
			      unsigned minY, unsigned maxY,
			      unsigned exposure_time)
{
  long	scrap;	//< Parameter we don't need

  if (!_status) { return false; };

  // Set the grabber one-shot mode and buffering.
  // One-shot mode means it will stop the graph from
  // running after a single frame capture.
  _pGrabber->SetOneShot(TRUE);
  _pGrabber->SetBufferSamples(TRUE);

  // Run the graph and wait until it captures a frame into its buffer
  //XXX do we need this? pMediaFilter->SetSyncSource(NULL); // Turn off the reference clock.
  _pMediaControl->Run(); // Run the graph.
  // Wait until done or 1 second has passed (timeout)
  if (S_OK != _pEvent->WaitForCompletion(1000, &scrap)) {
    fprintf(stderr,"directx_camera_server::read_one_frame(): Timeout\n");
    return false;
  }

  // Check the size of the required buffer by calling the get routine
  // with a NULL pointer.  Make sure our buffer is large enough to handle
  // the image, then read it in.
  long vidbuflen = 0;
  _pGrabber->GetCurrentBuffer(&vidbuflen, NULL); //< Call with NULL to get length
  if (vidbuflen != (int)_buflen) {
    fprintf(stderr,"directx_camera_server::read_one_frame(): Unexpected buffer size!\n");
    _status = false;
    return false;
  }
  _pGrabber->GetCurrentBuffer(&vidbuflen, reinterpret_cast<long*>(_buffer));

  return true;
}

//---------------------------------------------------------------------
// Open the camera and determine its available features and parameters.

bool directx_camera_server::open_and_find_parameters(void)
{
  //-------------------------------------------------------------------
  // Create COM and DirectX objects needed to access a video stream.

  // Initialize COM.  This must have a matching uninitialize in
  // the descructor.
  CoInitialize(NULL);

  // Create the filter graph manager
  CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, 
		      IID_IGraphBuilder, (void **)&_pGraph);
  _pGraph->QueryInterface(IID_IMediaControl, (void **)&_pMediaControl);
  _pGraph->QueryInterface(IID_IMediaEvent, (void **)&_pEvent);

  // Create the Capture Graph Builder.
  CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC, 
      IID_ICaptureGraphBuilder2, (void **)&_pBuilder);

  // Associate the graph with the builder.
  _pBuilder->SetFiltergraph(_pGraph);    

  //-------------------------------------------------------------------
  // Go find a video device to use: in this case, we are using the first
  // one we find.

  // Create the system device enumerator.
  ICreateDevEnum *pDevEnum = NULL;
  CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC, 
      IID_ICreateDevEnum, (void **)&pDevEnum);

  // Create an enumerator for video capture devices.
  IEnumMoniker *pClassEnum = NULL;
  pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pClassEnum, 0);

  ULONG cFetched;
  IMoniker *pMoniker = NULL;
  IBaseFilter *pSrc = NULL;
  if (pClassEnum->Next(1, &pMoniker, &cFetched) == S_OK)
  {
      // Bind the first moniker to a filter object.
      pMoniker->BindToObject(0, 0, IID_IBaseFilter, (void**)&pSrc);
      pMoniker->Release();
  }
  pClassEnum->Release();
  pDevEnum->Release();


  //-------------------------------------------------------------------
  // Construct the sample grabber that will be used to snatch images from
  // the video stream as they go by.

  // Create the Sample Grabber.
  CoCreateInstance(CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER,
      IID_IBaseFilter, reinterpret_cast<void**>(&_pSampleGrabberFilter));
  _pSampleGrabberFilter->QueryInterface(IID_ISampleGrabber,
      reinterpret_cast<void**>(&_pGrabber));

  // Set the media type to video
  AM_MEDIA_TYPE mt;
  ZeroMemory(&mt, sizeof(AM_MEDIA_TYPE));
  mt.majortype = MEDIATYPE_Video;
  _pGrabber->SetMediaType(&mt);

  //-------------------------------------------------------------------
  // Create a NULL renderer that will be used to discard the video frames
  // on the output pin of the sample grabber

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

  // XXX This is the default number of rows and columne, but we'd like to know the
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

  // Make sure that the image is not compressed and that we have 8 bits
  // per pixel.
  if ( (pVih->bmiHeader.biCompression != BI_RGB) || (pVih->bmiHeader.biBitCount != 24) ) {
    fprintf(stderr,"directx_camera_server::open_and_find_parameters(): Unsupported image format\n");
    return false;
  }

  //-------------------------------------------------------------------
  // Release resources that won't be used later and return
  pSrc->Release();
  pNull->Release();
  return true;
}

directx_camera_server::directx_camera_server(void)
{
  //---------------------------------------------------------------------

  if (!open_and_find_parameters()) {
    fprintf(stderr, "directx_camera_server::directx_camera_server(): Cannot open camera\n");
    _status = false;
    return;
  }

  //---------------------------------------------------------------------
  // Allocate a buffer that is large enough to read the maximum-sized
  // image with no binning.
  _buflen = (unsigned)(_num_rows * _num_columns * 3);	// Expect B,G,R; 8-bits each.
  if ( (_buffer = new char[_buflen]) == NULL) {
    fprintf(stderr,"directx_camera_server::directx_camera_server(): Cannot allocate enough locked memory for buffer\n");
    _status = false;
    return;
  }

  //---------------------------------------------------------------------
  // No image in memory yet.
  _minX = _minY = _maxX = _maxY = 0;

  _status = true;
}

//---------------------------------------------------------------------
// Close the camera and the system.  Free up memory.

directx_camera_server::~directx_camera_server(void)
{
  // Clean up.
  _pGrabber->Release();
  _pSampleGrabberFilter->Release();
  _pEvent->Release();
  _pMediaControl->Release();
  _pBuilder->Release();
  _pGraph->Release();
  CoUninitialize();
}

bool  directx_camera_server::read_image_to_memory(unsigned minX, unsigned maxX, unsigned minY, unsigned maxY,
					 double exposure_time)
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
  if (!read_one_frame(_minX, _maxX, _minY, _maxY, (int)exposure_time)) {
    fprintf(stderr, "directx_camera_server::read_image_to_memory(): Can't read image\n");
    return false;
  }

  //---------------------------------------------------------------------
  // Invert the image in place if we need to.
  if (_invert_y) { invert_memory_image_in_y(); }

  return true;
}

bool  directx_camera_server::write_memory_to_ppm_file(const char *filename) const
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
  if ( (_minX != 0) || (_maxX != (int)_num_columns-1) || (_minY != 0) || (_maxY != (int)_num_rows-1) ) {
    fprintf(stderr, "directx_camera_server::write_memory_to_ppm_file(): Can't write subset (not implemented)\n");
    return false;
  }

  //---------------------------------------------------------------------
  // Write out an uncompressed RGB PPM file.
  FILE *of = fopen(filename, "wb");
  if (of == NULL) {
    fprintf(stderr, "directx_camera_server::write_memory_to_ppm_file(): Can't open %s for write\n", filename);
    return false;
  }
  if (fprintf(of, "P6\n%d %d\n%d\n", _num_columns, _num_rows, 255) < 0) {
    fprintf(stderr, "directx_camera_server::write_memory_to_ppm_file(): Can't write header to %s\n", filename);
    return false;
  }
  // XXX Is the color order correct here?
  if (fwrite(_buffer, 1, _buflen, of) != _buflen) {
    fprintf(stderr, "directx_camera_server::write_memory_to_ppm_file(): Can't write data to %s\n", filename);
    return false;
  }
  fclose(of);

  return true;
}

bool	directx_camera_server::get_pixel_from_memory(int X, int Y, vrpn_uint8 &val, int RGB) const
{
  if (!_status) { return false; };

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

  char	*newbuf;    //< Holds the inverted image
  unsigned y;

  if ( (newbuf = new char[_buflen]) == NULL) {
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