/////////////////////////////////////////////////////////////////////////////
// Name:        ztracker.cpp
// Purpose:     realtime video bead tracking main form
// Author:      Ryan Schubert
// Created:     2/24/07
// Modified by:
/////////////////////////////////////////////////////////////////////////////


#include "ztracker.h"



BEGIN_EVENT_TABLE(zTracker, wxFrame)
    EVT_MENU(wxID_OPEN, zTracker::OnMenuFileOpen)
    EVT_MENU(wxID_EXIT, zTracker::OnMenuFileExit)
    EVT_MENU(wxID_HELP, zTracker::OnMenuHelpAbout)
END_EVENT_TABLE()


zTracker::zTracker(wxWindow* parent, int id, const wxString& title, const wxPoint& pos, const wxSize& size, long style):
    wxFrame(parent, id, title, pos, size, wxDEFAULT_FRAME_STYLE)
{
	SetIcon(wxIcon(sample_xpm));

	// Make the "File" menu
    wxMenu *fileMenu = new wxMenu;
    fileMenu->Append(wxID_OPEN, wxT("&Open..."));
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_EXIT, wxT("E&xit\tALT-X"));
    // Make the "Help" menu
    wxMenu *helpMenu = new wxMenu;
    helpMenu->Append(wxID_HELP, wxT("&About..."));

    wxMenuBar *menuBar = new wxMenuBar;
    menuBar->Append(fileMenu, wxT("&File"));
    menuBar->Append(helpMenu, wxT("&Help"));
    SetMenuBar(menuBar);


//    panel_1 = new wxPanel(this, wxID_ANY);
//    label_1 = new wxStaticText(panel_1, wxID_ANY, wxT("1111111111111111111111111111"));
//    text_ctrl_1 = new wxTextCtrl(panel_1, wxID_ANY, wxT("2222222222222222222222222222222222222222222"), wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE|wxTE_RICH2|wxTE_AUTO_URL);

	// to avoid assert failure from GLCanvas
	this->Show();

	m_canvas = new TestGLCanvas(this, wxID_ANY, wxDefaultPosition,
        wxSize(348, 260), wxSUNKEN_BORDER);
/*
	m_canvasZoomed = new TestGLCanvas(this, wxID_ANY, wxPoint(348,0),
        wxSize(174, 130), wxSUNKEN_BORDER);
*/
	m_horizLabel = new wxStaticText(this, wxID_ANY, wxT("Horizontal"), wxPoint(10,265));
	m_horizPixels = new PixelLine(this, wxID_ANY, wxPoint(5, 280), wxSize(160, 10), 0, "Horiz", true);

	m_canvas->SetHPixRef(m_horizPixels);


	m_vertLabel = new wxStaticText(this, wxID_ANY, wxT("Vertical"), wxPoint(300,5));
	m_vertPixels = new PixelLine(this, wxID_ANY, wxPoint(315, 20), wxSize(10, 160), 0, "Vert", false);

	m_canvas->SetVPixRef(m_vertPixels);

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

		// let the canvas handle this memory management!
		FileToTexture* temp = new FileToTexture(filename);

		//	temp->write_to_tiff_file("test.tiff", 1, 0, true);

		int width = temp->get_num_columns();
		int height = temp->get_num_rows();

		delete m_canvas;
		m_canvas = new TestGLCanvas(this, wxID_ANY, wxDefaultPosition,
			wxSize(width, height), wxNO_BORDER);
		m_canvas->SetInput(temp);
		m_canvas->SetHPixRef(m_horizPixels);
		m_canvas->SetVPixRef(m_vertPixels);

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

	m_horizLabel->SetPosition(wxPoint(10, y + h + 5));
	m_horizPixels->SetPosition(wxPoint(5, y + h + 20));

	m_vertLabel->SetPosition(wxPoint(x + w + 5, 5));
	m_vertPixels->SetPosition(wxPoint(x + w + 20, 20)); 
}

