#include  <math.h>
#include  <stdlib.h>
#include  <stdio.h>
#include  "spot_tracker.h"

// Hacked version of gettimeofday() from VRPN.
#ifdef	_WIN32
#include  <winsock.h>
#include  <sys/timeb.h>
int gettimeofday(timeval *tp, void *)
{
    if (tp != NULL) {
            struct _timeb t;
            _ftime(&t);
            tp->tv_sec = t.time;
            tp->tv_usec = (long)t. millitm * 1000;
    }
    return 0;
}
#else
#include  <sys/time.h>
#endif

static	double	duration(struct timeval t1, struct timeval t2)
{
    return (t1.tv_usec - t2.tv_usec) / 1000000.0 +
	   (t1.tv_sec - t2.tv_sec);
}

int main(int, char *[])
{
  double  testrad = 5.5, testx = 127.25, testy = 127.75;
  double  seedrad = 6, seedx = 120, seedy = 118;

  printf("Generating default test image with radius %lg disk at %lg, %lg\n", testrad, testx, testy);
  test_image  image(0,255, 0,255, 127, 5, testx, testy, testrad, 250);

  printf("Generating default spot tracker\n");
  disk_spot_tracker tracker(seedrad);

  printf("Looking for best fit within the image\n");
  tracker.locate_good_fit_in_image(image, seedx, seedy);

  printf("Optimization, starting at found location %lg, %lg,  rad %lg\n", seedx, seedy, seedrad);
  int i;
  double  x,y, rad, fit;
  tracker.take_single_optimization_step(image, x,y, seedx, seedy);
  for (i = 0; i < 5; i++) {
    tracker.take_single_optimization_step(image, x, y);
    rad = tracker.get_radius();
    fit = tracker.get_fitness();
    printf("Next step: X = %8.3lg,  Y = %8.3lg,  rad = %8.3lg, fit = %12.5lg\n", x,y,rad, fit);
  }

  printf("Chasing around a spot using full optimization\n");
  for (i = 0; i < 10; i++) {
    testrad += ( (rand()/(double)(RAND_MAX)) - 0.5) * 2 * 1;
    if (testrad < 3) { testrad = 3; }
    testx += ( (rand()/(double)(RAND_MAX)) - 0.5) * 2 * (testrad/2);
    testy += ( (rand()/(double)(RAND_MAX)) - 0.5) * 2 * (testrad/2);
    printf("  Generating test image with radius %6.2lf disk at %6.2lf, %6.2lf\n", testrad, testx, testy);
    {
      test_image image2(0,255, 0,255, 127, 5, testx, testy, testrad, 250);
      tracker.optimize(image2, x, y);
      rad = tracker.get_radius();
      fit = tracker.get_fitness();
      printf("  Found spot in image with radius   %6.2lf disk at %6.2lf, %6.2lf (fit %6.3lg)\n", rad,x,y, fit);
      if ( (fabs(rad-testrad) > 1) || (fabs(x - testx) > 1) || (fabs(y - testy) > 1) ) {
	fprintf(stderr,"Error -- off by more than unit radius or by more than one pixel\n");
      }
    }
  }

  printf("Timing how long it takes to fully optimize from a nearby position on average\n");
  int avgcount = 10;
  struct timeval start, end;
  tracker.optimize(image, x,y);	      // Get back to the correct starting location
  gettimeofday(&start, NULL);
  for (i = 0; i < avgcount; i++) {
    tracker.optimize(image, x, y,
      x + ( (rand()/(double)(RAND_MAX)) - 0.5) * 2 * (testrad/2),
      y + ( (rand()/(double)(RAND_MAX)) - 0.5) * 2 * (testrad/2));
  }
  gettimeofday(&end, NULL);
  printf("  Time: %lg seconds per optimization\n", duration(end, start)/avgcount);
  return 0;
}
