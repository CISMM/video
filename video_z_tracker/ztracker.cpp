/////////////////////////////////////////////////////////////////////////////
// Name:        ztracker.cpp
// Purpose:     realtime video bead tracking main form
// Author:      Ryan Schubert
// Created:     2/24/07
// Modified by:
/////////////////////////////////////////////////////////////////////////////


#include "ztracker.h"

// forward function declarations
bool  get_camera(const char *type, base_camera_server **camera, Controllable_Video **video);


// Application IDs used
const static int MENU_PLAY = 100;
const static int MENU_PAUSE = 101;
const static int MENU_SINGLE = 102;
const static int MENU_REWIND = 103;

const static int MENU_START_LOG = 104;
const static int MENU_STOP_LOG = 105;



BEGIN_EVENT_TABLE(zTracker, wxFrame)
    EVT_MENU(wxID_OPEN, zTracker::OnMenuFileOpen)
    EVT_MENU(wxID_EXIT, zTracker::OnMenuFileExit)
    EVT_MENU(wxID_HELP, zTracker::OnMenuHelpAbout)
	EVT_MENU(MENU_PLAY, zTracker::OnMenuVideoPlay)
	EVT_MENU(MENU_PAUSE, zTracker::OnMenuVideoPause)
	EVT_MENU(MENU_SINGLE, zTracker::OnMenuVideoSingle)
	EVT_MENU(MENU_REWIND, zTracker::OnMenuVideoRewind)
	
	EVT_MENU(MENU_START_LOG, zTracker::OnMenuFocusStart)
	EVT_MENU(MENU_STOP_LOG, zTracker::OnMenuFocusStop)

	EVT_IDLE(zTracker::Idle)
END_EVENT_TABLE()


zTracker::zTracker(wxWindow* parent, int id, const wxString& title, const wxPoint& pos, const wxSize& size, long style):
    wxFrame(parent, id, title, pos, size, wxDEFAULT_FRAME_STYLE)
{

	g_video = NULL;
	g_camera = NULL;
	g_image = NULL;

	m_frame_number = -1;

	m_channel = 0; // R

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

	// Make the "Video" menu
	wxMenu *videoMenu = new wxMenu;
	videoMenu->Append(MENU_PLAY, wxT("&Play"));
	videoMenu->Append(MENU_PAUSE, wxT("P&ause"));
	videoMenu->Append(MENU_SINGLE, wxT("&Single step"));
	videoMenu->Append(MENU_REWIND, wxT("&Rewind"));

	// Make the "Focus" menu
	wxMenu *focusMenu = new wxMenu;
	focusMenu->Append(MENU_START_LOG, wxT("&Start Logging"));
	focusMenu->Append(MENU_STOP_LOG, wxT("S&top Logging"));


	// Make it happen!
    wxMenuBar *menuBar = new wxMenuBar;
    menuBar->Append(fileMenu, wxT("&File"));
	menuBar->Append(videoMenu, wxT("&Video"));
	menuBar->Append(focusMenu, wxT("F&ocus"));
    menuBar->Append(helpMenu, wxT("&Help"));
    SetMenuBar(menuBar);

	CreateStatusBar(2);
	SetStatusText(wxT("Z tracker operational"));


//    panel_1 = new wxPanel(this, wxID_ANY);
//    label_1 = new wxStaticText(panel_1, wxID_ANY, wxT("1111111111111111111111111111"));
//    text_ctrl_1 = new wxTextCtrl(panel_1, wxID_ANY, wxT("2222222222222222222222222222222222222222222"), wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE|wxTE_RICH2|wxTE_AUTO_URL);

	// to avoid assert failure from GLCanvas
	this->Show();

	m_canvas = new TestGLCanvas(this, wxID_ANY, wxDefaultPosition,
        wxSize(348, 260), wxNO_BORDER);
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
		
	m_plotWindow = new PlotWindow(this, wxID_ANY, wxT("Focus measure curve"));

	
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
			exit(-1);
		}
		else
		{
			m_frame_number = -1;
		}

		// let the canvas handle this memory management!
//		FileToTexture* temp = new FileToTexture(filename);

		//	temp->write_to_tiff_file("test.tiff", 1, 0, true);

		int width = g_camera->get_num_columns();
		int height = g_camera->get_num_rows();

//		delete m_canvas;
//		m_canvas = new TestGLCanvas(this, wxID_ANY, wxDefaultPosition,
//			wxSize(width, height), wxNO_BORDER);
		m_canvas->SetInput(g_camera);
//		m_canvas->SetHPixRef(m_horizPixels);
//		m_canvas->SetVPixRef(m_vertPixels);
		m_canvas->SetSize(width, height);

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


void zTracker::OnMenuVideoPlay( wxCommandEvent& WXUNUSED(event) )
{
	if (g_video != NULL)
		g_video->play();
}

void zTracker::OnMenuVideoPause( wxCommandEvent& WXUNUSED(event) )
{
	if (g_video != NULL)
		g_video->pause();
}

void zTracker::OnMenuVideoSingle( wxCommandEvent& WXUNUSED(event) )
{
	if (g_video != NULL)
		g_video->single_step();
}

void zTracker::OnMenuVideoRewind( wxCommandEvent& WXUNUSED(event) )
{
	if (g_video != NULL)
		g_video->rewind();
	m_frame_number = -1;

	m_focus.clear();

	m_logging = false;
}

void zTracker::OnMenuFocusStart( wxCommandEvent& WXUNUSED(event) )
{
	if (!m_logging)
	{
		m_focus.clear();
	}
	m_logging = true;
	printf("Logging...\n");
}

void zTracker::OnMenuFocusStop( wxCommandEvent& WXUNUSED(event) )
{
	printf("Stopped logging...\n");

	m_focus.clear();

	m_logging = false;
}

void zTracker::Idle(wxIdleEvent& WXUNUSED(event))
{
	if (g_camera != NULL)
	{
		if (g_camera->read_image_to_memory()) 
		{
			++m_frame_number;
			printf("frame: %i\n", m_frame_number);


			if (m_logging)
			{
				float smd1 = m_horizPixels->calcSMD(m_channel);
				float smd2 = m_vertPixels->calcSMD(m_channel);
				m_focus.push_back(smd1 + smd2);
				m_plotWindow->setVals(m_focus);
			}
		}
		else
		{
			//fprintf(stderr, "Something incredibly wrong happened in zTracker::Idle...\t\n");
		}
	}

	if (m_canvas != NULL)
	{
		m_canvas->Refresh();
	}

	if (m_plotWindow != NULL)
	{
		m_plotWindow->Refresh();
	}
}



void zTracker::set_properties()
{
    //SetTitle(wxT("Hello World of Wx"));
//	label_1->SetMinSize(wxSize(250, 25));
}


void zTracker::do_layout()
{
	int x, y, w, h;
	m_canvas->GetPosition(&x, &y);
	m_canvas->GetClientSize(&w, &h);

	m_horizLabel->SetPosition(wxPoint(x + 10, y + h + 5));
	m_horizPixels->SetPosition(wxPoint(x + 5, y + h + 20));

	m_vertLabel->SetPosition(wxPoint(x + w + 5, y + 5));
	m_vertPixels->SetPosition(wxPoint(x + w + 20, y + 20)); 

	SetSize(w + 60, h + 150);

	m_plotWindow->SetPosition(this->GetPosition() + wxPoint(this->GetSize().x, 0));
}




/// Open the wrapped camera we want to use depending on the name of the
//  camera we're trying to open.
bool  get_camera(const char *type, base_camera_server **camera, Controllable_Video **video)
{
#ifdef VST_USE_ROPER
  if (!strcmp(type, "roper")) {
    // XXX Starts with binning of 2 to get the image size down so that
    // it fits on the screen.
    roper_server *r = new roper_server(2);
    *camera = r;
    g_camera_bit_depth = 12;
  } else
#endif  
#ifdef VST_USE_COOKE
  if (!strcmp(type, "cooke")) {
    // XXX Starts with binning of 2 to get the image size down so that
    // it fits on the screen.
    cooke_server *r = new cooke_server(2);
    *camera = r;
    g_camera_bit_depth = 16;
  } else
#endif  
#ifdef	VST_USE_DIAGINC
  if (!strcmp(type, "diaginc")) {
    // XXX Starts with binning of 2 to get the image size down so that
    // it fits on the screen.
    diaginc_server *r = new diaginc_server(2);
    *camera = r;
    g_exposure = 80;	// Seems to be the minimum exposure for the one we have
    g_camera_bit_depth = 12;
  } else
#endif  
#ifdef	VST_USE_EDT
  if (!strcmp(type, "edt")) {
    edt_server *r = new edt_server();
    *camera = r;
  } else
#endif  
#ifdef	VST_USE_DIRECTX
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
#ifdef	VST_USE_SEM
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
#ifdef	VST_USE_ROPER
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
#ifdef	VST_USE_SEM
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
#ifdef	VST_USE_DIRECTX
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