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

const int MAX_LATTICE = 8;	// Maximum lattice size for tracker optimization

static CUdevice     g_cuDevice;     // CUDA device
static CUcontext    g_cuContext;    // CUDA context on the device

#define USE_TEXTURE
#ifdef USE_TEXTURE
static cudaArray	*g_cuda_fromhost_array = NULL;
static cudaChannelFormatDesc	g_channel_desc;
static texture<float,cudaTextureType2D,cudaReadModeElementType>	g_tex_ref;
#else
static float        *g_cuda_fromhost_buffer = NULL;
#endif

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
  static bool okay = false;		// Did the initialization work?
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
    if ( (inbuf.nx != g_cuda_fromhost_nx) || (inbuf.ny != g_cuda_fromhost_ny) ) {
	    
#ifdef	USE_TEXTURE
		if (g_cuda_fromhost_array != NULL) {
			cudaFreeArray(g_cuda_fromhost_array);
		}
		// 32-bit floating-point values in the first texture component only.
		g_channel_desc = cudaCreateChannelDesc(32,0,0,0,cudaChannelFormatKindFloat);
		if (cudaMallocArray(&g_cuda_fromhost_array, &g_channel_desc, inbuf.nx, inbuf.ny)  != cudaSuccess) {
		  fprintf(stderr,"VST_ensure_cuda_ready(): Could not allocate array.\n");
		  return false;
		}
		if (g_cuda_fromhost_array == NULL) {
		  fprintf(stderr,"VST_ensure_cuda_ready(): Array is NULL pointer.\n");
		  return false;
		}
#else
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
#endif
		g_cuda_fromhost_nx = inbuf.nx;
		g_cuda_fromhost_ny = inbuf.ny;
	}
	
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
#ifdef	USE_TEXTURE
static __global__ void VST_cuda_gaussian_blur(float *out, int nx, int ny,
							unsigned aperture, float std)
#else
static __global__ void VST_cuda_gaussian_blur(const float *in, float *out, int nx, int ny,
							unsigned aperture, float std)
#endif
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
    // Changing the radius (affects the aperture) from 5 to 3 makes things
    // go 59 frames/second.
    // Moving the definition of int j into the i loop bumped it up to 44.  Looks
    // like the compiler doesn't always do the best optimizing for us...
    // After the above mods, changing the cuda_Gaussian() to = 1.0f made things
    // go 53 frames/second, so there may be some computational gain to be had
    // in there.
    // Switching to texture reads kept us at 44 frames/second (same 44.5 as floats).
    // XXX Switching the code below to one like the faster algorithm in the
    // CPU code may speed things up a bit more.
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
    int min_x = x - aperture_int; if (min_x < 0) { min_i += -min_x; }
    int min_y = y - aperture_int; if (min_y < 0) { min_j += -min_y; }
    int max_x = x + aperture_int; if (max_x >= nx) { max_i -= ( max_x - (nx-1) ); }
    int max_y = y + aperture_int; if (max_y >= ny) { max_j -= ( max_y - (ny-1) ); }
    int i;
    float sum = 0;
    float weight = 0;
    for (i = min_i; i <= max_i; i++) {
      int j;
      for (j = min_j; j <= max_j; j++) {
          float kval = cuda_Gaussian(std,i,j);
#ifdef	USE_TEXTURE
	  // Because array indices are not normalized to [0,1], we need to
	  // add 0.5f to each coordinate (quirk inherited from graphics).
	  float value = tex2D( g_tex_ref, x+i+0.5f, y+j+0.5f  );
#else
	  float value = in[x+i + (y+j)*nx];
#endif
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
#ifdef USE_TEXTURE
	if (cudaMemcpyToArray(g_cuda_fromhost_array, 0, 0, buf.buf, size, cudaMemcpyHostToDevice) != cudaSuccess) {
		fprintf(stderr, "VST_cuda_blur_image(): Could not copy array to CUDA\n");
		return false;
	}

	// Bind the texture reference to the texture after describing it.
	g_tex_ref.addressMode[0] = cudaAddressModeClamp;
	g_tex_ref.addressMode[1] = cudaAddressModeClamp;
	g_tex_ref.filterMode = cudaFilterModePoint;
	g_tex_ref.normalized = false;
	cudaBindTextureToArray(g_tex_ref, g_cuda_fromhost_array, g_channel_desc);
#else
	if (cudaMemcpy(g_cuda_fromhost_buffer, buf.buf, size, cudaMemcpyHostToDevice) != cudaSuccess) {
		fprintf(stderr, "VST_cuda_blur_image(): Could not copy memory to CUDA\n");
		return false;
	}
#endif

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

	// Set up enough threads (and enough blocks of threads) to at least
	// cover the size of the array.  We use a thread block size of 16x16
	// because that's what the example matrixMul code from nVidia does.
	// Changing them to 8 and 8 makes the Gaussian kernel slower.  Changing
	// them to 32 and 32 also makes blurring slower (by not as much)
	g_threads.x = 16;
	g_threads.y = 16;
	g_threads.z = 1;
	g_grid.x = ((g_cuda_fromhost_nx-1) / g_threads.x) + 1;
	g_grid.y = ((g_cuda_fromhost_ny-1) / g_threads.y) + 1;
	g_grid.z = 1;	

	// Call the CUDA kernel to do the blurring on the image, reading from
	// the global input buffer and writing to the blur buffer.
	// Synchronize the threads when
	// we are done so we know that they finish before we copy the memory
	// back.
#ifdef USE_TEXTURE
	VST_cuda_gaussian_blur<<< g_grid, g_threads >>>(blur_buf, blur_nx, blur_ny, aperture, std);
#else
	VST_cuda_gaussian_blur<<< g_grid, g_threads >>>(g_cuda_fromhost_buffer,
					blur_buf, blur_nx, blur_ny, aperture, std);
#endif
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
	bool  lost;
} CUDA_Tracker_Info;

// Find the maximum of three elements.  Return the
// index of which one was picked.
inline __device__ float max3(const float &v0, const float &v1, const float &v2,
							 unsigned &index)
{
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
// Use the hardware-accelerated bilerp function by using
// a texture to read from; beware that the interpolator will only do a maximum
// of 512 subpixel locations, which won't get us below a resolution of 1/256.

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
#ifdef	USE_TEXTURE
inline __device__ float	cuda_check_fitness_symmetric(
	const cudaArray *img, const int &nx, const int &ny,
	const CUDA_Tracker_Info &tkr)
#else
inline __device__ float	cuda_check_fitness_symmetric(
	const float *img, const int &nx, const int &ny,
	const CUDA_Tracker_Info &tkr)
#endif
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
		float count = 0.000001f;	// Avoids need for divide-by-zero check
		float valSum = 0.0f;
		float squareValSum = 0.0f;
		float rads_per_step = samplesep / r;
		float start = (r/samplesep)*rads_per_step*0.5f;
		float theta;
		// We use the fact that sin(theta+PI) = -sin(theta) and
		// cos(theta+PI) = -cos(theta) to only have to calculate the
		// sin and cosine half as many times -- we go halfway around the
		// circle and use two points per step on opposite sides.
		for (theta = start; theta < CUDART_PI_F + start; theta += rads_per_step) {
			float sintheta, costheta;
			sincos(theta, &sintheta, &costheta);

			// Do the point on the top half of the circle.
			float newx = x + r*costheta;
			float newy = y + r*sintheta;
			float val;
#ifdef	USE_TEXTURE
			// Because array indices are not normalized to [0,1], we need to
			// add 0.5f to each coordinate (quirk inherited from graphics).
			// This is doing bilinear interpolation because the g_tex_ref
			// we're using was set up to do this before our kernel was called.
			// Texture read will clamp if out of bounds.
			// This order of operations is faster than only setting val
			// once we know that we are in bounds.
			val = tex2D( g_tex_ref, newx+0.5f, newy+0.5f );
			if ( (newx >= 0) && (newy >= 0) && (newx < nx) && (newy < ny) ) {
#else
			if (cuda_read_pixel_bilerp(img, nx, ny, newx, newy, val)) {
#endif
				// Reordering these three lines makes no speed difference.
				count++;
				valSum += val;
				squareValSum += val*val;
			}

			// Do the point on the bottom half of the circle.
			newx = x - r*costheta;
			newy = y - r*sintheta;
#ifdef	USE_TEXTURE
			// Because array indices are not normalized to [0,1], we need to
			// add 0.5f to each coordinate (quirk inherited from graphics).
			// This is doing bilinear interpolation because the g_tex_ref
			// we're using was set up to do this before our kernel was called.
			// Texture read will clamp if out of bounds.
			// This order of operations is faster than only setting val
			// once we know that we are in bounds.
			val = tex2D( g_tex_ref, newx+0.5f, newy+0.5f );
			if ( (newx >= 0) && (newy >= 0) && (newx < nx) && (newy < ny) ) {
#else
			if (cuda_read_pixel_bilerp(img, nx, ny, newx, newy, val)) {
#endif
				// Reordering these three lines makes no speed difference.
				count++;
				valSum += val;
				squareValSum += val*val;
			}
		}
	
		ring_variance_sum += squareValSum - valSum*valSum / count;
	}
	
	return -ring_variance_sum;
}

// CUDA kernel to optimize the passed-in list of trackers based on the
// passed-in image.  Moves the X and Y position of each tracker to its
// final optimum location.
// Assumes a 2D array of threads.
// Assumes that we have EXACTLY as many blocks in X as we have trackers.
// Assumes a lattice of threads in X and Y that is square.
#ifdef	USE_TEXTURE
static __global__ void VST_cuda_symmetric_opt_kernel(const cudaArray *img, int nx, int ny,
							CUDA_Tracker_Info *tkrs, int nt)
#else
static __global__ void VST_cuda_symmetric_opt_kernel(const float *img, int nx, int ny,
							CUDA_Tracker_Info *tkrs, int nt)
#endif
{
  // All of the threads within one block access the same tracker.
  // There is only one block in Y; there are as many blocks in X as there
  // are trackers.  We ensure that the block is square and we sample on
  // a lattice with that many points on it.
  int tkr_id = blockIdx.x;
  int my_x = threadIdx.x;
  int my_y = threadIdx.y;
  int lattice = blockDim.x;
  if (blockDim.y != blockDim.x) { return; }
  if (lattice > MAX_LATTICE) { return; }

  // This is shared memory among the threads in a block that stores the
  // position offset for each group member and the fitness that was
  // found by that group member at its position.  We fill in the offsets
  // here, each member doing its own; they make a lattice from (-,-) to
  // (+,+) where the corners are normalized from -1 to 1, and are later
  // scaled by the current step size so that the last one is at the
  // step location.  Need to subtract 1.0f rather than 1 from lattice to
  // make sure we're using floating-point division.
  // This storage must be as large as the lattice.
  __shared__ float	dx[MAX_LATTICE][MAX_LATTICE], dy[MAX_LATTICE][MAX_LATTICE];
  __shared__ float	fitnesses[MAX_LATTICE][MAX_LATTICE];
  __shared__ float	pixelstep;
  __shared__ bool	done;

  dx[my_x][my_y] = (2.0f * my_x/(lattice - 1.0f) - 1.0f);
  dy[my_x][my_y] = (2.0f * my_y/(lattice - 1.0f) - 1.0f);

  // Synchronize all of the threads in the block.
  __syncthreads();

  // Do the whole optimization here, checking with smaller and smaller
  // steps until we reach the accuracy requested.
	// Get local aliases for the passed-in variables we need to use.    
	CUDA_Tracker_Info &t = tkrs[tkr_id];
	const float &accuracy = t.pixel_accuracy;

	// Find out the initial fitness value at the present location
	// for the tracker and set done to false if I'm the first thread
	// in the block.  Also set the pixel step size to its initial
	// value of 2.0.
	if ( (my_x == 0) && (my_y == 0) ) {
		pixelstep = 2.0f;	// Start with a large value.
		t.fitness = cuda_check_fitness_symmetric(img, nx, ny, t);
		done = false;
	}

	// Synchronize all of the threads in the block.
	__syncthreads();

	// Make my own tracker with its information copied from the original
	// passed-in tracker I'm associated with.
	CUDA_Tracker_Info member_t = t;

	// Synchronize all of the threads in the block.
	__syncthreads();

	// Compute the fitness for all offsets and then figure out which
	// is the best.  If the best location is not found on the outside
	// of the lattice, then we go ahead and divide by the lattice size
	// before doing the next step.
	do {	
		// Check all offsets and compute their fitness.
		member_t.x = t.x + dx[my_x][my_y] * pixelstep;
		member_t.y = t.y + dy[my_x][my_y] * pixelstep;
		fitnesses[my_x][my_y] = cuda_check_fitness_symmetric(img, nx, ny, member_t);
		
		// Synchronize all of the threads in the block.
		__syncthreads();

		// In the first thread, find the highest fitness and its index.
		// Compare this with the original fitness.  If one is better, set
		// its value as our next start and try again.  If none are better,
		// divide the pixelstep and try again until we have a step that is
		// below our requested accuracy.  If we found a better one at the
		// edge, then we repeat our test with the same step size.
		// XXX As we have larger numbers of threads, maybe do a faster
		// parallel reduction on these values.
		if ( (my_x == 0) && (my_y == 0) ) {
			int x,y;
			int best_x = -1;
			int best_y = -1;
			float best_fitness = t.fitness;
			for (x = 0; x < lattice; x++) {
			  for (y = 0; y < lattice; y++) {
				if (fitnesses[x][y] > best_fitness) {
					best_x = x;
					best_y = y;
					best_fitness = fitnesses[x][y];
				}
			  }
			}
			// If we found a better place, move there.
			if (best_x >= 0) {
				t.x += dx[best_x][best_y] * pixelstep;
				t.y += dy[best_x][best_y] * pixelstep;
				t.fitness = best_fitness;
			}

			// If we didn't find a better place at the edge of the
			// lattice (including not finding one at all), then go ahead
			// and divide the pixel step by the lattice size, so we
			// confine our search around the best point on this lattice
			// the next time around.
			if ( (best_x != 0) && (best_x != (lattice-1)) &&
				 (best_y != 0) && (best_y != (lattice-1)) ) {

				// If our just-checked size (including the lattice) is
				// below the required accuracy, we're done.
				// XXX We need to be careful here -- the interval size
				// is not always the lattice -- it is for 2 but not for
				// higher ones (there are N-1 gaps, not N).  This code will
				// fail for a 2x2 lattice, but works for the others.
				if ( (pixelstep / (lattice-1)) <= (accuracy/2.0f) ) {
					done = true;
				}
				
				// Try a smaller step size and see if that finds a better
				// solution.
				pixelstep /= (lattice-1);			
			}

		}

		// Synchronize all of the threads in the block.
		__syncthreads();

	} while (!done);	  
}

// Optimize the passed-in list of symmetric XY trackers based on the
// image buffer passed in.
bool VST_cuda_optimize_symmetric_trackers(const VST_cuda_image_buffer &buf,
                                                 std::list<Spot_Information *> &tkrs,
                                                 unsigned num_to_optimize)
{
	// Make sure we can initialize CUDA.  This also allocates the global
	// input buffer that we'll use to copy data from the host into.
	if (!VST_ensure_cuda_ready(buf)) { return false; }
	
	// Copy the input image from host memory into the GPU buffer.
	size_t imgsize = buf.nx * buf.ny * sizeof(float);
#ifdef USE_TEXTURE
	if (cudaMemcpyToArray(g_cuda_fromhost_array, 0, 0, buf.buf, imgsize, cudaMemcpyHostToDevice) != cudaSuccess) {
		fprintf(stderr, "VST_cuda_optimize_symmetric_trackers(): Could not copy array to CUDA\n");
		return false;
	}

	// Bind the texture reference to the texture after describing it.
	g_tex_ref.addressMode[0] = cudaAddressModeClamp;
	g_tex_ref.addressMode[1] = cudaAddressModeClamp;

	// Do linear interpolation on the result.  NOTE: The precision of the interpolator
	// is only 9 bits, so there are only 512 possible locations at which to sample
	// within a pixel in each axis; if we want better accuracy than 1/256, we won't
	// be happy with this result.
	g_tex_ref.filterMode = cudaFilterModeLinear;
	g_tex_ref.normalized = false;
	cudaBindTextureToArray(g_tex_ref, g_cuda_fromhost_array, g_channel_desc);
#else
	if (cudaMemcpy(g_cuda_fromhost_buffer, buf.buf, imgsize, cudaMemcpyHostToDevice) != cudaSuccess) {
		fprintf(stderr, "VST_cuda_optimize_symmetric_trackers(): Could not copy memory to CUDA\n");
		return false;
	}
#endif
	
	// Allocate an array of tracker information to pass down to the kernel.
	// with one entry per tracker we are optimizing.  This stores the tracking
	// parameters associated with each tracker along with its X and Y positions;
	// the kernel will replace the X and Y locations, which are then copied back
	// into the trackers.
	if (num_to_optimize > tkrs.size()) {
		fprintf(stderr, "VST_cuda_optimize_symmetric_trackers(): Not enough tracker for request\n");
		return false;
	}
	CUDA_Tracker_Info *ti = new CUDA_Tracker_Info[tkrs.size()];
	if (ti == NULL) {
		fprintf(stderr, "VST_cuda_optimize_symmetric_trackers(): Out of memory\n");
		return false;
	}
	int i;
	std::list<Spot_Information *>::iterator  loop;
	for (loop = tkrs.begin(), i = 0; i < (int)(num_to_optimize); loop++, i++) {
		const spot_tracker_XY *t = (*loop)->xytracker();
		ti[i].radius = static_cast<float>(t->get_radius());
		ti[i].sample_separation = static_cast<float>(t->get_sample_separation());
		ti[i].pixel_accuracy = static_cast<float>(t->get_pixel_accuracy());
		ti[i].x = static_cast<float>(t->get_x());
		ti[i].y = static_cast<float>(t->get_y());
		ti[i].lost = (*loop)->lost();
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

	// We have EXACTLY as many blocks in X as we have trackers.  We have one block
	// in Y.  We have a lattice of threads within a block that should be the
	// same number in X and Y; we test on points on that lattice and then
	// divide by that size to set a smaller lattice to check.
	g_threads.x = 8;
	g_threads.y = 8;
	g_threads.z = 1;
	g_grid.x = num_to_optimize;
	g_grid.y = 1;

	// Make sure we're not asking for too many threads, or a non-square lattice.
	if ( (g_threads.x > MAX_LATTICE) || (g_threads.y > MAX_LATTICE) ) {
		fprintf(stderr, "VST_cuda_optimize_symmetric_trackers(): Lattice to large\n");
		return false;
	}
	if ( g_threads.x != g_threads.y ) {
		fprintf(stderr, "VST_cuda_optimize_symmetric_trackers(): Lattice not square\n");
		return false;
	}

	// Call the CUDA kernel to do the tracking, reading from
	// the input buffer and editing the positions in place.
	// Synchronize the threads when we are done so we know that 
	// they finish before we copy the memory back.
#ifdef	USE_TEXTURE
	VST_cuda_symmetric_opt_kernel<<< g_grid, g_threads >>>(g_cuda_fromhost_array,
					buf.nx, buf.ny,
					gpu_ti, num_to_optimize);
#else
	VST_cuda_symmetric_opt_kernel<<< g_grid, g_threads >>>(g_cuda_fromhost_buffer,
					buf.nx, buf.ny,
					gpu_ti, num_to_optimize);
#endif
	if (cudaThreadSynchronize() != cudaSuccess) {
		fprintf(stderr, "VST_cuda_optimize_symmetric_trackers(): Could not synchronize threads\n");
		return false;
	}

	// Copy the tracker info back from the GPU to the host memory.
	if (cudaMemcpy(ti, gpu_ti, tkrsize, cudaMemcpyDeviceToHost) != cudaSuccess) {
		fprintf(stderr, "VST_cuda_optimize_symmetric_trackers(): Could not copy memory back to host\n");
		cudaFree(gpu_ti);
		delete [] ti;
		return false;
	}
	
	// Copy the positions and fitnesses back into the trackers.
	for (loop = tkrs.begin(), i = 0; i < (int)(num_to_optimize); loop++, i++) {
		spot_tracker_XY *t = (*loop)->xytracker();
		t->set_location(ti[i].x, ti[i].y);
		t->set_fitness(ti[i].fitness);
	}
		
	// Free the array of tracker info on the GPU and host sides.
	cudaFree(gpu_ti); gpu_ti = NULL;
	delete [] ti; ti = NULL;

	// Done!
	return true;
}

// CUDA kernel to check the fitness values in rings around the passed-in
// set of trackers, filling in the values around the ring.
// We have 1 thread in X and 512 threads in Y.
// We have as many blocks in X as we have trackers.
// We have as many blocks in Y as we have points to sample around
// the maximum-radius tracker location divided by 512 (the number of
// threads).

#ifdef	USE_TEXTURE
static __global__ void VST_cuda_symmetric_bright_lost_kernel(const cudaArray *img, int nx, int ny,
							CUDA_Tracker_Info *tkrs, int nt,
							float *fitness, unsigned num_radii)
#else
static __global__ void VST_cuda_symmetric_bright_lost_kernel(const float *img, int nx, int ny,
							CUDA_Tracker_Info *tkrs, int nt,
							float *fitness, unsigned num_radii)
#endif
{
  // Figure out which tracker and which spot within that tracker we're doing.
  int tkr_id = blockIdx.x;
  int spot_id = blockIdx.y * blockDim.y + threadIdx.y;
  
  // Figure out our offset from our tracker base, and where we should store our
  // result in the fitness array.  This calculation is redundant with that done
  // on the host, but it seems likely to be faster to recompute it here than to
  // try and fill in all the info we need and pass it down.
  unsigned r;					// Goes through the indices of radii
  unsigned point_count;			// How many points found for this radius
  unsigned total_count = 0;		// How many total points found so far
  float my_radius = -1;			// Pixel distance from tracker center
  int my_theta_index = -1;		// Index of my location around the circumference
  for (r = 0; r < num_radii; r++) {
	float radius = 2*r + 3;		// Actual radius we're checking.
	point_count = static_cast<unsigned>( 2 * CUDART_PI_F * radius );
	if (total_count + point_count > spot_id) {
		my_radius = r;
		my_theta_index = spot_id - total_count;
	}
	total_count += point_count;
  }
  // Figure out where I should put my answer.
  unsigned my_fitness_index = tkr_id * total_count + spot_id;
  
  // If we're a valid tracker and a valid spot_id, then compute the fitness for
  // our tracker at the specified offset and store it into the fitness array.
  if ( (tkr_id < nt) && (my_radius > 0) ) {
	// Make my own tracker with its information copied from the original
	// passed-in tracker I'm associated with.
	CUDA_Tracker_Info member_t = tkrs[tkr_id];

	// Compute the offset from the original location, then set it to that
	// offset.
	float theta = (1.0f/my_radius) * my_theta_index;
	float x = member_t.x + my_radius * cos(theta);
	float y = member_t.y + my_radius * sin(theta);
	member_t.x = x;
	member_t.y = y;
	
	fitness[my_fitness_index] = cuda_check_fitness_symmetric(img, nx, ny, member_t); 
  }
}

// Optimize the passed-in list of symmetric XY trackers based on the
// image buffer passed in.
bool VST_cuda_check_bright_lost_symmetric_trackers(const VST_cuda_image_buffer &buf,
                                                 std::list<Spot_Information *> &tkrs,
                                                 unsigned num_to_optimize,
                                                 float var_thresh)
{
	// Make sure we can initialize CUDA.  This also allocates the global
	// input buffer that we'll use to copy data from the host into.
	if (!VST_ensure_cuda_ready(buf)) { return false; }
	
	// Copy the input image from host memory into the GPU buffer.
	size_t imgsize = buf.nx * buf.ny * sizeof(float);
#ifdef USE_TEXTURE
	if (cudaMemcpyToArray(g_cuda_fromhost_array, 0, 0, buf.buf, imgsize, cudaMemcpyHostToDevice) != cudaSuccess) {
		fprintf(stderr, "VST_cuda_check_bright_lost_symmetric_trackers(): Could not copy array to CUDA\n");
		return false;
	}

	// Bind the texture reference to the texture after describing it.
	g_tex_ref.addressMode[0] = cudaAddressModeClamp;
	g_tex_ref.addressMode[1] = cudaAddressModeClamp;

	// Do linear interpolation on the result.  NOTE: The precision of the interpolator
	// is only 9 bits, so there are only 512 possible locations at which to sample
	// within a pixel in each axis; if we want better accuracy than 1/256, we won't
	// be happy with this result.
	g_tex_ref.filterMode = cudaFilterModeLinear;
	g_tex_ref.normalized = false;
	cudaBindTextureToArray(g_tex_ref, g_cuda_fromhost_array, g_channel_desc);
#else
	if (cudaMemcpy(g_cuda_fromhost_buffer, buf.buf, imgsize, cudaMemcpyHostToDevice) != cudaSuccess) {
		fprintf(stderr, "VST_cuda_check_bright_lost_symmetric_trackers(): Could not copy memory to CUDA\n");
		return false;
	}
#endif
	
	// Allocate an array of tracker information to pass down to the kernel.
	// with one entry per tracker we are checking.  This stores the tracking
	// parameters associated with each tracker along with its X and Y positions;
	// the kernel will replace the 'lost' value, which are then copied back
	// into the trackers.
	CUDA_Tracker_Info *ti = new CUDA_Tracker_Info[tkrs.size()];
	if (ti == NULL) {
		fprintf(stderr, "VST_cuda_check_bright_lost_symmetric_trackers(): Out of memory\n");
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
		ti[i].lost = (*loop)->lost();
	}
	
	// Allocate a GPU buffer to store the tracker information.
	// Copy the tracker information from the host to GPU memory.
	CUDA_Tracker_Info *gpu_ti;
	size_t tkrsize = num_to_optimize * sizeof(CUDA_Tracker_Info);
	if (cudaMalloc((void**)&gpu_ti, tkrsize) != cudaSuccess) {
	  fprintf(stderr,"VST_cuda_check_bright_lost_symmetric_trackers(): Could not allocate memory.\n");
	  delete [] ti;
	  return false;
	}
	if (cudaMemcpy(gpu_ti, ti, tkrsize, cudaMemcpyHostToDevice) != cudaSuccess) {
		fprintf(stderr, "VST_cuda_check_bright_lost_symmetric_trackers(): Could not copy memory to CUDA\n");
		delete [] ti;
		return false;
	}
	
	// Figure out how many samples we need to take in the worst case for
	// any tracker in our set.  This depends on the maximum radius among
	// the trackers -- we take a sample every pixel around rings, and the
	// rings go in steps of 2 pixels out from the center, starting at
	// a radius of 3 pixels.  Also figure out the start index into an
	// array of values for each ring.
	double max_radius = 0;
	for (loop = tkrs.begin(), i = 0; i < (int)(num_to_optimize); loop++, i++) {
		double radius = ti[i].radius;
		if (radius > max_radius) {
			max_radius = radius;
		}
	}
	int num_radii = static_cast<int>( (max_radius - 3)/2 + 1 );
	unsigned *point_counts = new unsigned[num_radii];
	unsigned *start_index = new unsigned[num_radii];
	if (!point_counts || !start_index) {
	  fprintf(stderr,"VST_cuda_check_bright_lost_symmetric_trackers(): Could not allocate index memory.\n");
	  delete [] ti;
	  return false;
	}
	unsigned total_points = 0;
	for (i = 0; i < num_radii; i++) {
		double radius = 2*i + 3;
		point_counts[i] = static_cast<unsigned>( 2 * M_PI * radius );
		total_points += point_counts[i];
		start_index[i] = total_points - point_counts[i];
	}
	
	// Allocate the array of fitness values with as many points per
	// tracker as there are in the maximum tracker.  We don't fill
	// these in here -- they will be filled in on the GPU.
	float *gpu_fitness;
	size_t fitness_size = num_to_optimize * total_points * sizeof(float);
	if (cudaMalloc((void**)&gpu_fitness, fitness_size) != cudaSuccess) {
	  fprintf(stderr,"VST_cuda_check_bright_lost_symmetric_trackers(): Could not allocate fitness memory.\n");
	  delete [] start_index;
	  delete [] point_counts;
	  delete [] ti;
	  return false;
	}
	
    // We have 1 thread in X and 512 threads in Y.
    // We have as many blocks in X as we have trackers.
    // We have as many blocks in Y as we have points to sample around
    // the maximum-radius tracker location divided by 512 (the number of
    // threads).
    g_threads.x = 1;
    g_threads.y = 512;
    g_threads.z = 1;
    g_grid.x = num_to_optimize;
    g_grid.y = (total_points / 512) + 1;
	
	// Call the CUDA kernel to compute the fitness values at.
	// the proper locations around the start for each tracker.
	// Synchronize the threads when we are done so we know that 
	// they finish before we copy the memory back.
#ifdef	USE_TEXTURE
	VST_cuda_symmetric_bright_lost_kernel<<< g_grid, g_threads >>>(g_cuda_fromhost_array,
					buf.nx, buf.ny,
					gpu_ti, num_to_optimize,
					gpu_fitness, num_radii);
#else
	VST_cuda_symmetric_bright_lost_kernel<<< g_grid, g_threads >>>(g_cuda_fromhost_buffer,
					buf.nx, buf.ny,
					gpu_ti, num_to_optimize,
					gpu_fitness, num_radii);
#endif
	if (cudaThreadSynchronize() != cudaSuccess) {
		fprintf(stderr, "VST_cuda_check_bright_lost_symmetric_trackers(): Could not synchronize threads\n");
		return false;
	}

	// Allocate a buffer to store the host-side fitness values.
	// Copy the fitness info back from the GPU to the host memory.
	float *fitness = new float[fitness_size / sizeof(float)];
	if (!fitness) {
		fprintf(stderr, "VST_cuda_check_bright_lost_symmetric_trackers(): Could not allocate host fitness memory\n");
		cudaFree(gpu_fitness);
		cudaFree(gpu_ti);
		delete [] start_index;
		delete [] point_counts;
		delete [] ti;
		return false;
	}
	if (cudaMemcpy(fitness, gpu_fitness, fitness_size, cudaMemcpyDeviceToHost) != cudaSuccess) {
		fprintf(stderr, "VST_cuda_check_bright_lost_symmetric_trackers(): Could not copy memory back to host\n");
		cudaFree(gpu_fitness);
		cudaFree(gpu_ti);
		delete [] fitness;
		delete [] start_index;
		delete [] point_counts;
		delete [] ti;
		return false;
	}
	
	// Determine whether each tracker is lost by looking at the minimum of
	// the maxima around the rings and comparing it to the lost-tracking
	// parameter.
	for (loop = tkrs.begin(), i = 0; i < (int)(num_to_optimize); loop++, i++) {
		float min_val = 1e20f;
		int r;
		num_radii = static_cast<int>( ((*loop)->xytracker()->get_radius() - 3)/2 + 1 );
		for (r = 0; r < num_radii; r++) {
			float max_val = -1e20f;
			unsigned j;
			for (j = 0; j < point_counts[r]; j++) {
				float val = fitness[ i*total_points + start_index[r] + j ];
				if (val > max_val) {
					max_val = val;
				}
			}
			if (max_val < min_val) {
				min_val = max_val;
			}
		}
		double scale_factor = 1 + 9*var_thresh;
		bool am_lost = false;
        if ((*loop)->xytracker()->get_fitness() * scale_factor < min_val) {
			am_lost = true;
		}
		(*loop)->lost(am_lost);
	}
		
	// Free the array of tracker info on the GPU and host sides.
	cudaFree(gpu_fitness); gpu_fitness = NULL;
	cudaFree(gpu_ti); gpu_ti = NULL;
	delete [] fitness;
	delete [] start_index;
	delete [] point_counts;
	delete [] ti; ti = NULL;

	// Done!
	return true;
}


//----------------------------------------------------------------------
// Notes on speedup attempts for tracking are below here
//----------------------------------------------------------------------

/*
Speed with CPU on two 20-radius trackers with precision 0.05 and spacing 0.5: 55 fps.
Speed with initial GPU implementation: 3.8 fps.
Changing the radius to 10 makes it12.7-13.2 fps.
Changing the sample separation from 0.5 to 1 for radius 20 makes it 7.1 fps.
Changing the sample separation from 0.5 to 0.1 for radius 20 makes it 0.8 fps.
Changing the precision from 0.05 to 0.5 makes it 8.1 fps.

Switching to using the bilinear interpolation texture hardware makes it around 7 fps.
	(turning off if() checks within bilerp only increased to 8.6 fps)
	(calling synchthreads has no impact -- only one thread per block!)
Running 32 threads in parallel per tracker in a fan pattern around the origin got 29 fps.
	This is 60 fps for a 10-pixel tracker.  And it scales up to lots of trackers.
	Changing sample spacing to 1.0 gets us up to 43 fps.
Running a lattice of 32x32 points around the current location got us to 24 fps.
Running a lattice of 24x24 points around the current location got us to 35 fps.
Running a lattice of 16x16 points around the current location got us to 39 fps.
	Slowed down to 36 with 8 trackers.
Running a lattice of 12x12 points around the current location got us to 39 fps.
Running a lattice of 8x8 points around the current location got us 38 fps.
Running a lattice of 6x6 points around the current location got us to 32 fps.
Running a lattice of 4x4 points around the current location got us to 33 fps.
	Stayed the same speed for 8 trackers.
Running a lattice of 2x2 points around the current location got us to 21 fps.
	Just checking the scaling, didn't expect speedup.

Recomputing the "are we done" sooner at a lattice of 4x4 points gets us to 38

Switching from cos() and sin() to sincos() gets us to 44 fps.

Switching the count from int to float and re-ordering the texure read in
	cuda_read_pixel_bilerp gets us up to 50 fps.
	
Pulling the bilerp texture call into the code from inline gets us to 52 fps.

*** Bug found.  The pixel separation around the circle on the symmetric
    tracker was being computed incorrectly.  This resulted in too few
    points being sampled.  This was in both the serial and the CUDA code.
    Timings below are from the new code.
    
Serial code ran at 34.5 fps.
CUDA code ran at pretty much the exact same speed.  It slowed down a little with
		9 beads.
Switching to only computing half the sin() and cos() and re-using them got 43
After fixing some bugs and passing information back, it got to 49
	This is surprising -- it should have gone slower.
	
Ideas:
	Precompute the kernel offsets and store them in shared memory
			Taking out the sin() and cos() from the inner loop speeds up to 60
	Remove if statements in inner loop (bilerp) (got a little less than 2x).
		Texture version is set to clamp, but this is not quite what we want.
		Pad image with -1 around border and use that to squash?
*/
