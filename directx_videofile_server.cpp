#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "directx_videofile_server.h"
#include <vrpn_BaseClass.h>

#define	DEBUG

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

#ifdef REGISTER_FILTERGRAPH

HRESULT AddGraphToRot(IUnknown *pUnkGraph, DWORD *pdwRegister) 
{
    IMoniker * pMoniker;
    IRunningObjectTable *pROT;
    if (FAILED(GetRunningObjectTable(0, &pROT))) 
    {
        return E_FAIL;
    }

    WCHAR wsz[128];
    wsprintfW(wsz, L"FilterGraph %08x pid %08x", (DWORD_PTR)pUnkGraph, 
              GetCurrentProcessId());

    HRESULT hr = CreateItemMoniker(L"!", wsz, &pMoniker);
    if (SUCCEEDED(hr)) 
    {
        hr = pROT->Register(0, pUnkGraph, pMoniker, pdwRegister);
        pMoniker->Release();
    }

    pROT->Release();
    return hr;
}

void RemoveGraphFromRot(DWORD pdwRegister)
{
    IRunningObjectTable *pROT;

    if (SUCCEEDED(GetRunningObjectTable(0, &pROT))) 
    {
        pROT->Revoke(pdwRegister);
        pROT->Release();
    }
}

#endif

//---------------------------------------------------------------------
// Open the file and determine the image size and such.

bool directx_videofile_server::open_and_find_parameters(const char *filename)
{
  //-------------------------------------------------------------------
  // Create COM and DirectX objects needed to access a video stream.

  // Initialize COM.  This must have a matching uninitialize in
  // the destructor.
  CoInitialize(NULL);

  // Create the filter graph manager
  CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, 
		      IID_IGraphBuilder, (void **)&_pGraph);
  if (_pGraph == NULL) {
    fprintf(stderr, "directx_videofile_server::open_and_find_parameters(): Can't create graph manager\n");
    return false;
  }
  _pGraph->QueryInterface(IID_IMediaControl, (void **)&_pMediaControl);
  _pGraph->QueryInterface(IID_IMediaEvent, (void **)&_pEvent);

  // Create the Capture Graph Builder.
  if (CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC, 
    IID_ICaptureGraphBuilder2, (void **)&_pBuilder) != S_OK) {
    _pBuilder = NULL;
    fprintf(stderr, "directx_videofile_server::open_and_find_parameters(): Can't create graph builder\n");
    return false;
  }

  // Associate the graph with the builder.
  _pBuilder->SetFiltergraph(_pGraph);    

  //-------------------------------------------------------------------
  // Construct the sample grabber callback handler that will be used
  // to receive image data from the sample grabber.
  if ( (_pCallback = new directx_samplegrabber_callback()) == NULL) {
    fprintf(stderr,"directx_camera_server::open_and_find_parameters(): Can't create sample grabber callback handler (out of memory?)\n");
    return false;
  }

  //-------------------------------------------------------------------
  // Construct the sample grabber that will be used to snatch images from
  // the video stream as they go by.

  // Create the Sample Grabber.
  CoCreateInstance(CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER,
      IID_IBaseFilter, reinterpret_cast<void**>(&_pSampleGrabberFilter));
  _pSampleGrabberFilter->QueryInterface(IID_ISampleGrabber,
      reinterpret_cast<void**>(&_pGrabber));

  // Set the media type to video and 8-bit RGB.
  AM_MEDIA_TYPE mt;
  ZeroMemory(&mt, sizeof(AM_MEDIA_TYPE));
  mt.majortype = MEDIATYPE_Video;	  // Ask for video media producers
  mt.subtype = MEDIASUBTYPE_RGB24;	  // Ask for 8 bit RGB
  _pGrabber->SetMediaType(&mt);

  // Set the callback, where '0' means 'use the SampleCB callback'
  _pGrabber->SetCallback(_pCallback, 0);

  //-------------------------------------------------------------------
  // Create a NULL renderer that will be used to discard the video frames
  // on the output pin of the sample grabber

  IBaseFilter *pNull = NULL;
  CoCreateInstance(CLSID_NullRenderer, NULL, CLSCTX_INPROC_SERVER,
      IID_IBaseFilter, reinterpret_cast<void**>(&pNull));

  //-------------------------------------------------------------------
  // Build the filter graph.  First add the filters and then connect them.

  // pSrc is a the file-reading filter.
  IBaseFilter *pSrc;
  wchar_t longfilename[2048];
  mbstowcs(longfilename, filename, 2047);
  longfilename[2047] = L'\0';
  if (FAILED(_pGraph->AddSourceFilter(longfilename, L"Source", &pSrc))) {
    fprintf(stderr,"directx_videofile_server::open_and_find_parameters(): Can't create reader for %s\n", filename);
    return false;
  }

  // Add the sample grabber filter
  if (FAILED(_pGraph->AddFilter(_pSampleGrabberFilter, L"SampleGrabber"))) {
    fprintf(stderr,"directx_videofile_server::open_and_find_parameters(): Can't create grabber filter\n");
    return false;
  }

  // Add the null renderer filter
  if (FAILED(_pGraph->AddFilter(pNull, L"NullRenderer"))) {
    fprintf(stderr,"directx_videofile_server::open_and_find_parameters(): Can't create Null Renderer\n");
    return false;
  }

  // Connect the output of the video reader to the sample grabber input
  ConnectTwoFilters(_pGraph, pSrc, _pSampleGrabberFilter);

  // Connect the output of the sample grabber to the NULL renderer input
  ConnectTwoFilters(_pGraph, _pSampleGrabberFilter, pNull);
  
  //-------------------------------------------------------------------
  // We don't need a reference clock for playout because we're going
  // to limit playback rate by controlling how fast we pass frames through
  // the callback handler.  We can't set a playout rate anyway, because
  // the NULL renderer won't do this for us.  Recall that the frames come
  // out of the file reader as fast as it can process them, and only
  // block up if the downstream buffers all fill up.
  pNull->SetSyncSource(NULL); // Turn off the reference clock.
  
  //-------------------------------------------------------------------
  // Find the control that lets you seek in the media (rewind uses this
  // to restart at the beginning of the file).

  if (FAILED(_pGraph->QueryInterface(IID_IMediaSeeking, (void **)&_pMediaSeeking))) {
    fprintf(stderr,"directx_videofile_server::open_and_find_parameters(): Can't create media seeker\n");
    pSrc->Release();
    pNull->Release();
    return false;
  }

  //-------------------------------------------------------------------
  // Find _num_rows and _num_columns, which is the maximum size.
  _pGrabber->GetConnectedMediaType(&mt);
  VIDEOINFOHEADER *pVih;
  if (mt.formattype == FORMAT_VideoInfo) {
      pVih = reinterpret_cast<VIDEOINFOHEADER*>(mt.pbFormat);
  } else {
    fprintf(stderr,"directx_videofile_server::open_and_find_parameters(): Can't get video header type\n");
    return false;
  }

  // Number of rows and columns.  This is different if we are using a target
  // rectangle (rcTarget) than if we are not.
  if (IsRectEmpty(&pVih->rcTarget)) {
    _num_columns = pVih->bmiHeader.biWidth;
    _num_rows = pVih->bmiHeader.biHeight;
  } else {
    _num_columns = pVih->rcTarget.right;
    _num_rows = pVih->bmiHeader.biHeight;
    printf("XXX directx_camera_server::open_and_find_parameters(): Warning: may not work correctly with target rectangle\n");
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
  int BytesPerPixel = pVih->bmiHeader.biBitCount / 8;
  if (BytesPerPixel != 3) {
    fprintf(stderr,"directx_camera_server::open_and_find_parameters(): Not 3 bytes per (%d)\n",
      pVih->bmiHeader.biBitCount);
    return false;
  }

  // A negative height indicates that the images are stored non-inverted in Y
  // Not sure what to do with images that have negative height -- need to
  // read the book some more to find out.
  if (_num_rows < 0) {
    fprintf(stderr,"directx_camera_server::open_and_find_parameters(): Num Rows is negative (internal error)\n");
    return false;
  }

  // Find the stride to take when moving from one row of video to the
  // next.  This is rounded up to the nearest DWORD.
  _stride = (_num_columns * BytesPerPixel + 3) & ~3;

  //-------------------------------------------------------------------
  // Release resources that won't be used later and return
  pSrc->Release();
  pNull->Release();
  return true;
}

directx_videofile_server::directx_videofile_server(const char *filename) :
  _pMediaSeeking(NULL)
{
  //---------------------------------------------------------------------
  if (!open_and_find_parameters(filename)) {
    fprintf(stderr, "directx_videofile_server::directx_videofile_server(): Cannot open file %s\n",
      filename);
    _status = false;
    return;
  }

  //---------------------------------------------------------------------
  // Let the graph editor connect and view this graph
#ifdef REGISTER_FILTERGRAPH
  if (FAILED(AddGraphToRot(_pGraph, &_dwGraphRegister))) {
      fprintf(stderr,"Failed to register filter graph with ROT!\n");
      _dwGraphRegister = 0;
  }
#endif

  //---------------------------------------------------------------------
  // Allocate a buffer that is large enough to read the maximum-sized
  // image with no binning.
  _buflen = (unsigned)(_num_rows * _num_columns * 3);	// Expect B,G,R; 8-bits each.
  if ( (_buffer = new unsigned char[_buflen]) == NULL) {
    fprintf(stderr,"directx_videofile_server::directx_videofile_server(): Out of memory for buffer\n");
    _status = false;
    return;
  }

  //---------------------------------------------------------------------
  // No image in memory yet.
  _minX = _minY = _maxX = _maxY = 0;

  //---------------------------------------------------------------------
  // Set mode to running, status to good and return
  _mode = 0;
  _status = true;
}

//---------------------------------------------------------------------
// Close the camera and the system.  Free up memory.

void  directx_videofile_server::close_device(void)
{
  if (_pMediaSeeking) { _pMediaSeeking->Release(); }
#ifdef REGISTER_FILTERGRAPH
  if (_dwGraphRegister) {
      RemoveGraphFromRot(_dwGraphRegister);
      _dwGraphRegister = 0;
  }
#endif
}
  
directx_videofile_server::~directx_videofile_server(void)
{
  close_device();
}

/** Begin playing the video file from the current location. */
void  directx_videofile_server::play(void)
{
  _pMediaControl->Run();
  _mode = 0;  // Running mode
}

/** Pause the video file at the current location. */
void  directx_videofile_server::pause(void)
{
  _pMediaControl->Pause();
  _mode = 1;  // Paused mode
}

/** Read a single frame of video, then pause. */
void  directx_videofile_server::single_step(void)
{
  _pMediaControl->Run();
  _mode = 2;  // Single-step mode
}

/** Rewind the videofile to the beginning and take one step. */
void  directx_videofile_server::rewind(void)
{
  LONGLONG pos = 0;

  // Seek to the beginning
  _pMediaSeeking->SetPositions(&pos, AM_SEEKING_AbsolutePositioning,
      NULL, AM_SEEKING_NoPositioning);
  single_step();
}

bool  directx_videofile_server::read_one_frame(unsigned minX, unsigned maxX,
			      unsigned minY, unsigned maxY,
			      unsigned exposure_millisecs)
{
    return directx_camera_server::read_one_frame(minX, maxX, minY, maxY, exposure_millisecs);
}
