/////////////////////////////////////////////////////////////////////////////
// Name:        PlotWindow.h
// Purpose:     plot window for focus measure over time
// Author:      Ryan Schubert
// Created:     3/5/07
// Modified by:
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include <wx/wx.h>

#include "PlotGLCanvas.h"

#include <vector>

class PlotWindow : public wxFrame {
public:
    PlotWindow(wxWindow* parent, int id, const wxString& title, const wxPoint& pos=wxDefaultPosition, const wxSize& size=wxDefaultSize, long style=wxDEFAULT_FRAME_STYLE);

//	void setVals(std::vector<float> vals);

	void Refresh();

	void SetIndicator(int index);

	void SetOffset(int offset) {	m_plot->setOffset(offset);	}

	void Update();

	void SetMethod(int m) {	method = m;	}
	void SetWeightedMethod(int m) {	weightedMethod = m;	}

	std::vector<float> vals;
	int method;
	int weightedMethod;

protected:
	void OnMouse(wxMouseEvent& event);

//	float* m_vals;
//	int m_num_vals;

	PlotGLCanvas* m_plot;

	int m_indicator;

private:

    void set_properties();
    void do_layout();

	DECLARE_EVENT_TABLE()
};



// icon
/* XPM */
static const char *plot_xpm[] = {
/* columns rows colors chars-per-pixel */
"32 32 6 1",
"  c black",
". c navy",
"X c red",
"o c yellow",
"O c gray100",
"+ c None",
/* pixels */
"++++++++++++++++++++++++++++++++",
"++++++++++++oooooooooooooooooo++",
"++++++++++oooXXXXXXXXXXXXXXXXo++",
"+++++++++ooXXXXXXXXXXXXXXXXXoo++",
"++++++++ooXXXXXXXXXXXXXXXXooo+++",
"+++++++ooXXXXXXXXXXXXXXXXoo+++++",
"+++++++oXXXXXXXooooooooooo++++++",
"++++++ooXXXXXXoo++++++++++++++++",
"+++++ooXXXXXXoo+++++++++++++++++",
"+++++oXXXXXXoo++++++++++++++++++",
"+++++oXXXXXXoo++++++++++++++++++",
"+++++oXXXXXXoooooooooooo++++++++",
"++++ooXXXXXXXXXXXXXXXXXo++++++++",
"++++oXXXXXXXXXXXXXXXXXoo++++++++",
"+++ooXXXXXXXXXXXXXXXXoo+++++++++",
"+++oXXXXXXoooooooooooo++++++++++",
"+++oXXXXXoo+++++++++++++++++++++",
"+++oXXXXXo++++++++++++++++++++++",
"+++oXXXXXo++++++++++++++++++++++",
"++ooXXXXXo++++++++++++++++++++++",
"++oXXXXXoo++++++++++++++++++++++",
"++oXXXXXo+++++++++++++++++++++++",
"++oXXXXXo+++++++++++++++++++++++",
"+ooXXXXXo+++++++++++++++++++++++",
"+oXXXXXoo+++++++++++++++++++++++",
"+oXXXXXo++++++++++++++++++++++++",
"+oXXXXXo++++++++++++++++++++++++",
"+oXXXXXo++++++++++++++++++++++++",
"+oXXXXXo++++++++++++++++++++++++",
"+ooooooo++++++++++++++++++++++++",
"++++++++++++++++++++++++++++++++",
"++++++++++++++++++++++++++++++++"
};

