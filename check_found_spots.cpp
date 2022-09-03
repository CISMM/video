#include  <math.h>
#include  <stdlib.h>
#include  <stdio.h>
#include  <string.h>

const char *version = "01.01";

void Usage (const char * s)
{
  fprintf(stderr,"Usage: %s [-v] infile1.csv\n", s);
  fprintf(stderr,"     -v: Verbose mode prints version and header\n");
  exit(0);
}

int main(int argc, char *argv[])
{
  const char *infilename1;
  FILE  *infile1;
  const unsigned MAX_LINE_LEN = 2047;
  char  line[MAX_LINE_LEN+1];
  bool verbose = false;               // Print out info along the way?
  bool found_spots [25] = { false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false };

  int	realparams = 0;
  int	i;
  i = 1;
  while (i < argc) {
	  if (!strncmp(argv[i], "-v", strlen("-v"))) {
		  verbose = true;
	  } else if (argv[i][0] == '-') {	// Unknown flag
		  Usage(argv[0]);
	  } else {
		  infilename1 = argv[i];
		  realparams++;
	  }
	  i++;
  }
  /*if (realparams != 2) {
	  Usage(argv[0]);
  }*/
  if (verbose) { printf("Version %s\n", version); }

  //------------------------------------------------------------------------------------
  // Open the input file and skip the header line
  infile1 = fopen(infilename1, "r");
  if (infile1 == NULL) {
    fprintf(stderr,"Error opening %s for reading\n", infilename1);
    return -1;
  }
  fgets(line, MAX_LINE_LEN, infile1);

  //------------------------------------------------------------------------------------
  // Read lines from each file.  There should be the same number of lines, and
  // the bead number should always be zero.  Compute statistics on the differences
  // between the traces in the two files.
  unsigned count = 0;
  while (fgets(line, MAX_LINE_LEN, infile1) != NULL) {
    // Parse the line read from file 1
    int frame, bead;
    double x, y;
    if (sscanf(line, "%d,%d,%lg,%lg", &frame, &bead, &x, &y) != 4) {
      fprintf(stderr,"Error parsing line %d from file %s:\n", count, infilename1);
      fprintf(stderr,"  '%s'\n", line);
      return -1;
    }

    // Check if the tracker has found a bead successfully, and if that bead has not yet been found, add it to the count.
	int spot_num = 0;
	for (double i = 40; i <= 360; i += 80) {
		for (double j = 40; j <= 360; j += 80) {
			if ( fabs(x - i) < 5 && fabs(y - j) < 5 && !(found_spots[spot_num])) {
				found_spots[spot_num] = true;
				count++;
			}
			spot_num++;
		}
	}
  
  }


  //------------------------------------------------------------------------------------
  // Print the count of true spots found.
  printf("%d\n", count);

  //------------------------------------------------------------------------------------
  // Clean up things allocated for the whole sequence
  if (infile1) { fclose(infile1); infile1 = NULL; };
  return 0;
}
