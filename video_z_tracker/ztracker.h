/////////////////////////////////////////////////////////////////////////////
// Name:        ztracker.h
// Purpose:     realtime video bead tracking main form
// Author:      Ryan Schubert
// Created:     2/24/07
// Modified by:
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include <wx/slider.h>
#include <wx/wx.h>
#include <wx/image.h>



#include "TestGLCanvas.h"
#include "PixelLine.h"
#include "PlotWindow.h"

//#include "wxDoubleSlider.h"

#include "Stage.h"

#include "SpotInformation.h"

#include "ZGuesser.h"


#include <vector>


enum VideoMode
{
	PLAYING,
	PAUSED,
	SINGLE_STEPPING
};



class zTracker : public wxFrame {
public:
    zTracker(wxWindow* parent, int id, const wxString& title, const wxPoint& pos=wxDefaultPosition, 
		const wxSize& size=wxDefaultSize, long style=wxDEFAULT_FRAME_STYLE, char* stage_name="Focus@mercury-cs.cs.unc.edu");

	~zTracker();

	void OnMenuFileOpen(wxCommandEvent& event);
    void OnMenuFileExit(wxCommandEvent& event);
    void OnMenuHelpAbout(wxCommandEvent& event);
	void OnVideoPlay(wxCommandEvent& event);
	void OnVideoPause(wxCommandEvent& event);
	void OnVideoSingle(wxCommandEvent& event);
	void OnVideoRewind(wxCommandEvent& event);

	void OnMenuShowAdv(wxCommandEvent& event);
	void OnMenuFocusStart(wxCommandEvent& event);
	void OnMenuFocusStop(wxCommandEvent& event);

	void OnSelectDirectX(wxCommandEvent& event);
	void OnSelectPulnix(wxCommandEvent& event);
	void OnSelectRoper(wxCommandEvent& event);

	void OnFrameScroll(wxScrollEvent& event);

	void OnResize(wxSizeEvent& event); // ***********************

	void OnCrossCheck(wxCommandEvent& event);

	void OnNewPlot(wxCommandEvent& event);
	
	void OnNewPlotArray(wxCommandEvent& event);

	void On8BitsCheck(wxCommandEvent& event);

	void OnDo(wxCommandEvent& event);

	void Idle(wxIdleEvent& event);

	void CalcFocus();
	bool UpdateSpotTracker();

protected:

	wxPanel* m_panel;

	wxBoxSizer* m_videoControlSizer;
	wxBoxSizer* m_assortedSizer;

	wxBoxSizer* m_rootSizer;

	wxBoxSizer* m_sizer;

	wxBoxSizer* m_vertSizer;
	wxBoxSizer* m_horizSizer;

	wxBoxSizer* m_canvasSizer;

	wxBoxSizer* m_frameSizer;
	wxBoxSizer* m_frameLabelSizer;

	wxStaticText* m_minFrameLabel;
	wxStaticText* m_maxFrameLabel;
	wxStaticText* m_curFrameLabel;


	wxBoxSizer* m_manualFocusSizer;
	wxSlider* m_manualFocusSlider;


	wxCheckBox* m_showCrossCheck;

	wxSlider* m_frameSlider;

	std::vector<PlotWindow*> m_plotWindows;

	wxPanel* m_video_control_panel;

	TestGLCanvas *m_canvas;

	wxStaticText* m_horizLabel;
	PixelLine* m_horizPixels;

	wxStaticText* m_vertLabel;
	PixelLine* m_vertPixels;

	wxButton* m_play;
	wxButton* m_pause;
	wxButton* m_step;
	wxButton* m_rewind;

	int m_frame_number;

	VideoMode m_videoMode;

	bool m_logging;

	base_camera_server  *g_camera;	//< Camera used to get an image
	image_wrapper       *g_image;	//< Image, possibly from camera and possibly computed
	Controllable_Video  *g_video;	//< Video controls, if we have them

	wxBoxSizer* m_advancedSizer;

	wxBoxSizer* m_focusMethodSizer;
	wxRadioBox* m_focusMethodRadio;
	wxRadioBox* m_focusWeightRadio;

	wxBoxSizer* m_newPlotSizer;
	wxButton* m_newPlotButton;
	wxButton* m_newPlotArrayButton;

	wxCheckBox* m_8Bits;

	wxButton* m_Do;

	int m_channel;

	Stage* m_stage;

	float m_focus;
	float m_lastFocus;

	// calibration numbers -- these should be moved into ZGuesser class
	float m_maxFocus;
	float m_minFocus;

	wxBoxSizer* m_zSizer;
	wxStaticText* m_zLabel;
	wxTextCtrl* m_zText;
	wxTextCtrl* m_zUpText;
	wxTextCtrl* m_zVelText;
	wxTextCtrl* m_zDownText;
	
	float m_zVel;

	wxBoxSizer* m_trackingSizer;
	wxCheckBox* m_Ztracking;
	wxCheckBox* m_XYtracking;


	Spot_Information* m_spotTracker;
	bool m_predict;
	bool m_newTrackingData;
	int m_invert;
	int m_precision;
	double m_searchRadius;
	double m_sampleSpacing;
	int m_parabolafit;
	copy_of_image	    *m_last_image;
	float m_lossSensitivity;
	bool m_tracker_is_lost;
	bool m_optZ;

	ZGuesser* m_zGuess;


private:

	spot_tracker_XY  *create_appropriate_xytracker(double x, double y, double r);
	spot_tracker_Z  *create_appropriate_ztracker(void);
	void optimize_tracker(Spot_Information *tracker);

    void set_layout();
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

