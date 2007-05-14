/////////////////////////////////////////////////////////////////////////////
// Name:        ztracker.cpp
// Purpose:     realtime video bead tracking main form
// Author:      Ryan Schubert
// Created:     2/24/07
// Modified by:
/////////////////////////////////////////////////////////////////////////////


#include "ztracker.h"

#include <string>


// FFTW for FFT computation
#include "fftw3.h"


// forward declarations
bool  get_camera(const char *type, base_camera_server **camera, Controllable_Video **video);



// spot tracker library stuff...this is ugly
unsigned  Spot_Information::d_static_index = 0;	  //< Start the first instance of a Spot_Information index at zero.

static const int KERNEL_DISK = 0;
static const int KERNEL_CONE = 1;
static const int KERNEL_SYMMETRIC = 2;
static const int KERNEL_FIONA = 3;

int g_kernel_type = KERNEL_SYMMETRIC;



// Application IDs used
enum 
{
	PLAY = 100,
	PAUSE,
	SINGLE,
	REWIND,

	MENU_SHOW_ADV,
	MENU_START_LOG,
	MENU_STOP_LOG,
	MENU_DIRECTX,
	MENU_PULNIX,
	MENU_ROPER,

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

	EVT_MENU(MENU_SHOW_ADV, zTracker::OnMenuShowAdv)
	EVT_MENU(MENU_START_LOG, zTracker::OnMenuFocusStart)
	EVT_MENU(MENU_STOP_LOG, zTracker::OnMenuFocusStop)

	EVT_MENU(MENU_DIRECTX, zTracker::OnSelectDirectX)
	EVT_MENU(MENU_PULNIX, zTracker::OnSelectPulnix)
	EVT_MENU(MENU_ROPER, zTracker::OnSelectRoper)

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

zTracker::zTracker(wxWindow* parent, int id, const wxString& title, const wxPoint& pos, const wxSize& size, long style, char* stage_name):
    wxFrame(parent, id, title, pos, size, wxDEFAULT_FRAME_STYLE)
{
	g_video = NULL;
	g_camera = NULL;
	g_image = NULL;

	m_frame_number = -1;

	m_channel = 0;			// Red

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


	// Make the "Advanced" menu
	wxMenu *advancedMenu = new wxMenu;
	advancedMenu->Append(MENU_SHOW_ADV, wxT("Toggle &Adv. Options"));
	advancedMenu->Append(MENU_START_LOG, wxT("&Start Logging"));
	advancedMenu->Append(MENU_STOP_LOG, wxT("S&top Logging"));

	// Make the "Camera" menu
	wxMenu *cameraMenu = new wxMenu;
	cameraMenu->Append(MENU_DIRECTX, wxT("&DirectX Camera"));
	cameraMenu->Append(MENU_PULNIX, wxT("&Pulnix"));
	cameraMenu->Append(MENU_ROPER, wxT("&Roper"));


	// Make it happen!
    wxMenuBar *menuBar = new wxMenuBar;
    menuBar->Append(fileMenu, wxT("&File"));
	menuBar->Append(cameraMenu, wxT("&Camera"));
	menuBar->Append(advancedMenu, wxT("&Advanced"));
    menuBar->Append(helpMenu, wxT("&Help"));
    SetMenuBar(menuBar);

	CreateStatusBar(3);
	SetStatusText(wxT("Z tracker operational"));

	m_panel = new wxPanel(this);

	// to avoid assert failure from GLCanvas
	this->Show();

	m_canvas = new TestGLCanvas(m_panel, wxID_ANY, wxDefaultPosition,
        wxSize(200, 200), wxNO_BORDER);

	m_horizLabel = new wxStaticText(m_panel, wxID_ANY, wxT("Horizontal"), wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxNO_BORDER);
	m_horizPixels = new PixelLine(m_panel, wxID_ANY, wxDefaultPosition, wxSize(160, 10), wxNO_BORDER, "Horiz", true);

	m_vertLabel = new wxStaticText(m_panel, wxID_ANY, wxT("Vertical"), wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxNO_BORDER);
	m_vertPixels = new PixelLine(m_panel, wxID_ANY, wxDefaultPosition, wxSize(10, 160), wxNO_BORDER, "Vert", false);

	m_canvas->SetVPixRef(m_vertPixels);
	m_canvas->SetHPixRef(m_horizPixels);

	m_frameSlider = new wxSlider(m_panel, wxID_ANY, 0, 0, 299, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxNO_BORDER);

	m_minFrameLabel = new wxStaticText(m_panel, wxID_ANY, wxT("0"));
	m_maxFrameLabel = new wxStaticText(m_panel, wxID_ANY, wxT("max"));
	m_curFrameLabel = new wxStaticText(m_panel, wxID_ANY, wxT("cur"));

	// Make all our sizers we'll need for a nice layout
	m_rootSizer = new wxBoxSizer(wxVERTICAL);
	m_zSizer = new wxBoxSizer(wxVERTICAL);
	m_frameSizer = new wxBoxSizer(wxVERTICAL);
	m_frameLabelSizer = new wxBoxSizer(wxHORIZONTAL);
	m_sizer = new wxBoxSizer(wxVERTICAL);
	m_vertSizer = new wxBoxSizer(wxVERTICAL);
	m_horizSizer = new wxBoxSizer(wxVERTICAL);
	m_canvasSizer = new wxBoxSizer(wxHORIZONTAL);

	m_frameSlider->Disable();

	m_assortedSizer = new wxBoxSizer(wxHORIZONTAL);
	m_videoControlSizer = new wxBoxSizer(wxHORIZONTAL);

	m_play = new wxButton(m_panel, PLAY, "play");
	m_pause = new wxButton(m_panel, PAUSE, "pause");
	m_step = new wxButton(m_panel, SINGLE, "single step");
	m_rewind = new wxButton(m_panel, REWIND, "rewind");

	m_showCrossCheck = new wxCheckBox(m_panel, SHOW_CROSS, "Show crosshairs");
	m_showCrossCheck->SetValue(true);

	m_advancedSizer = new wxBoxSizer(wxHORIZONTAL);
	
	m_focusMethodSizer = new wxBoxSizer(wxVERTICAL);
	const wxString focusMethods[] = {
        wxT("SMD"),
        wxT("Scaled SMD")
    };
	m_focusMethodRadio = new wxRadioBox(m_panel, wxID_ANY, "Focus method", wxDefaultPosition, wxDefaultSize, 
		2, focusMethods, 1, wxRA_SPECIFY_ROWS);

	const wxString focusWeights[] = {
        wxT("Uniform"),
        wxT("Tent")
    };
	m_focusWeightRadio = new wxRadioBox(m_panel, wxID_ANY, "Focus weighting", wxDefaultPosition, wxDefaultSize, 
		2, focusWeights, 1, wxRA_SPECIFY_ROWS);


	m_newPlotSizer = new wxBoxSizer(wxVERTICAL);
	m_newPlotButton = new wxButton(m_panel, NEW_PLOT, "New plot window");	
	m_newPlotArrayButton = new wxButton(m_panel, NEW_PLOT_ARRAY, "plot array");



	m_8Bits = new wxCheckBox(m_panel, BIT_DEPTH, "8 bits");
	m_8Bits->SetValue(false);


	m_Do = new wxButton(m_panel, DO, "Do!");

	m_trackingSizer = new wxBoxSizer(wxVERTICAL);
	m_Ztracking = new wxCheckBox(m_panel, wxID_ANY, "Track Z");
	m_XYtracking = new wxCheckBox(m_panel, wxID_ANY, "Track XY");
	m_zVel = 0;


	m_zLabel = new wxStaticText(m_panel, wxID_ANY, "Z");
	m_zText = new wxTextCtrl(m_panel, wxID_ANY, "0", wxDefaultPosition, wxSize(50, 20), wxTE_CENTER | wxTE_READONLY);
	m_zUpText = new wxTextCtrl(m_panel, wxID_ANY, " ", wxDefaultPosition, wxSize(20, 20), wxTE_CENTER | wxTE_READONLY);
	m_zVelText = new wxTextCtrl(m_panel, wxID_ANY, "0", wxDefaultPosition, wxSize(50, 20), wxTE_CENTER | wxTE_READONLY);
	m_zDownText = new wxTextCtrl(m_panel, wxID_ANY, " ", wxDefaultPosition, wxSize(20, 20), wxTE_CENTER | wxTE_READONLY);


	m_manualFocusSizer = new wxBoxSizer(wxVERTICAL);
	m_manualFocusSlider = new wxSlider(m_panel, wxID_ANY, 0, 0, 100, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxNO_BORDER);



	// kinda arbitrary default values!
	m_minFocus = 0;
	m_maxFocus = 10;



	// spot tracker stuff
	m_invert = 0;
	m_precision = 0.25;
	m_sampleSpacing = 1;

	m_optZ = false;
	m_predict = false;
	m_newTrackingData = true;

	m_searchRadius = 0;
	m_parabolafit = 0;
	m_last_image = NULL;
	
	m_lossSensitivity = 0;
	m_tracker_is_lost = false;
	m_optZ = false;

	m_spotTracker = new Spot_Information(create_appropriate_xytracker(m_canvas->GetSelectX(), m_canvas->GetSelectY(),
		m_canvas->GetRadius()), create_appropriate_ztracker());





	m_zGuess = NULL;






	m_stage = new Stage(stage_name);



	// ****************************************
	// *** DO ALL INSTANTIATING BEFORE THIS ***
	// ****************************************
	set_layout();
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
		m_8Bits->Enable();

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

		m_sizer->Show(m_frameSizer, true, true);
		m_sizer->Show(m_videoControlSizer, true, true);

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

void zTracker::OnSelectDirectX(wxCommandEvent& WXUNUSED(event))
{
	// let's look for a simple directx camera
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

	m_sizer->Show(m_frameSizer, false, true);
	m_sizer->Show(m_videoControlSizer, false, true);

	do_layout();
}

void zTracker::OnSelectPulnix(wxCommandEvent& WXUNUSED(event))
{
	if (!get_camera("edt", &g_camera, &g_video)) 
	{
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

	m_sizer->Show(m_frameSizer, false, true);
	m_sizer->Show(m_videoControlSizer, false, true);

	do_layout();
}


// Roper camera support is NOT tested!
void zTracker::OnSelectRoper(wxCommandEvent& WXUNUSED(event))
{
	if (!get_camera("roper", &g_camera, &g_video)) 
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

		m_canvas->SetNumBits(12); // *** We only have toggle for 8 bits in GUI!
		m_8Bits->SetValue(false);
		m_8Bits->Disable();
	}

	m_sizer->Show(m_frameSizer, false, true);
	m_sizer->Show(m_videoControlSizer, false, true);

	do_layout();
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

	double x, y, z;
	m_stage->GetPosition(x, y, z);
	m_stage->SetPosition(x, y, m_frame_number / 10.0f);

	// do a single step to update displayed image to the first frame
	m_videoMode = SINGLE_STEPPING;
	m_logging = false;
}

void zTracker::OnMenuShowAdv( wxCommandEvent& WXUNUSED(event) )
{
	// hierarchically show/hide everything in m_advancedSizer
	m_sizer->Show(m_advancedSizer, !m_sizer->IsShown(m_advancedSizer), true);
	// update the gui layout accordingly
	do_layout();
}

// clear all current data from plot windows, set the offset (based on the current frame
//		and set m_logging to true)
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
}

// sets m_logging to false
void zTracker::OnMenuFocusStop( wxCommandEvent& WXUNUSED(event) )
{
	m_logging = false;
}

// *** THERE IS ONLY A SINGLE CALLBACK FOR ALL SCROLL EVENTS--THIS IS IT ***
void zTracker::OnFrameScroll(wxScrollEvent& event)
{
	double newZ = m_manualFocusSlider->GetValue();
	printf("changing z to %f\n", newZ);

	double x, y, z;
	m_stage->GetPosition(x, y, z);
	m_stage->MoveTo(x, y, newZ);
	

	/* *** FIX THIS TO WORK WITH MULTIPLE SCROLLERS! ***
	m_logging = false; // stop logging since we're jumping anyway

	int curFrame = m_frameSlider->GetValue();
	if (g_video->jump_to_frame(curFrame))
	{
		m_frame_number = curFrame - 1;
	}
	double x, y, z;
	m_stage.GetPosition(x, y, z);
	m_stage.SetPosition(x, y, m_frame_number / 10.0f);

	m_videoMode = SINGLE_STEPPING;
	*/
}

void zTracker::OnResize(wxSizeEvent& event)
{
	// nothing here...wxwindows wasn't happy with this event--not sure why...
	// could have been prior to putting everything under a single wxPanel,
	//	so this might work now, if ever needed.
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
	int method = m_focusMethodRadio->GetSelection();
	int weight = m_focusWeightRadio->GetSelection();

	title = title + m_focusMethodRadio->GetStringSelection().c_str() + ", ";
	
	title = title + m_focusWeightRadio->GetStringSelection().c_str();
	
	printf("creating new window...\n");

	PlotWindow* temp = new PlotWindow(m_panel, wxID_ANY, title.c_str());
	temp->SetMethod(method);
	temp->SetWeightedMethod(weight);
	m_plotWindows.push_back(temp);

	printf("new window created.\n");
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
			title = m_focusMethodRadio->GetString(method);

			title = title + ", " + m_focusWeightRadio->GetString(weight).c_str();

			temp = new PlotWindow(m_panel, wxID_ANY, title.c_str(), pos);
			temp->SetMethod(method);
			temp->SetWeightedMethod(weight);
			m_plotWindows.push_back(temp);

			pos = pos + wxPoint(temp->GetSize().x, 0);
		}
		pos = pos - wxPoint(weight * temp->GetSize().x, 0);
		pos = pos + wxPoint(0, temp->GetSize().y);
	}
}

bool zTracker::UpdateSpotTracker()
{

	bool awesome = true; // well, duh!

	return awesome;
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
		}

		if (!m_plotWindows.empty())
		{
			m_minFocus = m_plotWindows[0]->GetMin();
			m_maxFocus = m_plotWindows[0]->GetMax();
		}
	}

	int method = m_focusMethodRadio->GetSelection();
	int weight = m_focusWeightRadio->GetSelection();

	smd1 = m_horizPixels->calcFocus(m_channel, method, weight);
	smd2 = m_vertPixels->calcFocus(m_channel, method, weight);
	float focusMeasure = smd1 + smd2;


	double stageX, stageY, stageZ;
	m_stage->GetPosition(stageX, stageY, stageZ);

	
	if (m_Ztracking->GetValue()) // if we're doing active, automatic tracking
	{
		float delta = focusMeasure - m_lastFocus;

		double z_percision = 0.0001;

		//if (abs(delta) 
		if (delta > 0)
		{
			// we seem to have gone in the right direction!
			if (m_zVel > 0)
				m_zVel += z_percision;
			else
				m_zVel -= z_percision;

//			printf("right dir...\n");
		}
		else if (delta < 0)
		{
			// we got worse!
			if (m_zVel > 0)
				m_zVel -= z_percision*2;
			else
				m_zVel += z_percision*2;

//			printf("wrong dir...\n");
		}
		else
		{
			m_zVel += 0.1 * z_percision * ((rand()/(float)RAND_MAX) - 0.5); // wiggle a bit just in case :)
//			printf("random...\n");
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

//		printf("moving stage to: (%f, %f, %f)\n", stageX, stageY, stageZ + m_zVel);

		m_stage->MoveTo(stageX, stageY, stageZ + m_zVel);

	}


	m_lastFocus = focusMeasure;
}

void zTracker::OnDo(wxCommandEvent& event)
{
	/*
	if (m_zGuess == NULL)
	{
	if (!m_plotWindows.empty())
	{
		m_zGuess = new ZGuesser(m_plotWindows[0]->vals, 10);
	}
	}
	else
	{
		printf("m_zGuess->guessOffset(1.1, true) = %f\n", m_zGuess->guessOffset(1.1, true));
	}
	*/

	/*
	fftw_complex *in, *out;
	fftw_plan p;

	int N = 11;

	in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N);
	out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N);
	p = fftw_plan_dft_1d(N, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

	float input[] = {0, 0, 0.9, 0.95, 1, 1, 1, 0.95, 0.9, 0, 0};

	// assign values to 'in'
	for (int i=0; i < N; ++i)
	{
		//(in[i])[0] = 0.1f;
		//(in[i])[0] = rand() % 256;
		//(in[i])[0] = rand() / (float)RAND_MAX;
		(in[i])[0] = input[i];
	}

	wxString dbgop;

	fftw_execute(p); // repeat as needed 

	dbgop.Printf("in: [%f, %f, %f, %f, %f, %f]\nout: [%f, %f, %f, %f, %f, %f]\n",
		(in[0])[0], (in[1])[0], (in[2])[0], (in[3])[0], (in[4])[0], (in[5])[0], 
		(out[0])[0], (out[1])[0], (out[2])[0], (out[3])[0], (out[4])[0], (out[5])[0]);

	wxMessageBox(dbgop);

	fftw_destroy_plan(p);
	fftw_free(in); fftw_free(out);
	*/

	double x, y, z;
	m_stage->GetPosition(x, y, z);
	m_stage->CalculateOffset(z * METERS_PER_MICRON);
}

void zTracker::Idle(wxIdleEvent& WXUNUSED(event))
{
	// optimize our spot tracker!
	if (m_XYtracking->GetValue())
	{
//		printf("loc = (%f, %f)\n", m_canvas->GetSelectX(), m_canvas->GetSelectYflip());
		m_spotTracker->xytracker()->set_location(m_canvas->GetSelectX(), m_canvas->GetSelectYflip());
		if (g_image)
			optimize_tracker(m_spotTracker);
		m_canvas->SetSelectYflip(m_spotTracker->xytracker()->get_x(), m_spotTracker->xytracker()->get_y());
//		printf("new loc = (%f, %f)\n", m_spotTracker->xytracker()->get_x(), m_spotTracker->xytracker()->get_y());
	}


	if (m_logging)
	{
		SetStatusText("Logging...", 1);
	}
	else
	{
		SetStatusText("", 1);
	}

	if (m_Ztracking->GetValue())
	{
		m_videoMode = PAUSED; // stop 'playing' or 'single stepping' if we're auto-Z-tracking
	}


	double x, y, z;
	m_stage->GetPosition(x, y, z);

	if (m_logging)
	{
		//printf("Moving from:\nz = %f to\nz = %f\n", z, z+0.1);
		m_stage->MoveTo(x, y, z+0.1);
	}

#ifdef FAKE_STAGE
	// update "image" (i.e. frame_number) based on z position of stage! (unless we're playing/stepping)
	if (m_videoMode == PAUSED)
	{
		m_frame_number = z * 10;
//		printf("simulated frame num!\n");
	}

	if (g_video != NULL && m_videoMode == PAUSED)
		g_video->jump_to_frame(m_frame_number - 1);

#endif

	char buffer[20] = "";
	sprintf(buffer, "%.3f", z);	
	m_zText->SetValue(buffer);
	sprintf(buffer, "%.3f", m_zVel);	
	m_zVelText->SetValue(buffer);

	if (g_camera != NULL)
	{
		g_image = g_camera;

		if (g_camera->read_image_to_memory()) 
		{
			if (m_videoMode == SINGLE_STEPPING || m_videoMode == PLAYING)
			{
				++m_frame_number;
				m_stage->SetPosition(x, y, m_frame_number / 10.0f);
			}
			for (int i = 0; i < m_plotWindows.size(); ++i)
			{
				m_plotWindows[i]->SetIndicator(m_frame_number);
			}

//			printf("frame: %i\n", m_frame_number);

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


	m_stage->Update(); // update our virtual stage
}


// only called once at end of constructor
void zTracker::set_layout()
{
	SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_3DLIGHT));

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

	m_trackingSizer->Add(m_Ztracking, 0, wxALL, 3);
	m_trackingSizer->Add(m_XYtracking, 0, wxALL, 3);

	m_focusMethodSizer->Add(m_focusMethodRadio, 0, wxALL, 3);
	m_focusMethodSizer->Add(m_focusWeightRadio, 0, wxALL, 3);
	
	m_newPlotSizer->Add(m_newPlotButton, 0, wxALL, 3);
	m_newPlotSizer->Add(m_newPlotArrayButton, 0, wxALL, 3);

	m_advancedSizer->Add(m_newPlotSizer, 0, 0, 0);
	m_advancedSizer->Add(m_focusMethodSizer, 0, 0, 0);
	m_advancedSizer->Add(m_Do, 0, wxALL, 3);

	m_manualFocusSizer->Add(m_manualFocusSlider, 0, wxALL, 3);

	m_assortedSizer->Add(m_showCrossCheck, 0, wxALL, 3);
	m_assortedSizer->Add(m_8Bits, 0, wxALL, 3);
	m_assortedSizer->Add(m_trackingSizer, 0, 0, 0);
	m_assortedSizer->Add(m_manualFocusSizer, 0, 0, 0);

	m_sizer->Add(m_canvasSizer, 0, 0, 0);
	m_sizer->Add(m_horizSizer, 0, wxLEFT, 3);
	m_sizer->Add(m_frameSizer, 0, wxEXPAND, 0);
	m_sizer->Add(m_videoControlSizer, 0, wxEXPAND | wxLEFT | wxRIGHT, 3);
	m_sizer->Add(m_assortedSizer, 0, wxEXPAND, 0);
	m_sizer->Add(m_advancedSizer, 0, wxEXPAND, 0);
	m_sizer->Show(m_advancedSizer, false, true); // hide the advanced controls by default

	m_rootSizer->Add(m_sizer, 0, 0, 0);
}

// updates the layout whenever things change
void zTracker::do_layout()
{

	m_sizer->SetSizeHints(m_panel);

	m_rootSizer->SetSizeHints(this);
	this->SetSizer(m_rootSizer);

	m_rootSizer->Layout();

	this->SetClientSize(m_panel->GetSize());

	this->Refresh();
}

/// Create a pointer to a new tracker of the appropriate type,
// given the global settings for interpolation and inversion.
// Return NULL on failure.

spot_tracker_XY* zTracker::create_appropriate_xytracker(double x, double y, double r)
{
	spot_tracker_XY *tracker = NULL;

	int interpolate = 1; // this isn't really used at the moment

	// FIONA KERNEL IS NOT SUPPORTED YET! ***
	if (g_kernel_type == KERNEL_FIONA) {
/*
		// Estimate the background value as the minimum pixel value within a square that is 2*diameter (4*radius)
		// on a side.  Estimate the total volume as the sum over this same region with the background subtracted
		// from it.
		double background = 1e50;
		double volume = 0.0;
		unsigned count = 0;
		int i,j;
		for (i = x - 2*r; i <= x + 2*r; i++) {
			for (j = y - 2*r; j <= y + 2*r; j++) {
				double value;
				if (g_image->read_pixel(i, j, value, g_colorIndex)) {
					volume += value;
					if (value < background) { background = value; }
					count++;
				}
			}
		}
		if (count == 0) {
			background = 0.0;
		} else {
			volume -= count*background;
		}
		printf("XXX FIONA kernel: background = %lg, volume = %lg\n", background, volume);
		tracker = new FIONA_spot_tracker(r, (g_invert != 0), g_precision, 0.1, g_sampleSpacing, background, volume);
*/
	} else if (g_kernel_type == KERNEL_SYMMETRIC) {
		tracker = new symmetric_spot_tracker_interp(r,(m_invert != 0), m_precision, 0.1, m_sampleSpacing);
	} else if (g_kernel_type == KERNEL_CONE) {
		tracker = new cone_spot_tracker_interp(r,(m_invert != 0), m_precision, 0.1, m_sampleSpacing);
	} else if (interpolate) {
		tracker = new disk_spot_tracker_interp(r,(m_invert != 0), m_precision, 0.1, m_sampleSpacing);
	} else {
		tracker = new disk_spot_tracker(r,(m_invert != 0), m_precision, 0.1, m_sampleSpacing);
	}


	tracker->set_location(x,y);
	tracker->set_radius(r);
	return tracker;
}

spot_tracker_Z* zTracker::create_appropriate_ztracker(void)
{
	double g_precision = 0.25; // ***
	if (true) { // ***
		return NULL;
	} else {
		return new radial_average_tracker_Z("g_psf_filename", g_precision);
	}
}


// Optimize to find the best fit starting from last position for a tracker.
// Invert the Y values on the way in and out.
// Don't let it adjust the radius here (otherwise it gets too jumpy).
void zTracker::optimize_tracker(Spot_Information *tracker)
{

	double  x, y;
	double last_pos[2];
	last_pos[0] = tracker->xytracker()->get_x();
	last_pos[1] = tracker->xytracker()->get_y();
	double last_vel[2];
	tracker->get_velocity(last_vel);



	// If we are doing prediction, apply the estimated last velocity to
	// move the estimated position to a new location
	if ( m_predict && m_newTrackingData) {
		double new_pos[2];
		new_pos[0] = last_pos[0] + last_vel[0];
		new_pos[1] = last_pos[1] + last_vel[1];
		tracker->xytracker()->set_location(new_pos[0], new_pos[1]);
	}


	// If the frame number has changed, and we are doing global searches
	// within a radius, then create an image-based tracker at the last
	// location for the current tracker on the last frame; scan all locations
	// within radius and find the maximum fit on this frame; move the tracker
	// location to that maximum before doing the optimization.  We use the
	// image-based tracker for this because other trackers may have maximum
	// fits outside the region where the bead is -- the symmetric tracker often
	// has a local maximum at best fit and global maxima elsewhere.
	if ( (m_searchRadius > 0) && m_newTrackingData) {

		double x_base = tracker->xytracker()->get_x();
		double y_base = tracker->xytracker()->get_y();

		// If we are doing prediction, reduce the search radius to 3 because we rely
		// on the prediction to get us close to the global maximum.  If we make it 2.5,
		// it starts to lose track with an accuracy of 1 pixel for a bead on cilia in
		// pulnix video (acquired at 120 frames/second).
		double used_search_radius = m_searchRadius;
		if ( m_predict && (m_searchRadius > 3) ) {
			used_search_radius = 3;
		}

		// Create an image spot tracker and initize it at the location where the current
		// tracker started this frame (before prediction), but in the last image.  Grab enough
		// of the image that we will be able to check over the used_search_radius for a match.
		// Use the faster twolines version of the image-based tracker.
		twolines_image_spot_tracker_interp max_find(tracker->xytracker()->get_radius(), (m_invert != 0), m_precision,
			0.1, m_sampleSpacing);
		max_find.set_location(last_pos[0], last_pos[1]);
		max_find.set_image(*m_last_image, m_channel, last_pos[0], last_pos[1], tracker->xytracker()->get_radius() + used_search_radius);

		// Loop over the pixels within used_search_radius of the initial location and find the
		// location with the best match over all of these points.  Do this in the current image,
		// at the (possibly-predicted) starting location and find the offset from the (possibly
		// predicted) current location to get to the right place.
		double radsq = used_search_radius * used_search_radius;
		int x_offset, y_offset;
		int best_x_offset = 0;
		int best_y_offset = 0;
		double best_value = max_find.check_fitness(*g_image, m_channel);
		for (x_offset = -floor(used_search_radius); x_offset <= floor(used_search_radius); x_offset++) {
			for (y_offset = -floor(used_search_radius); y_offset <= floor(used_search_radius); y_offset++) {
				if ( (x_offset * x_offset) + (y_offset * y_offset) <= radsq) {
					max_find.set_location(x_base + x_offset, y_base + y_offset);
					double val = max_find.check_fitness(*g_image, m_channel);
					if (val > best_value) {
						best_x_offset = x_offset;
						best_y_offset = y_offset;
						best_value = val;
					}
				}
			}
		}

		// Put the tracker at the location of the maximum, so that it will find the
		// total maximum when it finds the local maximum.
		tracker->xytracker()->set_location(x_base + best_x_offset, y_base + best_y_offset);
	}


	// Here's where the tracker is optimized to its new location.
	// FIONA trackers always try to optimize radius along with XY.
	if (m_parabolafit) 
	{
		tracker->xytracker()->optimize_xy_parabolafit(*g_image, m_channel, x, y, tracker->xytracker()->get_x(), tracker->xytracker()->get_y() );
	} 
	else 
	{
		if (g_kernel_type == KERNEL_FIONA) {
			tracker->xytracker()->optimize(*g_image, m_channel, x, y, tracker->xytracker()->get_x(), tracker->xytracker()->get_y() );
		} 
		else
		{
			tracker->xytracker()->optimize_xy(*g_image, m_channel, x, y, tracker->xytracker()->get_x(), tracker->xytracker()->get_y() );
		}
	}

	printf("(x, y) = (%f, %f)\n", x, y);


	// If we are doing prediction, update the estimated velocity based on the
	// step taken.
	if ( m_predict && m_newTrackingData ) {
		const double vel_frac_to_use = 0.9; //< 0.85-0.95 seems optimal for cilia in pulnix; 1 is too much, 0.83 is too little
		const double acc_frac_to_use = 0.0; //< Due to the noise, acceleration estimation makes things worse

		// Compute the new velocity estimate as the subtraction of the
		// last position from the current position.
		double new_vel[2];
		new_vel[0] = (tracker->xytracker()->get_x() - last_pos[0]) * vel_frac_to_use;
		new_vel[1] = (tracker->xytracker()->get_y() - last_pos[1]) * vel_frac_to_use;

		// Compute the new acceleration estimate as the subtraction of the new
		// estimate from the old.
		double new_acc[2];
		new_acc[0] = (new_vel[0] - last_vel[0]) * acc_frac_to_use;
		new_acc[1] = (new_vel[1] - last_vel[1]) * acc_frac_to_use;

		// Re-estimate the new velocity by taking into account the acceleration.
		new_vel[0] += new_acc[0];
		new_vel[1] += new_acc[1];

		// Store the quantities for use next time around.
		tracker->set_velocity(new_vel);
		tracker->set_acceleration(new_acc);
	}

	// If we have a non-zero threshold for determining if we are lost,
	// check and see if we are.  This is done by finding the value of
	// the fitness function at the actual tracker location and comparing
	// it to the maximum values located on concentric rings around the
	// tracker.  We look for the minimum of these maximum values (the
	// deepest moat around the peak in the center) and determine if we're
	// lost based on the type of kernel we are using.
	// Some ideas for determining goodness of tracking for a bead...
	//   (It looks like different metrics are needed for symmetric and cone and disk.)
	// For symmetric:
	//    Value compared to initial value when tracking that bead: When in a flat area, it can be better.
	//    Minimum over area vs. center value: get low-valued lobes in certain directions, but other directions bad.
	//    Maximum of all minima as you travel in different directions from center: dunno.
	//    Maximum at radius from center: How do you know what radius to select?
	//      Max at radius of the minimum value from center?
	//      Minimum of the maxima over a number of radii? -- Yep, we're trying this one.
	if (m_lossSensitivity > 0.0) {
		double min_val = 1e100; //< Min over all circles
		double start_x = tracker->xytracker()->get_x();
		double start_y = tracker->xytracker()->get_y();
		double this_val;
		double r, theta;
		double x, y;
		// Only look every two-pixel radius from three out to the radius of the tracker.
		for (r = 3; r <= tracker->xytracker()->get_radius(); r += 2) {
			double max_val = -1e100;  //< Max over this particular circle
			// Look at every pixel around the circle.
			for (theta = 0; theta < 2*M_PI; theta += r / (2 * M_PI) ) {
				x = r * cos(theta);
				y = r * sin(theta);
				tracker->xytracker()->set_location(x + start_x, y + start_y);
				this_val = tracker->xytracker()->check_fitness(*g_image, m_channel);
				if (this_val > max_val) { max_val = this_val; }
			}
			if (max_val < min_val) { min_val = max_val; }
		}
		//Put the tracker back where it started.
		tracker->xytracker()->set_location(start_x, start_y);

		// See if we are lost.  The way we tell if we are lost or not depends
		// on the type of kernel we are using.  It also depends on the setting of
		// the "lost sensitivity" parameter, which varies from 0 (not sensitive at
		// all) to 1 (most sensitive).
		double starting_value = tracker->xytracker()->check_fitness(*g_image, m_channel);
		//printf("XXX Center = %lg, min of maxes = %lg, scale = %lg\n", starting_value, min_val, min_val/starting_value);
		if (g_kernel_type == KERNEL_SYMMETRIC) {
			// The symmetric kernel reports values that are strictly non-positive for
			// its fitness function.  Some observation of values reveals that the key factor
			// seems to be how many more times negative the "moat" is than the central peak.
			// Based on a few observations of different bead tracks, it looks like a factor
			// of 2 is almost always too small, and a factor of 4-10 is almost always fine.
			// So, we'll set the "scale factor" to be between 1 (when sensitivity is just
			// above zero) and 10 (when the scale factor is just at one).  If a tracker gets
			// lost, set it to be the active tracker so the user can tell which one is
			// causing trouble.
			double scale_factor = 1 + 9*m_lossSensitivity;
			if (starting_value * scale_factor < min_val) {
				m_tracker_is_lost = true;
				m_spotTracker = tracker; // ***
			}
		} else if (g_kernel_type == KERNEL_CONE) {
			// Differences (not factors) of 0-5 seem more appropriate for a quick test of the cone kernel.
			double difference = 5*m_lossSensitivity;
			if (starting_value - min_val < difference) {
				m_tracker_is_lost = true;
				m_spotTracker = tracker; // ***
			}
		} else {  // Must be a disk kernel
			// Differences (not factors) of 0-5 seem more appropriate for a quick test of the disk kernel.
			double difference = 5*m_lossSensitivity;
			if (starting_value - min_val < difference) {
				m_tracker_is_lost = true;
				m_spotTracker = tracker; // ***
			}
		}
	}

	// If we are optimizing in Z, then do it here.
	if (m_optZ) {
		double  z = 0;
		tracker->ztracker()->locate_close_fit_in_depth(*g_image, m_channel, tracker->xytracker()->get_x(), tracker->xytracker()->get_y(), z);
		tracker->ztracker()->optimize(*g_image, m_channel, tracker->xytracker()->get_x(), tracker->xytracker()->get_y(), z);
	}
}


zTracker::~zTracker()
{
	//
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
//    g_camera_bit_depth = 12;
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
    *camera = new edt_server(true, 4);
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
//      g_camera_bit_depth = 12;

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
