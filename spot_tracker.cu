/*********************************************************************
This is the source file for the CUDA version of the spot_tracker library
function calls..

WARNING: All of the CUDA code for the entire project has to be in here
so that we only initialize the device once.
**********************************************************************/

#include "image_wrapper.h"
#include <stdio.h>
#include <cuda.h>
#include <math_constants.h>

//----------------------------------------------------------------------
// Definitions and routines needed by all functions below.
//----------------------------------------------------------------------

static CUdevice     g_cuDevice;     // CUDA device
static CUcontext    g_cuContext;    // CUDA context on the device

// Open the CUDA device and get a context.  Return false
// if we cannot get one.  This function can be called every time a
// CUDA_using function is called, but it only does the device opening
// and image-buffer allocation once.
static bool VST_ensure_cuda_ready(void)
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
    
    // Everything worked, so we're okay.
    okay = true;
  }

  // Return true if we are okay.
  return okay;
}

//----------------------------------------------------------------------
// Functions called from image_wrapper.cpp.
//----------------------------------------------------------------------

bool VST_cuda_blur_image(VST_cuda_image_buffer &buf, unsigned aperture, float std)
{
// XXX until we fix this to work
return false;

	// Make sure we can initialize CUDA.
	if (!VST_ensure_cuda_ready()) { return false; }
	
	// Allocate a CUDA buffer on the card to store the image.  Copy the
	// image to the buffer.
	// XXX;
	
	// Call the CUDA kernel to do the blurring on the image.
	// XXX
	
	// Copy the buffer back from the card.
	// XXX
	
	// Free the CUDA buffer that was allocated to copy to and from the
	// card.
	// XXX
	
	// Everything worked!
	return true;
}


//----------------------------------------------------------------------
// XXX Constants and stuff from Panoptes Simulator.
//----------------------------------------------------------------------

const unsigned PSCS_cols = 648;   // Image size for the camera.
const unsigned PSCS_rows = 488;
typedef float PSCS_buffer[PSCS_cols][PSCS_rows];

// Holds the information needed to render one bead.  This is a struct
// that can be used in either standard code or CUDA code, so that we
// can accelerate the simulation if needed.
typedef struct {
  float position[3];        // X, Y, Z location of the bead
  float radius;             // Radius of the bead
  float fluoro1_response;   // Number of photons emitted for a 1-second exposure, fluorophore 1
  float fluoro2_response;   // Number of photons emitted for a 1-second exposure, fluorophore 2
} PS_Bead;

static float        *g_cuda_buffer = NULL;

// For the camera simulator, block size and number of kernels to run to cover a whole grid.
// Initialized once in ensure_cuda_ready();
static dim3         g_threads;      // 16x16x1
static dim3         g_grid;         // Computed to cover array (slightly larger than array)

// Allocate a PCSC_buffer-sized element on the GPU.  Return false
// if we cannot get one.  This function can be called every time a
// CUDA_using function is called, but it only does the device opening
// and image-buffer allocation once.
static bool ensure_cuda_ready(void)
{
  static bool initialized = false;
  if (!initialized) {
    // Open the largest-ID CUDA device in the system
    cuInit(0);
    int num_devices = 0;
    cuDeviceGetCount(&num_devices);
    if (num_devices == 0) {
      fprintf(stderr,"ensure_cuda_ready(): No CUDA devices.\n");
      return false;
    }
    if (cuDeviceGet(&g_cuDevice, num_devices-1) != CUDA_SUCCESS) {
      fprintf(stderr,"ensure_cuda_ready(): Could not get device.\n");
      return false;
    }
    if (cuCtxCreate( &g_cuContext, 0, g_cuDevice ) != CUDA_SUCCESS) {
      fprintf(stderr,"ensure_cuda_ready(): Could not get context.\n");
      return false;
    }

    // Allocate a buffer to be used on the GPU.  It will be
    // copied back and forth from host memory.
    unsigned int numBytes = PSCS_cols * PSCS_rows * sizeof(float);
    if (cudaMalloc((void**)&g_cuda_buffer, numBytes) != cudaSuccess) {
      fprintf(stderr,"ensure_cuda_ready(): Could not allocate memory.\n");
      return false;
    }
    if (g_cuda_buffer == NULL) {
      fprintf(stderr,"ensure_cuda_ready(): Buffer is NULL pointer.\n");
      return false;
    }

    // Set up enough threads (and enough blocks of threads) to at least
    // cover the size of the array.  We use a thread block size of 16x16
    // because that's what the example matrixMul code from nVidia does.
    g_threads.x = 16;
    g_threads.y = 16;
    g_threads.z = 1;
    g_grid.x = (PSCS_cols / g_threads.x) + 1;
    g_grid.y = (PSCS_rows / g_threads.y) + 1;
    g_grid.z = 1;

    initialized = true;
  }

  // We're good if we have a buffer.
  return g_cuda_buffer != NULL;
}

//----------------------------------------------------------------------
// Functions for the camera simulator.
//----------------------------------------------------------------------

// CUDA kernel to clear all of the elements of the matrix to zero.
// Told the buffer beginning and the buffer size.  Assumes at least
// as many threads are run as there are elements in the buffer.
// Assumes a 2D array of threads.
static __global__ void clear_float_buffer(float *buf, int nx, int ny)
{
  int x = blockIdx.x * blockDim.x + threadIdx.x;
  int y = blockIdx.y * blockDim.y + threadIdx.y;
  if ( (x < nx) && (y < ny) ) {
    buf[y + ny*x] = 0.0;
  }
}

// Clear every pixel in the image to black.  Return true on success
// and false on failure (for example, if no CUDA).
bool PSCS_cuda_clear(PSCS_buffer &buf)
{
  if (!ensure_cuda_ready()) { return false; }

  // We don't need to copy the buffer from host memory here, because
  // we're going to set it to a certain value no matter what it started
  // out as.

  // Run the CUDA kernel to clear the memory.  Uses the block and thread
  // counts found by ensure_cuda_ready().  Synchronize the threads when
  // we are done so we know that they finish before we copy the memory
  // back.
  clear_float_buffer<<< g_grid, g_threads >>>(g_cuda_buffer, PSCS_cols, PSCS_rows);
  if (cudaThreadSynchronize() != cudaSuccess) {
    fprintf(stderr, "PSCS_cuda_clear(): Could not synchronize threads\n");
    return false;
  }

  // Copy the buffer back from CUDA memory to host memory.
  size_t size = PSCS_cols * PSCS_rows * sizeof(float);
  if (cudaMemcpy(buf, g_cuda_buffer, size, cudaMemcpyDeviceToHost) != cudaSuccess) {
    fprintf(stderr, "PSCS_cuda_clear(): Could not copy memory back to host\n");
    return false;
  }

  // Done!
  return true;
}

// CUDA kernel to add to all of the elements of the matrix.
// Told the buffer beginning and the buffer size.  Assumes at least
// as many threads are run as there are elements in the buffer.
// Assumes a 2D array of threads.
static __global__ void add_to_float_buffer(float *buf, int nx, int ny, float increment)
{
  int x = blockIdx.x * blockDim.x + threadIdx.x;
  int y = blockIdx.y * blockDim.y + threadIdx.y;
  if ( (x < nx) && (y < ny) ) {
    buf[y + ny*x] += increment;
  }
}

// Add the specified amount of photons to the current value of the buffer that
// is passed in.  Return true on success
// and false on failure (for example, if no CUDA).
bool PSCS_cuda_accumulate_brightfield_photons(PSCS_buffer &buf, float photons)
{
  if (!ensure_cuda_ready()) { return false; }

  // Copy the buffer from host memory to CUDA memory so we have something to
  // add to.
  size_t size = PSCS_cols * PSCS_rows * sizeof(float);
  if (cudaMemcpy(g_cuda_buffer, buf, size, cudaMemcpyHostToDevice) != cudaSuccess) {
    fprintf(stderr, "PSCS_cuda_accumulate_brightfield_photons(): Could not copy memory to CUDA\n");
    return false;
  }

  // Run the CUDA kernel to add to the memory.  Uses the block and thread
  // counts found by ensure_cuda_ready().  Synchronize the threads when
  // we are done so we know that they finish before we copy the memory
  // back.
  add_to_float_buffer<<< g_grid, g_threads >>>(g_cuda_buffer, PSCS_cols, PSCS_rows, photons);
  if (cudaThreadSynchronize() != cudaSuccess) {
    fprintf(stderr, "PSCS_cuda_accumulate_brightfield_photons(): Could not synchronize threads\n");
    return false;
  }

  // Copy the buffer back from CUDA memory to host memory.
  if (cudaMemcpy(buf, g_cuda_buffer, size, cudaMemcpyDeviceToHost) != cudaSuccess) {
    fprintf(stderr, "PSCS_cuda_accumulate_brightfield_photons(): Could not copy memory back to host\n");
    return false;
  }

  // Done!
  return true;
}

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
  float s_meters,      //< standard deviation (square root of variance)
  float x, float y)	//< Point to sample (relative to origin)
{
  float variance = s_meters * s_meters;
  float R_squared = x*x + y*y;

  const float twoPI = static_cast<float>(2*CUDART_PI_F);
  const float twoPIinv = 1.0f / twoPI;
  float A = twoPIinv / variance;
  float B = -1 / (2 * variance);

  return A * __expf(B * R_squared);
}

// CUDA kernel to add sums of Gaussians to the image.
// Told the buffer beginning and the buffer size.
// The count of how many Gaussians there are is followed by a
// floating-point 1D array that has four entries in it for each bead: the X and Y
// position, the radius, and the value of the Gaussian.  Assumes at least
// as many threads are run as there are elements in the buffer.
// Assumes a 2D array of threads.
static __global__ void add_gaussians_to_float_buffer(float *buf, int nx, int ny,
                                                     size_t count, float *params)
{
  // Find out which pixel I'm responsible for.
  int x = blockIdx.x * blockDim.x + threadIdx.x;
  int y = blockIdx.y * blockDim.y + threadIdx.y;

  // For each Gaussian in the list, add its contribution to my pixel
  // based on the distance from my pixel and it.  Only do this if my
  // pixel is within the image.
  if ( (x < nx) && (y < ny) ) {
    size_t i;
    for (i = 0; i < count; i++) {
      float gx = params[4*i + 0];
      float gy = params[4*i + 1];
      float r = params[4*i + 2];
      float v = params[4*i + 3];
      buf[y + ny*x] += v * cuda_Gaussian(r, gx-x, gy-y);
    }
  }
}

// Sum the set of Gaussians whose parameters are passed in into the floating-point
// buffer passed in.  The count of how many Gaussians there are is followed by a
// floating-point 1D array that has four entries in it for each bead: the X and Y
// position, the radius, and the value of the Gaussian.  Return true on success
// and false on failure (for example, if no CUDA).
bool PSCS_cuda_accumulate_gaussians(PSCS_buffer &buf, size_t count, float *params)
{
  if (!ensure_cuda_ready()) { return false; }
  //return false;

  // We need to copy the bead-parameter buffer from host memory to CUDA
  // memory before calling the kernel.  But we need to make sure there is
  // enough room for the buffer.  To avoid having to re-allocate a buffer
  // each time we are called, we keep a static buffer around and make it
  // bigger whenever we have to.
  static size_t max_bead_params = 0;
  static float  *cuda_bead_params = NULL;
  size_t psize = 4 * count * sizeof(float);
  if (count > max_bead_params) {
    max_bead_params = count;
    if (cuda_bead_params != NULL) {
      cudaFree(cuda_bead_params);
    }
    if (cudaMalloc((void**)&cuda_bead_params, psize) != cudaSuccess) {
      fprintf(stderr, "PSCS_cuda_accumulate_gaussians(): Out of CUDA memory\n");
      return false;
    }
  }
  if (cudaMemcpy(cuda_bead_params, params, psize, cudaMemcpyHostToDevice) != cudaSuccess) {
    fprintf(stderr, "PSCS_cuda_accumulate_gaussians(): Could not copy parameters to CUDA\n");
    return false;
  }

  // Copy the buffer from host memory to CUDA memory so we have something to
  // add to.
  size_t size = PSCS_cols * PSCS_rows * sizeof(float);
  if (cudaMemcpy(g_cuda_buffer, buf, size, cudaMemcpyHostToDevice) != cudaSuccess) {
    fprintf(stderr, "PSCS_cuda_accumulate_gaussians(): Could not copy memory to CUDA\n");
    return false;
  }

  // Run the CUDA kernel to add the Gaussians to the memory.  Uses the block and thread
  // counts found by ensure_cuda_ready().  Synchronize the threads when
  // we are done so we know that they finish before we copy the memory
  // back.
  add_gaussians_to_float_buffer<<< g_grid, g_threads >>>(g_cuda_buffer, PSCS_cols, PSCS_rows, count, cuda_bead_params);
  if (cudaThreadSynchronize() != cudaSuccess) {
    fprintf(stderr, "PSCS_cuda_accumulate_gaussians(): Could not synchronize threads\n");
    return false;
  }

  // Copy the buffer back from CUDA memory to host memory.
  if (cudaMemcpy(buf, g_cuda_buffer, size, cudaMemcpyDeviceToHost) != cudaSuccess) {
    fprintf(stderr, "PSCS_cuda_accumulate_gaussians(): Could not copy memory back to host\n");
    return false;
  }

  return true;
}

//----------------------------------------------------------------------
// Functions for the bead simulator.
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Helper functions

// Horrible random-number generator.  May work to some extent if you do modulo
// some reasonable number.  Does make things jump around.
// When I tried more-sophisticated ones from the web, they locked up Windows.
inline __device__ unsigned quickrand(unsigned &seed, const unsigned a, const unsigned c)
{
    seed = seed * a + c;
    return seed;
}

// Polar method for normal density discussed in Knuth.
// This one is passed two random numbers between 0 and 1
// and it turns them into a random normally-distributed
// number.

inline __device__ float random_normal_sample(unsigned &seed, unsigned a, unsigned c)
{
  float u1, u2, v1, v2;
  float S = 2;
  while (S >= 1) {
    u1 = (quickrand(seed, a, c) % 10000) / 9999.0f;
    u2 = (quickrand(seed, a, c) % 10000) / 9999.0f;
    v1 = 2*u1 - 1;
    v2 = 2*u2 - 1;
    S = v1*v1 + v2*v2;
  };
  return v1*sqrtf( (-2*__logf(S))/S );
}

// CUDA kernel to add random offsets to a list of beads
// Told the buffer beginning and the buffer size.
// Assumes a 1D array of threads.  Assumes as many random
// number seeds as there are blocks.
static __global__ void move_beads_kernel(PS_Bead *buf, size_t count, float delta_t,
                                         float mean_r,
                                         float motion_x, float motion_y, float motion_z,
                                         unsigned *seeds)
{
  // Initializing the random-number generator using the
  // seeds we got from the host.  We seed based on our
  // block ID and then run it forward based on our thread
  // ID.
  unsigned a = 4529;
  unsigned c = 19723;
  unsigned seed = seeds[blockIdx.x];

  // Find out which bead I'm responsible for.
  size_t me = blockIdx.x * blockDim.x + threadIdx.x;

  // Randomly move the bead I am responsible based on the time interval
  // and its characteristics.
  if (me < count) {
    // Compute the change in coefficient to be taken based on the
    // difference between one second (the duration for which the
    // motion parameter was specified) and the actual time period
    // asked for.

    // The root mean squared displacement of a bead over time is
    // given by sqrt( 2 * D * t ), where t is the duration in time
    // between measurements.  Therefore, the time step should be
    // divided by the square-root of 1 (which is 1) and multiplied
    // by the square-root of delta_t.

    float time_scale = sqrtf(delta_t);

    // Compute change in coefficient to be made based on the
    // difference between the mean and actual parameter for this
    // bead.  In the above equation, D is the diffusion coefficient
    // and D = kB * T / b, where only b depends on the bead radius,
    // and b = 6 * pi * viscosity * r, where r is the bead radius.
    // Keeping all other terms constant, this means that the change
    // due to r in D is an inverse relationship, as is the change
    // with respect to D.  Do the final change is the square root
    // of the inverse change: delta = sqrt( (1/r) / (1/mean) )
    // = sqrt( mean/r )

    float radius_scale = sqrtf( mean_r / buf[me].radius );

    // Scale the standard deviation motion parameter by the above
    // scales and then sample from a Gaussian with that standard
    // deviation for each axis.

    float std_x = motion_x * time_scale * radius_scale;
    float std_y = motion_y * time_scale * radius_scale;
    float std_z = motion_z * time_scale * radius_scale;

    // Get three random normal samples.  We have to pass a random
    // number between 0 and 1 to each, which we get by scaling the
    // unsigned random numbers.
    float normal_samples[3] = {0, 0, 0};
    unsigned i;
    for (i = 0; i < 3; i++) {
      normal_samples[i] = random_normal_sample(seed, a, c);
    }
    buf[me].position[0] += std_x * normal_samples[0];
    buf[me].position[1] += std_y * normal_samples[1];
    buf[me].position[2] += std_z * normal_samples[2];
  }
}

// Move the beads whose parameters are passed in into the floating-point
// buffer passed in.  Return true on success
// and false on failure (for example, if no CUDA).
bool PSBS_cuda_move_beads(PS_Bead *buf, unsigned count, float delta_t,
                          float mean_r, float motion[3])
{
  if (!ensure_cuda_ready()) { return false; }

  unsigned threads = 256;
  unsigned blocks = (count / threads) + 1; // Make sure we have at least enough.

  // Allocate a block of random-number seeds on CUDA, which we initialize
  // each time we are called.  We need to have as many seeds as there are
  // blocks.  We re-allocate if we don't have enough.
  static unsigned *cuda_seeds = NULL;
  static size_t max_seeds = 0;

  size_t rsize = blocks * sizeof(unsigned);
  if (blocks > max_seeds) {
    max_seeds = blocks;
    if (cuda_seeds != NULL) {
      cudaFree(cuda_seeds);
    }
    if (cudaMalloc((void**)&cuda_seeds, rsize) != cudaSuccess) {
      fprintf(stderr, "PSBS_cuda_move_beads(): Out of CUDA memory\n");
      return false;
    }
  }

  // Generate a list of random-number seeds, one per thread.  Pass this list down to
  // CUDA memory.
  unsigned *seeds = new unsigned[blocks];
  if (seeds == NULL) {
    fprintf(stderr, "PSBS_cuda_move_beads(): Out of memory\n");
    return false;
  }
  unsigned i;
  for (i = 0; i < blocks; i++) {
    seeds[i] = rand();
  }
  if (cudaMemcpy(cuda_seeds, seeds, rsize, cudaMemcpyHostToDevice) != cudaSuccess) {
    fprintf(stderr, "PSBS_cuda_move_beads(): Could not copy random seeds to CUDA\n");
    return false;
  }
  delete [] seeds;

  // We need to copy the bead-parameter buffer from host memory to CUDA
  // memory before calling the kernel.  But we need to make sure there is
  // enough room for the buffer.  To avoid having to re-allocate a buffer
  // each time we are called, we keep a static buffer around and make it
  // bigger whenever we have to.
  static size_t max_bead_params = 0;
  static PS_Bead  *cuda_bead_params = NULL;
  size_t psize = count * sizeof(PS_Bead);
  if (count > max_bead_params) {
    max_bead_params = count;
    if (cuda_bead_params != NULL) {
      cudaFree(cuda_bead_params);
    }
    if (cudaMalloc((void**)&cuda_bead_params, psize) != cudaSuccess) {
      fprintf(stderr, "PSBS_cuda_move_beads(): Out of CUDA memory\n");
      return false;
    }
  }
  if (cudaMemcpy(cuda_bead_params, buf, psize, cudaMemcpyHostToDevice) != cudaSuccess) {
    fprintf(stderr, "PSBS_cuda_move_beads(): Could not copy parameters to CUDA\n");
    return false;
  }

  // Run the CUDA kernel to move the beads.  Synchronize the threads when
  // we are done so we know that they finish before we copy the memory
  // back.
  move_beads_kernel<<< blocks, threads >>>(cuda_bead_params, count, delta_t,
    mean_r, motion[0], motion[1], motion[2], cuda_seeds);
  if (cudaThreadSynchronize() != cudaSuccess) {
    fprintf(stderr, "PSBS_cuda_move_beads(): Could not synchronize threads\n");
    return false;
  }

  // Copy the buffer back from CUDA memory to host memory.
  if (cudaMemcpy(buf, cuda_bead_params, psize, cudaMemcpyDeviceToHost) != cudaSuccess) {
    fprintf(stderr, "PSBS_cuda_move_beads(): Could not copy parameters back to host\n");
    return false;
  }

  return true;
}
