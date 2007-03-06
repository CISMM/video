#include "PlotGLCanvas.h"



// ---------------------------------------------------------------------------
// PlotGLCanvas
// ---------------------------------------------------------------------------

BEGIN_EVENT_TABLE(PlotGLCanvas, wxGLCanvas)
    EVT_SIZE(PlotGLCanvas::OnSize)
    EVT_PAINT(PlotGLCanvas::OnPaint)
    EVT_ERASE_BACKGROUND(PlotGLCanvas::OnEraseBackground)
    EVT_MOUSE_EVENTS(PlotGLCanvas::OnMouse)
END_EVENT_TABLE()

PlotGLCanvas::PlotGLCanvas(wxWindow *parent, wxWindowID id,
    const wxPoint& pos, const wxSize& size, long style, const wxString& name)
    : wxGLCanvas(parent, id, pos, size, style|wxFULL_REPAINT_ON_RESIZE, name)
{

	m_min = 0;
	m_max = 1;


	InitGL();
	ResetProjectionMode();
}

PlotGLCanvas::~PlotGLCanvas()
{
	if (m_vals != NULL)
		delete m_vals;
}

void PlotGLCanvas::setVals(float* v, int n, float min, float max)
{
	if (m_vals != NULL)
		delete m_vals;

	m_vals = v;
	m_num = n;

	m_min = min;
	m_max = max;
}


void PlotGLCanvas::OnPaint( wxPaintEvent& WXUNUSED(event) )
{
    // must always be here
    wxPaintDC dc(this);

#ifndef __WXMOTIF__
    if (!GetContext()) return;
#endif

    SetCurrent();

    // Clear
	glClearColor( 0, 0, 0, 1.0f );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	if (m_num < 2)
	{
		; // we dont have enough values to plot even one line!
	}
	else
	{
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();	

		gluOrtho2D(-0.1, 1.1, -0.1, 1.1);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();


		float dx = 1.0f / (float)m_num;
		float scale = 1.0f / m_max;

		glBegin(GL_LINES);

		glColor3f(1, 1, 1);

		for (int i = 0; i < m_num - 1; ++i)
		{
			glVertex3f(i * dx, m_vals[i] * scale, 0);
			glVertex3f((i + 1) * dx, m_vals[i + 1] * scale, 0);
		}

		glEnd();
	}

    // Flush
    glFlush();

    // Swap
    SwapBuffers();
}

void PlotGLCanvas::OnSize(wxSizeEvent& event)
{
    // this is also necessary to update the context on some platforms
    wxGLCanvas::OnSize(event);
    // Reset the OpenGL view aspect
    ResetProjectionMode();
}

void PlotGLCanvas::OnEraseBackground(wxEraseEvent& WXUNUSED(event))
{
    // Do nothing, to avoid flashing on MSW
}


void PlotGLCanvas::OnMouse(wxMouseEvent& event)
{
	wxSize sz(GetClientSize());

    if (event.Dragging())
    {
        /* orientation has changed, redraw mesh */
        //Refresh(false);
    }

	if (event.LeftIsDown())
	{
		
	}

	if (event.RightDown())
	{
		// nothing yet
	}

	if (event.RightIsDown())
	{
		
	}
}


void PlotGLCanvas::InitGL()
{
    glEnable(GL_DEPTH_TEST);

    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_COLOR_MATERIAL);
}

void PlotGLCanvas::ResetProjectionMode()
{
    int w, h;
    GetClientSize(&w, &h);
#ifndef __WXMOTIF__
    if ( GetContext() )
#endif
    {
        SetCurrent();
        glViewport(0, 0, (GLint) w, (GLint) h);

/*
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(45.0f, (GLfloat)w/h, 1.0, 100.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
*/

		// set up the projection matrix 
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();	

		gluOrtho2D(-0.1, 1.1, -0.1, 1.1);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
    }
}