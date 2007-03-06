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
#include "PlotWindow.h"

#include <vector>


class zTracker : public wxFrame {
public:
    zTracker(wxWindow* parent, int id, const wxString& title, const wxPoint& pos=wxDefaultPosition, const wxSize& size=wxDefaultSize, long style=wxDEFAULT_FRAME_STYLE);

	void OnMenuFileOpen(wxCommandEvent& event);
    void OnMenuFileExit(wxCommandEvent& event);
    void OnMenuHelpAbout(wxCommandEvent& event);
	void OnMenuVideoPlay(wxCommandEvent& event);
	void OnMenuVideoPause(wxCommandEvent& event);
	void OnMenuVideoSingle(wxCommandEvent& event);
	void OnMenuVideoRewind(wxCommandEvent& event);

	void OnMenuFocusStart(wxCommandEvent& event);
	void OnMenuFocusStop(wxCommandEvent& event);


	void Idle(wxIdleEvent& event);

//	void SetCanvas( TestGLCanvas *canvas ) { m_canvas = canvas; }
//    TestGLCanvas *GetCanvas() { return m_canvas; }

protected:

	PlotWindow *m_plotWindow;

//	wxStaticText* label_1;
//	wxTextCtrl* text_ctrl_1;
	wxPanel* m_video_control_panel;

	TestGLCanvas *m_canvas;
//	TestGLCanvas *m_canvasZoomed;

	wxStaticText* m_horizLabel;
	PixelLine* m_horizPixels;

	wxStaticText* m_vertLabel;
	PixelLine* m_vertPixels;

	wxButton* m_play;
	wxButton* m_pause;
	wxButton* m_stop;
	wxButton* m_rewind;

	int m_frame_number;

	bool m_logging;

	base_camera_server  *g_camera;	//< Camera used to get an image
	image_wrapper       *g_image;	//< Image, possibly from camera and possibly computed
	Controllable_Video  *g_video;	//< Video controls, if we have them

	std::vector<float> m_focus;

	int m_channel;


private:

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

