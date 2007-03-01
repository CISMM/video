/////////////////////////////////////////////////////////////////////////////
// Name:        ztracker.h
// Purpose:     realtime video bead tracking main form
// Author:      Ryan Schubert
// Created:     2/24/07
// Modified by:
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include <wx/wx.h>
#include <wx/image.h>

#include "TestGLCanvas.h"
#include "PixelLine.h"




class zTracker : public wxFrame {
public:
    zTracker(wxWindow* parent, int id, const wxString& title, const wxPoint& pos=wxDefaultPosition, const wxSize& size=wxDefaultSize, long style=wxDEFAULT_FRAME_STYLE);

	void OnMenuFileOpen(wxCommandEvent& event);
    void OnMenuFileExit(wxCommandEvent& event);
    void OnMenuHelpAbout(wxCommandEvent& event);

//	void SetCanvas( TestGLCanvas *canvas ) { m_canvas = canvas; }
//    TestGLCanvas *GetCanvas() { return m_canvas; }

protected:

//	wxStaticText* label_1;
//	wxTextCtrl* text_ctrl_1;
//	wxPanel* panel_1;

	TestGLCanvas *m_canvas;
//	TestGLCanvas *m_canvasZoomed;

	wxStaticText* m_horizLabel;
	PixelLine* m_horizPixels;

	wxStaticText* m_vertLabel;
	PixelLine* m_vertPixels;


private:

	// these aren't used right now!
    void set_properties();
    void do_layout();

	DECLARE_EVENT_TABLE()
};



// icon
/* XPM */
static const char *sample_xpm[] = {
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

