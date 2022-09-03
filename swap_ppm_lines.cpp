#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "ppm.h"

bool  swap_lines_for_file(const char *name)
{
  // Make sure the file extension is ".ppm"
  if ( (name[strlen(name)-4] != '.') ||
       (tolower(name[strlen(name)-3]) != 'p') ||
       (tolower(name[strlen(name)-2]) != 'p') ||
       (tolower(name[strlen(name)-1]) != 'm') ) {
    fprintf(stderr, "Input file not PPM file: %s\n", name);
    return false;
  }

  // Open the file whose name is passed as a PPM file
  PPM ppm(name);

  // Make sure there is a nonzero even number of rows.
  if ( (ppm.Ny() == 0) | (ppm.Ny() % 2 != 0) ) {
    fprintf(stderr, "Input file (%s) doesn't have nonzero even number of lines (%d)\n", name, ppm.Ny());
    return false;
  }

  // Swap the even lines with the odd lines in the PPM file
  int r,g,b, tr, tg, tb;
  int x, y;
  for (y = 0; y < ppm.Ny(); y += 2) {
    for (x = 0; x < ppm.Nx(); x++) {
      ppm.Tellppm(x, y+1, &tr, &tg, &tb);
      ppm.Tellppm(x, y, &r, &g, &b);
      ppm.Putppm(x, y+1, r, g, b);
      ppm.Putppm(x, y, tr, tg, tb);
    }
  }

  // Write the file out with the same name but with "swap" before .ppm
  char	outname[2048];
  if (strlen(name) > 2040) {
    fprintf(stderr, "Name too long: %s\n", name);
    return false;
  }
  strcpy(outname, name);
  outname[strlen(outname)-4] = '\0';
  strcat(outname, ".swap.ppm");

  return ppm.Write_to(outname) == 0;
}

int main(int argc, char **argv)
{
  // For each command-line argument, translate the file.
  int i;
  for (i = 1; i < argc; i++) {
    if (!swap_lines_for_file(argv[i])) {
      printf("Failed to write %s\n", argv[i]);
      return -1;
    }
  }

  return 0;
}