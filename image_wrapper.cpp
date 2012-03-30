#include  <stdlib.h>
#include  <math.h>
#include  <stdio.h>

#include  "image_wrapper.h"

// Sum all the pixels for one color (defaults to the first) in an image.
double image_wrapper_sum(const image_wrapper &img, unsigned rgb)
{
	double sum = 0;
	int minx, maxx, miny, maxy;
	img.read_range(minx, maxx, miny, maxy);
	int x,y;
	for (x = minx; x <= maxx; x++) {
		for (y = miny; y <= maxy; y++) {
			sum += img.read_pixel_nocheck(x, y, rgb);
		}
	}
	return sum;
}

// Sum all the squared values of all pixels for one color
// (defaults to the first) in an image.
double image_wrapper_square_sum(const image_wrapper &img, unsigned rgb)
{
	double sum = 0;
	int minx, maxx, miny, maxy;
	img.read_range(minx, maxx, miny, maxy);
	int x,y;
	for (x = minx; x <= maxx; x++) {
		for (y = miny; y <= maxy; y++) {
			double val = img.read_pixel_nocheck(x, y, rgb);
			sum += val*val;
		}
	}
	return sum;
}

disc_image::disc_image(int minx, int maxx, int miny, int maxy,
		       double background, double noise,
		       double diskx, double disky, double diskr,
		       double diskintensity, int oversample) :
  double_image(minx, maxx, miny, maxy),
  _oversample(oversample)
{
  int i,j, index;
  double oi,oj;	  //< Oversample steps

  // Make sure the parameters are meaningful
  if ( (_oversample <= 0) ) {
    fprintf(stderr,"disc_image::disc_image(): Bad oversample\n");
    _minx = _maxx = _miny = _maxy = 0;
    return;
  }

  // Fill in the background intensity.
  for (i = _minx; i <= _maxx; i++) {
    for (j = _miny; j <= _maxy; j++) {
      write_pixel_nocheck(i, j, background);
    }
  }

  // Add in 0.5 pixel to the X and Y positions
  // so that we center the disc on the middle of the pixel rather than
  // having it centered between pixels.
  diskx += 0.5;
  disky += 0.5;

  // Fill in the disk intensity (the part of it that is within the image).
  // Oversample the image by the factor specified in _oversample, averaging
  // all results within the pixel.  
#ifdef	DEBUG
  printf("disc_image::disc_image(): Making disk of radius %lg\n", diskr);
#endif
  double  disk2 = diskr * diskr;
  for (i = (int)floor(diskx - diskr); i <= (int)ceil(diskx + diskr); i++) {
    for (j = (int)floor(disky - diskr); j <= (int)ceil(disky + diskr); j++) {
      if (find_index(i,j,index)) {
	_image[index] = 0;
	for (oi = 0; oi < _oversample; oi++) {
	  for (oj = 0; oj < _oversample; oj++) {
	    double x = i + oi/_oversample;
	    double y = j + oj/_oversample;
	    if ( disk2 >= (x - diskx)*(x - diskx) + (y - disky)*(y - disky) ) {
	      _image[index] += diskintensity;
	    } else {
	      _image[index] += background;
	    }
	  }
	}
	_image[index] /= (_oversample * _oversample);
      }
    }
  }

  // Add zero-mean uniform noise to the image, with the specified width
  for (i = _minx; i <= _maxx; i++) {
    for (j = _miny; j <= _maxy; j++) {
      if (find_index(i,j,index)) {
	double	unit_rand = (double)(rand()) / RAND_MAX;
	_image[index] += (unit_rand - 0.5) * 2 * noise;
      }
    }
  }  
}

multi_disc_image::multi_disc_image(std::vector<bead>& b, 
                           int minx, int maxx, int miny, int maxy,
		                   double background, double noise,
		                   int oversample) :
  double_image(minx, maxx, miny, maxy),
  _oversample(oversample)
{
  int i,j, index;
  double oi,oj;	// Oversample steps
  
  // Make sure the parameters are meaningful
  if ( (_oversample <= 0) ) {
    fprintf(stderr,"multi_disc_image::multi_disc_image(): Bad oversample\n");
    _minx = _maxx = _miny = _maxy = 0;
    return;
  }
  
  // Fill in the background intensity.
  for (i = _minx; i <= _maxx; i++) {
    for (j = _miny; j <= _maxy; j++) {
      write_pixel_nocheck(i, j, background);
    }
  }
  
  // visualize all the beads in the bead vector, b
  for(std::vector<bead>::size_type bead = 0; bead < b.size(); bead++){
    // Add in 0.5 pixel to the X and Y positions
    // so that we center the disc on the middle of the pixel rather than
    // having it centered between pixels.
    // b[bead].x += 0.5;
    // b[bead].y += 0.5;
  
    // Fill in the disk intensity (the part of it that is within the image).
    // Oversample the image by the factor specified in _oversample, averaging
    // all results within the pixel.  
#ifdef	DEBUG
  printf("multi_disc_image::multi_disc_image(): Making disk of radius %lg\n", b[bead].r);
#endif
    double disk2 = b[bead].r * b[bead].r;
    for (i = (int)floor(b[bead].x - b[bead].r); i <= (int)ceil(b[bead].x + b[bead].r); i++) {
      for (j = (int)floor(b[bead].y - b[bead].r); j <= (int)ceil(b[bead].y + b[bead].r); j++) {
        if (find_index(i,j,index)) {
  	      _image[index] = 0;
  	      for (oi = 0; oi < _oversample; oi++) {
  	        for (oj = 0; oj < _oversample; oj++) {
  	          double x = i + oi/_oversample;
  	          double y = j + oj/_oversample;
  	          if ( disk2 >= (x - b[bead].x)*(x - b[bead].x) + (y - b[bead].y)*(y - b[bead].y) ) {
  	            _image[index] += b[bead].intensity;
  	          } else {
  	            _image[index] += background;
  	          }
  	        }
  	      }
          _image[index] /= (_oversample * _oversample);
        }
      }
    }
  }
  
  // Add zero-mean uniform noise to the image, with the specified width
  for (i = _minx; i <= _maxx; i++) {
    for (j = _miny; j <= _maxy; j++) {
      if (find_index(i,j,index)) {
        // This line is commented for testing because it affects the positions of points in images of different sizes.
  	    //double unit_rand = (double)(rand()) / RAND_MAX;
        // This line is commented because it uses the results of the previous line. 
  	    //_image[index] += (unit_rand - 0.5) * 2 * noise;
      }
    }
  }  
}

cone_image::cone_image(int minx, int maxx, int miny, int maxy,
		       double background, double noise,
		       double diskx, double disky, double diskr,
		       double centeritensity, int oversample) :
  double_image(minx, maxx, miny, maxy),
  _oversample(oversample)
{
  int i,j, index;
  int oi,oj;	  //< Oversample steps

  // Fill in the background intensity.
  for (i = _minx; i <= _maxx; i++) {
    for (j = _miny; j <= _maxy; j++) {
      write_pixel_nocheck(i, j, background);
    }
  }

  // Compute where the samples should be taken.  These need to be taken
  // symmetrically around the pixel center and cover all samples within
  // the pixel exactly once.  First, compute the step size between the
  // samples, then the one with the smallest value that lies within a
  // half-pixel-width of the center of the pixel as the starting offset.
  // Proceeed until we exceed a half-pixel-radius on the high side of
  // the pixel.

  double step = 1.0 / _oversample;
  double start;
  if (_oversample % 2 == 1) { // Odd number of steps
    start = - step * floor( _oversample / 2.0 );
  } else {    // Even number of steps
    start = - step * ( (_oversample / 2 - 1) + 0.5 );
  }

  // Fill in the cone intensity (the part of it that is within the image).
#ifdef	DEBUG
  printf("cone_image::cone_image(): Making cone of radius %lg\n", diskr);
#endif
  for (i = (int)floor(diskx - diskr); i <= (int)ceil(diskx + diskr); i++) {
    for (j = (int)floor(disky - diskr); j <= (int)ceil(disky + diskr); j++) {
      if (find_index(i,j,index)) {
	_image[index] = 0;
	for (oi = 0; oi < _oversample; oi++) {
	  for (oj = 0; oj < _oversample; oj++) {
	    double x = i + start + oi*step;
	    double y = j + start + oj*step;
	    double dist = sqrt((x-diskx)*(x-diskx) + (y-disky)*(y-disky));
	    if ( diskr >= dist ) {
	      double frac = dist / diskr;
	      double intensity = frac * background + (1 - frac) * centeritensity;
	      _image[index] += intensity;
	    } else {
	      _image[index] += background;
	    }
	  }
	}
	_image[index] /= (_oversample * _oversample);
      }
    }
  }

  // Add zero-mean uniform noise to the image, with the specified width
  for (i = _minx; i <= _maxx; i++) {
    for (j = _miny; j <= _maxy; j++) {
      if (find_index(i,j,index)) {
	double	unit_rand = (double)(rand()) / RAND_MAX;
	_image[index] += (unit_rand - 0.5) * 2 * noise;
      }
    }
  }  
}

multi_cone_image::multi_cone_image(std::vector<bead>& b,
               int minx, int maxx, int miny, int maxy,
		       double background, double noise,
		       int oversample) :
  double_image(minx, maxx, miny, maxy),
  _oversample(oversample)
{ 
  int i,j, index;
  int oi,oj; // Oversample steps
  
  // Fill in the background intensity.
  for (i = _minx; i <= _maxx; i++) {
    for (j = _miny; j <= _maxy; j++) {
      write_pixel_nocheck(i, j, background);
    }
  }
  
  // Compute where the samples should be taken.  These need to be taken
  // symmetrically around the pixel center and cover all samples within
  // the pixel exactly once.  First, compute the step size between the
  // samples, then the one with the smallest value that lies within a
  // half-pixel-width of the center of the pixel as the starting offset.
  // Proceeed until we exceed a half-pixel-radius on the high side of
  // the pixel.
  
  double step = 1.0 / _oversample;
  double start;
  if (_oversample % 2 == 1) { // Odd number of steps
    start = - step * floor( _oversample / 2.0 );
  } else {    // Even number of steps
    start = - step * ( (_oversample / 2 - 1) + 0.5 );
  }
  
  // Fill in the cone intensity (the part of it that is within the image).
  for (std::vector<bead>::size_type bead = 0; bead < b.size(); bead++) {  
#ifdef	DEBUG
  printf("multi_cone_image::multi_cone_image(): Making cone of radius %lg\n", b[bead].r));
#endif
    for (i = (int)floor(b[bead].x - b[bead].r); i <= (int)ceil(b[bead].x + b[bead].r); i++) {
      for (j = (int)floor(b[bead].y - b[bead].r); j <= (int)ceil(b[bead].y + b[bead].r); j++) {
        if (find_index(i,j,index)) {
          _image[index] = 0;
          for (oi = 0; oi < _oversample; oi++) {
            for (oj = 0; oj < _oversample; oj++) {
              double x = i + start + oi*step;
              double y = j + start + oj*step;
              double dist = sqrt((x-b[bead].x)*(x-b[bead].x) + (y-b[bead].y)*(y-b[bead].y));
              if ( b[bead].r >= dist ) {
                double frac = dist / b[bead].r;
                double intensity = frac * background + (1 - frac) * b[bead].intensity;
                _image[index] += intensity;
              } else {
                _image[index] += background;
              }
	        }
          }
          _image[index] /= (_oversample * _oversample);
        }
      }
    }
  }
  
  // Add zero-mean uniform noise to the image, with the specified width
  for (i = _minx; i <= _maxx; i++) {
    for (j = _miny; j <= _maxy; j++) {
      if (find_index(i,j,index)) {
        double	unit_rand = (double)(rand()) / RAND_MAX;
        _image[index] += (unit_rand - 0.5) * 2 * noise;
      }
    }
  }
}

Integrated_Gaussian_image::Integrated_Gaussian_image(int minx, int maxx, int miny, int maxy,
	     double background, double noise,
	     double centerx, double centery, double std_dev,
	     double summedvolume, int oversample) :
  double_image(minx, maxx, miny, maxy),
  _oversample(oversample),
  _background(background)
{
  recompute(background, noise, centerx, centery, std_dev, summedvolume, oversample);
}

void Integrated_Gaussian_image::recompute(double background, double noise,
	     double centerx, double centery, double std_dev,
	     double summedvolume, int oversample)
{
  _oversample = oversample;
  int i,j, index;

//  XXX Verify that this centers the pixel like it should.
  // Compute where the samples should be taken.  These need to be taken
  // symmetrically around the pixel center and cover all samples within
  // the pixel exactly once.  First, compute the step size between the
  // samples, then the one with the smallest value that lies within a
  // half-pixel-width of the center of the pixel as the starting offset.
  // Proceeed until we exceed a half-pixel-radius on the high side of
  // the pixel.

  // Fill in the Gaussian intensity plus background plus noise (if any).
  // Note that the area under the curve for the unit Gaussian is 1; we
  // multiply by the summedvolume (which needs to be the sum above or
  // below the background) to produce an overall volume matching the
  // one requested.
#ifdef	DEBUG
  printf("Gaussian_image::recompute(): Making Gaussian of standard deviation %lg, background %lg, volume %lg\n", std_dev, background, summedvolume);
#endif
  for (i = _minx; i <= _maxx; i++) {
    for (j = _miny; j <= _maxy; j++) {
      double x0 = (i - centerx) - 0.5;    // The left edge of the pixel in Gaussian space
      double x1 = x0 + 1;                 // The right edge of the pixel in Gaussian space
      double y0 = (j - centery) - 0.5;    // The bottom edge of the pixel in Gaussian space
      double y1 = y0 + 1;                 // The top edge of the pixel in Gaussian space
      if (find_index(i,j,index)) {
        // For this call to ComputeGaussianVolume, we assume 1-meter pixels.
        // This makes std dev and other be in pixel units without conversion.
	_image[index] = background + ComputeGaussianVolume(summedvolume, std_dev,
                        x0,x1, y0,y1, _oversample);
        
        // Add zero-mean uniform noise to the image, with the specified width
        if (noise > 0.0) {
	  double  unit_rand = (double)(rand()) / RAND_MAX;
	  _image[index] += (unit_rand - 0.5) * 2 * noise;
        }
      }
    }
  }
}

Integrated_multi_Gaussian_image::Integrated_multi_Gaussian_image(std::vector<bead>& b,
         int minx, int maxx, int miny, int maxy,
	     double background, double noise,
	     int oversample) :
  double_image(minx, maxx, miny, maxy),
  _oversample(oversample),
  _background(background)
{
  multi_recompute(b, background, noise, oversample);
}

void Integrated_multi_Gaussian_image::multi_recompute(std::vector<bead>& b,
         double background, double noise,
	     int oversample)
{
  _oversample = oversample;
  int i,j, index;
  double summedvolume;

//  XXX Verify that this centers the pixel like it should.
  // Compute where the samples should be taken.  These need to be taken
  // symmetrically around the pixel center and cover all samples within
  // the pixel exactly once.  First, compute the step size between the
  // samples, then the one with the smallest value that lies within a
  // half-pixel-width of the center of the pixel as the starting offset.
  // Proceeed until we exceed a half-pixel-radius on the high side of
  // the pixel.

  // Fill in the Gaussian intensity plus background plus noise (if any).
  // Note that the area under the curve for the unit Gaussian is 1; we
  // multiply by the summedvolume (which needs to be the sum above or
  // below the background) to produce an overall volume matching the
  // one requested.
  
  // Fill in the background intensity.
  for (i = _minx; i <= _maxx; i++) {
    for (j = _miny; j <= _maxy; j++) {
      write_pixel_nocheck(i, j, background);
    }
  }
  
  for(std::vector<bead>::size_type bead = 0; bead < b.size(); bead++){
#ifdef	DEBUG
  printf("multi_Gaussian_image::multi_recompute(): Making Gaussian of standard deviation %lg, background %lg, volume %lg\n",
          b[bead].r, background, b[bead].intensity * b[bead].r*b[bead].r * 2 * M_PI);
#endif
    for (i = _minx; i <= _maxx; i++) {
      for (j = _miny; j <= _maxy; j++) {
        double x0 = (i - b[bead].x) - 0.5;    // The left edge of the pixel in Gaussian space
        double x1 = x0 + 1;                   // The right edge of the pixel in Gaussian space
        double y0 = (j - b[bead].y) - 0.5;    // The bottom edge of the pixel in Gaussian space
        double y1 = y0 + 1;                   // The top edge of the pixel in Gaussian space
        if (find_index(i,j,index)) {
          // For this call to ComputeGaussianVolume, we assume 1-meter pixels.
          // This makes std dev and other be in pixel units without conversion.
          summedvolume = b[bead].intensity * b[bead].r*b[bead].r * 2 * M_PI;
          _image[index] += ComputeGaussianVolume(summedvolume, b[bead].r, x0,x1, y0,y1, _oversample); 
        }
      }
    }
  }
  
  // Add zero-mean uniform noise to the image, with the specified width
  for (i = _minx; i <= _maxx; i++) {
    for (j = _miny; j <= _maxy; j++) {
      if (find_index(i,j,index)) {
  	    double	unit_rand = (double)(rand()) / RAND_MAX;
  	    _image[index] += (unit_rand - 0.5) * 2 * noise;
      }
    }
  }
}

Point_sampled_Gaussian_image::Point_sampled_Gaussian_image(
	     double background, double noise,
	     double centerx, double centery, double std_dev,
	     double summedvolume)
{
    set_new_parameters(background, noise, centerx, centery, std_dev, summedvolume);
}

double Point_sampled_Gaussian_image::read_pixel_nocheck(int x, int y, unsigned /* RGB ignored */) const
{
  double result = _background + _summedvolume * ComputeGaussianAtPoint(_std_dev, x - _centerx, y - _centery);
  if (_noise != 0.0) {
    double  unit_rand = (double)(rand()) / RAND_MAX;
    result += (unit_rand - 0.5) * 2 * _noise;
  }
  return result;
}

// Read a pixel from the image into a double; return true if the pixel
// was in the image, false if it was not.
bool Point_sampled_Gaussian_image::read_pixel(int x, int y, double &result, unsigned /* RGB ignored */) const
{
  result = read_pixel_nocheck(x, y, 0);
  return true;
}

rod_image::rod_image(int minx, int maxx, int miny, int maxy,
		       double background, double noise,
		       double rodx, double rody, double rodr,
		       double rodlength, double rodangleradians,
                       double rodintensity, int oversample) :
  double_image(minx, maxx, miny, maxy),
  _oversample(oversample)
{
  int i,j, index;
  double oi,oj;	  //< Oversample steps

  // Make sure the parameters are meaningful
  if ( (_oversample <= 0) ) {
    fprintf(stderr,"rod_image::rod_image(): Bad oversample\n");
    _minx = _maxy = _minx = _maxx = 0;
    return;
  }

  // Fill in the background intensity.
  for (i = _minx; i <= _maxx; i++) {
    for (j = _miny; j <= _maxy; j++) {
      write_pixel_nocheck(i, j, background);
    }
  }

  // Add in 0.5 pixel to the X and Y positions
  // so that we center the rod on the middle of the pixel rather than
  // having it centered between pixels.
  rodx += 0.5;
  rody += 0.5;

  // Find the points A and B at either end of the line segment
  double Ax = rodx + cos(rodangleradians) * (rodlength/2);
  double Ay = rody + sin(rodangleradians) * (rodlength/2);
  double Bx = rodx - cos(rodangleradians) * (rodlength/2);
  double By = rody - sin(rodangleradians) * (rodlength/2);

  // Fill in the rod intensity (the part of it that is within the image).
  // Oversample the image by the factor specified in _oversample, averaging
  // all results within the pixel.  
#ifdef	DEBUG
  printf("rod_image::disc_image(): Making rod of radius %lg\n", rodr);
#endif
  double  radius2 = rodr * rodr;
  // Only do these checks within the region that MAY contain a rod, which
  // is length/2 + radius from the center.
  double range = rodlength/2 + rodr;
  for (i = (int)floor(rodx - range); i <= (int)ceil(rodx + range); i++) {
    for (j = (int)floor(rody - range); j <= (int)ceil(rody + range); j++) {
      if (find_index(i,j,index)) {
	_image[index] = 0;
	for (oi = 0; oi < _oversample; oi++) {
	  for (oj = 0; oj < _oversample; oj++) {
	    double Px = i + oi/_oversample;
	    double Py = j + oj/_oversample;

            // Find out if our particular sample is within the rod or not.
            // If so, we add in the rod intensity.  If not, we add in the
            // background.  We do this by checking if it is within radius
            // of the nearest point on the line segment that joins the two
            // ends of the rod.  Each end is located a distance of rodlength/2
            // from the center, along vectors that are in the +angle direction
            // and its opposite (pi radians away).  We find the two points
            // and then use the algorithm described on pages 10-13 of
            // Graphics Gems II to determine the (square of the) distance from the segment
            // between them.
            double a2 = (Py - Ay)*(Bx - Ax) - (Px - Ax)*(By - Ay);
            double perpdist2 = (a2*a2) / ( (Bx - Ax)*(Bx - Ax) + (By - Ay)*(By - Ay) );
            double t = (Px - Ax)*(Bx - Ax) + (Py - Ay)*(By - Ay);
            double dist2;
            if (t < 0) {  // The perpendicular is beyond A, so use A
              dist2 = (Px - Ax)*(Px - Ax) + (Py - Ay)*(Py - Ay);
            } else {
              t = (Bx - Px)*(Bx - Ax) + (By - Py)*(By - Ay);
              if (t < 0) {  // The perpendicular is beyond B, so use B
                dist2 = (Px - Bx)*(Px - Bx) + (Py - By)*(Py - By);
              } else {  // We're in the middle, so use perpendicular distance
                dist2 = perpdist2;
              }
            }

	    if ( radius2 >= dist2 ) {
	      _image[index] += rodintensity;
	    } else {
	      _image[index] += background;
	    }
	  }
	}
	_image[index] /= (_oversample * _oversample);
      }
    }
  }

  // Add zero-mean uniform noise to the image, with the specified width
  for (i = _minx; i <= _maxx; i++) {
    for (j = _miny; j <= _maxy; j++) {
      if (find_index(i,j,index)) {
	double	unit_rand = (double)(rand()) / RAND_MAX;
	_image[index] += (unit_rand - 0.5) * 2 * noise;
      }
    }
  }  
}


// XXX This could be made much faster if needed.
gaussian_blurred_image::gaussian_blurred_image(const image_wrapper &input
    , const unsigned aperture
    , const float std)
    : float_image(0, input.get_num_rows()-1,
                   0, input.get_num_columns()-1)
{
  //printf("dbg: Aperture = %u\n", aperture);

  // There is a simpler and a (slightly) faster version of this code.  The
  // simpler is easier to follow and see what it does, but it does
  // a ton of comparisons for pixel boundaries, which slows it
  // way down.
//#define VIDEO_SIMPLE_GAUSSIAN_BLUR
#ifdef VIDEO_SIMPLE_GAUSSIAN_BLUR
  // Construct a temporary Gaussian image with a size that is twice
  // the aperture plus one to store the Gaussian with which we will
  // convolve the input image.  Fill it with the subset of a unit
  // Gaussian whose standard deviation is the one passed in.
  unsigned kernel_size = 2 * aperture + 1;
  Integrated_Gaussian_image kernel(0, kernel_size - 1, 0, kernel_size - 1,
    0, 0, aperture, aperture, std, 1, 4);

  // Convolve the image with the Gaussian kernel.  At each pixel, we
  // sum up the amount of the kernel that is on the inside of time image
  // and rescale by this so that we get equal energy at all locations,
  // even near the edges.
  unsigned x,y;
  unsigned center = aperture; // Index of the center pixel in the kernel
  double value;
  for (x = 0; x < get_num_rows(); x++) {
    for (y = 0; y < get_num_columns(); y++) {
      int i, j;
      int aperture_int = aperture;
      double sum = 0;
      double weight = 0;
      for (i = -aperture_int; i <= aperture_int; i++) {
        for (j = -aperture_int; j <= aperture_int; j++) {
          if (input.read_pixel(x+i, y+j, value)) {
            double kval = kernel.read_pixel_nocheck(center+i,center+j);
            weight += kval;
            sum += kval * value;
          }
        }
      }
      write_pixel(x, y, sum/weight);
    }
  }
#else
  // Faster version of convolution that avoids checking for boundary
  // conditions on the images.  We first create a Gaussian kernel image
  // and copy it into a buffer that is fast for reading.  We then copy
  // the input image into a buffer that has a border around it so we can
  // run past during convolution.  Inside the original image, we place
  // the values; outside, zero.  We then make a second weighting image
  // that has weights of 1 inside the image an 0 in the border.  That way,
  // we can run the same algorithm at every pixel and not have to check
  // whether we're going outside the image.  We write the results directly
  // into our floating-point image values.

  // Construct a temporary Gaussian image with a size that is twice
  // the aperture plus one to store the Gaussian with which we will
  // convolve the input image.  Fill it with the subset of a unit
  // Gaussian whose standard deviation is the one passed in.
  unsigned kernel_size = 2 * aperture + 1;
  Integrated_Gaussian_image kernel(0, kernel_size - 1, 0, kernel_size - 1,
    0, 0, aperture, aperture, std, 1, 4);

  // Construct a temporary floating-point image to store the padded
  // version of the input image.  Fill the boundaries with zeroes and
  // then fill the image with values.
  float_image copy(0, input.get_num_rows() + 2*aperture -1,
                   0, input.get_num_columns() + 2*aperture -1);
  int x;  // Needs to be int for OpenMP
  // We parallelize the inner loop here because aperture will be small.
  for (x = 0; x < (int)(aperture); x++) {
    int y;  // Needs to be int for OpenMP
    #pragma omp parallel for
    for (y = 0; y < (int)(input.get_num_columns() + 2 * aperture); y++) {
      copy.write_pixel_nocheck(x, y, 0);
      copy.write_pixel_nocheck(input.get_num_rows() + 2*aperture - 1 - x, y, 0);
    }
  }
  #pragma omp parallel for
  for (x = 0; x < (int)(input.get_num_rows() + 2 * aperture); x++) {
    int y;  // Needs to be int for OpenMP
    for (y = 0; y < (int)(aperture); y++) {
      copy.write_pixel_nocheck(x, y, 0);
      copy.write_pixel_nocheck(x, input.get_num_columns() + 2*aperture - 1 - y, 0);
    }
  }
  #pragma omp parallel for
  for (x = 0; x < (int)(input.get_num_rows()); x++) {
    int y;  // Needs to be int for OpenMP
    for (y = 0; y < (int)(input.get_num_columns()); y++) {
      copy.write_pixel_nocheck(x+aperture, y+aperture,
        input.read_pixel_nocheck(x,y));
    }
  }

  // Construct a temporary floating-point mask image to store the weights
  // which are 1 within the copied image and 0 around the border.
  float_image mask(0, input.get_num_rows() + 2*aperture -1,
                   0, input.get_num_columns() + 2*aperture -1);
  #pragma omp parallel for
  for (x = 0; x < (int)(aperture); x++) {
    int y;  // Needs to be int for OpenMP
    for (y = 0; y < (int)(input.get_num_columns() + 2 * aperture); y++) {
      mask.write_pixel_nocheck(x, y, 0);
      mask.write_pixel_nocheck(input.get_num_rows() + 2*aperture - 1 - x, y, 0);
    }
  }
  #pragma omp parallel for
  for (x = 0; x < (int)(input.get_num_rows() + 2 * aperture); x++) {
    int y;  // Needs to be int for OpenMP
    for (y = 0; y < (int)(aperture); y++) {
      mask.write_pixel_nocheck(x, y, 0);
      mask.write_pixel_nocheck(x, input.get_num_columns() + 2*aperture - 1 - y, 0);
    }
  }
  #pragma omp parallel for
  for (x = 0; x < (int)(input.get_num_rows()); x++) {
    int y;  // Needs to be int for OpenMP
    for (y = 0; y < (int)(input.get_num_columns()); y++) {
      mask.write_pixel_nocheck(x+aperture, y+aperture, 1);
    }
  }

  // Convolve the image with the Gaussian kernel.  At each pixel, we
  // sum up the amount of the kernel that is on the inside of time image
  // and rescale by this so that we get equal energy at all locations,
  // even near the edges.
  unsigned center = aperture; // Index of the center pixel in the kernel
  int aperture_int = aperture;
  #pragma omp parallel for
  for (x = 0; x < (int)(get_num_rows()); x++) {
    int y;  // Needs to be int for OpenMP
    for (y = 0; y < (int)(get_num_columns()); y++) {
      int i, j;
      double sum = 0;
      double weight = 0;
      for (i = -aperture_int; i <= aperture_int; i++) {
        for (j = -aperture_int; j <= aperture_int; j++) {
          double kval = kernel.read_pixel_nocheck(center+i,center+j);
          weight += kval * mask.read_pixel_nocheck(x+aperture+i, y+aperture+j);
          sum += kval * copy.read_pixel_nocheck(x+aperture+i, y+aperture+j);
        }
      }
      write_pixel(x, y, sum/weight);
    }
  }
#endif
}


