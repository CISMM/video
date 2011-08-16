#pragma once

#include "wx/defs.h"
#include "wx/app.h"
#include "wx/menu.h"
#include "wx/dcclient.h"
#include "wx/wfstream.h"

#include "wx/glcanvas.h"

#include "GL/glu.h"

#include <vrpn_Imager.h>
#include <VRPN_Imager_camera_server.h>
#include "vrpn_Auxiliary_Logger.h"

#include "PixelLine.h"
#include "cameraImage.h"

class TestGLCanvas: public wxGLCanvas
{
public:
    TestGLCanvas(wxWindow *parent, wxWindowID id = wxID_ANY,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize, long style = 0,
        const wxString& name = wxT("TestGLCanvas"),
		char* deviceName = "TestImage@localhost");

    ~TestGLCanvas();

	void SetInput(vrpn_Imager_Remote* newInput);

	void LogToFile(char* filename);

	void SetHPixRef(PixelLine* ref)	{	m_hPixRef = ref;	}
	void SetVPixRef(PixelLine* ref)	{	m_vPixRef = ref;	}

	void SetNumBits(int bits) {	m_bits = bits;	}

	void SetShowCross(bool show) {	m_showCross = show;	}

	double GetSelectX() {	return m_selectX;	}
	double GetSelectY() {	return m_selectY;	}

	void SetSelect(double x, double y) {	m_selectX = x; m_selectY = y;	}

	double GetSelectYflip() 
	{	
		wxSize sz(GetClientSize());
		return (sz.GetY() - 1 - m_selectY);	
	}

	void SetSelectYflip(double x, double y) 
	{	
		m_selectX = x;
		wxSize sz(GetClientSize());
		m_selectY = sz.GetY() - 1 - y;	
	}


	void ToggleHUDColor() { m_textColor = 1 - m_textColor; }

	int GetRadius()	{	return m_pixelRadius;	}

	void UpdateSlices();

	void SetZ(float z) {	m_z = z;	}
	void SetX(float x) {	m_x = x;	}
	void SetY(float y) {	m_y = y;	}

	int rows, cols;

	void Update();

	image_wrapper* GetWrappedImage() {	return m_image;	}

	bool m_newImage;

	char	*device_name;

	bool m_imagelogging;



	int m_middleMouseDragX;
	int m_middleMouseDragY;
	float m_micronsPerPixel;
	float m_mouseWheelDelta;
	float m_micronsPerMouseWheel;

	bool m_middleMouseDown;

	int m_imageGain;

protected:
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnEraseBackground(wxEraseEvent& event);
    void OnMouse(wxMouseEvent& event);
	void OnMouseWheel(wxMouseEvent& event);

	void DrawSelectionBox();
	void DrawHUD();

	//base_camera_server* m_image;
	vrpn_Imager_Remote* m_imager;
	//unsigned char* m_image;
	CameraImage* m_image;

	vrpn_Auxiliary_Logger_Remote* m_logger;

	//VRPN_Imager_camera_server* m_camera;

	int m_xDim;
	int m_yDim;
	bool m_already_posted;
	bool m_ready_for_region;
	bool m_draining;

	static void VRPN_CALLBACK handle_description_message(void *, const struct timeval);
	static void VRPN_CALLBACK handle_end_of_frame(void *,const struct _vrpn_IMAGERENDFRAMECB);
	static void  VRPN_CALLBACK handle_region_change(void *userdata, const vrpn_IMAGERREGIONCB info);

	PixelLine* m_hPixRef;
	PixelLine* m_vPixRef;

	bool m_showCross;

	//vrpn_Connection *m_connection; // we were using this for oldschool logging

private:

	int m_mouseX, m_mouseY;
	double m_selectX, m_selectY;
	//float m_radius;
	int m_pixelRadius;

	int m_bits;

	float m_z, m_x, m_y;

	bool m_got_dimensions;

	bool m_logging;

	float m_textColor;

    void InitGL();
    void ResetProjectionMode();

    DECLARE_EVENT_TABLE()
};