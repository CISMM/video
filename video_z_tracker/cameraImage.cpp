#include "cameraImage.h"

CameraImage::CameraImage(int width, int height)
{
	m_image = NULL;

	m_width = width;
	m_height = height;

	m_image = new unsigned char[height * width * 3];
	
	// initialize our image memory, if we want
	for (int x = 0; x < width; ++x)
	{
		for (int y = 0; y < height; ++y)
		{
			m_image[3 * (x + width * y) + 0] = 0;
			m_image[3 * (x + width * y) + 1] = 0;
			m_image[3 * (x + width * y) + 2] = 255;
		}
	}
}

CameraImage::~CameraImage()
{
	if (m_image != NULL)
		delete [] m_image;
}

void CameraImage::read_range(int &minx, int &maxx, int &miny, int &maxy) const
{
	minx = 0;
	maxx = m_width - 1;
	miny = 0;
	maxy = m_height - 1;
}

unsigned CameraImage::get_num_colors() const
{
	return 3;
}

bool CameraImage::read_pixel(int x, int y, double &result, unsigned rgb) const
{
	result = 0.0;
	if (x < 0 || x >= m_width || y < 0 || y >= m_height)
		return false;

	result = read_pixel_nocheck(x, y, rgb);
	return true;
}

double CameraImage::read_pixel_nocheck(int x, int y, unsigned rgb) const
{
	return m_image[3 * (x + m_width * y) + rgb];
}

bool CameraImage::write_to_opengl_quad(double scale, double offset)
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();	

	gluOrtho2D(-1.0, 1.0, -1.0, 1.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
		
	glRasterPos2f(-1, -1);
	glDrawPixels(m_width, m_height, GL_RGB, GL_UNSIGNED_BYTE, m_image);
	
	return true;
}

unsigned char* CameraImage::getRawImagePointer()
{
	return m_image;
}