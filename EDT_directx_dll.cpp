//----------------------------------------------------------------------------
//   This program reads from a camera attached to an EDT video input board
// and implements a Microsoft DirectShow source filter that produces what
// it receives as a video source that can be linked to by DirectShow applications.
//   This is a DLL that must be registered using the regsvr32.exe program.
//   It is based on the simple_take application supplied by EDT, and uses
// the same library files that the "take" example application does.
//
// XXX Add in time stamps that keep track of how long had passed since the first
//     frame was gotten and puts that into the record.  This enables us to write
//     AVI files with the data in them and include time stamps.
// XXX Its settings and ranges assume that the pixels are actually 8-bit values.

//#define	DEBUG_ON

#include "edtinc.h"
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
// the real work happens in the CEDTPushPin
// class, which is defined first.  The Source that is constructed and used is
// defined second.  The methods for each class follow both class definitions.

static const GUID CLSID_EDTSource = { /* 3afc9237-cfb9-447e-9d15-9cc7762a3093 */
    0x3afc9237,
    0xcfb9,
    0x447e,
    {0x9d, 0x15, 0x9c, 0xc7, 0x76, 0x2a, 0x30, 0x93}
  };

static const GUID IID_IEDTSource = { /* eac20d82-e2f2-4b11-9187-909d5650eb01 */
    0xeac20d82,
    0xe2f2,
    0x4b11,
    {0x91, 0x87, 0x90, 0x9d, 0x56, 0x50, 0xeb, 0x01}
  };

class CEDTPushPin : public CSourceStream {
public:
  CEDTPushPin(HRESULT *phr, CSource *pFilter);
  ~CEDTPushPin();

  // Don't support quality control
  STDMETHODIMP Notify(IBaseFilter *, Quality) { return E_FAIL; };

protected:
  REFERENCE_TIME  m_rtStartTime;    //< When the first sample was grabbed.

  BOOL	m_bDiscontinuity;   //< If true, set discontinuity flag

  // These are CSourceStream methods that we have to override.
  HRESULT GetMediaType(CMediaType *pMediaType);
  HRESULT CheckMediaType(const CMediaType *pMediaType);
  HRESULT DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pRequest);

  // This method is called repeatedly by the streaming video thread
  // as long as the filter is running or paused.
  HRESULT FillBuffer(IMediaSample *pSample);

  PdvDev  *m_pdv_p;		    //< Pointer to the device that we're using
  int	  m_unit;		    //< Which unit is being used
  int	  m_channel;		    //< Which channel is being used
  int	  m_nRows;		    //< Number of rows
  int	  m_nCols;		    //< Number of columns
  int	  m_num_timeouts;	    //< Total number of timeouts found so far
  int	  m_numbufs;		    //< Number of buffers to use
};

class CEDTSource : public CSource {
public:
  static CUnknown *WINAPI CreateInstance(IUnknown *pUnk, HRESULT *phr);

protected:
  // The constructor needs to be private to force the application to
  // use CreateInstance to construct one.
  CEDTSource(IUnknown *pUnk, HRESULT *phr);
};

//----------------------------------------------------------------------------
// CEDTPushPin Methods

CEDTPushPin::CEDTPushPin(HRESULT *phr, CSource *pFilter)
: CSourceStream(NAME("CEDTPushPin"), phr, pFilter, L"Out")
, m_rtStartTime(0)
, m_pdv_p(NULL)
, m_unit(0)
, m_channel(0)
, m_nRows(100)	      //< Default size in case we don't get a connection
, m_nCols(100)	      //< Default size in case we don't get a connection
, m_num_timeouts(0)
, m_numbufs(4)
{
#ifdef	DEBUG_ON
  dialog("CEDTPushPin::CEDTPushPin","Entering constructor");
#endif

  // Open the TempImager client and set the callback
  // for new data and for information about the size of
  // the image.
#ifdef	DEBUG_ON
  dialog("CEDTPushPin::CEDTPushPin", "opening device");
#endif

  // Open the device and read its parameters.
  char devname[128];
  strcpy(devname, EDT_INTERFACE);
  if ((m_pdv_p = pdv_open_channel(devname, m_unit, m_channel)) == NULL)
  {
#ifdef	DEBUG_ON
    dialog("CEDTPushPin::CEDTPushPin()","Can't open device");
#endif
    return;
  }
  m_nCols = pdv_get_width(m_pdv_p);
  m_nRows = pdv_get_height(m_pdv_p);
  int depth = pdv_get_depth(m_pdv_p);
  if (depth != 8) {
#ifdef	DEBUG_ON
    dialog("CEDTPushPin::CEDTPushPin()","Depth not 8 bits per pixel");
#endif
    pdv_close(m_pdv_p);
    m_pdv_p = NULL;
    return;
  }

  char *cameratype = pdv_get_cameratype(m_pdv_p);
#ifdef	DEBUG_ON
    dialog("CEDTPushPin::CEDTPushPin() got camera type",cameratype);
#endif

  // Allocate four buffers for optimal pdv ring buffer pipeline (reduce if memory is at a premium)
  pdv_multibuf(m_pdv_p, m_numbufs);

  // Start only one image here.
  pdv_start_image(m_pdv_p);

  // Make sure that we can read a frame from the device, or else timeout and
  // close it again.  Record the time at which we took this first frame, converted
  // into Microsoft DirectShow units.
  u_int timevec[2]; //< Seconds and nanoseconds
  u_char *image_p = pdv_wait_image_timed(m_pdv_p, timevec);
  int timeouts = pdv_timeouts(m_pdv_p);
  if (timeouts != 0) {
#ifdef	DEBUG_ON
    dialog("CEDTPushPin::CEDTPushPin()","Timed out reading from camera");
#endif
    pdv_close(m_pdv_p);
    m_pdv_p = NULL;
    return;
  }

  // Convert nanoseconds and seconds to 100-nanosecond steps and store as start time
  m_rtStartTime = ((LONGLONG)(timevec[1])) / 100 + ((LONGLONG)(timevec[0])) * 10000000L;

  // Start images coming our way.
  pdv_start_images(m_pdv_p, m_numbufs);
}

CEDTPushPin::~CEDTPushPin()
{
#ifdef	DEBUG_ON
  dialog("CEDTPushPin::CEDTPushPin", "Exiting\n");
#endif
  if (m_pdv_p) { pdv_close(m_pdv_p); };
}

// Media type negotiation handlers

HRESULT CEDTPushPin::GetMediaType(CMediaType *pMediaType)
{
  CheckPointer(pMediaType, E_POINTER);
  CAutoLock cAutoLock(m_pFilter->pStateLock());

  // We want 24-bit (RGB) video.  We'd really like 8-bit luminance, but that doesn't seem to be a choice!
  AM_MEDIA_TYPE mt;
  ZeroMemory(&mt, sizeof(AM_MEDIA_TYPE));
  mt.majortype = MEDIATYPE_Video;	  // Ask for video media producers
  mt.subtype = MEDIASUBTYPE_RGB24;	  // Ask for 24-bit RGB
  *pMediaType = mt;

#ifdef	DEBUG_ON
  { char size[1024];
    sprintf(size, "Size %d by %d", m_nCols, m_nRows);
    dialog("CEDTPushPin::GetMediaType", size);
  }
#endif
  ZeroMemory(&mt, sizeof(AM_MEDIA_TYPE));
  mt.majortype = MEDIATYPE_Video;	  // Ask for video media producers
  mt.subtype = MEDIASUBTYPE_RGB24;	  // Ask for 24-bit RGB
  mt.pbFormat = (BYTE*)CoTaskMemAlloc(sizeof(VIDEOINFOHEADER));
  VIDEOINFOHEADER *pVideoHeader = (VIDEOINFOHEADER*)mt.pbFormat;
  ZeroMemory(pVideoHeader, sizeof(VIDEOINFOHEADER));
  pVideoHeader->bmiHeader.biBitCount = 24;
  pVideoHeader->bmiHeader.biWidth = m_nCols;
  pVideoHeader->bmiHeader.biHeight = m_nRows;
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

HRESULT CEDTPushPin::CheckMediaType(const CMediaType *pMediaType)
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
    // to reduce video bandwidth.  For now, we don't support this.
    return E_FAIL;
  }

  /* XXX Check was here for valid frame rate, but how would we know what it should be?
  if (pVih->AvgTimePerFrame != m_rtFrameLength) { return E_FAIL; }*/

  return S_OK;
}

HRESULT CEDTPushPin::DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pRequest)
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

HRESULT CEDTPushPin::FillBuffer(IMediaSample *pSample)
{
#ifdef	DEBUG_ON
  dialog("CEDTPushPin::FillBuffer", "Entered");
#endif

  //---------------------------------------------------------------------
  // See page 278 and following to see the example code for this.

  // Get a pointer to the place where the data is to go.
  BYTE *pData;
  pSample->GetPointer(&pData);

  // If the video format has changed, set the new one.  We'll check later to make sure
  // it matches what we expect it to be.
  CMediaType *pmt;
  if (pSample->GetMediaType((AM_MEDIA_TYPE**)&pmt) == S_OK) {
    SetMediaType(pmt);
    DeleteMediaType(pmt);
  }

  // Make sure the video format is what we expect it to be.
  if (m_mt.formattype != FORMAT_VideoInfo) {
#ifdef	DEBUG_ON
    dialog("CVRPNPushPin::handle_region_change", "Not video format!\n");
#endif
    return VFW_E_INVALIDMEDIATYPE;
  }
  if (m_mt.cbFormat < sizeof(VIDEOINFOHEADER)) {
#ifdef	DEBUG_ON
    dialog("CVRPNPushPin::handle_region_change", "CVRPNSource::handle_region_change(): Header too small!\n");
#endif
    return VFW_E_RUNTIME_ERROR;
  }

  // Number of rows and columns.  This is different if we are using a target
  // rectangle (rcTarget) than if we are not.
  VIDEOINFOHEADER *pVih = reinterpret_cast<VIDEOINFOHEADER*>(m_mt.pbFormat);
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
  if ( (width != m_nCols) || (height != m_nRows) ) {
#ifdef	DEBUG_ON
    dialog("CVRPNPushPin::handle_region_change", "Wrong size image!\n");
#endif
    return VFW_E_INVALID_RECT;
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
    return VFW_E_INVALIDMEDIATYPE;
  }
  int BytesPerPixel = pVih->bmiHeader.biBitCount / 8;
  if (BytesPerPixel != 3) {
#ifdef	DEBUG_ON
    dialog("CVRPNPushPin::handle_region_change", "Not 3 bytes per pixel\n");
#endif
    return VFW_E_INVALIDMEDIATYPE;
  }

  // A negative height indicates that the images are stored non-inverted in Y
  // Not sure what to do with images that have negative height -- need to
  // read the book some more to find out.
  if (height < 0) {
#ifdef	DEBUG_ON
    dialog("CVRPNPushPin::handle_region_change", "Height is negative (internal error)\n");
#endif
    return VFW_E_INVALIDMEDIATYPE;
  }

  //--------------------------------------------------------------------------------
  // Get a video sample, or time out trying.  If we don't have a device open,
  // then fill the image with black.
  if (m_pdv_p == NULL) {
    // Find the stride to take when moving from one row of video to the
    // next.  This is rounded up to the nearest DWORD.
    DWORD stride = (width * BytesPerPixel + 3) & ~3;

    // Copy the image from our internal buffer into the buffer we were passed in,
    // copying one row at a time and then skipping the appropriate distance for
    // the internal buffer and for the buffer we're writing to
    BYTE *outbuf = pData;
    DWORD outstep = stride;
    int r, c, offset;
    for (r = 0; r < height; r++) {
      for (c = 0; c < width; c++) {
	offset = 3*c; //< First byte in the pixel in this line in output image
	outbuf[0 + offset] = 0;  //< Blue channel
	outbuf[1 + offset] = 0;  //< Green channel
	outbuf[2 + offset] = 0;  //< Red channel
      }
      outbuf += outstep;
    }

    // Have the media play out at one sample per second.  This increments the
    // m_rtStartTime value by a second's worth of 100-nm ticks and sets the
    // new time each frame.
    m_rtStartTime += 10000000L;
    pSample->SetTime(&m_rtStartTime, &m_rtStartTime);
  } else {
    u_int timevec[2]; //< Seconds and nanoseconds
    u_char *image_p = pdv_wait_image_timed(m_pdv_p, timevec);

    // Convert nanoseconds and seconds to 100-nanosecond steps and store as start time.
    // Then subtract the time when the first sample was taken to put us into units of
    // time since the stream began.  Set the sample's time to this value.  No idea how
    // far into the future the stop time is, so set the start time for both.
    REFERENCE_TIME rtNowTime = ((LONGLONG)(timevec[1])) / 100 + ((LONGLONG)(timevec[0])) * 10000000L;
    REFERENCE_TIME rtStreamTime = rtNowTime - m_rtStartTime;
    pSample->SetTime(&rtStreamTime, &rtStreamTime);

    // Start a new video sample coming our way for next time, keeping the hopper full
    pdv_start_image(m_pdv_p);

    // If we timed out, then restart the images coming our way
    int timeouts = pdv_timeouts(m_pdv_p);
    if (timeouts > m_num_timeouts)
    {
	/*
	 * pdv_timeout_cleanup helps recover gracefully after a timeout,
	 * particularly if multiple buffers were prestarted
	 */
	if (m_numbufs > 1)
	{
	    int     pending = pdv_timeout_cleanup(m_pdv_p);
	    pdv_start_images(m_pdv_p, pending);
	}
	m_num_timeouts = timeouts;
    }

    //--------------------------------------------------------------------------------
    // Copy the image into the outgoing buffer.
    // XXX This will copy unknown stuff into the buffer if there was
    // a timeout and we didn't get any data...

    // Find the stride to take when moving from one row of video to the
    // next.  This is rounded up to the nearest DWORD.
    DWORD stride = (width * BytesPerPixel + 3) & ~3;

    // Copy the image from our internal buffer into the buffer we were passed in,
    // copying one row at a time and then skipping the appropriate distance for
    // the internal buffer and for the buffer we're writing to
    BYTE *mybuf = image_p;
    DWORD mystep = width;
    BYTE *outbuf = pData;
    DWORD outstep = stride;
    int r, c, offset;
    for (r = 0; r < height; r++) {
      for (c = 0; c < width; c++) {
	offset = 3*c; //< First byte in the pixel in this line in output image
	outbuf[0 + offset] = mybuf[c];  //< Blue channel
	outbuf[1 + offset] = mybuf[c];  //< Green channel
	outbuf[2 + offset] = mybuf[c];  //< Red channel
      }
      mybuf += mystep;
      outbuf += outstep;
    }
  }

  return S_OK;
};

//----------------------------------------------------------------------------
// CEDTSource Methods

CEDTSource::CEDTSource(IUnknown *pUnk, HRESULT *phr)
: CSource(NAME("EDTSource"), pUnk, CLSID_EDTSource)
{
  // Create the output pin, which magically adds itself to the pin array
  CEDTPushPin *pPin = new CEDTPushPin(phr, this);
  if (pPin == NULL) { *phr = E_OUTOFMEMORY; }
}

CUnknown *WINAPI CEDTSource::CreateInstance(IUnknown *pUnk, HRESULT *phr)
{
  CEDTSource *pNewFilter = new CEDTSource(pUnk, phr);
  if (pNewFilter == NULL) { *phr = E_OUTOFMEMORY; }
  return pNewFilter;
}


// EDT_directx_dll.cpp : Defines the entry point for the DLL application.
//

//----------------------------------------------------------------------------
// DirectShow specification of the filter.  Described starting on page 261 of
// the DirectShow book.

const AMOVIESETUP_PIN psudEDTSourcePins[] =
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

const AMOVIESETUP_FILTER sudEDTSource =
{ &CLSID_EDTSource	// clsID
, L"EDTSource"		// strName
, MERIT_DO_NOT_USE	// dwMerit prevents auto-connect from using
, 1			// nPins
, psudEDTSourcePins	// lpPin
};

// List of class IDs and creator functions for the class factory. This
// provides the link between the OLE entry point in the DLL and an object
// being created. The class factory will call the static CreateInstance.

const unsigned NUM_TEMPLATES = 1;
CFactoryTemplate g_Templates[NUM_TEMPLATES] = 
{ 
  L"EDTSource",			// Name
  &CLSID_EDTSource,		// CLSID
  CEDTSource::CreateInstance,	// Method to create an instance of MyComponent
  NULL,				// Initialization function
  &sudEDTSource			// Set-up information (for filters)
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
