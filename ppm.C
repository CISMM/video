#include	<stdio.h>
#ifdef	_WIN32
#include	<io.h>
#endif
#include	<sys/types.h>
#ifndef	_WIN32
#include	<unistd.h>
#endif
#include	<stdlib.h>
#include	<fcntl.h>
#include	<string.h>
#include	<ctype.h>
#include	"ppm.h"

// For Windows binary file format
#ifndef	O_BINARY
#define	O_BINARY	0
#endif

/*	This routine will read an integer from the file whose descriptor
 * that is passed to it.  White space before the integer will be skipped.
 * One character past the integer will be read, so it will hopefully be
 * white space that is not needed.
 *	This routine will skip anything between a '#' character and the
 * end of the line on which the character is found - this information is
 * a comment in the ppm file.
 *	This routine returns 0 on success and -1 on failure. */

static	int	read_int(int des, int *i)
{
	int	temp = 0;
	char	c;
	int	done_skipping = 0;

	/* Look for the first character, skipping any white space or comments */
	do {
		do {
			read(des, &c, 1);
		} while ( isspace(c) );

		/* If this is the comment character, skip to the end of line */
		if (c == '#') {	/* Comment - skip this line and start on next */
			do {
				if (read(des, &c, 1) != 1) {
					return(-1);
				}
			} while (c != '\n');
		} else {	/* No comment - this better be the integer! */
			done_skipping = 1;
		}
	} while (!done_skipping);
	if (!isdigit(c)) return(-1);

	/* Read the characters in and make the integer */
	while (isdigit(c)) {
		temp = temp*10 + (c - '0');
		read(des,&c,1);
	}

	*i = temp;
	return(0);
}

/*	This routine will read a ppm file from the file whose descriptor
 * is passed to it into an internal structure. */

PPM::PPM(const char *filename) : nx(0), ny(0)
{
	int		infile;

	char		inbuf[200];
	int		maxc;		/* Maximum color value */

	PPMROW		ppmrow;		/* One row from the ppm file */

	int		x,y;

	/* Open the file for reading */
	if ( (infile = open(filename,O_RDONLY|O_BINARY)) == -1) {
		perror("Could not open ppm file");
		return;
	}

	/* Scan the header information from the ppm file */
	if (read(infile,inbuf, 2) != 2) {
		perror("Could not read magic number from ppm file");
		perror("Could not open ppm file");
		return;
	}
	if (strncmp(inbuf,"P6",2) != 0) {
		fprintf(stderr,"Magic number not P6 (not ppm file)\n");
		return;
	}
	if (read_int(infile,&nx)) {
		perror("Could not read number of x pixels");
		return;
	}
	if (read_int(infile,&ny)) {
		perror("Could not read number of y pixels");
		return;
	}
	if (read_int(infile,&maxc)) {
		perror("Could not read maximum value");
		return;
	}
	if (maxc != 255) {
		fprintf(stderr,"Max color not 255.  Can't handle it.\n");
		return;
	}

	/* Set up the buffer for one row of the image */
	if ( (ppmrow = (P_COLOR *)malloc(nx*sizeof(P_COLOR))) == NULL) {
		fprintf(stderr,"Not enough memory for ppm row buffer\n");
		return;
	}

	/* Allocate the ppm info structure and fill it in */
	if ((pixels = (PPMROW*)malloc(ny*sizeof(PPMROW*))) == NULL) {
		fprintf(stderr,"Not enough memory for ppm info buffer\n");
		return;
	}
	for (y = 0; y < ny; y++) {
		if ((pixels[y] =
			(PPMROW)malloc(nx*sizeof(P_COLOR))) == NULL) {
			fprintf(stderr,"Not enough memory for ppm pixels\n");
			return;
		}
	}

	for (y = 0; y < ny; y++) {

	  /* Read one row of values from the file */
	  if (read(infile, ppmrow, nx*sizeof(P_COLOR)) != (int)(nx*sizeof(P_COLOR))) {
		perror("Cannot read line of pixels from ppm file");
		return;
	  }

	  // Copy the values into the texture pixels
	  for (x = 0; x < nx; x++) {
	    ((pixels)[y])[x][0] = ppmrow[x][0];
	    ((pixels)[y])[x][1] = ppmrow[x][1];
	    ((pixels)[y])[x][2] = ppmrow[x][2];
	  }

	}

	free(ppmrow);

} // End of PPM::PPM()


/*	This routine will return the given element from the ppm info
 * structure.  The red, green, and blue values are filled in.
 *	0 is returned on success and -1 on failure. */

int	PPM::Tellppm(int x,int y, int *red,int *green,int *blue) const
{
	if ( (x < 0) || (x >= nx) ) return(-1);
	if ( (y < 0) || (y >= ny) ) return(-1);

	*red = pixels[y][x][0];
	*green = pixels[y][x][1];
	*blue = pixels[y][x][2];

	return(0);
}


/*	This routine will set the given element in the ppm info
 * structure.  The red, green, and blue values are filled in.
 *	0 is returned on success and -1 on failure. */

int	PPM::Putppm(int x,int y, int red,int green,int blue)
{
	if ( (x < 0) || (x >= nx) ) return(-1);
	if ( (y < 0) || (y >= ny) ) return(-1);

	pixels[y][x][0] = red;
	pixels[y][x][1] = green;
	pixels[y][x][2] = blue;

	return(0);
}


/*	This routine will return the given element from the ppm info
 * structure.  The red, green, and blue values are filled in.
 *	This routine takes a float from -1 to 1 in x and y and maps this into
 * the whole range of x and y for the ppm file.
 *	Bilinear interpolation is performed between the pixel values to
 * keep from getting horrible aliasing when the picture is larger than
 * the texture map.
 *	This routine inverts y so that the ppm image comes up rightside-
 * up.
 *	0 is returned on success and -1 on failure. */

int	PPM::Value_at_normalized(double normx,double normy, int *red,int *green,int *blue) const
{
	float	sx = (float)normx, sy = (float)normy;	/* Scaled to 0-->1 */
	int x,y;			/* Truncated integer coordinate */
	double	dx,dy;			/* Delta from the integer coord */

	/* Map normx, normy to 0-->1 range */
	sx = (sx+1)/2;
	sy = 1-(sy+1)/2;

	if ( (sx < 0) || (sx >= 1) ) {
		*red = *green = *blue = 0;
		return(0);
	}
	if ( (sy < 0) || (sy >= 1) ) {
		*red = *green = *blue = 0;
		return(0);
	}

	x = (int)( sx * (nx - 1) );
	y = (int)( sy * (ny - 1) );

	/* Find the normalized distance from the integer coordinate
	 * (the d's will be from 0 to 1 and will represent weighting) */

	dx = sx * (nx - 1) - x;
	dy = sy * (ny - 1) - y;

	/* If we are at the right or bottom edge, no interpolation */

	if ( (x == (nx - 1)) || (y == (ny - 1)) ) {
		*red = pixels[y][x][0];
		*green = pixels[y][x][1];
		*blue = pixels[y][x][2];

	/* Otherwise, perform interpolation based on the d's */
	} else {
		*red = (int)( pixels[y][x][0] * (1-dx)*(1-dy) +
			pixels[y][x+1][0] * dx*(1-dy) +
			pixels[y+1][x][0] * (1-dx)*dy +
			pixels[y+1][x+1][0] * dx*dy );
		*green=(int)( pixels[y][x][1] * (1-dx)*(1-dy) +
			pixels[y][x+1][1] * dx*(1-dy) +
			pixels[y+1][x][1] * (1-dx)*dy +
			pixels[y+1][x+1][1] * dx*dy );
		*blue =(int)( pixels[y][x][2] * (1-dx)*(1-dy) +
			pixels[y][x+1][2] * dx*(1-dy) +
			pixels[y+1][x][2] * (1-dx)*dy +
			pixels[y+1][x+1][2] * dx*dy );
	}

	return(0);
}

int	PPM::Write_to(char *filename)
{
	FILE	*outfile;
	int	y;

	/* Open the file for writing */
#ifdef	_WIN32
	if ( (outfile = fopen(filename,"wb")) == NULL) {
#else
	if ( (outfile = fopen(filename,"w")) == NULL) {
#endif
		perror("PPM:Write_to(): Could not open ppm file");
		return(-1);
	}

	/* Write the header information to the ppm file */
	if (fprintf(outfile,"P6\n%d %d\n255\n",nx,ny) < 0) {
		perror("PPM:Write_to(): Couldn't write PPM header");
		fclose(outfile);
		return(-1);
	}

	for (y = 0; y < ny; y++) {

	  /* Write one row of values to the file */
	  if (fwrite(pixels[y], sizeof(P_COLOR),nx,outfile) != (unsigned)(nx)) {
		perror("PPM:Write_to(): Cannot write line of pixels");
		fclose(outfile);
		return(-1);
	  }
	}

	if (fclose(outfile) == EOF) {
		perror("PPM:Write_to(): Cannot close file");
		return(-1);
	}

	return(0);
}
