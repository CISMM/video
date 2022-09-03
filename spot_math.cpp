#include <stdio.h>
#include "spot_math.h"

/** Returns the value of the Bessel function of the first
 * kind with order n.  The default value for n is 1 because
 * that is what is needed by diffraction-limited spots and
 * that's what I'm writing this code for.
 *  This is based on Bessel's first integral as described
 * at http://mathworld.wolfram.com/BesselsFirstIntegral.hmtl.
 *  This implementation is not optimized for either speed or
 * accuracy at this time.  Brief inspection of a number of
 * values for this function compared to the Microsoft Excel
 * function's return value indicate that the accuracy is
 * good to 8 digits past the decimal point (usually 9) with
 * 137-bin integration if the first and last point
 * are symmetric at 0 and PI.
 **/
double	BesselFirstKind(
	const double x,	  //< Location to be evaluated
	const double n)   //< Order of the function
{
  const	double	invPi = 1.0 / M_PI;
  const double	count = 137.0;
  const	double	countDivide = 1.0 / count;
  const	double	increment = M_PI / count;
  const	double	end = M_PI + increment*0.9; //< stop when go past the end, avoids requiring exact floating-point equals from sum
  double  loop;		//< Goes from 0 to PI
  double  sum = 0.0;	//< Keep track of the sum

  // Integrate the function cos(nT - xsin(T)) dT over the
  // range 0 to PI, then divide by PI.  There is a subtlety
  // here because the number of points being added is
  // actually count + 1 and we're dividing by count.  This is
  // because we're using the trapezoidal approximation for each
  // of the count bins, where the average value of the two endpoints
  // is used to determine the value integrated in that bin; in this
  // method, the first and last point in the list only receive 1/2
  // weight each going into the sum (the others receive 1), which
  // means that there is an overall reduction in the weights of 1
  // (1/2 for each of them).
  for (loop = 0.0; loop <= end; loop += increment) {
    sum += cos(n*loop - x * sin(loop));
  }

  return invPi * sum * countDivide;
}
