#pragma once

#include "wx/defs.h"
#include "wx/app.h"
#include "wx/menu.h"
#include "wx/dcclient.h"
#include "wx/wfstream.h"

#include "wx/glcanvas.h"

#include "GL/glu.h"


class PixelLine : public wxGLCanvas
{
public:
    PixelLine(wxWindow *parent, wxWindowID id = wxID_ANY,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize, long style = 0,
        const wxString& name = wxT("PixelLine"), bool horizontal = true);

    ~PixelLine();


	float calcFocus(int channel = 0, int method = 0, int weightedMethod = 0);


	// the RGB values for the pixel line
	float R[256];
	float G[256];
	float B[256];

	// number of pixels in current strip
	int p_len;

protected:
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnEraseBackground(wxEraseEvent& event);
    void OnMouse(wxMouseEvent& event);

private:

	bool m_horizontal;

    void InitGL();
    void ResetProjectionMode();


    DECLARE_EVENT_TABLE()
};