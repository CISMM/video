#pragma once

#include "wx/defs.h"
#include "wx/app.h"
#include "wx/menu.h"
#include "wx/dcclient.h"
#include "wx/wfstream.h"

#include "wx/glcanvas.h"

#include "GL/glu.h"

#include "fileToTexture.h"
#include "PixelLine.h"

class TestGLCanvas: public wxGLCanvas
{
public:
    TestGLCanvas(wxWindow *parent, wxWindowID id = wxID_ANY,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize, long style = 0,
        const wxString& name = wxT("TestGLCanvas"));

    ~TestGLCanvas();

	void SetInput(base_camera_server* newInput);

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


	int GetRadius()	{	return m_pixelRadius;	}

	void UpdateSlices();

	void SetZ(float z) {	m_z = z;	}

protected:
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnEraseBackground(wxEraseEvent& event);
    void OnMouse(wxMouseEvent& event);

	void DrawSelectionBox();
	void DrawHUD();

	base_camera_server* m_image;

	PixelLine* m_hPixRef;
	PixelLine* m_vPixRef;

	bool m_showCross;

private:

	int m_mouseX, m_mouseY;
	double m_selectX, m_selectY;
	float m_radius;
	int m_pixelRadius;

	int m_bits;

	float m_z;


    void InitGL();
    void ResetProjectionMode();

    DECLARE_EVENT_TABLE()
};