#include  <math.h>
#include  <stdlib.h>
#include  <stdio.h>
#include  <string.h>

const char *version = "01.00";

void Usage (const char * s)
{
  fprintf(stderr,"Usage: %s [-v] infile1.csv infile2.csv\n", s);
  fprintf(stderr,"     -v: Verbose mode prints version and header\n");
  exit(0);
}

int main(int argc, char *argv[])
{
  const char *infilename1, *infilename2;
  FILE  *infile1, *infile2;
  const unsigned MAX_LINE_LEN = 2047;
  char  line[MAX_LINE_LEN+1];
  bool verbose = false;               // Print out info along the way?

  int	realparams = 0;
  int	i;
  i = 1;
  while (i < argc) {
    if (!strncmp(argv[i], "-v", strlen("-v"))) {
          verbose = true;
    } else if (argv[i][0] == '-') {	// Unknown flag
	  Usage(argv[0]);
    } else switch (realparams) {		// Non-flag parameters
      case 0:
          infilename1 = argv[i];
          realparams++;
          break;
      case 1:
          infilename2 = argv[i];
          realparams++;
          break;
      default:
	  Usage(argv[0]);
    }
    i++;
  }
  if (realparams != 2) {
    Usage(argv[0]);
  }

  if (verbose) { printf("Version %s\n", version); }

  //------------------------------------------------------------------------------------
  // Open the two input files and skip the header line on each
  infile1 = fopen(infilename1, "r");
  if (infile1 == NULL) {
    fprintf(stderr,"Error opening %s for reading\n", infilename1);
    return -1;
  }
  fgets(line, MAX_LINE_LEN, infile1);
  infile2 = fopen(infilename2, "r");
  if (infile2 == NULL) {
    fprintf(stderr,"Error opening %s for reading\n", infilename2);
    return -1;
  }
  fgets(line, MAX_LINE_LEN, infile2);

  //------------------------------------------------------------------------------------
  // Read lines from each file.  There should be the same number of lines, and
  // the bead number should always be zero.  Compute statistics on the differences
  // between the traces in the two files.
  unsigned count = 0;
  double biasx = 0, biasy = 0;
  double maxx = 0, maxy = 0, maxrad = 0;
  double meanx = 0, meany = 0, meanrad = 0;
  while (fgets(line, MAX_LINE_LEN, infile1) != NULL) {
    // Parse the line read from file 1
    int frame1, bead1;
    double x1, y1, z1;
    if (sscanf(line, "%d,%d,%lg,%lg,%lg", &frame1, &bead1, &x1, &y1, &z1) != 5) {
      fprintf(stderr,"Error parsing line %d from file %s:\n", count, infilename1);
      fprintf(stderr,"  '%s'\n", line);
      return -1;
    }

    // Read and parse the line from the second file.
    if (fgets(line, MAX_LINE_LEN, infile2) == NULL) {
      fprintf(stderr,"Error reading line from file %s:\n", infilename2);
      return -1;
    }
    int frame2, bead2;
    double x2, y2, z2;
    if (sscanf(line, "%d,%d,%lg,%lg,%lg", &frame2, &bead2, &x2, &y2, &z2) != 5) {
      fprintf(stderr,"Error parsing line %d from file %s:\n", count, infilename2);
      fprintf(stderr,"  '%s'\n", line);
      return -1;
    }

    // Ensure the bead and frame number match
    if ( (frame1 != frame2) || (bead1 != bead2) ) {
      fprintf(stderr,"Mismatched bead/frame from two files on line %d\n", count);
      fprintf(stderr,"  Frame1 = %d, Frame2 = %d;  Bead1 = %d, Bead2 = %d\n", frame1, frame2, bead1, bead2);
      return -1;
    }

    // Compute the statistics.
    double dx = x2 - x1;
    double dy = y2 - y1;
    double drad = sqrt ( dx*dx + dy*dy );

    biasx += dx;
    biasy += dy;
    meanx += fabs(dx);
    meany += fabs(dy);
    meanrad += drad;
    if (fabs(dx) > maxx) { maxx = fabs(dx); }
    if (fabs(dy) > maxy) { maxy = fabs(dy); }
    if (drad > maxrad) { maxrad = drad; }
    count++;
  }
  if (count == 0) {
    fprintf(stderr,"Could not read any matching lines from files\n");
    return -2;
  }
  biasx /= count;
  biasy /= count;
  meanx /= count;
  meany /= count;
  meanrad /= count;

  //------------------------------------------------------------------------------------
  // Print the statistics. If verbose, print a header.
  if (verbose) {
    printf("meanrad,meanx,meany,maxrad,maxx,maxy,biasx,biasy\n");
  }
  printf("%lg,%lg,%lg,%lg,%lg,%lg,%lg,%lg\n", meanrad,meanx,meany,maxrad,maxx,maxy,biasx,biasy);

  //------------------------------------------------------------------------------------
  // Clean up things allocated for the whole sequence
  if (infile1) { fclose(infile1); infile1 = NULL; };
  if (infile2) { fclose(infile2); infile2 = NULL; };
  return 0;
}
