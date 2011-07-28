#include "point_grey_server.h"

point_grey_server::point_grey_server(double framerate, double msExposure, int binning, bool trigger, float gain) {
	_status = false;
	m_cam = new PGRCam(framerate, msExposure, binning, trigger, gain); // initialize the camera inside this constructor call

	_num_columns = m_cam->cols;
	_num_rows = m_cam->rows;
	_minX = _minY = 0;
	_maxX = _num_columns-1;
	_maxY = _num_rows-1;
	_binning = 1;

	_status = m_cam->connected;
}

point_grey_server::~point_grey_server() {
	if (m_cam != NULL) {

		delete m_cam;
		m_cam = NULL;
	}
}

bool point_grey_server::write_to_opengl_texture(GLuint tex_id) {
	// *** stub
	return true;
}

bool point_grey_server::get_pixel_from_memory(unsigned int X, unsigned int Y, vrpn_uint8 &val, int RGB) const {
	val = (vrpn_uint8)m_cam->GetImagePointer()[_num_columns * Y + X];
	return true;
}

bool point_grey_server::get_pixel_from_memory(unsigned int X, unsigned int Y, vrpn_uint16 &val, int RGB) const {
	val = (vrpn_uint16)m_cam->GetImagePointer()[_num_columns * Y + X];
	return true;
}

bool point_grey_server::read_image_to_memory(unsigned int minX, unsigned int maxX, unsigned int minY, unsigned int maxY, double exposure_time_millisecs) {
	vrpn_gettimeofday(&m_timestamp, NULL);
	if (m_cam != NULL) {
		return m_cam->GetNewImage();
	}
	else
		return false;
}

bool point_grey_server::send_vrpn_image(vrpn_Imager_Server* svr, vrpn_Connection* svrcon, double g_exposure, int svrchan, int num_chans) {
	// Make sure we have a valid, open device
	if (!_status) { return false; };

	// Send the current frame over to the client in chunks as big as possible (limited by vrpn_IMAGER_MAX_REGION).
	int nRowsPerRegion=vrpn_IMAGER_MAX_REGIONu8/_num_columns;
	//printf("nRowsPerRegion = %i\n", nRowsPerRegion);
	//printf("(_num_columns, _num_rows) = (%i, %i)\n", _num_columns, _num_rows);
	
	svr->send_begin_frame(0, _num_columns-1, 0, _num_rows-1, 0, 0, &m_timestamp);
	unsigned y;
	for(y=0; y<_num_rows; y+=nRowsPerRegion) {
		svr->send_region_using_base_pointer(svrchan,0,_num_columns-1,y,min(_num_rows,y+nRowsPerRegion)-1,
			(vrpn_uint8*)m_cam->GetImagePointer(), 1, _num_columns, _num_rows, true, 0, 0, 0, &m_timestamp);
		svr->mainloop();
	}
	svr->send_end_frame(0, _num_columns-1, 0, _num_rows-1, 0, 0, &m_timestamp);
	svr->mainloop();

	// Mainloop the server connection (once per server mainloop, not once per object).
	svrcon->mainloop();
	

	return true;
}
