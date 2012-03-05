#include <stdlib.h>	// For exit()
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <vector>

using namespace std;

void Usage (const char * s)
{
  fprintf(stderr,"Usage: %s reference_file tracking_file\n",s);
  fprintf(stderr,"     reference_file : (string) Name of the XML file with the reference traces\n");
  fprintf(stderr,"     tracking_file : (string) Name of the XML file with the tracking traces\n");
  fprintf(stderr,"     Computes statistics on the difference between the files (based on the 2012 particle-tracking contest)\n");
  exit(0);
}

// Stores a single place and time of a bead.
class beadinfo {
public:
  double x,y,z;	// Location of the bead
  unsigned t;	// Time frame of the location

  // Compute the distance to another bead.  If it is not in the same
  // time step, we return a large positive distance.
  inline double dist(const beadinfo &b) {
    if (b.t != t) { return 1e10; }
    return sqrt( (b.x-x)*(b.x-x) + (b.y-y)*(b.y-y) + (b.z-z)*(b.z-z) );
  }
};

// Read one word from an XML line and fill in the appropriate value for
// the beadinfo.  These words start with t=, x=, y= or z= and then
// have a quote and a number after.  So we look for the first quote,
// then read the number, then store the number into the appropriate
// field.  Return true on success, false on failure to find the text
// we expect.
bool parse_word(const char *w, beadinfo *b)
{
  const char *quote = strchr(w, '"');
  if (quote == NULL) { return false; }
  double val;
  if (sscanf( quote+1, "%lf", &val ) != 1) { return false; }
  switch (w[0]) {
    case 'x': b->x = val; break;
    case 'y': b->y = val; break;
    case 'z': b->z = val; break;
    case 't': b->t = static_cast<unsigned>(val); break;
    default: return false;
  }
  return true;
}

int main (int argc, char * argv[])
{
  //------------------------------------------------------------------------
  // Parse the command line
  char	*reference_file_name = NULL;
  char	*tracking_file_name = NULL;
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
	  reference_file_name = argv[i];
	  realparams++;
	  break;
      case 1:
	  tracking_file_name = argv[i];
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

  //------------------------------------------------------------------------
  // Open the reference and tracking files for reading.
  FILE *ref = fopen(reference_file_name, "r");
  if (!ref) {
	fprintf(stderr,"Could not open %s for reading\n", reference_file_name);
	return -1;
  }
  FILE *trk = fopen(tracking_file_name, "r");
  if (!trk) {
	fprintf(stderr,"Could not open %s for reading\n", tracking_file_name);
	return -1;
  }

  //------------------------------------------------------------------------
  // Read the lines from the reference file, making a vector of points, one
  // per detection line in the file.  This a vector of positions and times.
  char line[1024];
  vector<beadinfo>	refs;
  while (fgets(line, sizeof(line)-1, ref) != NULL) {
	//Parse the line to pull out the beadinfo and particle ID
	char word1[1024], word2[1024], word3[1024], word4[1024], word5[1024];
	if (sscanf(line, "%s%s%s%s%s", word1,word2,word3,word4,word5) != 5) {
          continue;
	}
	if (strncmp(word1, "<detection", strlen("<detection")) == 0) {
		beadinfo newbead;
		parse_word(word2, &newbead);
		parse_word(word3, &newbead);
		parse_word(word4, &newbead);
		parse_word(word5, &newbead);
		refs.push_back(newbead);
	}
  }
  printf("Found %u detections in reference file\n",
      static_cast<unsigned>(refs.size()) );

  //------------------------------------------------------------------------
  // Read the lines from the tracking file, making a vector of points, one
  // per detection line in the file.  This a vector of positions and times.
  vector<beadinfo>	trks;
  while (fgets(line, sizeof(line)-1, trk) != NULL) {
	//Parse the line to pull out the beadinfo and particle ID
	char word1[1024], word2[1024], word3[1024], word4[1024], word5[1024];
	if (sscanf(line, "%s%s%s%s%s", word1,word2,word3,word4,word5) != 5) {
          continue;
	}
	if (strncmp(word1, "<detection", strlen("<detection")) == 0) {
		beadinfo newbead;
		parse_word(word2, &newbead);
		parse_word(word3, &newbead);
		parse_word(word4, &newbead);
		parse_word(word5, &newbead);
		trks.push_back(newbead);
	}
  }
  printf("Found %u detections in tracker file\n",
      static_cast<unsigned>(trks.size()) );

  //------------------------------------------------------------------------
  // Done with the input files.
  fclose(ref);
  fclose(trk);

  //------------------------------------------------------------------------
  // Compute distances from closest-matched detections and leave only
  // unmatched entried in each list.
  // For each element in the tracker file, find the closest bead in the
  // reference set with the same time step.  If that bead is more than 5
  // pixels away, it does not count.  If it is less, then remove the
  // beads from both lists and add an entry into our list of distances.
  vector<double>  dists;
  size_t t = 0;
  while ( (t < trks.size()) && (refs.size() > 0) ) {
    size_t r;
    size_t which = 0;
    double mindist = trks[t].dist(refs[0]);
    for (r = 1; r < refs.size(); r++) {
      double dist = trks[t].dist(refs[r]);
      if (dist < mindist) {
        mindist = dist;
        which = r;
      }
    }
    if (mindist > 5) {
      // Did not find a match.  Move on to the next tracker.
      t++;
      continue;
    } else {
      // Found a good match.  Add it to the distances, then remove
      // the associated entries from each of the lists.  Leave the t
      // index the same, because we have moved the end entry from the
      // vector to its position.
      dists.push_back(mindist);

      trks[t] = trks[trks.size()-1];
      trks.pop_back();

      refs[which] = refs[refs.size()-1];
      refs.pop_back();
    }
  }

  //------------------------------------------------------------------------
  // Compute and report statistics on the traces.
  printf("%u non-matched tracker reports\n",
    static_cast<unsigned>(trks.size()) );
  printf("%u non-matched reference reports\n",
    static_cast<unsigned>(refs.size()) );
  if (dists.size() > 0) {
    size_t i;
    double mean = 0;
    double maxd = -1;
    for (i = 0; i < dists.size(); i++) {
      mean += dists[i];
      if (dists[i] > maxd) {
        maxd = dists[i];
      }
    }
    printf("Mean distance between matched reports = %lg\n",
      mean/dists.size());
    printf("Max distance between matched reports = %lg\n", maxd);
  }

  return 0;
}

