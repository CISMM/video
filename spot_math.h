#ifndef	SPOT_MATH_H
#define	SPOT_MATH_H

#include <math.h>
#ifndef	M_PI
const double M_PI = 2*asin(1.0);
#endif
const double TWOPI = 2 * M_PI;

// Equations in this file are from http://scienceworld.wolfram.com/physics.

/// Returns the value of a Bessel function of the first kind with order n at position x
double	BesselFirstKind(double x, double n = 1);

/// Returns the wavenumber of the photon whose wavelength is passed in
inline double	WaveNumber(double lambda) { return 2 * M_PI / lambda; }

/// Returns the Fraunhofer wavefunction at coordinates away from the center of an aperture.
/** The aperture has a radius R.  The coordinate rho is the distance from center on the image
  * plane in meters.  The wavelength of the light in meters is lambda. **/
// XXX How to deal with the aperture factor (the fact that we have an incoming spherical
// wave).  See http://scienceworld.wolfram.com/physics/FraunhoferDiffraction.html and
// compare the equation there to the one found in
// http://scienceworld.wolfram.com/physics/FraunhoferDiffractionEllipticalAperture.html.
// XXX Dealing with zero by replacing it by something very close to zero (function at zero
// is a limit).  This is a bit unstable.
inline double	FraunhoferWaveFunction(double R, double rho, double lambda) {
  if (rho == 0) {
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
/** The aperture has a radius R.  The coordinate rho is the distance from center on the image
  * plane in meters.  The wavelength of the light in meters is lambda.  The C value is a
  * constant that adjusts the overall intensity and it wasn't clear how to choose it. **/
inline double	FraunhoferIntensity(double R, double rho, double lambda, double C = 1.0) {
  double fwf = FraunhoferWaveFunction(R, rho, lambda);
  return C*C * fwf*fwf;
}

/// Returns the Fraunhofer intensity at coordinates away from the center of an aperture.
/** The aperture has a radius R.  The coordinates x and y are the distance from center on the image
  * plane in meters.  The wavelength of the light in meters is lambda.  The C value is a
  * constant that adjusts the overall intensity and it wasn't clear how to choose it. **/
inline double	FraunhoferIntensity(double R, double x, double y, double lambda, double C = 1.0) {
  double fwf = FraunhoferWaveFunction(R, x, y, lambda);
  return C*C * fwf*fwf;
}

#endif
