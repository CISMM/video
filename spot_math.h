#ifndef	SPOT_MATH_H
#define	SPOT_MATH_H

#include <math.h>
#ifndef	M_PI
#ifndef M_PI_DEFINED
const double M_PI = 2*asin(1.0);
#define M_PI_DEFINED
#endif
#endif
const double TWOPI = 2 * M_PI;

// Equations in this file are from http://scienceworld.wolfram.com/physics.

/// Returns the value of a Bessel function of the first kind with order n at position x
double	BesselFirstKind(double x, double n = 1);

/// Returns the wavenumber of the photon whose wavelength is passed in
inline double	WaveNumber(double lambda) { return 2 * M_PI / lambda; }

/// Returns the Fraunhofer wavefunction at coordinates away from the center of an aperture.
/** The aperture has a radius R.  The coordinate rho is the positive distance from center
  * on the image plane in meters.  The wavelength of the light in meters is lambda. **/
// XXX How to deal with the aperture factor (the fact that we have an incoming spherical
// wave).  See http://scienceworld.wolfram.com/physics/FraunhoferDiffraction.html and
// compare the equation there to the one found in
// http://scienceworld.wolfram.com/physics/FraunhoferDiffractionEllipticalAperture.html.
// XXX Dealing with zero by replacing it by something very close to zero (function at zero
// is a limit).  This is a bit unstable.
inline double	FraunhoferWaveFunction(double R, double rho, double lambda) {
  if (rho < 1e-10) {
    rho = 1e-10;
  }
  return TWOPI * R*R * BesselFirstKind(WaveNumber(lambda) * R * rho) / ( WaveNumber(lambda) * R * rho );
}

/// Returns the Fraunhofer wavefunction at coordinates away from the center of an aperture.
/** The aperture has a radius R.  The coordinates x and y are the distance from center on the image
  * plane in meters.  The wavelength of the light in meters is lambda. **/
inline double	FraunhoferWaveFunction(double R, double x, double y, double lambda) {
  return FraunhoferWaveFunction(R, sqrt(x*x+y*y), lambda);
}

/// Returns the Fraunhofer intensity at coordinates away from the center of an aperture.
/** The aperture has a radius R.  The coordinate rho is the positive distance from center
  * on the image plane in meters.  The wavelength of the light in meters is lambda.
  * XXX The C value is a constant that adjusts the overall intensity and it wasn't clear how to choose it. **/
inline double	FraunhoferIntensity(double R, double rho, double lambda, double C) {
  double fwf = FraunhoferWaveFunction(R, rho, lambda);
  return C*C * fwf*fwf;
}

/// Returns the Fraunhofer intensity at coordinates away from the center of an aperture.
/** The aperture has a radius R.  The coordinates x and y are the distance from center on the image
  * plane in meters.  The wavelength of the light in meters is lambda.
  * XXX The C value is a constant that adjusts the overall intensity and it wasn't clear how to choose it. **/

inline double	FraunhoferIntensity(double R, double x, double y, double lambda, double C) {
  double fwf = FraunhoferWaveFunction(R, x, y, lambda);
  return C*C * fwf*fwf;
}

// Compute the volume under part of the Airy disk.  Do this by
// sampling the disk densely, finding the average value within the area,
// and then multiplying by the area.

inline double	ComputeAiryVolume(
  double radius_meters, //< Aperture radius of the Airy disk
  double wavelength_meters, //< Wavelength of the light in meters
  double x0,		//< Low end of X integration range in Airy-disk coordinates
  double x1,		//< High end of X integration range
  double y0,		//< Low end of Y integration range
  double y1,		//< High end of Y integration range
  int samples)		//< How many samples to take in each of X and Y
{
  int count = 0;	//< How many pixels we have summed up
  double x;		//< Steps through X
  double y;		//< Steps through Y
  double sx = (x1 - x0) / (samples + 1);
  double sy = (y1 - y0) / (samples + 1);
  double sum = 0;

  for (x = x0 + sx/2; x < x1; x += sx) {
    for (y = y0 + sy/2; y < y1; y += sy) {
      count++;
      sum += FraunhoferIntensity(radius_meters, x, y, wavelength_meters, 1.0);
    }
  }

  return (sum / count) * (x1-x0) * (y1-y0);
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

inline double	ComputeGaussianAtPoint(
  double s_meters,      //< standard deviation (square root of variance)
  double x, double y)	//< Point to sample (relative to origin)
{
  double variance = s_meters * s_meters;
  double R_squared = x*x + y*y;

  const double twoPI = 2*M_PI;
  const double twoPIinv = 1.0 / twoPI;
  double A = twoPIinv / variance;
  double B = -1 / (2 * variance);

  return A * exp(B * R_squared);
}

// Compute the volume under part of a Gaussian.  Do this by
// sampling the function densely, finding the average value within the area,
// and then multiplying by the area.  The function is 2D, centered at the
// origin.  The "standard normal distribution" Gaussian has an integrated
// volume of 1 over all space and a variance of 1.  It is defined as:
//               1           -(R^2)/2
//   G(x) = ------------ * e
//             2*PI
// where R is the radius of the sample point from the origin.
// We let the user set the magnitude under the curve m (multiplies the first
// term) and the standard deviation s, changing the function to:
//                  m           -(R^2)/(2*s^2)
//   G(x) = --------------- * e
//           s^2 * 2*PI
// For computational effeciency, we refactor this into A * e ^ (B * R^2).

inline double	ComputeGaussianVolume(
  double m,             //< Magnitude (summed volume under curve over all space)
  double s_meters,      //< standard deviation (square root of variance)
  double x0,		//< Low end of X integration range in Airy-disk coordinates
  double x1,		//< High end of X integration range
  double y0,		//< Low end of Y integration range
  double y1,		//< High end of Y integration range
  int samples)		//< How many samples to take in each of X and Y
{
  double variance = s_meters * s_meters;
  int count = 0;	//< How many pixels we have summed up
  double x;		//< Steps through X
  double y;		//< Steps through Y
  double sx = (x1 - x0) / (samples + 1);
  double sy = (y1 - y0) / (samples + 1);
  double sum = 0;

  const double twoPI = 2*M_PI;
  double A = m / ( variance * twoPI );
  double B = -1 / (2 * variance);
  double R_squared;

  for (x = x0 + sx/2; x < x1; x += sx) {
    for (y = y0 + sy/2; y < y1; y += sy) {
      R_squared = x*x + y*y;
      count++;
      sum += exp(B * R_squared);
    }
  }

  return A * (sum / count) * (x1-x0) * (y1-y0);
}


#endif
