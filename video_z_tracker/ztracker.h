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

#include "vrpn_Auxiliary_Logger.h"

#include <vector>


class zTracker : public wxFrame {
public:
    zTracker(wxWindow* parent, int id, const wxString& title, const wxPoint& pos=wxDefaultPosition, 
		const wxSize& size=wxDefaultSize, long style=wxDEFAULT_FRAME_STYLE, 
		char* stage_name="Focus@localhost", char* video_name="TestImage@localhost");

	~zTracker();

	void OnMenuFileOpen(wxCommandEvent& event);
    void OnMenuFileExit(wxCommandEvent& event);
    void OnMenuHelpAbout(wxCommandEvent& event);

	void OnMenuShowAdv(wxCommandEvent& event);
	void OnMenuFocusStart(wxCommandEvent& event);
	void OnMenuFocusStop(wxCommandEvent& event);

	void OnSelectDirectX(wxCommandEvent& event);
	void OnSelectPulnix(wxCommandEvent& event);
	void OnSelectRoper(wxCommandEvent& event);

	void OnFrameScroll(wxScrollEvent& event);

	void OnResize(wxSizeEvent& event); // ***********************

	void OnCrossCheck(wxCommandEvent& event);

	void OnHUDColorCheck(wxCommandEvent& event);

	void OnNewPlot(wxCommandEvent& event);
	
	void OnNewPlotArray(wxCommandEvent& event);

	void On8BitsCheck(wxCommandEvent& event);

	void OnCalibrateStageZ(wxCommandEvent& event);

	void OnDo(wxCommandEvent& event);

	void OnLoggingButton(wxCommandEvent& event);

	void OnUpdateStageCheck(wxCommandEvent& event);

	void OnZTrackDampingText(wxCommandEvent& event);

	void OnMicronsPerPixelText(wxCommandEvent& event);

	void OnMicronsPerFocusText(wxCommandEvent& event);

	void OnTargetOOFText(wxCommandEvent& event);

	void Idle(wxIdleEvent& event);

	void CalcFocus();

	void CalculateZOffset();

protected:

	wxPanel* m_panel;

	wxBoxSizer* m_assortedSizer;

	wxBoxSizer* m_rootSizer;

	wxBoxSizer* m_sizer;

	wxBoxSizer* m_vertSizer;
	wxBoxSizer* m_horizSizer;

	wxBoxSizer* m_canvasSizer;

	//wxBoxSizer* m_frameSizer;
	//wxBoxSizer* m_frameLabelSizer;

	//wxStaticText* m_minFrameLabel;
	//wxStaticText* m_maxFrameLabel;
	//wxStaticText* m_curFrameLabel;


//	wxBoxSizer* m_manualFocusSizer;
//	wxSlider* m_manualFocusSlider;


	wxCheckBox* m_showCrossCheck;

	wxCheckBox* m_HUDColorCheck;

	//wxSlider* m_frameSlider;

	std::vector<PlotWindow*> m_plotWindows;

	TestGLCanvas *m_canvas;

	wxStaticText* m_horizLabel;
	PixelLine* m_horizPixels;

	wxStaticText* m_vertLabel;
	PixelLine* m_vertPixels;


	//int m_frame_number;


	bool m_logging;

//	base_camera_server  *g_camera;	//< Camera used to get an image
//	image_wrapper       *g_image;	//< Image, possibly from camera and possibly computed
//	Controllable_Video  *g_video;	//< Video controls, if we have them
	image_wrapper		*m_image; // actually, we're gonna point this at a CameraImage

	wxBoxSizer* m_advancedSizer;

	wxBoxSizer* m_focusMethodSizer;
	wxRadioBox* m_focusMethodRadio;
	wxRadioBox* m_focusWeightRadio;

	wxBoxSizer* m_newPlotSizer;
	wxButton* m_newPlotButton;
	wxButton* m_newPlotArrayButton;

	wxCheckBox* m_8Bits;

	wxButton* m_calibrateStageZ;

	wxButton* m_Do;
	
	wxCheckBox* m_updateStage;

	int m_channel;

	Stage* m_stage;

	float m_focus;
	float m_lastFocus;

	// calibration numbers -- these should be moved into ZGuesser class
	float m_maxFocus;
	float m_minFocus;

	float m_targetOOF;
	float m_micronsPerFocus;
	float m_zTrackDamping;

	wxTextCtrl* m_zTrackDampingText;
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

	wxBoxSizer* m_loggingSizer;
	wxTextCtrl* m_logfileText;
	wxButton* m_loggingButton;

	wxTextCtrl* m_focusMeasureText;
	wxTextCtrl* m_micronsPerFocusText;
	wxTextCtrl* m_targetOOFText;

	// tracking mode 'keep bead centered' stuff
	wxTextCtrl* m_micronsPerPixelText;
	wxCheckBox* m_keepBeadCentered;
	float m_micronsPerPixel;


	// spot tracker stuff
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

	float m_stageX, m_stageY, m_stageZ;

	vrpn_Auxiliary_Logger_Remote* m_stageLogger;
	bool m_videoAndStageLogging;

	vrpn_Connection	    *m_vrpn_connection;    //< Connection to send position over
	vrpn_Tracker_Server *m_vrpn_tracker;	  //< Tracker server to send positions
	vrpn_Analog_Server  *m_vrpn_analog;        //< Analog server to report frame number

private:

	// these are used to hold stage position for updating stage positions in Idle()
	double x, y, z;

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
"16 16 6 1",
"  c black",
". c navy",
"X c red",
"o c green",
"O c gray100",
"+ c None",
/* pixels */
"++++++++++++++++",
"++ooooooooooooo+",
"oooooo+++ooooooo",
"+ooo+++++++oooo+",
"++++++++++ooo+++",
"+++++++++ooo++++",
"++++++++ooo+++++",
"+++ooooooooooo++",
"++ooooooooooo+++",
"+++++ooo++++++++",
"++++ooo+++++++++",
"+++ooo++++++++++",
"+oooo+++++++ooo+",
"ooooooo+++oooooo",
"+ooooooooooooo++",
"++++++++++++++++"
};

