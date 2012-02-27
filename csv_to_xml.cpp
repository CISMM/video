#include <stdlib.h>	// For exit()
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <vector>

using namespace std;

void Usage (const char * s)
{
  fprintf(stderr,"Usage: %s input_file\n",s);
  fprintf(stderr,"     input_file : (string) Name of the input CSV file\n");
  fprintf(stderr,"     Converts video_spot_tracker CSV to particle contest XML format\n");
  exit(0);
}

// Stores a single place and time of a bead.
class beadinfo {
public:
  double x,y,z;	// Location of the bead
  unsigned t;	// Time frame of the location
};

int main (int argc, char * argv[])
{
  //------------------------------------------------------------------------
  // Parse the command line
  char	*input_file_name = NULL;
  int	realparams = 0;
  int	i;
  i = 1;
  while (i < argc) {
    if (strcmp(argv[i], "-help") == 0) {
	Usage(argv[0]);
    } else if (argv[i][0] == '-') {	// Unknown flag
	Usage(argv[0]);
    } else switch (realparams) {		// Non-flag parameters
      case 0:
	  input_file_name = argv[i];
	  realparams++;
	  break;
      default:
	  Usage(argv[0]);
    }
    i++;
  }
  if (realparams != 1) {
    Usage(argv[0]);
  }

  //------------------------------------------------------------------------
  // Open the input file for reading.  Skip the header line in the file.
  FILE *in = fopen(input_file_name, "r");
  char line[1024];
  if (!in) {
	fprintf(stderr,"Could not open %s for reading\n", input_file_name);
	return -1;
  }
  if (fgets(line, sizeof(line)-1, in) == NULL) {
	fprintf(stderr,"Could not skip header line\n");
	return -2;
  }

  //------------------------------------------------------------------------
  // Read the lines from the input file, making a vector of traces, one trace
  // per bead in the file.  Each trace is a vector of positions and times.
  vector< vector<beadinfo> >	traces;
  while (fgets(line, sizeof(line)-1, in) != NULL) {
	//Parse the line to pull out the beadinfo and particle ID
	unsigned frame, bead;
	double	x,y,z;
	if (sscanf(line, "%u,%u,%lf,%lf,%lf", &frame, &bead, &x,&y,&z) != 5) {
		fprintf(stderr,"Could not parse line: %s\n", line);
	}

	// Make sure we've allocated enough trace vectors to store this
	// trace info.
	while (traces.size() < bead) {
		vector<beadinfo> newtrace;
		traces.push_back(newtrace);
	}

	// Store a new entry into this trace
	beadinfo newbead;
	newbead.x = x;
	newbead.y = y;
	newbead.z = z;
	newbead.t = frame;
	traces[bead-1].push_back(newbead);
  }

  //------------------------------------------------------------------------
  // Done with the input file.
  fclose(in);

  //------------------------------------------------------------------------
  // Make the name of the output file by appending ".xml" to the input.
  char *output_file_name = new char[strlen(input_file_name)+10];
  if (!output_file_name) {
	fprintf(stderr,"Out of memory\n");
	return -3;
  }
  sprintf(output_file_name, "%s.xml", input_file_name);

  //------------------------------------------------------------------------
  // Open and write the output file describing the traces.
  FILE *out = fopen(output_file_name, "w");
  if (!out) {
	fprintf(stderr,"Could not open %s for writing\n", output_file_name);
	return -4;
  }

  fprintf(out,"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n");
  fprintf(out,"<root>\n");
  fprintf(out,"<TrackContestISBI2012 SNR=\"?\" density=\"unknown\" generationDateTime=\"unknown\" info=\"none\" scenario=\"unnown\">\n");

  size_t p,t;
  for (p = 0; p < traces.size(); p++) {
    // Don't write particles that have no entries.
    if (traces[p].size() > 0) {
      fprintf(out,"<particle>\n");
      for (t = 0; t < traces[p].size(); t++) {
	  fprintf(out,"<detection t=\"%u\" x=\"%lf\" y=\"%lf\" z=\"%lf\"/>\n",
		  traces[p][t].t,
		  traces[p][t].x, traces[p][t].y, traces[p][t].z);
      }
      fprintf(out,"</particle>\n");
    }
  }

  fprintf(out,"</TrackContestISBI2012>\n");
  fprintf(out,"</root>\n");

  fclose(out);

  return 0;
}

