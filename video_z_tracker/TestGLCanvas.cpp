#include "TestGLCanvas.h"
#include "GL/glut.h"


// some helper functions that may be usefull

// toGLCoords : converts from int wxwindows coords in frame to corresponding OpenGL coords
void toGLCoords(float &x, float &y, wxSize sz);


static void gprintf(char *str)
{
   int l=strlen(str);

   for(int i = 0; i < l; ++i)
      glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *str++);
}




//----------------------------------------------------------------------------
// Capture timing information and print out how many frames per second
// are being drawn.  Remove this function if you don't want timing info.
static void print_timing_info(void)
{ static struct timeval last_print_time;
  struct timeval now;
  static bool first_time = true;
  static int frame_count = 0;

  if (first_time) {
    vrpn_gettimeofday(&last_print_time, NULL);
    first_time = false;
  } else {
    frame_count++;
    vrpn_gettimeofday(&now, NULL);
    double timesecs = 0.001 * vrpn_TimevalMsecs(vrpn_TimevalDiff(now, last_print_time));
    if (timesecs >= 5) {
      double frames_per_sec = frame_count / timesecs;
      frame_count = 0;
      printf("Displayed frames per second = %lg\n", frames_per_sec);
      last_print_time = now;
    }
  }
}






// ---------------------------------------------------------------------------
// TestGLCanvas
// ---------------------------------------------------------------------------

BEGIN_EVENT_TABLE(TestGLCanvas, wxGLCanvas)
    EVT_SIZE(TestGLCanvas::OnSize)
    EVT_PAINT(TestGLCanvas::OnPaint)
    EVT_ERASE_BACKGROUND(TestGLCanvas::OnEraseBackground)

	EVT_MOUSE_EVENTS(TestGLCanvas::OnMouse)
/*
	EVT_LEFT_DOWN(TestGLCanvas::OnMouse)
	EVT_LEFT_UP(TestGLCanvas::OnMouse)
	EVT_LEFT_DCLICK(TestGLCanvas::OnMouse)
	EVT_RIGHT_DOWN(TestGLCanvas::OnMouse)
	EVT_RIGHT_UP(TestGLCanvas::OnMouse)
	EVT_RIGHT_DCLICK(TestGLCanvas::OnMouse)
	EVT_MIDDLE_DOWN(TestGLCanvas::OnMouse)
	EVT_MIDDLE_UP(TestGLCanvas::OnMouse)
	EVT_MIDDLE_DCLICK(TestGLCanvas::OnMouse)
	EVT_MOTION(TestGLCanvas::OnMouse)
	EVT_ENTER_WINDOW(TestGLCanvas::OnMouse)
	EVT_LEAVE_WINDOW(TestGLCanvas::OnMouse)
*/	
	EVT_MOUSEWHEEL(TestGLCanvas::OnMouseWheel)

END_EVENT_TABLE()

TestGLCanvas::TestGLCanvas(wxWindow *parent, wxWindowID id,
    const wxPoint& pos, const wxSize& size, long style, const wxString& name,
	char* deviceName)
    : wxGLCanvas(parent, id, pos, size, style|wxFULL_REPAINT_ON_RESIZE, name)
{
	InitGL();
	ResetProjectionMode();
//	m_imager = NULL;
	m_hPixRef = NULL;
	m_vPixRef = NULL;

	m_mouseX = m_mouseY = -1000;
	m_selectX = m_selectY = -1000;
//	m_radius = 0.05;
	m_pixelRadius = 10;

	m_bits = 16;  // default to 16 bits

	m_showCross = true;

	m_image = NULL;

	m_textColor = 0;

	m_xDim = 0;
	m_yDim = 0;

	m_middleMouseDragX = 0;
	m_middleMouseDragY = 0;
	m_micronsPerPixel = 0.152f; // *** THIS SHOULDN'T BE HARD CODED

	m_mouseWheelDelta = 0;
	m_micronsPerMouseWheel = 1;

	m_middleMouseDown = false;

	m_already_posted = false;
	m_ready_for_region = false;
	m_draining = true;

	m_imageGain = 1;

	m_imagelogging = false;
	//m_connection = NULL;

	m_logger = NULL;

	device_name = deviceName;
	printf("Opening %s\n", device_name);
	m_imager = new vrpn_Imager_Remote(device_name);
	m_imager->register_description_handler(this, handle_description_message);
	m_imager->register_region_handler(this, handle_region_change);
	//g_imager->register_discarded_frames_handler(NULL, handle_discarded_frames);
	m_imager->register_end_frame_handler(this, handle_end_of_frame);

	//m_camera = new VRPN_Imager_camera_server(device_name);

	m_got_dimensions = false;

	printf("Waiting to hear the image dimensions...\n");
	while (!m_got_dimensions) {
		printf(".");
		m_imager->mainloop();
		vrpn_SleepMsecs(1);
	}

	// attempt to fix horrible video logging rates?
	//m_imager->connectionPtr()->Jane_stop_this_crazy_thing(50);

	printf("got image dimensions!\n");
	cols = m_imager->nCols();
	rows = m_imager->nRows();

	m_xDim = cols;
	m_yDim = rows;

	//m_image = new unsigned char[rows * cols * 3];
	m_image = new CameraImage(cols, rows);

	printf("image dimensions: %i by %i\n", cols, rows);

	// now we can safely use our image!
	m_ready_for_region = true;


	printf("Draining image buffer...\n");
	m_imager->throttle_sender(0);
	timeval drainStart;
	timeval cur;
	vrpn_gettimeofday(&drainStart, NULL);
	while (m_draining) {
		m_imager->mainloop();
		vrpn_gettimeofday(&cur, NULL);
		double seconds = 0.001 * vrpn_TimevalMsecs(vrpn_TimevalDiff(cur, drainStart));
		if (seconds >= 2)
			m_draining = false;
	}
	m_imager->throttle_sender(1);

	// setup logging now!
	m_logger = new vrpn_Auxiliary_Logger_Remote(device_name);
	m_logging = false;
}

TestGLCanvas::~TestGLCanvas()
{
	if (m_imager != NULL)
		delete m_imager;

	if (m_image != NULL)
	{
		delete m_image;
	}

	if (m_logger != NULL)
		delete m_logger;
}


void TestGLCanvas::LogToFile(char* filename)
{
	m_logging = (filename != "");
	if (m_logging)
		printf("Start logging!\n");
	else
		printf("Stop logging!\n");

	if (!m_logger->send_logging_request(filename, "", "", ""))
		printf("Logging request failed!\n");
}


void TestGLCanvas::Update()
{
	m_imager->mainloop();
	m_logger->mainloop();
	//vrpn_SleepMsecs(5);
}


void TestGLCanvas::SetInput(vrpn_Imager_Remote* newInput)
{
/*	printf("setting new input...\n");
	if (m_imager != NULL)
		delete m_imager;

	m_imager = newInput;

	if (m_image == NULL)
	{
		m_image = new unsigned char[m_imager->nRows() * m_imager->nCols() * 3];
	}

	Refresh();
*/
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
	glClearColor( 0, 0, 0, 1.0f );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	DrawHUD();
	
	if (m_showCross)
	{
		DrawSelectionBox();
	}

	//printf("drawing current image...\n");
	// draw current image frame
	
	if (m_imager != NULL && m_image != NULL)
	{
		/*
		// set up the projection matrix 
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();	

		gluOrtho2D(-1.0, 1.0, -1.0, 1.0);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		
		glRasterPos2f(-1, -1);
		glDrawPixels(m_imager->nCols(),m_imager->nRows(), GL_RGB, GL_UNSIGNED_BYTE, m_image);
		*/
		m_image->write_to_opengl_quad(m_imageGain, 1);
	}
	
	//m_camera->write_opengl_texture_to_quad(1, 1);
	//printf("finished drawing!\n");
	
    // Flush
    //glFlush();

	m_already_posted = false;

    // Swap
    SwapBuffers();

	UpdateSlices();
}



void TestGLCanvas::DrawSelectionBox()
{
	// draw selection box
	wxSize sz(GetClientSize());
	float cx = m_selectX, cy = m_selectY;
	//toGLCoords(cx, cy, sz); ** we don't want to do this anymore **
	
	// draw the "+"

	glDisable(GL_TEXTURE_2D);
	glDisable(GL_LIGHTING);

	//glDisable(GL_DEPTH_TEST);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();

	gluOrtho2D(0, sz.GetX(), sz.GetY(), 0);

	glColor4f(m_textColor, m_textColor, m_textColor, 0.7);

	//glRasterPos2i(0, sz.GetY());

	// *** DRAW A ROUND SPOT TRACKER TYPE SELECTION MARKER
	double stepsize = M_PI / m_pixelRadius;
	double runaround;
	glBegin(GL_LINE_STRIP);
	for (runaround = 0; runaround <= 2*M_PI; runaround += stepsize) {
		glVertex3f(cx + 2*m_pixelRadius*cos(runaround),cy + 2*m_pixelRadius*sin(runaround), 0.1f);
	}
	glVertex3f(cx + 2*m_pixelRadius, cy, 0.1f);  // Close the circle
	glEnd();
	// Then, make four lines coming from the cirle in to the radius of the spot
	// so we can tell what radius we have set
	glBegin(GL_LINES);
	glVertex2f(cx+m_pixelRadius,cy); glVertex2f(cx+2*m_pixelRadius,cy);
	glVertex2f(cx,cy+m_pixelRadius); glVertex2f(cx,cy+2*m_pixelRadius);
	glVertex2f(cx-m_pixelRadius,cy); glVertex2f(cx-2*m_pixelRadius,cy);
	glVertex2f(cx,cy-m_pixelRadius); glVertex2f(cx,cy-2*m_pixelRadius);
	glEnd();


	/* *** DRAW CROSSHAIRS ***
	glBegin(GL_LINE_LOOP);
	glColor4f(0, 0, 1, 1);
	glVertex3f(cx - 1, cy - m_pixelRadius - 1, 0.1);
	glVertex3f(cx - 1, cy + m_pixelRadius + 1, 0.1);
	glVertex3f(cx + 1, cy + m_pixelRadius + 1, 0.1);
	glVertex3f(cx + 1, cy - m_pixelRadius - 1, 0.1);
	glEnd();

	glBegin(GL_LINE_LOOP);
	glColor4f(1, 0, 0, 1);
	glVertex3f(cx - m_pixelRadius - 1, cy - 1, 0.1);
	glVertex3f(cx - m_pixelRadius - 1, cy + 1, 0.1);
	glVertex3f(cx + m_pixelRadius + 1, cy + 1, 0.1);
	glVertex3f(cx + m_pixelRadius + 1, cy - 1, 0.1);
	glEnd();
	*** */

	glPopMatrix();
}

void TestGLCanvas::DrawHUD()
{
	char buf[80];

	int w, h;
    GetClientSize(&w, &h);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	
	gluOrtho2D(0, w, 0, h);

	glDisable(GL_TEXTURE_2D);
	glDisable(GL_LIGHTING);

	int y = h - 12;
	int x = 3;
	glColor4f(m_textColor, m_textColor, m_textColor, 0.7);

	glRasterPos2i(x,y);
	gprintf("Stage");
	
	y -= 14;	
	glRasterPos2i(x,y);
	sprintf(buf,"z: %f", m_z);
	gprintf(buf);

	y -= 14;
	glRasterPos2i(x,y);
	sprintf(buf,"(%f, %f)", m_x, m_y);
	gprintf(buf);

	// draw a nice red dot if we're logging!
	if (m_logging)
	{
		glPointSize(10);
		glEnable(GL_POINT_SMOOTH);
		glBegin(GL_POINTS);
		glColor4f(1.0, 0, 0, 0.5);
		glVertex3f(w - 15, h - 15, 0.1);
		glEnd();

	}

	glPopMatrix();
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

void TestGLCanvas::OnMouseWheel(wxMouseEvent& event)
{
	// if we have some mouse wheel rotation, then change Z (think: focus knob)
	int rotation = event.GetWheelRotation() / (float)event.GetWheelDelta();
	if (rotation != 0)
	{
		m_mouseWheelDelta += (m_micronsPerMouseWheel * rotation);
	}
}


void TestGLCanvas::OnMouse(wxMouseEvent& event)
{
	// update mouse position for use otherwhere
	int lastX = m_mouseX;
	int lastY = m_mouseY;
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

	// handle XY stage movement with middle dragging
	if (event.MiddleIsDown())
	{
		m_middleMouseDown = true;
		int xDiff = m_mouseX - lastX;
		int yDiff = m_mouseY - lastY;
		
		m_middleMouseDragX += xDiff;
		/*if (m_middleMouseDragX < 0)
			m_middleMouseDragX = 0;
		else if (m_middleMouseDragX > 200)
			m_middleMouseDragX = 200;*/

		m_middleMouseDragY += yDiff;
		/*if (m_middleMouseDragY < 0)
			m_middleMouseDragY = 0;
		else if (m_middleMouseDragY > 200)
			m_middleMouseDragY = 200;*/
	}
	else
	{
		m_middleMouseDown = false;
	}

	// we still need to pass this event on, so things like focus are
	//  changed correctly!!
	event.Skip();
}

void TestGLCanvas::UpdateSlices()
{
	if (m_imager != NULL && m_hPixRef != NULL && m_vPixRef != NULL)
	{

		wxSize sz(GetClientSize());

		int pixelY = sz.GetHeight() - (m_selectY);
		int pixelX = m_selectX;


		float scale = 1.0f / (pow(2.0f, m_bits) - 1.0f);

		// update horizontal pixelLine slice

		// set current strip length
		m_hPixRef->p_len = (m_pixelRadius * 2) + 1;

		double r = 0, g = 0, b = 0;
		int e = 0;
		for (int i = m_selectX - m_pixelRadius; i <= m_selectX + m_pixelRadius; ++i)
		{
			//r = m_image[3 * (i + m_imager->nCols() * pixelY) + 0];
			//g = m_image[3 * (i + m_imager->nCols() * pixelY) + 1];
			//b = m_image[3 * (i + m_imager->nCols() * pixelY) + 2];
			m_image->read_pixel(i, pixelY, r, 0);
			m_image->read_pixel(i, pixelY, g, 1);
			m_image->read_pixel(i, pixelY, b, 2);
//			printf("(%f, %f, %f)\n", r, g, b);
			m_hPixRef->R[e] = r * scale;
			m_hPixRef->G[e] = g * scale;
			m_hPixRef->B[e] = b * scale;
			++e;
		}

		m_hPixRef->Refresh();


		// update vertical pixelLine slice

		// set current strip length
		m_vPixRef->p_len = (m_pixelRadius * 2) + 1;

		e = 0;
		for (int i = (sz.GetHeight() - m_selectY) - m_pixelRadius; i <= (sz.GetHeight() - m_selectY) + m_pixelRadius; ++i)
		{
			//r = m_image[3 * (pixelX + m_imager->nCols() * i) + 0];
			//g = m_image[3 * (pixelX + m_imager->nCols() * i) + 1];
			//b = m_image[3 * (pixelX + m_imager->nCols() * i) + 2];
			m_image->read_pixel(pixelX, i, r, 0);
			m_image->read_pixel(pixelX, i, g, 1);
			m_image->read_pixel(pixelX, i, b, 2);
			m_vPixRef->R[e] = r * scale;
			m_vPixRef->G[e] = g * scale;
			m_vPixRef->B[e] = b * scale;
			++e;
		}

		m_vPixRef->Refresh();
	}
	else
	{
		//printf("Something was NULL in TestGLCanvas::UpdateSlices()\n");
	}
	
}


void TestGLCanvas::InitGL()
{
	/*
    glEnable(GL_DEPTH_TEST);

    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_COLOR_MATERIAL);
	*/
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
	glDisable(GL_DEPTH_TEST);

	glEnable(GL_POINT_SMOOTH);
	glPointSize(10);

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


void  VRPN_CALLBACK TestGLCanvas::handle_region_change(void *testglcanvas, const vrpn_IMAGERREGIONCB info)
{
	//printf("handle_region_change()\n");

	// Just leave things alone if we haven't set up the drawable things
    // yet.
	if (!((TestGLCanvas*)testglcanvas)->m_ready_for_region) { return; }

	const vrpn_Imager_Region  *region=info.region;
    const vrpn_Imager_Remote  *imager = ((TestGLCanvas*)testglcanvas)->m_imager;
	unsigned char* image = (((TestGLCanvas*)testglcanvas)->m_image)->getRawImagePointer();
	int xDim = ((TestGLCanvas*)testglcanvas)->m_xDim;
	int yDim = ((TestGLCanvas*)testglcanvas)->m_yDim;

    // Just leave things alone if we haven't set up the drawable things
    // yet.
	if (!((TestGLCanvas*)testglcanvas)->m_ready_for_region) { return; }

    // Copy pixels into the image buffer.
    // Flip the image over in Y so that the image coordinates
    // display correctly in OpenGL.
    // Figure out which color to put the data in depending on the name associated
    // with the channel index.  If it is one of "red", "green", or "blue" then put
    // it into that channel.
    if (strcmp(imager->channel(region->d_chanIndex)->name, "red") == 0) {
      region->decode_unscaled_region_using_base_pointer((vrpn_uint8*)image+0, 3, 3*xDim, 0, yDim, true);
    } else if (strcmp(imager->channel(region->d_chanIndex)->name, "green") == 0) {
      region->decode_unscaled_region_using_base_pointer((vrpn_uint8*)image+1, 3, 3*xDim, 0, yDim, true);
    } else if (strcmp(imager->channel(region->d_chanIndex)->name, "blue") == 0) {
      region->decode_unscaled_region_using_base_pointer((vrpn_uint8*)image+2, 3, 3*xDim, 0, yDim, true);
    } else {
      // This uses a repeat count of three to put the data into all channels.
      // NOTE: This copies each channel into all buffers, rather
      // than just a specific one (overwriting all with the last one written).  A real
      // application will probably want to provide a selector to choose which
      // is drawn.  It can check region->d_chanIndex to determine which channel
      // is being reported for each callback.
      region->decode_unscaled_region_using_base_pointer((vrpn_uint8*)image, 3, 3*xDim, 0, yDim, true, 3);
    }

    // We do not post a redisplay here, because we want to do that only
    // when we've gotten the end of a frame.  It is done in the
    // end_of_frame message handler.

	//printf("done handling region change!\n");
}


void  VRPN_CALLBACK TestGLCanvas::handle_end_of_frame(void *thisCanvas,const struct _vrpn_IMAGERENDFRAMECB)
{
	//printf("handle_end_of_frame()\n");

    // Tell Glut it is time to draw.  Make sure that we don't post the redisplay
    // operation more than once by checking to make sure that it has been handled
    // since the last time we posted it.  If we don't do this check, it gums
    // up the works with tons of redisplay requests and the program won't
    // even handle windows events.

    // NOTE: This exposes a race condition.  If more video messages arrive
    // before the frame-draw is executed, then we'll end up drawing some of
    // the new frame along with this one.  To make really sure there is not tearing,
    // double buffer: fill partial frames into one buffer and draw from the
    // most recent full frames in another buffer.  You could use an OpenGL texture
    // as the second buffer, sending each full frame into texture memory and
    // rendering a textured polygon.

	
	((TestGLCanvas*)thisCanvas)->m_newImage = true;

	if (!((TestGLCanvas*)thisCanvas)->m_already_posted) {
		//printf("we have not already posted--so let's do it!\n");
		((TestGLCanvas*)thisCanvas)->m_already_posted = true;

		((TestGLCanvas*)thisCanvas)->Refresh();
		print_timing_info();
    }

	// ask the video server for 1 more frame.
	((TestGLCanvas*)thisCanvas)->m_imager->throttle_sender(1);

	//printf("done handling end of frame!\n");
}


void  VRPN_CALLBACK TestGLCanvas::handle_description_message(void *thisCanvas, const struct timeval)
{
  // This method is different from other VRPN callbacks because it simply
  // reports that values have been filled in on the Imager_Remote class.
  // It does not report what the new values are, only the time at which
  // they changed.


	printf("handle_description_message()\n");

  // If we have already heard the dimensions, then check and make sure they
  // have not changed.  Also ensure that there is at least one channel.  If
  // not, then print an error and exit.
  if (((TestGLCanvas*)thisCanvas)->m_got_dimensions) {
    /*if ( (g_Xdim != g_imager->nCols()) || (g_Ydim != g_imager->nRows()) ) {
      fprintf(stderr,"Error -- different image dimensions reported\n");
      exit(-1);
    }*/
    if (((TestGLCanvas*)thisCanvas)->m_imager->nChannels() <= 0) {
      fprintf(stderr,"Error -- No channels to display!\n");
      exit(-1);
    }
  }

  // Record that the dimensions are filled in.  Fill in the globals needed
  // to store them.
  //g_Xdim = g_imager->nCols();
  //g_Ydim = g_imager->nRows();
  ((TestGLCanvas*)thisCanvas)->m_got_dimensions = true;
}







// converts from int wxwindows coords in frame to corresponding OpenGL coords
void toGLCoords(float &x, float &y, wxSize sz)
{
	x = x / (float)(sz.GetWidth() - 2);
	x = (x - 0.5f) * 2.0f; // rescale to -1 to 1 instead of 0 to 1
	y = ((sz.GetHeight() - 2) - y) / (float)(sz.GetHeight() - 2);
	y = (y - 0.5f) * 2.0f; // rescale to -1 to 1 instead of 0 to 1
}


