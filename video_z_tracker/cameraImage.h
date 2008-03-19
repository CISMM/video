#pragma once

#include  "base_camera_server.h"

#include "GL/glu.h"

class CameraImage : public image_wrapper {
public:
	CameraImage(int width = 640, int height = 480);
	~CameraImage();

	void read_range(int &minx, int &maxx, int &miny, int &maxy) const;

	unsigned get_num_colors() const;

	bool read_pixel(int x, int y, double &result, unsigned rgb = 0) const;

	double read_pixel_nocheck(int x, int y, unsigned rgb = 0) const;

	bool write_to_opengl_quad(double scale = 1.0, double offset = 0.0);

	unsigned char* getRawImagePointer();

protected:

private:
	unsigned char* m_image;

	int m_width;
	int m_height;

};