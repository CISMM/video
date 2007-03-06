/////////////////////////////////////////////////////////////////////////////
// Name:        PlotWindow.cpp
// Purpose:     plot window for focus measure over time
// Author:      Ryan Schubert
// Created:     3/5/07
// Modified by:
/////////////////////////////////////////////////////////////////////////////


#include "PlotWindow.h"


BEGIN_EVENT_TABLE(PlotWindow, wxFrame)
   
END_EVENT_TABLE()


PlotWindow::PlotWindow(wxWindow* parent, int id, const wxString& title, const wxPoint& pos, const wxSize& size, long style):
    wxFrame(parent, id, title, pos, size, wxDEFAULT_FRAME_STYLE)
{	
	SetIcon(wxIcon(plot_xpm));

	CreateStatusBar(2);
	SetStatusText(wxT("Focus curve over time"));

	// to avoid assert failure from GLCanvas
	this->Show();

	m_plot = new PlotGLCanvas(this, wxID_ANY, wxDefaultPosition, wxSize(348, 260), wxNO_BORDER);

    do_layout();
}

void PlotWindow::Refresh()
{
	m_plot->Refresh();
}

void PlotWindow::setVals(std::vector<float> vals)
{
	m_num_vals = vals.size();

	// we're gonna let PlotGLCanvas handle this memory cleanup!!
	m_vals = new float[vals.size()];

	float min = vals[0];
	float max = vals[0];

	for (int i = 0; i < vals.size(); ++i)
	{
		m_vals[i] = vals[i];

		if (vals[i] > max)
			max = vals[i];

		if (vals[i] < min)
			min = vals[i];
	}

	m_plot->setVals(m_vals, m_num_vals, min, max);

	m_plot->Update();
}

void PlotWindow::plot()
{

}


void PlotWindow::set_properties()
{
    //SetTitle(wxT("Hello World of Wx"));
//	label_1->SetMinSize(wxSize(250, 25));
}


void PlotWindow::do_layout()
{
	wxSize sz(GetClientSize());
	m_plot->SetSize(sz);
}