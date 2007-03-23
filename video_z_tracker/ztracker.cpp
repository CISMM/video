/////////////////////////////////////////////////////////////////////////////
// Name:        ztracker.cpp
// Purpose:     realtime video bead tracking main form
// Author:      Ryan Schubert
// Created:     2/24/07
// Modified by:
/////////////////////////////////////////////////////////////////////////////


#include "ztracker.h"
#include <string>

// forward declaration
bool  get_camera(const char *type, base_camera_server **camera, Controllable_Video **video);


// Application IDs used
enum 
{
	PLAY = 100,
	PAUSE,
	SINGLE,
	REWIND,

	MENU_START_LOG,
	MENU_STOP_LOG,

	SHOW_CROSS,

	NEW_PLOT,
	NEW_PLOT_ARRAY,

	FOCUS_METHOD,
	FOCUS_WEIGHT,

	BIT_DEPTH,

	DO,

	LAST_LOCAL_ID
};


BEGIN_EVENT_TABLE(zTracker, wxFrame)
    EVT_MENU(wxID_OPEN, zTracker::OnMenuFileOpen)
    EVT_MENU(wxID_EXIT, zTracker::OnMenuFileExit)
    EVT_MENU(wxID_HELP, zTracker::OnMenuHelpAbout)	
	EVT_MENU(MENU_START_LOG, zTracker::OnMenuFocusStart)
	EVT_MENU(MENU_STOP_LOG, zTracker::OnMenuFocusStop)

	EVT_BUTTON(PLAY, zTracker::OnVideoPlay)
	EVT_BUTTON(PAUSE, zTracker::OnVideoPause)
	EVT_BUTTON(SINGLE, zTracker::OnVideoSingle)
	EVT_BUTTON(REWIND, zTracker::OnVideoRewind)

	EVT_SCROLL_THUMBTRACK(zTracker::OnFrameScroll)
	// EVT_SCROLL_CHANGED will also take into account keyboard controlling of the
	//		slider as well as clicking on the slider, but not at the current position
	EVT_SCROLL_CHANGED(zTracker::OnFrameScroll) 

	EVT_CHECKBOX(SHOW_CROSS, zTracker::OnCrossCheck)

	EVT_BUTTON(NEW_PLOT, zTracker::OnNewPlot)
	EVT_BUTTON(NEW_PLOT_ARRAY, zTracker::OnNewPlotArray)

	EVT_CHECKBOX(BIT_DEPTH, zTracker::On8BitsCheck)

	EVT_BUTTON(DO, zTracker::OnDo)

	EVT_IDLE(zTracker::Idle)
END_EVENT_TABLE()


zTracker::zTracker(wxWindow* parent, int id, const wxString& title, const wxPoint& pos, const wxSize& size, long style):
    wxFrame(parent, id, title, pos, size, wxDEFAULT_FRAME_STYLE)
{
	g_video = NULL;
	g_camera = NULL;
	g_image = NULL;

	m_frame_number = -1;

	m_channel = 0;			// R

	m_logging = false;

	SetIcon(wxIcon(sample_xpm));

	// Make the "File" menu
    wxMenu *fileMenu = new wxMenu;
    fileMenu->Append(wxID_OPEN, wxT("&Open..."));
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_EXIT, wxT("E&xit\tALT-X"));

    // Make the "Help" menu
    wxMenu *helpMenu = new wxMenu;
    helpMenu->Append(wxID_HELP, wxT("&About..."));


	// Make the "Focus" menu
	wxMenu *focusMenu = new wxMenu;
	focusMenu->Append(MENU_START_LOG, wxT("&Start Logging"));
	focusMenu->Append(MENU_STOP_LOG, wxT("S&top Logging"));


	// Make it happen!
    wxMenuBar *menuBar = new wxMenuBar;
    menuBar->Append(fileMenu, wxT("&File"));
	menuBar->Append(focusMenu, wxT("F&ocus"));
    menuBar->Append(helpMenu, wxT("&Help"));
    SetMenuBar(menuBar);

	CreateStatusBar(3);
	SetStatusText(wxT("Z tracker operational"));

	// to avoid assert failure from GLCanvas
	this->Show();

	m_canvas = new TestGLCanvas(this, wxID_ANY, wxDefaultPosition,
        wxSize(200, 200), wxNO_BORDER);

/*
	m_canvasZoomed = new TestGLCanvas(this, wxID_ANY, wxPoint(348,0),
        wxSize(174, 130), wxSUNKEN_BORDER);
*/
	m_horizLabel = new wxStaticText(this, wxID_ANY, wxT("Horizontal"), wxPoint(10,265));
	m_horizPixels = new PixelLine(this, wxID_ANY, wxDefaultPosition, wxSize(160, 10), 0, "Horiz", true);

	m_vertLabel = new wxStaticText(this, wxID_ANY, wxT("Vertical"), wxPoint(300,5));
	m_vertPixels = new PixelLine(this, wxID_ANY, wxDefaultPosition, wxSize(10, 160), 0, "Vert", false);

	m_canvas->SetVPixRef(m_vertPixels);
	m_canvas->SetHPixRef(m_horizPixels);
	
	m_frameSlider = new wxSlider(this, wxID_ANY, 0, 0, 299);

	m_minFrameLabel = new wxStaticText(this, wxID_ANY, wxT("0"));
	m_maxFrameLabel = new wxStaticText(this, wxID_ANY, wxT("max"));
	m_curFrameLabel = new wxStaticText(this, wxID_ANY, wxT("cur"));

	// Make all our sizers we'll need for a nice layout
	m_frameSizer = new wxBoxSizer(wxVERTICAL);
	m_frameLabelSizer = new wxBoxSizer(wxHORIZONTAL);
	m_sizer = new wxBoxSizer(wxVERTICAL);
	m_vertSizer = new wxBoxSizer(wxVERTICAL);
	m_horizSizer = new wxBoxSizer(wxVERTICAL);
	m_canvasSizer = new wxBoxSizer(wxHORIZONTAL);

	m_frameSlider->Disable();

	m_assortedSizer = new wxBoxSizer(wxHORIZONTAL);
	m_videoControlSizer = new wxBoxSizer(wxHORIZONTAL);

	m_play = new wxButton(this, PLAY, "play");
	m_pause = new wxButton(this, PAUSE, "pause");
	m_step = new wxButton(this, SINGLE, "single step");
	m_rewind = new wxButton(this, REWIND, "rewind");

	m_showCrossCheck = new wxCheckBox(this, SHOW_CROSS, "Show crosshairs");
	m_showCrossCheck->SetValue(true);

	m_newPlotButton = new wxButton(this, NEW_PLOT, "New plot window");	

	m_newPlotArrayButton = new wxButton(this, NEW_PLOT_ARRAY, "plot array");


	m_focusMethodSizer = new wxBoxSizer(wxVERTICAL);
	m_focusMethod0Radio = new wxRadioButton(this, FOCUS_METHOD, "SMD", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	m_focusMethod1Radio = new wxRadioButton(this, FOCUS_METHOD, "scaled SMD");

	m_focusWeightSizer = new wxBoxSizer(wxVERTICAL);
	m_focusWeight0Radio = new wxRadioButton(this, FOCUS_WEIGHT, "Uniform", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	m_focusWeight1Radio = new wxRadioButton(this, FOCUS_WEIGHT, "Tent");


	m_8Bits = new wxCheckBox(this, BIT_DEPTH, "8 bits");
	m_8Bits->SetValue(false);


	m_Do = new wxButton(this, DO, "Do!");

	m_tracking = new wxCheckBox(this, wxID_ANY, "Auto-track");
	m_goingUp = false;
	m_zVel = 0;


	m_zSizer = new wxBoxSizer(wxVERTICAL);
	m_zLabel = new wxStaticText(this, wxID_ANY, "Z");
	m_zText = new wxTextCtrl(this, wxID_ANY, "0", wxDefaultPosition, wxSize(40, 20), wxTE_CENTER | wxTE_READONLY);
	m_zUpText = new wxTextCtrl(this, wxID_ANY, " ", wxDefaultPosition, wxSize(20, 20), wxTE_CENTER | wxTE_READONLY);
	m_zVelText = new wxTextCtrl(this, wxID_ANY, "0", wxDefaultPosition, wxSize(50, 20), wxTE_CENTER | wxTE_READONLY);
	m_zDownText = new wxTextCtrl(this, wxID_ANY, " ", wxDefaultPosition, wxSize(20, 20), wxTE_CENTER | wxTE_READONLY);






	// ****************************************
	// *** DO ALL INSTANTIATING BEFORE THIS ***
	// ****************************************
	set_layout();


	// by default let's look for a simple directx camera
	if (!get_camera("directx", &g_camera, &g_video)) 
	{
		fprintf(stderr,"Cannot open camera\n");
		if (g_camera) { delete g_camera; g_camera = NULL; }
		return;
	}
	else
	{
		m_frame_number = -1;

		int width = g_camera->get_num_columns();
		int height = g_camera->get_num_rows();

		m_canvas->SetInput(g_camera);

		m_canvas->SetSize(width, height);
		m_canvas->SetMinSize(wxSize(width, height));

		m_canvas->SetNumBits(8);
		m_8Bits->SetValue(true);
	}


	do_layout();
}


// File|Open... command
void zTracker::OnMenuFileOpen( wxCommandEvent& WXUNUSED(event) )
{

    wxString filename = wxFileSelector(wxT("Select input image."), wxT(""), wxT(""), wxT(""),
        wxT("All files (*.*)|*.*"),
		wxFD_OPEN);

	if (!filename.IsEmpty())
	{
//		printf("filename.Length() = %i\n", filename.Length());

		if (!get_camera(filename.c_str(), &g_camera, &g_video)) 
		{
			fprintf(stderr,"Cannot open camera\n");
			if (g_camera) { delete g_camera; g_camera = NULL; }
			return;
		}

		m_frame_number = -1;

		int width = g_camera->get_num_columns();
		int height = g_camera->get_num_rows();

		m_canvas->SetInput(g_camera);

		m_canvas->SetSize(width, height);
		m_canvas->SetMinSize(wxSize(width, height));

		m_canvas->SetNumBits(16);
		m_8Bits->SetValue(false);

		m_minFrameLabel->SetLabel(wxT("0"));
		m_maxFrameLabel->SetLabel(wxT("max"));

		int numFrames = g_video->get_num_frames();
		if (numFrames >= 0)
		{
			m_frameSlider->Enable();

			m_frameSlider->SetMax(numFrames - 1);
			char* maxFrame = new char[8];
			itoa(numFrames - 1, maxFrame, 10);
			m_maxFrameLabel->SetLabel(maxFrame);
			delete maxFrame;
		}
		else
		{
			m_frameSlider->Disable();
		}

		for (int i = 0; i < m_plotWindows.size(); ++i)
		{
			m_plotWindows[i]->Destroy();
		}
		m_plotWindows.clear();

		do_layout();
	}
}

// File|Exit command
void zTracker::OnMenuFileExit( wxCommandEvent& WXUNUSED(event) )
{
    // true is to force the frame to close
    Close(true);
}

// Help|About... command
void zTracker::OnMenuHelpAbout( wxCommandEvent& WXUNUSED(event) )
{
    wxMessageBox(wxT("Realtime video-based 3D bead tracker prototype version."));
}


void zTracker::OnVideoPlay( wxCommandEvent& WXUNUSED(event) )
{
	if (g_video != NULL)
		g_video->play();
	m_videoMode = PLAYING;
}

void zTracker::OnVideoPause( wxCommandEvent& WXUNUSED(event) )
{
	if (g_video != NULL)
		g_video->pause();
	m_videoMode = PAUSED;
}

void zTracker::OnVideoSingle( wxCommandEvent& WXUNUSED(event) )
{
	if (g_video != NULL)
		g_video->single_step();
	m_videoMode = SINGLE_STEPPING;
}

void zTracker::OnVideoRewind( wxCommandEvent& WXUNUSED(event) )
{
	if (g_video != NULL)
		g_video->rewind();
	m_frame_number = -1;

	float x, y, z;
	m_stage.GetPosition(x, y, z);
	m_stage.SetPosition(x, y, m_frame_number / 10.0f);


	m_videoMode = SINGLE_STEPPING;
	m_logging = false;
}

void zTracker::OnMenuFocusStart( wxCommandEvent& WXUNUSED(event) )
{
	if (!m_logging)
	{
		for (int i = 0; i < m_plotWindows.size(); ++i)
		{
			m_plotWindows[i]->vals.clear();
			m_plotWindows[i]->SetOffset(m_frame_number);
		}
	}
	m_logging = true;
	printf("Logging...\n");
}

void zTracker::OnMenuFocusStop( wxCommandEvent& WXUNUSED(event) )
{
	printf("Stopped logging...\n");

	m_logging = false;
}


void zTracker::OnFrameScroll(wxScrollEvent& event)
{
	m_logging = false; // stop logging since we're jumping anyway

	int curFrame = m_frameSlider->GetValue();
	if (g_video->jump_to_frame(curFrame))
	{
		m_frame_number = curFrame - 1;
	}
	float x, y, z;
	m_stage.GetPosition(x, y, z);
	m_stage.SetPosition(x, y, m_frame_number / 10.0f);

}

void zTracker::OnResize(wxSizeEvent& event)
{
	// nothing here...wxwindows wasn't happy with this event--not sure why...
}

void zTracker::OnCrossCheck(wxCommandEvent& WXUNUSED(event))
{
	m_canvas->SetShowCross(m_showCrossCheck->IsChecked());
}

void zTracker::On8BitsCheck(wxCommandEvent& WXUNUSED(event))
{
	if (m_8Bits->GetValue())
	{
		m_canvas->SetNumBits(8);
	}
	else
	{
		m_canvas->SetNumBits(16);
	}
}

void zTracker::OnNewPlot(wxCommandEvent& event)
{
	std::string title = "";
	int method = 0, weight = 0;

	if (m_focusMethod0Radio->GetValue())
	{
		method = 0;
		title += "SMD, ";
	}
	if (m_focusMethod1Radio->GetValue())
	{
		method = 1;
		title += "scaled SMD, ";
	}

	if (m_focusWeight0Radio->GetValue())
	{
		weight = 0;
		title += "Uniform";
	}
	if (m_focusWeight1Radio->GetValue())
	{
		weight = 1;
		title += "Tent";
	}

	PlotWindow* temp = new PlotWindow(this, wxID_ANY, title.c_str());
	temp->SetMethod(method);
	temp->SetWeightedMethod(weight);
	m_plotWindows.push_back(temp);
}

void zTracker::OnNewPlotArray(wxCommandEvent& event)
{
	wxPoint pos = this->GetPosition();
	pos = pos + wxPoint(this->GetSize().x, 0);

	std::string title = "";
	int method = 0, weight = 0;
	for (method = 0; method <= 1; ++method)
	{
		PlotWindow* temp;
		for (weight = 0; weight <= 1; ++weight)
		{
			if (method == 0)
				title = "SMD, ";
			else if (method == 1)
				title = "scaled SMD, ";

			if (weight == 0)
				title += "Uniform";
			else if (weight == 1)
				title += "Tent";

			temp = new PlotWindow(this, wxID_ANY, title.c_str(), pos);
			temp->SetMethod(method);
			temp->SetWeightedMethod(weight);
			m_plotWindows.push_back(temp);

			pos = pos + wxPoint(temp->GetSize().x, 0);
		}
		pos = pos - wxPoint(weight * temp->GetSize().x, 0);
		pos = pos + wxPoint(0, temp->GetSize().y);
	}
}

void zTracker::CalcFocus()
{
	float smd1, smd2;

	if (m_logging)
	{
		for (int i = 0; i < m_plotWindows.size(); ++i)
		{
			smd1 = m_horizPixels->calcFocus(m_channel, m_plotWindows[i]->method, m_plotWindows[i]->weightedMethod);
			smd2 = m_vertPixels->calcFocus(m_channel, m_plotWindows[i]->method, m_plotWindows[i]->weightedMethod);
			m_plotWindows[i]->vals.push_back(smd1 + smd2);
			m_plotWindows[i]->Update();
//			printf("%i: (%f)\n", m_frame_number, smd1 + smd2);
		}
	}

	int method = 0, weight = 0;
	if (m_focusMethod0Radio->GetValue())
		method = 0;
	if (m_focusMethod1Radio->GetValue())
		method = 1;

	if (m_focusWeight0Radio->GetValue())
		weight = 0;
	if (m_focusWeight1Radio->GetValue())
		weight = 1;

	smd1 = m_horizPixels->calcFocus(m_channel, method, weight);
	smd2 = m_vertPixels->calcFocus(m_channel, method, weight);
	float focusMeasure = smd1 + smd2;

	float stageX, stageY, stageZ;
	m_stage.GetPosition(stageX, stageY, stageZ);

	if (m_tracking->GetValue()) // if we're doing active, automatic tracking
	{
		float delta = focusMeasure - m_lastFocus;
			
		if (delta > 0)// || abs(delta) < focusMeasure / 100.0f)
		{
			// we seem to have gone in the right direction!
			if (m_zVel > 0)
				m_zVel += 0.01;
			else
				m_zVel -= 0.01;
		}
		else if (delta < 0)
		{
			// we got worse!
			if (m_zVel > 0)
				m_zVel -= 0.02;
			else
				m_zVel += 0.02;
		}
		else
		{
			m_zVel += 0.001 * ((rand()/(float)RAND_MAX) - 0.5); // move a bit just in case :)
		}

//		printf("\tdelta = %f\t", delta);
//		printf("m_zVel = %f\n", m_zVel);
		if (m_zVel > 0)
		{
			m_zUpText->SetValue("/\\");
			m_zDownText->SetValue("");
		}
		else if (m_zVel < 0)
		{
			m_zUpText->SetValue("");
			m_zDownText->SetValue("\\/");
		}
		else
		{
			m_zUpText->SetValue("");
			m_zDownText->SetValue("");
		}


		m_stage.MoveTo(stageX, stageY, stageZ + m_zVel);

	}

	m_lastFocus = focusMeasure;
}

void zTracker::OnDo(wxCommandEvent& event)
{
	float stageX, stageY, stageZ;
	m_stage.GetTargetPosition(stageX, stageY, stageZ);
	m_stage.MoveTo(stageX, stageY, stageZ + 1);
}

void zTracker::Idle(wxIdleEvent& WXUNUSED(event))
{
	m_stage.Update();

	if (m_logging)
	{
		SetStatusText("Logging...", 1);
	}
	else
	{
		SetStatusText("", 1);
	}

	if (m_tracking->GetValue())
	{
		m_videoMode = PAUSED; // stop 'playing' or 'single stepping' if we're auto-tracking
	}


	float x, y, z;
	m_stage.GetPosition(x, y, z);
	// update "image" (i.e. frame_number) based on z position of stage! (unless we're playing/stepping)
	if (m_videoMode == PAUSED)
	{
		m_frame_number = z * 10;
	}


	char buffer[20] = "";
	sprintf(buffer, "%.4f", z);	
	m_zText->SetValue(buffer);
	sprintf(buffer, "%.4f", m_zVel);	
	m_zVelText->SetValue(buffer);

	if (g_video != NULL && m_videoMode != PAUSED)
		g_video->jump_to_frame(m_frame_number - 1);

	if (g_camera != NULL)
	{
		if (g_camera->read_image_to_memory()) 
		{
			if (m_videoMode == SINGLE_STEPPING || m_videoMode == PLAYING)
			{
				++m_frame_number;
				m_stage.SetPosition(x, y, m_frame_number / 10.0f);
			}
			for (int i = 0; i < m_plotWindows.size(); ++i)
			{
				m_plotWindows[i]->SetIndicator(m_frame_number);
			}

			printf("frame: %i\n", m_frame_number);

			char* frame = new char[8];
			itoa(m_frame_number, frame, 10);
			m_curFrameLabel->SetLabel(frame);
			m_frameSlider->SetValue(m_frame_number);
			delete frame;

			CalcFocus();

			if (m_videoMode == SINGLE_STEPPING)
				m_videoMode = PAUSED;
		}
		else
		{
			//fprintf(stderr, "Something incredibly wrong happened in zTracker::Idle...\t\n");
			if (m_videoMode == PLAYING || m_videoMode == SINGLE_STEPPING)
			{
				CalcFocus();
				m_logging = false;
				m_videoMode = PAUSED;
			}
		}
	}

	if (m_canvas != NULL)
	{
		m_canvas->Refresh();
	}

	for (int i = 0; i < m_plotWindows.size(); ++i)
	{
		if (m_plotWindows[i] != NULL)
		{
			m_plotWindows[i]->Refresh();
		}
	}
}


// only called once at end of constructor
void zTracker::set_layout()
{
	m_vertSizer->Add(m_vertLabel, 0, wxALIGN_CENTER | wxALL, 3);
	m_vertSizer->Add(m_vertPixels, 0, wxALIGN_CENTER | wxALL, 3);

	m_zSizer->Add(m_zLabel, 0, wxALIGN_CENTER);
	m_zSizer->Add(m_zText, 0, wxALIGN_CENTER);
	m_zSizer->Add(m_zUpText, 0, wxALIGN_CENTER);
	m_zSizer->Add(m_zVelText, 0, wxALIGN_CENTER);
	m_zSizer->Add(m_zDownText, 0, wxALIGN_CENTER);

	m_canvasSizer->Add(m_canvas, 0, wxALL, 3);
	m_canvasSizer->Add(m_vertSizer, 0, 0, 0);
	m_canvasSizer->Add(m_zSizer, 0, wxALL, 3);

	m_horizSizer->Add(m_horizLabel, 0, wxALIGN_CENTER | wxALL, 3);
	m_horizSizer->Add(m_horizPixels, 0, wxALIGN_CENTER | wxALL, 3);

	m_frameLabelSizer->Add(m_minFrameLabel, 0, wxLEFT | wxRIGHT, 3);
	m_frameLabelSizer->Add(0, 0, 1);
	m_frameLabelSizer->Add(m_curFrameLabel, 0, 0, 0);
	m_frameLabelSizer->Add(0, 0, 1);
	m_frameLabelSizer->Add(m_maxFrameLabel, 0, wxLEFT | wxRIGHT, 3);

	m_frameSizer->Add(m_frameSlider, 0, wxEXPAND | wxLEFT | wxRIGHT, 3);
	m_frameSizer->Add(m_frameLabelSizer, 0, wxEXPAND | wxLEFT | wxRIGHT, 3);

	m_videoControlSizer->Add(0, 0, 1);
	m_videoControlSizer->Add(m_play, 0, wxEXPAND | wxTOP | wxBOTTOM, 3);
	m_videoControlSizer->Add(m_step, 0, wxEXPAND | wxTOP | wxBOTTOM, 3);
	m_videoControlSizer->Add(m_pause, 0, wxEXPAND | wxTOP | wxBOTTOM, 3);
	m_videoControlSizer->Add(m_rewind, 0, wxEXPAND | wxTOP | wxBOTTOM, 3);
	m_videoControlSizer->Add(0, 0, 1);

	m_focusMethodSizer->Add(new wxStaticText(this, wxID_ANY, "Focus Method"));
	m_focusMethodSizer->Add(m_focusMethod0Radio, 0, 0);
	m_focusMethodSizer->Add(m_focusMethod1Radio, 0, 0);

	m_focusWeightSizer->Add(new wxStaticText(this, wxID_ANY, "Focus Weighting"));
	m_focusWeightSizer->Add(m_focusWeight0Radio, 0, 0);
	m_focusWeightSizer->Add(m_focusWeight1Radio, 0, 0);

	m_assortedSizer->Add(m_showCrossCheck, 0, wxALL, 3);
	m_assortedSizer->Add(m_newPlotButton, 0, wxALL, 3);
	m_assortedSizer->Add(m_focusMethodSizer, 0, wxALL, 3);
	m_assortedSizer->Add(m_focusWeightSizer, 0, wxALL, 3);
	m_assortedSizer->Add(m_newPlotArrayButton, 0, wxALL, 3);
	m_assortedSizer->Add(m_8Bits, 0, wxALL, 3);
	m_assortedSizer->Add(m_Do, 0, wxALL, 3);
	m_assortedSizer->Add(m_tracking, 0, wxALL, 3);

	m_sizer->Add(m_canvasSizer, 0, 0, 0);
	m_sizer->Add(m_horizSizer, 0, wxLEFT, 3);
	m_sizer->Add(m_frameSizer, 0, wxEXPAND, 0);
	m_sizer->Add(m_videoControlSizer, 0, wxEXPAND | wxLEFT | wxRIGHT, 3);
	m_sizer->Add(m_assortedSizer, 0, 0, 0);
}

// updates the layout whenever things change
void zTracker::do_layout()
{
	m_sizer->SetSizeHints(this);
	SetSizer(m_sizer);

	// set the seperate plot window position next to the main window
	//m_plotWindow->SetPosition(this->GetPosition() + wxPoint(this->GetSize().x, 0));
}



/// Open the wrapped camera we want to use depending on the name of the
//  camera we're trying to open.
bool  get_camera(const char *type, base_camera_server **camera, Controllable_Video **video)
{
#ifdef VZT_USE_ROPER
  if (!strcmp(type, "roper")) {
    // XXX Starts with binning of 2 to get the image size down so that
    // it fits on the screen.
    roper_server *r = new roper_server(2);
    *camera = r;
    g_camera_bit_depth = 12;
  } else
#endif  
#ifdef VZT_USE_COOKE
  if (!strcmp(type, "cooke")) {
    // XXX Starts with binning of 2 to get the image size down so that
    // it fits on the screen.
    cooke_server *r = new cooke_server(2);
    *camera = r;
    g_camera_bit_depth = 16;
  } else
#endif  
#ifdef	VZT_USE_DIAGINC
  if (!strcmp(type, "diaginc")) {
    // XXX Starts with binning of 2 to get the image size down so that
    // it fits on the screen.
    diaginc_server *r = new diaginc_server(2);
    *camera = r;
    g_exposure = 80;	// Seems to be the minimum exposure for the one we have
    g_camera_bit_depth = 12;
  } else
#endif  
#ifdef	VZT_USE_EDT
  if (!strcmp(type, "edt")) {
    edt_server *r = new edt_server();
    *camera = r;
  } else
#endif  
#ifdef	VZT_USE_DIRECTX
  if (!strcmp(type, "directx")) {
    // Passing width and height as zero leaves it open to whatever the camera has
    directx_camera_server *d = new directx_camera_server(1,0,0);	// Use camera #1 (first one found)
    *camera = d;
  } else if (!strcmp(type, "directx640x480")) {
    directx_camera_server *d = new directx_camera_server(1,640,480);	// Use camera #1 (first one found)
    *camera = d;

  // If this is a VRPN URL for an SEM device, then open the file and set up
  // to read from that device.
  } else
#endif  
#ifdef	VZT_USE_SEM
  if (!strncmp(type, "SEM@", 4)) {
    SEM_Controllable_Video  *s = new SEM_Controllable_Video (type);
    *camera = s;
    *video = s;
    g_camera_bit_depth = 16;

  // Unknown type, so we presume that it is a file.  Now we figure out what
  // kind of file based on the extension and open the appropriate type of
  // imager.
  } else
#endif  
  {
    fprintf(stderr,"get_camera(): Assuming filename (%s)\n", type);

    // If the extension is ".raw" then we assume it is a Pulnix file and open
    // it that way.
    if (  (strcmp(".raw", &type[strlen(type)-4]) == 0) ||
	  (strcmp(".RAW", &type[strlen(type)-4]) == 0) ) {
      Pulnix_Controllable_Video *f = new Pulnix_Controllable_Video(type);
      *camera = f;
      *video = f;

    // If the extension is ".spe" then we assume it is a Roper file and open
    // it that way.
#ifdef	VZT_USE_ROPER
    } else if ( (strcmp(".spe", &type[strlen(type)-4]) == 0) ||
		(strcmp(".SPE", &type[strlen(type)-4]) == 0) ) {
      SPE_Controllable_Video *f = new SPE_Controllable_Video(type);
      *camera = f;
      *video = f;
      g_camera_bit_depth = 12;

    // If the extension is ".sem" then we assume it is a VRPN-format file
    // with an SEM device in it, so we form the name of the device and open
    // a VRPN Remote object to handle it.
#endif
#ifdef	VZT_USE_SEM
    } else if (strcmp(".sem", &type[strlen(type)-4]) == 0) {
      char *name;
      if ( NULL == (name = new char[strlen(type) + 20]) ) {
	fprintf(stderr,"Out of memory when allocating file name\n");
	cleanup();
        exit(-1);
      }
      sprintf(name, "SEM@file:%s", type);
      SEM_Controllable_Video *s = new SEM_Controllable_Video(name);
      *camera = s;
      *video = s;
      delete [] name;

    // If the extension is ".tif" or ".tiff" or ".bmp" then we assume it is
    // a file or stack of files to be opened by ImageMagick.
#endif
    } else if (   (strcmp(".tif", &type[strlen(type)-4]) == 0) ||
		  (strcmp(".TIF", &type[strlen(type)-4]) == 0) ||
		  (strcmp(".bmp", &type[strlen(type)-4]) == 0) ||
		  (strcmp(".BMP", &type[strlen(type)-4]) == 0) ||
		  (strcmp(".jpg", &type[strlen(type)-4]) == 0) ||
		  (strcmp(".JPG", &type[strlen(type)-4]) == 0) ||
		  (strcmp(".tiff", &type[strlen(type)-5]) == 0) || 
		  (strcmp(".TIFF", &type[strlen(type)-5]) == 0) ) {
      FileStack_Controllable_Video *s = new FileStack_Controllable_Video(type);
      *camera = s;
      *video = s;
//      g_camera_bit_depth = 16;

    // If the extension is ".stk"  then we assume it is a Metamorph file
    // to be opened by the Metamorph reader.
#ifdef	USE_METAMORPH
    } else if (strcmp(".stk", &type[strlen(type)-4]) == 0) {
      MetamorphStack_Controllable_Video *s = new MetamorphStack_Controllable_Video(type);
      *camera = s;
      *video = s;

    // File of unknown type.  We assume that it is something that DirectX knows
    // how to open.
#endif
    } else {
#ifdef	VZT_USE_DIRECTX
      Directx_Controllable_Video *f = new Directx_Controllable_Video(type);
      *camera = f;
      *video = f;
#else
	fprintf(stderr,"Unknown camera or file type\n");
	return false;
#endif
    }
  }
  return true;
}
