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

protected:
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnEraseBackground(wxEraseEvent& event);
    void OnMouse(wxMouseEvent& event);

	void DrawSelectionBox();

	void UpdateSlices();

	base_camera_server* m_image;

	PixelLine* m_hPixRef;
	PixelLine* m_vPixRef;

private:

	int m_mouseX, m_mouseY;
	int m_selectX, m_selectY;
	float m_radius;
	int m_pixelRadius;




    void InitGL();
    void ResetProjectionMode();

    DECLARE_EVENT_TABLE()
};