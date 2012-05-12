/*********************************************************************
This is the source file for the CUDA version of the spot_tracker library
function calls..

WARNING: All of the CUDA code for the entire project has to be in here
so that we only initialize the device once.
**********************************************************************/

#include "image_wrapper.h"
#include "spot_tracker.h"
#include <stdio.h>
#include <cuda.h>
#include <math_constants.h>

//----------------------------------------------------------------------
// Definitions and routines needed by all functions below.
//----------------------------------------------------------------------

static CUdevice     g_cuDevice;     // CUDA device
static CUcontext    g_cuContext;    // CUDA context on the device
static float        *g_cuda_fromhost_buffer = NULL;
static unsigned		g_cuda_fromhost_nx = 0;
static unsigned		g_cuda_fromhost_ny = 0;

// For the GPU code, block size and number of kernels to run to cover a whole grid.
// Initialized once in VST_ensure_cuda_ready();
static dim3         g_threads;      // 16x16x1
static dim3         g_grid;         // Computed to cover array (slightly larger than array)

// Open the CUDA device and get a context.  Also allocate a buffer of
// appropriate size.  Do this allocation only when the size of the buffer
// allocated is different from the newly-requested size.  Return false
// if we cannot get one.  This function can be called every time a
// CUDA_using function is called, but it only does the device opening
// and image-buffer allocation once.
static bool VST_ensure_cuda_ready(const VST_cuda_image_buffer &inbuf)
{
  static bool initialized = false;	// Have we initialized yet?
  static bool okay = false;			// Did the initialization work?
  if (!initialized) {
    // Whether this works or not, we'll be initialized.
    initialized = true;
    
    // Open the largest-ID CUDA device in the system
    cuInit(0);
    int num_devices = 0;
    cuDeviceGetCount(&num_devices);
    if (num_devices == 0) {
      fprintf(stderr,"VST_ensure_cuda_ready(): No CUDA devices.\n");
      return false;
    }
    if (cuDeviceGet(&g_cuDevice, num_devices-1) != CUDA_SUCCESS) {
      fprintf(stderr,"VST_ensure_cuda_ready(): Could not get device.\n");
      return false;
    }
    if (cuCtxCreate( &g_cuContext, 0, g_cuDevice ) != CUDA_SUCCESS) {
      fprintf(stderr,"VST_ensure_cuda_ready(): Could not get context.\n");
      return false;
    }
    
    // Allocate a buffer to be used on the GPU.  It will be
    // copied from host memory.
    if ( (inbuf.nx != g_cuda_fromhost_nx) || (inbuf.ny != g_cuda_fromhost_nx) ) {
	    
		unsigned int numBytes = inbuf.nx * inbuf.ny * sizeof(float);
		if (g_cuda_fromhost_buffer != NULL) {
			cudaFree(g_cuda_fromhost_buffer);
		}
		if (cudaMalloc((void**)&g_cuda_fromhost_buffer, numBytes) != cudaSuccess) {
		  fprintf(stderr,"VST_ensure_cuda_ready(): Could not allocate memory.\n");
		  return false;
		}
		if (g_cuda_fromhost_buffer == NULL) {
		  fprintf(stderr,"VST_ensure_cuda_ready(): Buffer is NULL pointer.\n");
		  return false;
		}
		g_cuda_fromhost_nx = inbuf.nx;
		g_cuda_fromhost_ny = inbuf.ny;
	}
	
    // Set up enough threads (and enough blocks of threads) to at least
    // cover the size of the array.  We use a thread block size of 16x16
    // because that's what the example matrixMul code from nVidia does.
    // Changing them to 8 and 8 makes the Gaussian kernel slower.  Changing
    // them to 32 and 32 also makes it slower (by not as much)
    g_threads.x = 16;
    g_threads.y = 16;
    g_threads.z = 1;
    g_grid.x = (g_cuda_fromhost_nx / g_threads.x) + 1;
    g_grid.y = (g_cuda_fromhost_ny / g_threads.y) + 1;
    g_grid.z = 1;	

    // Everything worked, so we're okay.
    okay = true;
  }

  // Return true if we are okay.
  return okay;
}

//----------------------------------------------------------------------
// Functions called from image_wrapper.cpp.
//----------------------------------------------------------------------

// Compute the value of a Gaussian at the specified point.  The function is 2D,
// centered at the origin.  The "standard normal distribution" Gaussian has an integrated
// volume of 1 over all space and a variance of 1.  It is defined as:
//               1           -(R^2)/2
//   G(x) = ------------ * e
//             2*PI
// where R is the radius of the sample point from the origin.
// We let the user set the standard deviation s, changing the function to:
//                  1           -(R^2)/(2*s^2)
//   G(x) = --------------- * e
//           s^2 * 2*PI
// For computational efficiency, we refactor this into A * e ^ (B * R^2).

inline __device__ float	cuda_Gaussian(
  const float &s_meters,      //< standard deviation (square root of variance)
  const float &x, const float &y)	//< Point to sample (relative to origin)
{
  float variance = s_meters * s_meters;
  float R_squared = x*x + y*y;

  const float twoPI = static_cast<float>(2*CUDART_PI_F);
  const float twoPIinv = 1.0f / twoPI;
  float B = -1 / (2 * variance);
  float A = twoPIinv / variance;

  return A * __expf(B * R_squared);
}

// CUDA kernel to do a Gaussian blur of the passed-in image and place
// it into the output images.
// Told the buffer beginning and the buffer size.  Assumes at least
// as many threads are run as there are elements in the buffer.
// Assumes a 2D array of threads.
static __global__ void VST_cuda_gaussian_blur(float *in, float *out, int nx, int ny,
							unsigned aperture, float std)
{
  int x = blockIdx.x * blockDim.x + threadIdx.x;
  int y = blockIdx.y * blockDim.y + threadIdx.y;
  if ( (x < nx) && (y < ny) ) {

	// Replacing the cuda_Gaussian() calls below with 1 changed the speed from
	// 23 frames/second to 25 frames/second, so it is not the bottleneck.
	// Replacing the in[] read call with 1 slows things down to 21.
	// Replacing the inside-if with (weight++; sum++) speeds things to 29.
	// Removing the if() test speeds things up to 45 (but gets funky answers).
	// Pulling the if() statement out of the inner loop and into the bounds
	// setting for the i and j loops made the speed 39.
	// Swapping the sum += and weight += lines below to put weight later brought
	// us up to 43.
	// Swapping the kval = and value = lines to put kval first brought it up
	// to 43.8.
	// Moving the kval and value definitions outside the loop dropped back to
	// 39.
	// Moving the definition of int j into the i loop bumped it up to 44.  Looks
	// like the compiler doesn't always do the best optimizing for us...
	// After the above mods, changing the cuda_Gaussian() to = 1.0f made things
	// go 53 frames/second, so there may be some computational gain to be had
	// in there.
	// XXX Switching the code below to one like the faster algorithm in the
	// CPU code may speed things up a bit more.
	// Changing the radius (affects the aperture) from 5 to 3 makes things
	// go 41 frames/second.
    // If we don't have an integer version of aperture, the "-aperture"
    // below turns into a large positive number, meaning that
    // the loops never get executed.
    int aperture_int = aperture;
    
    // Determine the safe bounds to read from around this point.  This avoids
    // having to put an if() statement in the inner loop, which slows us down
    // a bunch.
    int min_i = -aperture_int;
    int max_i = aperture_int;
    int min_j = -aperture_int;
    int max_j = aperture_int;
    int min_x = x - aperture_int; if (min_x < 0) { min_i = -min_x; }
    int min_y = y - aperture_int; if (min_y < 0) { min_j = -min_y; }
    int max_x = x + aperture_int; if (max_x >= nx) { max_i -= ( max_x - (nx-1) ); }
    int max_y = y + aperture_int; if (max_y >= ny) { max_j -= ( max_y - (ny-1) ); }
    int i;
    float sum = 0;
    float weight = 0;
    for (i = min_i; i <= max_i; i++) {
	  int j;
      for (j = min_j; j <= max_j; j++) {
          float kval = cuda_Gaussian(std,i,j);
		  float value = in[x+i + (y+j)*nx];
          sum += kval * value;
          weight += kval;
      }
    }
	out[x + y*nx] = sum/weight;
	
  }
}

bool VST_cuda_blur_image(VST_cuda_image_buffer &buf, unsigned aperture, float std)
{
	// Make sure we can initialize CUDA.  This also allocates the global
	// input buffer that we'll copy data from the host into.
	if (!VST_ensure_cuda_ready(buf)) { return false; }
	
	// Copy the input image from host memory into the GPU buffer.
	size_t size = buf.nx * buf.ny * sizeof(float);
	if (cudaMemcpy(g_cuda_fromhost_buffer, buf.buf, size, cudaMemcpyHostToDevice) != cudaSuccess) {
		fprintf(stderr, "VST_cuda_blur_image(): Could not copy memory to CUDA\n");
		return false;
	}
	
	// Allocate a CUDA buffer to blur into from the input buffer.  It should
	// be the same size as the input buffer.  We only allocate this when the
	// size changes.
	static int blur_nx = 0;
	static int blur_ny = 0;
	static float *blur_buf = NULL;
	if ( (blur_nx != g_cuda_fromhost_nx) || (blur_ny != g_cuda_fromhost_ny) ) {
		if (blur_buf != NULL) { cudaFree(blur_buf); }
		blur_nx = g_cuda_fromhost_nx;
		blur_ny = g_cuda_fromhost_ny;
		if (cudaMalloc((void**)&blur_buf, size) != cudaSuccess) {
		  fprintf(stderr,"VST_cuda_blur_image(): Could not allocate memory.\n");
		  return false;
		}
	}
	if (blur_buf == NULL) {
	  fprintf(stderr,"VST_cuda_blur_image(): Buffer is NULL pointer.\n");
	  return false;
	}
	
	// Call the CUDA kernel to do the blurring on the image, reading from
	// the global input buffer and writing to the blur buffer.
	// Synchronize the threads when
	// we are done so we know that they finish before we copy the memory
	// back.
	VST_cuda_gaussian_blur<<< g_grid, g_threads >>>(g_cuda_fromhost_buffer,
					blur_buf, blur_nx, blur_ny, aperture, std);
	if (cudaThreadSynchronize() != cudaSuccess) {
		fprintf(stderr, "VST_cuda_blur_image(): Could not synchronize threads\n");
		return false;
	}

	// Copy the buffer back from the GPU to host memory.
	if (cudaMemcpy(buf.buf, blur_buf, size, cudaMemcpyDeviceToHost) != cudaSuccess) {
		fprintf(stderr, "VST_cuda_blur_image(): Could not copy memory back to host\n");
		return false;
	}
	
	// Everything worked!
	return true;
}

//----------------------------------------------------------------------
// Functions called from spot_tracker.cpp.
//----------------------------------------------------------------------

typedef struct {
	float radius;
	float sample_separation;
	float pixel_accuracy;
	float x;
	float y;
	float fitness;
	float pixelstep;
} CUDA_Tracker_Info;

// Find the maximum of three elements.  Return the
// index of which one was picked.
inline __device__ float max3(const float &v0, const float &v1, const float &v2,
							 unsigned &index) {
  float max = v0; index = 0;
  if (v1 > max) { max = v1; index = 1; }
  if (v2 > max) { max = v2; index = 2; }
  return max;
}

// Read a pixel value from the specified image.
// Return false if the coordinate its outside the image.
// Return the correct result and true if the coordinate is inside.
inline __device__ bool cuda_read_pixel(
	const float *img, const int &nx, const int &ny,
	const int &x, const int &y,
	float &result)
{
	if ( (x < 0) || (y < 0) || (x >= nx) || (y >= ny) ) {
		return false;
	}
	result = img[x + y*nx];
	return true;
}

// Do bilinear interpolation to read from the image, in order to
// smoothly interpolate between pixel values.
// All sorts of speed tweaks in here because it is in the inner loop for
// the spot tracker and other codes.
// Return a result of zero and false if the coordinate its outside the image.
// Return the correct interpolated result and true if the coordinate is inside.
// XXX Later, consider using the hardware-accelerated bilerp function by using
// a texture to read from; beware that the interpolator will only do a maximum
// of 256 subpixel locations, which won't get us below a resolution of 1/256.
inline __device__ bool cuda_read_pixel_bilerp(
	const float *img, const int &nx, const int &ny,
	const float &x, const float &y,
	float &result)
{
	result = 0;	// In case of failure.
	// The order of the following statements is optimized for speed.
	// The float version is used below for xlowfrac comp, ixlow also used later.
	// Slightly faster to explicitly compute both here to keep the answer around.
	float xlow = floor(x); int ixlow = (int)xlow;
	// The float version is used below for ylowfrac comp, ixlow also used later
	// Slightly faster to explicitly compute both here to keep the answer around.
	float ylow = floor(y); int iylow = (int)ylow;
	int ixhigh = ixlow+1;
	int iyhigh = iylow+1;
	float xhighfrac = x - xlow;
	float yhighfrac = y - ylow;
	float xlowfrac = 1.0 - xhighfrac;
	float ylowfrac = 1.0 - yhighfrac;
	float ll, lh, hl, hh;

	// Combining the if statements into one using || makes it slightly slower.
	// Interleaving the result calculation with the returns makes it slower.
	if (!cuda_read_pixel(img, nx, ny, ixlow, iylow, ll)) { return false; }
	if (!cuda_read_pixel(img, nx, ny, ixlow, iyhigh, lh)) { return false; }
	if (!cuda_read_pixel(img, nx, ny, ixhigh, iylow, hl)) { return false; }
	if (!cuda_read_pixel(img, nx, ny, ixhigh, iyhigh, hh)) { return false; }
	result = ll * xlowfrac * ylowfrac + 
		 lh * xlowfrac * yhighfrac +
		 hl * xhighfrac * ylowfrac +
		 hh * xhighfrac * yhighfrac;
	return true;
};

// Check the fitness of the specified symmetric tracker within the
// specified image at the specified location.
// This code should compute exactly the same thing as the
// symmetric_spot_tracker_interp function.
// XXX Later, put the locations to search into constant memory rather
// than computing them on the fly here.
inline __device__ float	cuda_check_fitness_symmetric(
	const float *img, const int &nx, const int &ny,
	const CUDA_Tracker_Info &tkr)
{
	// Construct aliases for the parameters that give us easy local names.
	const float &radius = tkr.radius;
	const float &samplesep = tkr.sample_separation;
	const float &x = tkr.x;
	const float &y = tkr.y;
	
	// Sum up over rings that are samplesep away from the center; we
	// don't count the center pixel because it will never have variance.
	float r;
	float ring_variance_sum = 0;
	for (r = samplesep; r <= radius; r += samplesep) {
		int count = 0;
		float valSum = 0.0f;
		float squareValSum = 0.0f;
		float rads_per_step = 1.0f / r;
		float start = (r/samplesep)*rads_per_step*0.5f;
		float theta;
		for (theta = start; theta < 2*CUDART_PI_F + start; theta += rads_per_step) {
			float newx = x + r*cos(theta);
			float newy = y + r*sin(theta);
			float val;
			if (cuda_read_pixel_bilerp(img, nx, ny, newx, newy, val)) {
				squareValSum += val*val;
				valSum += val;
				count++;
			}
		}
	
		if (count) {
			ring_variance_sum += squareValSum - valSum*valSum / count;
		}
	}
	
	return -ring_variance_sum;
}

// Optimize starting at the specified location to find the best-fit disk.
// Take only one optimization step.  Return whether we ended up finding a
// better location or not.  Return new location in any case.  One step means
// one step in X,Y, and radius space each.  We always optimize in xy only.
inline __device__ bool cuda_take_single_optimization_step_xy(const float *img,
							const int &nx, const int &ny,
							CUDA_Tracker_Info &t)
{
  // Aliases to make it easier to refer to things.
  const float &pixelstep = t.pixelstep;
  float &fitness = t.fitness;
  float &x = t.x;
  float &y = t.y;
  
  float	  new_fitness;	    //< Checked fitness value to see if it is better than current one
  bool	  betterxy = false; //< Do we find a better location?

  // Try going in +/- X and see if we find a better location.  It is
  // important that we check both directions before deciding to step
  // to avoid unbiased estimations.
  {
    float v0, vplus, vminus;
    float starting_x = x;
    v0 = fitness;                                   // Value at starting location
    // XXX Need to do this differently when we run in parallel.  Also for Y.
    x = starting_x + pixelstep;
    vplus = cuda_check_fitness_symmetric(img, nx, ny, t);
    x = starting_x - pixelstep;
    vminus = cuda_check_fitness_symmetric(img, nx, ny, t);
    unsigned which;
    new_fitness = max3(v0, vplus, vminus, which);
    switch (which) {
      case 0: x = starting_x;
              break;
      case 1: x = starting_x + pixelstep;
              betterxy = true;
              break;
      case 2: x = starting_x - pixelstep;
              betterxy = true;
              break;
    }
    fitness = new_fitness;
  }
  
  // Try going in +/- Y and see if we find a better location.  It is
  // important that we check both directions before deciding to step
  // to avoid unbiased estimations.
  {
    float v0, vplus, vminus;
    float starting_y = y;
    v0 = fitness;                                      // Value at starting location
    y = starting_y + pixelstep;	// Try going a step in +Y
    vplus = cuda_check_fitness_symmetric(img, nx, ny, t);
    y = starting_y - pixelstep;	// Try going a step in +Y
    vminus = cuda_check_fitness_symmetric(img, nx, ny, t);
    unsigned which;
    new_fitness = max3(v0, vplus, vminus, which);
    switch (which) {
      case 0: y = starting_y;
              break;
      case 1: y = starting_y + pixelstep;
              betterxy = true;
              break;
      case 2: y = starting_y - pixelstep;
              betterxy = true;
              break;
    }
    fitness = new_fitness;
  }
  
  // Tell if we found a better location.
  return betterxy;
}

// CUDA kernel to optimize the passed-in list of trackers based on the
// passed-in image.  Moves the X and Y position of each tracker to its
// final optimum location.
// Assumes that we have at least as many threads in X as we have trackers.
// Assumes a 2D array of threads.
// XXX Later, at least use four threads for the four directional checks
// at each level to speed things up.
static __global__ void VST_cuda_symmetric_opt_kernel(const float *img, int nx, int ny,
							CUDA_Tracker_Info *tkrs, int nt)
{
  // For now, just do one thread per tracker and have it be the one with y=0.
  int tx = blockIdx.x * blockDim.x + threadIdx.x;
  int ty = blockIdx.y * blockDim.y + threadIdx.y;
  int tkr_id = tx;
  int group_id = ty;

  // Do the whole optimization here, checking with smaller and smaller
  // steps until we reach the accuracy requested.
  if ( (tkr_id < nt) && (group_id == 0) ) {
  
	// Get local aliases for the passed-in variables we need to use.    
	CUDA_Tracker_Info &t = tkrs[tkr_id];
	const float &accuracy = t.pixel_accuracy;
	float &fitness = t.fitness;	// Keeps track of current fitness
	float &pixelstep = t.pixelstep;
	pixelstep = 2.0f;	// Start with a large value.
	
	fitness = cuda_check_fitness_symmetric(img, nx, ny, t);
	do {
		while (cuda_take_single_optimization_step_xy(img, nx, ny, t)) {};
		if (pixelstep <= accuracy) {
			break;
		}
		pixelstep /= 2.0f;
	} while (true);
	
  }
}

// Optimize the passed-in list of symmetric XY trackers based on the
bool VST_cuda_optimize_symmetric_trackers(const VST_cuda_image_buffer &buf,
                                                 std::list<Spot_Information *> &tkrs,
                                                 unsigned num_to_optimize)
{
	// Make sure we can initialize CUDA.  This also allocates the global
	// input buffer that we'll copy data from the host into.
	if (!VST_ensure_cuda_ready(buf)) { return false; }
	
	// Copy the input image from host memory into the GPU buffer.
	size_t imgsize = buf.nx * buf.ny * sizeof(float);
	if (cudaMemcpy(g_cuda_fromhost_buffer, buf.buf, imgsize, cudaMemcpyHostToDevice) != cudaSuccess) {
		fprintf(stderr, "VST_cuda_optimize_symmetric_trackers(): Could not copy memory to CUDA\n");
		return false;
	}
	
	// Allocate an array of tracker information to pass down to the kernel.
	// with one entry per tracker we are optimizing.  This stores the tracking
	// parameters associated with each tracker along with its X and Y positions;
	// the kernel will replace the X and Y locations, which are then copied back
	// into the trackers.
	CUDA_Tracker_Info *ti = new CUDA_Tracker_Info[tkrs.size()];
	if (ti == NULL) {
		fprintf(stderr, "VST_cuda_optimize_symmetric_trackers(): Out of memory\n");
		return false;
	}
	int i;
	std::list<Spot_Information *>::iterator  loop;
	for (loop = tkrs.begin(), i = 0; i < (int)(num_to_optimize); loop++, i++) {
		spot_tracker_XY *t = (*loop)->xytracker();
		ti[i].radius = static_cast<float>(t->get_radius());
		ti[i].sample_separation = static_cast<float>(t->get_sample_separation());
		ti[i].pixel_accuracy = static_cast<float>(t->get_pixel_accuracy());
		ti[i].x = static_cast<float>(t->get_x());
		ti[i].y = static_cast<float>(t->get_y());
	}
	
	// Allocate a GPU buffer to store the tracker information.
	// Copy the tracker information from the host to GPU memory.
	CUDA_Tracker_Info *gpu_ti;
	size_t tkrsize = num_to_optimize * sizeof(CUDA_Tracker_Info);
	if (cudaMalloc((void**)&gpu_ti, tkrsize) != cudaSuccess) {
	  fprintf(stderr,"VST_cuda_optimize_symmetric_trackers(): Could not allocate memory.\n");
	  delete [] ti;
	  return false;
	}
	if (cudaMemcpy(gpu_ti, ti, tkrsize, cudaMemcpyHostToDevice) != cudaSuccess) {
		fprintf(stderr, "VST_cuda_optimize_symmetric_trackers(): Could not copy memory to CUDA\n");
		delete [] ti;
		return false;
	}
	
    // Set up enough threads (and enough blocks of threads) to at least
    // cover the number of trackers in X, and we have only one thread per
    // tracker (so y = 1).  We use a thread block size of 1x1
    // because we don't want to share a cache between different trackers.
	// XXX Later, consider how to make use of multiple threads in the
	// same tracker (doing 4+ offsets at once).
    g_threads.x = 1;
    g_threads.y = 1;
    g_threads.z = 1;
    g_grid.x = num_to_optimize;
    g_grid.y = 1;
    g_grid.z = 1;	
	
	// Call the CUDA kernel to do the tracking, reading from
	// the input buffer and editing the positions in place.
	// Synchronize the threads when we are done so we know that 
	// they finish before we copy the memory back.
	VST_cuda_symmetric_opt_kernel<<< g_grid, g_threads >>>(g_cuda_fromhost_buffer,
					buf.nx, buf.ny,
					gpu_ti, num_to_optimize);
	if (cudaThreadSynchronize() != cudaSuccess) {
		fprintf(stderr, "VST_cuda_blur_image(): Could not synchronize threads\n");
		return false;
	}

	// Copy the tracker info back from the GPU to the host memory.
	if (cudaMemcpy(ti, gpu_ti, tkrsize, cudaMemcpyDeviceToHost) != cudaSuccess) {
		fprintf(stderr, "VST_cuda_optimize_symmetric_trackers(): Could not copy memory back to host\n");
		cudaFree(gpu_ti);
		delete [] ti;
		return false;
	}
	
	// Copy the positions back into the trackers.
	for (loop = tkrs.begin(), i = 0; i < (int)(num_to_optimize); loop++, i++) {
		spot_tracker_XY *t = (*loop)->xytracker();
		t->set_location(ti[i].x, ti[i].y);
	}
		
	// Free the array of tracker info on the GPU and host sides.
	cudaFree(gpu_ti); gpu_ti = NULL;
	delete [] ti; ti = NULL;

	// Done!
	return true;
}
