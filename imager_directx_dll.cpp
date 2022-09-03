//XXX How to know what device to connect to when it is run?
//XXX How to let the client be run before the server, so that it
// doesn't have to connect right away?  Who will call mainloop()?
// How do we know what video resolution (or how to change it)?
//XXX This will block up the server when the DirectShow graph
// is stopped because the images won't be read.  We'd like the
// client to be mainloop()ing the server even when the graph
// is stopped, though not spitting out any images.

//----------------------------------------------------------------------------
//   This program reads from a Imager server and implements a Microsoft
// DirectShow source filter that produces what it receives as input from
// VRPN as a video source that can be linked to by DirectShow applications.
//   This is a DLL that must be registered using the regsvr32.exe program.
//
// XXX It assumes that the size of the imager does not change during the run.
// XXX Its settings and ranges assume that the pixels are actually 8-bit values.

//#define	DEBUG_ON

#include <vrpn_Connection.h>
#include <vrpn_Imager.h>
#include <streams.h>

//--------------------------------------------------------------------------
// Version string for this program
static const char *g_version_string = "01.00";

//--------------------------------------------------------------------------
// Helper function to pop up dialog boxes so we can debug things.
static const void dialog(const char *s1, const char *s2)
{
      TCHAR szMsg[MAX_PATH + MAX_PATH + 100];

      wsprintf(szMsg, TEXT(s2));
      OutputDebugString(szMsg);
      MessageBox(NULL, szMsg, TEXT(s1), MB_ICONINFORMATION | MB_OK);
}


//----------------------------------------------------------------------------
// This is the class definition and implementation for the DirectShow source
// filter.  It is based on Chapter 12 of the "Programming Microsoft Directshow for
// Digital Video and Television" book by Mark Pesce.  I did not break the seal
// on the CD sample code, so this code should be distributable in source code
// form.  I wrote this based on the text description.  The GUID and CLSID stuff
// comes from around page 240-246 (generating new GUIDs is done as described on
// page 202-203, using the uuidgen.exe program with the -s argument).  All of
// the real work happens in the CVRPNPushPin
// class, which is defined first.  The Source that is constructed and used is
// defined second.  The methods for each class follow both class definitions.

static const GUID CLSID_VRPNSource = { /* 010e1393-aaa0-48f9-9691-e5ea93ca4016 */
    0x010e1393,
    0xaaa0,
    0x48f9,
    {0x96, 0x91, 0xe5, 0xea, 0x93, 0xca, 0x40, 0x16}
  };
static const GUID IID_IVRPNSource = { /* 10ebcde1-f4cc-4f65-b51b-bec78cafb432 */
    0x10ebcde1,
    0xf4cc,
    0x4f65,
    {0xb5, 0x1b, 0xbe, 0xc7, 0x8c, 0xaf, 0xb4, 0x32}
  };

class CVRPNPushPin : public CSourceStream {
public:
  CVRPNPushPin(HRESULT *phr, CSource *pFilter);
  ~CVRPNPushPin();

  // Don't support quality control
  STDMETHODIMP Notify(IBaseFilter *, Quality) { return E_FAIL; };

protected:
  BOOL	m_bDiscontinuity;   //< If true, set discontinuity flag
  IMediaSample	*m_pSample; //< Stores a pointer to the media sample to fill in.
  bool	m_bGotRegion;	    //< Keeps track of whether we got a region when calling mainloop()

  // These are CSourceStream methods that we have to override.
  HRESULT GetMediaType(CMediaType *pMediaType);
  HRESULT CheckMediaType(const CMediaType *pMediaType);
  HRESULT DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pRequest);

  // This method is called repeatedly by the streaming video thread
  // as long as the filter is running or paused.
  HRESULT FillBuffer(IMediaSample *pSample);

  vrpn_Imager_Remote  *m_ti;    //< TempImager client object
  bool	  m_ready_for_region;	    //< Everything set up to handle a region?
  bool	  m_ready_to_send_region;   //< Are we trying to fill a buffer?
  unsigned char *m_image;	    //< Pointer to the storage for the image

  static  void  VRPN_CALLBACK handle_region_change(void *userdata, const vrpn_IMAGERREGIONCB info);
  static  void  VRPN_CALLBACK handle_description_message(void *userdata, const struct timeval);
};

class CVRPNSource : public CSource {
public:
  static CUnknown *WINAPI CreateInstance(IUnknown *pUnk, HRESULT *phr);

protected:
  // The constructor needs to be private to force the application to
  // use CreateInstance to construct one.
  CVRPNSource(IUnknown *pUnk, HRESULT *phr);
};

//----------------------------------------------------------------------------
// CVRPNPushPin Methods

CVRPNPushPin::CVRPNPushPin(HRESULT *phr, CSource *pFilter)
: CSourceStream(NAME("CVRPNPushPin"), phr, pFilter, L"Out")
, m_ti(NULL)
, m_ready_for_region(false)
, m_ready_to_send_region(false)
, m_image(NULL)
{
  char	*device_name = "TestImage@localhost:4511";

#ifdef	DEBUG_ON
  dialog("CVRPNPushPin::CVRPNPushPin","Entering constructor");
#endif

  // Open the Imager client and set the callback
  // for new data and for information about the size of
  // the image.
#ifdef	DEBUG_ON
  { char out[1024];
    sprintf(out, "Opening %s\n", device_name);
    dialog("CVRPNPushPin::CVRPNPushPin", out);
  }
#endif
  m_ti = new vrpn_Imager_Remote(device_name);
  m_ti->register_description_handler(this, handle_description_message);
  m_ti->register_region_handler(this, handle_region_change);

  // Run the connection object for up to half a second, waiting to
  // be ready for images.  If we're not ready, then our setup will
  // fail.
  struct timeval start;
  struct timeval now;
  double interval;
  gettimeofday(&start, NULL);
  do {
    m_ti->mainloop();
    vrpn_SleepMsecs(1);	//< Avoid eating the entire processor
    gettimeofday(&now, NULL);
    interval = vrpn_TimevalMsecs(vrpn_TimevalDiff(now, start));
  } while ( (!m_ready_for_region) && (interval < 500) );
}

CVRPNPushPin::~CVRPNPushPin()
{
#ifdef	DEBUG_ON
  dialog("CVRPNPushPin::CVRPNPushPin", "Exiting\n");
#endif
  delete m_ti;
  if (m_image) { delete [] m_image; };
}

void  CVRPNPushPin::handle_description_message(void *userdata, const struct timeval)
{
  CVRPNPushPin *me = (CVRPNPushPin*)userdata;

  // This assumes that the size of the image does not change -- application
  // should allocate a new image array and get a new window size whenever it
  // does change.  Here, we just record that the entries in the imager client
  // are valid.
#ifdef	DEBUG_ON
  dialog("CVRPNPushPin::handle_description_message", "Waiting to hear the image dimensions...\n");
#endif
  if ( (me->m_image = new unsigned char[me->m_ti->nCols() * me->m_ti->nRows() * 3]) == NULL) {
#ifdef	DEBUG_ON
    dialog("CVRPNPushPin::handle_description_message", "Out of memory when allocating image!\n");
#endif
    me->m_ready_for_region = false;
    return;
  }
  me->m_ready_for_region = true;
#ifdef	DEBUG_ON
  dialog("CVRPNPushPin::handle_description_message", "Ready to receive images\n");
#endif

}

// New pixels coming; fill them into the image and sending a new image
// down the filter graph when we have a complete one.  The userdata is
// a pointer to the object that is to be used.

void  CVRPNPushPin::handle_region_change(void *userdata, const vrpn_IMAGERREGIONCB info)
{
    CVRPNPushPin  *me = (CVRPNPushPin*)(userdata);  //< Userdata is a pointer to the object
    int r,c;	//< Row, Column
    int offset,RegionOffset;
    const vrpn_Imager_Region* region=info.region;

    int infoLineSize=region->d_cMax-region->d_cMin+1;
    vrpn_int32 nCols=me->m_ti->nCols();
    vrpn_int32 nRows=me->m_ti->nRows();

    // If we've not connected, we're not ready for a region.  Sleep a bit between
    // tests so that we don't flood the filter graph; send only about 2 frames
    // per second.
    // XXX Should fill the image region with blue or something (better yet -- a
    // message saying that there's no video connection!
    if (!me->m_ready_for_region) {
#ifdef	DEBUG_ON
      dialog("CVRPNPushPin::handle_region_change", "Waiting to be ready for regions");
#endif
      vrpn_SleepMsecs(500);
      return;
    }

#if 0
      dialog("CVRPNPushPin::handle_region_change", "Got a region");
#endif
    //XXX Code crashed right after getting a region, before we had tried to
    // connect to the filter graph or run it or anything.  Still during addition to
    // the filter graph.

    //--------------------------------------------------------------------------------
    // If we're not yet ready to send an image (mainloop() was called on our
    // object somewhere besides the routine that fills and sends a buffer), then
    // we're done.
    if (!me->m_ready_to_send_region) {
      return;
    }

    //--------------------------------------------------------------------------------
    // Copy pixels into the image buffer.
    // Flip the image over in
    // Y so that the image coordinates match DirectX's expectations.
    //XXX Do this with memcpy() rather than a pixel at a time, since
    // we are not scaling?  Nope... need to fill into RGBA structure.
    // Maybe rethink and go just a single-valued grayscale type.
    for (r = info.region->d_rMin,RegionOffset=(r-region->d_rMin)*infoLineSize; r <= region->d_rMax; r++,RegionOffset+=infoLineSize) {
      int ir = nRows - 1 - r;
      int row_offset = ir*nCols;
      for (c = info.region->d_cMin; c <= region->d_cMax; c++) {
	vrpn_uint8 val;
	info.region->read_unscaled_pixel(c,r,val);
	offset = 3 * (c + row_offset);

	me->m_image[0 + offset] = val;
	me->m_image[1 + offset] = val;
	me->m_image[2 + offset] = val;
      }
    }

    //--------------------------------------------------------------------------------
    // Go ahead and push a frame through the filter graph whenever we've filled in any
    // region of the image.  This will cause the least latency in the updates, although
    // the frame rate coming out of the filter graph will be higher than the VRPN frame
    // rate coming in.  XXX This will cause tearing in the video.
    // See page 278 and following to see the example code for this.

    // Get a pointer to the place where the data is to go.
    BYTE *pData;
    me->m_pSample->GetPointer(&pData);

    // If the video format has changed, set the new one.  We'll check later to make sure
    // it matches what we expect it to be.
    CMediaType *pmt;
    if (me->m_pSample->GetMediaType((AM_MEDIA_TYPE**)&pmt) == S_OK) {
      me->SetMediaType(pmt);
      DeleteMediaType(pmt);
    }

    // Make sure the video format is what we expect it to be.
    if (me->m_mt.formattype != FORMAT_VideoInfo) {
#ifdef	DEBUG_ON
      dialog("CVRPNPushPin::handle_region_change", "Not video format!\n");
#endif
      return;
    }
    if (me->m_mt.cbFormat < sizeof(VIDEOINFOHEADER)) {
#ifdef	DEBUG_ON
      dialog("CVRPNPushPin::handle_region_change", "CVRPNSource::handle_region_change(): Header too small!\n");
#endif
      return;
    }

    // Number of rows and columns.  This is different if we are using a target
    // rectangle (rcTarget) than if we are not.
    VIDEOINFOHEADER *pVih = reinterpret_cast<VIDEOINFOHEADER*>(me->m_mt.pbFormat);
    int	width, height;
    if (IsRectEmpty(&pVih->rcTarget)) {
      width = pVih->bmiHeader.biWidth;
      height = pVih->bmiHeader.biHeight;
    } else {
      width = pVih->rcTarget.right;
      height = pVih->bmiHeader.biHeight;
#ifdef	DEBUG_ON
      dialog("CVRPNPushPin::handle_region_change", "XXX Warning: may not work correctly with target rectangle\n");
#endif
    }
    if ( (width != me->m_ti->nCols()) || (height != me->m_ti->nRows()) ) {
#ifdef	DEBUG_ON
      dialog("CVRPNPushPin::handle_region_change", "Wrong size image!\n");
#endif
      return;
    }

    // Make sure that the image is not compressed and that we have 8 bits
    // per pixel.
    if (pVih->bmiHeader.biCompression != BI_RGB) {
#ifdef	DEBUG_ON
      dialog("CVRPNPushPin::handle_region_change", "Compression not RGB\n");
      switch (pVih->bmiHeader.biCompression) {
	case BI_RLE8:
	  dialog("CVRPNPushPin::handle_region_change", "  (It is BI_RLE8)\n");
	  break;
	case BI_RLE4:
	  dialog("CVRPNPushPin::handle_region_change", "  (It is BI_RLE4)\n");
	case BI_BITFIELDS:
	  dialog("CVRPNPushPin::handle_region_change", "  (It is BI_BITFIELDS)\n");
	  break;
	default:
	  dialog("CVRPNPushPin::handle_region_change", "  (Unknown compression type)\n");
      }
#endif
      return;
    }
    int BytesPerPixel = pVih->bmiHeader.biBitCount / 8;
    if (BytesPerPixel != 3) {
#ifdef	DEBUG_ON
      dialog("CVRPNPushPin::handle_region_change", "Not 3 bytes per pixel\n");
#endif
      return;
    }

    // A negative height indicates that the images are stored non-inverted in Y
    // Not sure what to do with images that have negative height -- need to
    // read the book some more to find out.
    if (height < 0) {
#ifdef	DEBUG_ON
      dialog("CVRPNPushPin::handle_region_change", "Height is negative (internal error)\n");
#endif
      return;
    }

    // Find the stride to take when moving from one row of video to the
    // next.  This is rounded up to the nearest DWORD.
    DWORD stride = (width * BytesPerPixel + 3) & ~3;

    // Copy the image from our internal buffer into the buffer we were passed in,
    // copying one row at a time and then skipping the appropriate distance for
    // the internal buffer and for the buffer we're writing to
    BYTE *mybuf = me->m_image;
    DWORD mystep = BytesPerPixel * width;
    BYTE *outbuf = pData;
    DWORD outstep = stride;
    for (r = 0; r < height; r++) {
      memcpy(outbuf, mybuf, mystep);
      mybuf += mystep;
      outbuf += outstep;
    }

    //--------------------------------------------------------------------------------
    // Capture timing information and print out how many frames per second
    // are being received.

#if 0
    { static struct timeval last_print_time;
      struct timeval now;
      static bool first_time = true;
      static int frame_count = 0;

      if (first_time) {
	gettimeofday(&last_print_time, NULL);
	first_time = false;
      } else {
	static	unsigned  last_r = 10000;
	if (last_r > info.region->d_rMin) {
	  frame_count++;
	}
	last_r = info.region->d_rMin;
	gettimeofday(&now, NULL);
	double timesecs = 0.001 * vrpn_TimevalMsecs(vrpn_TimevalDiff(now, last_print_time));
	if (timesecs >= 5) {
	  double frames_per_sec = frame_count / timesecs;
	  frame_count = 0;
	  dialog("CVRPNPushPin::handle_region_change", "Received frames per second = %lg\n", frames_per_sec);
	  last_print_time = now;
	}
      }
    }
#endif

    //--------------------------------------------------------------------------------
    // Tell that we did get a region, so we can return from the FillBuffer
    // routine.
    me->m_bGotRegion = true;
}

// Media type negotiation handlers

HRESULT CVRPNPushPin::GetMediaType(CMediaType *pMediaType)
{
  CheckPointer(pMediaType, E_POINTER);
  CAutoLock cAutoLock(m_pFilter->pStateLock());

  // We want 24-bit (RGB) video.
  // XXX Not sure if this is going to do the trick properly or not.
  // XXX Nope: Probably need to specify things in pbFormat (width, height,
  // bit depth, compression, etc.)  How do we know the size before we've
  // connected?  How do we connect before the filter graph has started?
  AM_MEDIA_TYPE mt;
  ZeroMemory(&mt, sizeof(AM_MEDIA_TYPE));
  mt.majortype = MEDIATYPE_Video;	  // Ask for video media producers
  mt.subtype = MEDIASUBTYPE_RGB24;	  // Ask for 8 bit RGB
  *pMediaType = mt;

  ZeroMemory(&mt, sizeof(AM_MEDIA_TYPE));
  mt.majortype = MEDIATYPE_Video;	  // Ask for video media producers
  mt.subtype = MEDIASUBTYPE_RGB24;	  // Ask for 8 bit RGB
  mt.pbFormat = (BYTE*)CoTaskMemAlloc(sizeof(VIDEOINFOHEADER));
  VIDEOINFOHEADER *pVideoHeader = (VIDEOINFOHEADER*)mt.pbFormat;
  ZeroMemory(pVideoHeader, sizeof(VIDEOINFOHEADER));
  pVideoHeader->bmiHeader.biBitCount = 24;
  pVideoHeader->bmiHeader.biWidth = m_ti->nCols();
  pVideoHeader->bmiHeader.biHeight = m_ti->nRows();
  pVideoHeader->bmiHeader.biPlanes = 1;
  pVideoHeader->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  pVideoHeader->bmiHeader.biSizeImage = DIBSIZE(pVideoHeader->bmiHeader);

  // Set the format type and size.
  mt.formattype = FORMAT_VideoInfo;
  mt.cbFormat = sizeof(VIDEOINFOHEADER);

  // Set the sample size.
  mt.bFixedSizeSamples = TRUE;
  mt.lSampleSize = DIBSIZE(pVideoHeader->bmiHeader);

  // Point the parameter at the media type and then return.
  *pMediaType = mt;
  return S_OK;
}

HRESULT CVRPNPushPin::CheckMediaType(const CMediaType *pMediaType)
{
  CAutoLock cAutoLock(m_pFilter->pStateLock());

  // Make sure it is a video type, in particular 24-bit RGB
  if (pMediaType->majortype != MEDIATYPE_Video) { return E_FAIL; }
  if (pMediaType->subtype != MEDIASUBTYPE_RGB24) { return E_FAIL; }

  // Make sure we have a video info header.
  if ((pMediaType->formattype != FORMAT_VideoInfo) ||
      (pMediaType->cbFormat < sizeof(VIDEOINFOHEADER)) ||
      (pMediaType->pbFormat == NULL)) {
    return E_FAIL;
  }

  // Make sure all of the parameters match what we expect
  VIDEOINFOHEADER *pVih = (VIDEOINFOHEADER*)pMediaType->pbFormat;
  if (!IsRectEmpty(&(pVih->rcSource))) {
    // XXX We might at some point like to implement source rectangles,
    // even sending the request back to the VRPN server, to reduce
    // video bandwidth.  For now, we don't support this.
    return E_FAIL;
  }

  /* XXX Check was here for valid frame rate, but how would we know what it should be?
  if (pVih->AvgTimePerFrame != m_rtFrameLength) { return E_FAIL; }*/

  return S_OK;
}

HRESULT CVRPNPushPin::DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pRequest)
{
  CAutoLock cAutoLock(m_pFilter->pStateLock());
  HRESULT hr;
  VIDEOINFO *pvi = (VIDEOINFO*)(m_mt.Format());
  if (pvi == NULL) {
    return E_FAIL;
  }

  // Require at least one buffer
  if (pRequest->cBuffers == 0) { pRequest->cBuffers = 1; }

  // It needs to be at least large enough to hold an image
  if ((long)pvi->bmiHeader.biSizeImage > pRequest->cbBuffer) {
    pRequest->cbBuffer = pvi->bmiHeader.biSizeImage;
  }

  // Ask for these
  ALLOCATOR_PROPERTIES	props;
  hr = pAlloc->SetProperties(pRequest, &props);
  if (FAILED(hr)) { return hr; }

  // Make sure it is as big as we need
  if (props.cbBuffer < pRequest->cbBuffer) { return E_FAIL; }

  return S_OK;
}

HRESULT CVRPNPushPin::FillBuffer(IMediaSample *pSample)
{
  // Point the member sample at the media sample we are
  // to use.  The member sample was passed to the VRPN callback
  // handler creation routine as the userdata parameter.
  m_pSample = pSample;

#ifdef	DEBUG_ON
  dialog("CVRPNPushPin::FillBuffer", "Entered");
#endif
  m_bGotRegion = false;
  m_ready_to_send_region = true;
  do {
    m_ti->mainloop();
    vrpn_SleepMsecs(1);
#ifdef	DEBUG_ON
    dialog("CVRPNPushPin::FillBuffer", "Looping");
#endif
  } while (!m_bGotRegion);

  m_ready_to_send_region = false;
  return S_OK;
};

//----------------------------------------------------------------------------
// CVRPNSource Methods

CVRPNSource::CVRPNSource(IUnknown *pUnk, HRESULT *phr)
: CSource(NAME("VRPNSource"), pUnk, CLSID_VRPNSource)
{
  // Create the output pin, which magically adds itself to the pin array
  CVRPNPushPin *pPin = new CVRPNPushPin(phr, this);
  if (pPin == NULL) { *phr = E_OUTOFMEMORY; }
}

CUnknown *WINAPI CVRPNSource::CreateInstance(IUnknown *pUnk, HRESULT *phr)
{
  CVRPNSource *pNewFilter = new CVRPNSource(pUnk, phr);
  if (pNewFilter == NULL) { *phr = E_OUTOFMEMORY; }
  return pNewFilter;
}


// imager_directx_dll.cpp : Defines the entry point for the DLL application.
//

//----------------------------------------------------------------------------
// DirectShow specification of the filter.  Described starting on page 261 of
// the DirectShow book.

const AMOVIESETUP_PIN psudVRPNSourcePins[] =
{ { L"Output"	  // strName
  , FALSE	  // bRendered
  , TRUE	  // bOutput
  , FALSE	  // bZero
  , FALSE	  // bMany
  , &CLSID_NULL	  // clsConnectsToFilter
  , L""		  // strConnectsToPin
  , 0		  // nTypes
  , NULL	  // lpTypes
}};

const AMOVIESETUP_FILTER sudVRPNSource =
{ &CLSID_VRPNSource	// clsID
, L"VRPNSource"		// strName
, MERIT_DO_NOT_USE	// dwMerit prevents auto-connect from using
, 1			// nPins
, psudVRPNSourcePins	// lpPin
};

// List of class IDs and creator functions for the class factory. This
// provides the link between the OLE entry point in the DLL and an object
// being created. The class factory will call the static CreateInstance.

const unsigned NUM_TEMPLATES = 1;
CFactoryTemplate g_Templates[NUM_TEMPLATES] = 
{ 
  L"VRPNSource",		      // Name
  &CLSID_VRPNSource,		      // CLSID
  CVRPNSource::CreateInstance,	      // Method to create an instance of MyComponent
  NULL,				      // Initialization function
  &sudVRPNSource		      // Set-up information (for filters)
};

int g_cTemplates = NUM_TEMPLATES;

//----------------------------------------------------------------------------
// Add entry points to Windows DLLs to provide system-wide objects.
// There is also a separate VPRNSource.def module that defines the
// DLL entry points for our source filter.

STDAPI DllRegisterServer()
{
  return AMovieDllRegisterServer2(TRUE);
}

STDAPI DllUnregisterServer()
{
  return AMovieDllRegisterServer2(FALSE);
}

extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL APIENTRY DllMain(HANDLE hModule, DWORD dwReason, LPVOID lpReserved)
{
  return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}
