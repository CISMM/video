#include "TestGLCanvas.h"



// some helper functions that may be usefull

// toGLCoords : converts from int wxwindows coords in frame to corresponding OpenGL coords
void toGLCoords(float &x, float &y, wxSize sz);





// ---------------------------------------------------------------------------
// TestGLCanvas
// ---------------------------------------------------------------------------

BEGIN_EVENT_TABLE(TestGLCanvas, wxGLCanvas)
    EVT_SIZE(TestGLCanvas::OnSize)
    EVT_PAINT(TestGLCanvas::OnPaint)
    EVT_ERASE_BACKGROUND(TestGLCanvas::OnEraseBackground)
    EVT_MOUSE_EVENTS(TestGLCanvas::OnMouse)
END_EVENT_TABLE()

TestGLCanvas::TestGLCanvas(wxWindow *parent, wxWindowID id,
    const wxPoint& pos, const wxSize& size, long style, const wxString& name)
    : wxGLCanvas(parent, id, pos, size, style|wxFULL_REPAINT_ON_RESIZE, name)
{
	InitGL();
	ResetProjectionMode();
	m_image = NULL;
	m_hPixRef = NULL;
	m_vPixRef = NULL;

	m_mouseX = m_mouseY = -1000;
	m_selectX = m_selectY = -1000;
	m_radius = 0.05;
	m_pixelRadius = 10;
}

TestGLCanvas::~TestGLCanvas()
{
	if (m_image != NULL)
		delete m_image;
}


void TestGLCanvas::SetInput(base_camera_server* newInput)
{
	if (m_image != NULL)
		delete m_image;

	m_image = newInput;

	Refresh();
}


void TestGLCanvas::OnPaint( wxPaintEvent& WXUNUSED(event) )
{
    // must always be here
    wxPaintDC dc(this);

#ifndef __WXMOTIF__
    if (!GetContext()) return;
#endif

    SetCurrent();

    // Clear
	float red, green, blue;

	red = rand()/(float)RAND_MAX;
	green = rand()/(float)RAND_MAX;
	blue = rand()/(float)RAND_MAX;

    //glClearColor( red, green, blue, 1.0f );
	glClearColor( 0, 0, 0, 1.0f );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    // Transformations
    glLoadIdentity();

	DrawSelectionBox();

	// draw current image frame
	if (m_image != NULL)
	{
		// set up the projection matrix 
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();	

		gluOrtho2D(-1.0, 1.0, -1.0, 1.0);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		m_image->write_to_opengl_quad();
	}

	
    // Flush
    glFlush();

    // Swap
    SwapBuffers();

	UpdateSlices();
}



void TestGLCanvas::DrawSelectionBox()
{
	// draw selection box
	wxSize sz(GetClientSize());
	float cx = m_selectX, cy = m_selectY;
	toGLCoords(cx, cy, sz);

	float radius = m_radius;

	int pixelRadius = m_pixelRadius;

/*
	glBegin(GL_LINE_LOOP);

	glColor3f(1, 0, 0);
	glVertex2f(cx - radius, cy - radius);

	glColor3f(1, 1, 0);
	glVertex2f(cx - radius, cy + radius);

	glColor3f(0, 1, 1);
	glVertex2f(cx + radius, cy + radius);

	glColor3f(1, 0, 1);
	glVertex2f(cx + radius, cy - radius);

	glEnd();
*/

/*
	// radius box 
	gluOrtho2D(0, sz.GetX() - 2, sz.GetY() - 2, 0);
	glBegin(GL_LINE_LOOP);

	glColor3f(1, 0, 0);
	glVertex2f(m_selectX - m_pixelRadius, m_selectY - m_pixelRadius);

	glColor3f(1, 1, 0);
	glVertex2f(m_selectX - m_pixelRadius, m_selectY + m_pixelRadius);

	glColor3f(0, 1, 1);
	glVertex2f(m_selectX + m_pixelRadius, m_selectY + m_pixelRadius);

	glColor3f(1, 0, 1);
	glVertex2f(m_selectX + m_pixelRadius, m_selectY - m_pixelRadius);

	glEnd();
//*/

	// +
	gluOrtho2D(0, sz.GetX(), sz.GetY(), 0);

	glBegin(GL_LINE_LOOP);

	glColor3f(0, 0, 1);
	glVertex2f(m_selectX - 1, m_selectY - m_pixelRadius - 1);

	glColor3f(0, 0, 1);
	glVertex2f(m_selectX - 1, m_selectY + m_pixelRadius + 1);

	glColor3f(0, 0, 1);
	glVertex2f(m_selectX + 1, m_selectY + m_pixelRadius + 1);

	glColor3f(0, 0, 1);
	glVertex2f(m_selectX + 1, m_selectY - m_pixelRadius - 1);

	glEnd();


	glBegin(GL_LINE_LOOP);

	glColor3f(1, 0, 0);
	glVertex2f(m_selectX - m_pixelRadius - 1, m_selectY - 1);

	glColor3f(1, 0, 0);
	glVertex2f(m_selectX - m_pixelRadius - 1, m_selectY + 1);

	glColor3f(1, 0, 0);
	glVertex2f(m_selectX + m_pixelRadius + 1, m_selectY + 1);

	glColor3f(1, 0, 0);
	glVertex2f(m_selectX + m_pixelRadius + 1, m_selectY - 1);

	glEnd();



}

void TestGLCanvas::OnSize(wxSizeEvent& event)
{
    // this is also necessary to update the context on some platforms
    wxGLCanvas::OnSize(event);
    // Reset the OpenGL view aspect
    ResetProjectionMode();
}

void TestGLCanvas::OnEraseBackground(wxEraseEvent& WXUNUSED(event))
{
    // Do nothing, to avoid flashing on MSW
}


void TestGLCanvas::OnMouse(wxMouseEvent& event)
{
	// update mouse position for use otherwhere
	m_mouseX = event.GetX();
	m_mouseY = event.GetY();
//	printf("mouse (x, y) = (%i, %i)\n", m_mouseX, m_mouseY);

	wxSize sz(GetClientSize());

    if (event.Dragging())
    {

        /* orientation has changed, redraw mesh */
        //Refresh(false);
    }

	if (event.LeftIsDown())
	{
		m_selectX = m_mouseX;
		m_selectY = m_mouseY;
//		Refresh(false);
		Update();
	}

	if (event.RightDown())
	{
		// nothing yet
	}

	if (event.RightIsDown())
	{
		int deltaX = abs(m_mouseX - m_selectX);
		int deltaY = abs(m_mouseY - m_selectY);

		if (deltaX >= deltaY)
			m_pixelRadius = deltaX;
		else
			m_pixelRadius = deltaY;

		// 127 because right now our PixelLine buffer is limited to 256 pixels!
		if (m_pixelRadius > 127)
			m_pixelRadius = 127;

		Update();
	}

	/*
	if (event.LeftIsDown() || event.RightIsDown())
	{
		
		UpdateSlices(pixelX, pixelY);		
	}
	*/

}

void TestGLCanvas::UpdateSlices()
{

	if (m_image != NULL && m_hPixRef != NULL && m_vPixRef != NULL)
	{

		wxSize sz(GetClientSize());

		int pixelY = sz.GetHeight() - (m_selectY);
		int pixelX = m_selectX;



		// update horizontal pixelLine slice

		// set current strip length
		m_hPixRef->p_len = (m_pixelRadius * 2) + 1;

		double r = 0, g = 0, b = 0;
		int e = 0;
		for (int i = m_selectX - m_pixelRadius; i <= m_selectX + m_pixelRadius; ++i)
		{
			m_image->read_pixel(i, pixelY, r, 0);
			m_image->read_pixel(i, pixelY, g, 1);
			m_image->read_pixel(i, pixelY, b, 2);
			m_hPixRef->R[e] = r / 65535.0f;
			m_hPixRef->G[e] = g / 65535.0f;
			m_hPixRef->B[e] = b / 65535.0f;
			++e;
		}

		m_hPixRef->Refresh();


		// update vertical pixelLine slice

		// set current strip length
		m_vPixRef->p_len = (m_pixelRadius * 2) + 1;

		e = 0;
		for (int i = (sz.GetHeight() - m_selectY) - m_pixelRadius; i <= (sz.GetHeight() - m_selectY) + m_pixelRadius; ++i)
		{
			m_image->read_pixel(pixelX, i, r, 0);
			m_image->read_pixel(pixelX, i, g, 1);
			m_image->read_pixel(pixelX, i, b, 2);
			m_vPixRef->R[e] = r / 65535.0f;
			m_vPixRef->G[e] = g / 65535.0f;
			m_vPixRef->B[e] = b / 65535.0f;
			++e;
		}

		m_vPixRef->Refresh();
	}
}


void TestGLCanvas::InitGL()
{
    glEnable(GL_DEPTH_TEST);

    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_COLOR_MATERIAL);
}

void TestGLCanvas::ResetProjectionMode()
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

		gluOrtho2D(-1.0, 1.0, -1.0, 1.0);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
    }
}


// converts from int wxwindows coords in frame to corresponding OpenGL coords
void toGLCoords(float &x, float &y, wxSize sz)
{
	x = x / (float)(sz.GetWidth() - 2);
	x = (x - 0.5f) * 2.0f; // rescale to -1 to 1 instead of 0 to 1
	y = ((sz.GetHeight() - 2) - y) / (float)(sz.GetHeight() - 2);
	y = (y - 0.5f) * 2.0f; // rescale to -1 to 1 instead of 0 to 1
}