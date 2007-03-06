#pragma once

#include "wx/defs.h"
#include "wx/app.h"
#include "wx/menu.h"
#include "wx/dcclient.h"
#include "wx/wfstream.h"

#include "wx/glcanvas.h"

#include "GL/glu.h"

class PlotGLCanvas: public wxGLCanvas
{
public:
    PlotGLCanvas(wxWindow *parent, wxWindowID id = wxID_ANY,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize, long style = 0,
        const wxString& name = wxT("PlotGLCanvas"));

    ~PlotGLCanvas();

	void setVals(float* v, int n, float min, float max);

	void setMin(float m) { m_min = m; }
	void setMax(float m) { m_min = m; }

protected:
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnEraseBackground(wxEraseEvent& event);
    void OnMouse(wxMouseEvent& event);

	float* m_vals;
	int m_num;

	float m_min, m_max;

private:

    void InitGL();
    void ResetProjectionMode();

    DECLARE_EVENT_TABLE()
};