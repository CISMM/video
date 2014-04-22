// This file has been modified to create a raw video file rather than
// individual files.  This is in the hopes that it will be able to write
// fast enough to take all the data to disk without dropping frames.
// It is based on the simple_take.c file from the EDT SDK.  See below
// for the original comments from the file.  This program will only
// work on Windows (boo!)

// On IDE drives, we can't capture at 120 Hz -- we only get around 80 Hz.  Add a
// flag to skip a number of samples before saving each.  Looks like we
// might get away with only 2.  This is not a problem for fast enough SCSI
// drives.

/*
 * simple_take.c
 * 
 * Example program to show usage of EDT PCI DV library to acquire and save
 * single or multiple images from device connected to EDT high speed digital
 * video interface, using ring buffers to pipeline image acquisition and
 * processing.
 * 
 * This example is provided as a starting point example for adding digital video
 * acquisition to a user program.  Since it does the ring-buffer/ pipelining
 * thing, the name is a bit misleading -- it really isn't the simplest way to
 * do image acquisition -- for that see simpler_take.c.
 * 
 * For more complex operations, including error detection, diagnostics, changing
 * camera exposure, gain and blacklevel, and tuning the acquisition in
 * various ways, refer to the "take.c" example.
 * 
 * For a sample Windows GUI application code, see wintake.
 * 
 * (C) 1997-2002 Engineering Design Team, Inc.
 */
#include "edtinc.h"

const char VERSION_STRING[] = "01.03";

void
usage(char *progname)
{
    puts("");
    printf("%s: simple example program that acquires images from an\n", progname);
    printf("EDT Digital Video Interface board (PCI DV, PCI DVK, etc.)\n");
    puts("");
    printf("usage: %s\n", progname);
#ifdef _NT_
    printf("  -b fname 	      output to MS bitmap file\n");
#else
    printf("  -b fname        output to Sun Raster file\n");
#endif
    printf("  -l loops        number of loops (images to take)\n");
    printf("  -N numbufs      number of ring buffers (see users guide) (default 4)\n");
    printf("  -u unit         %s unit number (default 0)\n", EDT_INTERFACE);
    printf("  -~	      swap every pair of lines before writing the file (bug work-around)\n");
    printf("  -s skipframes   write every Nth frame (default 1)\n");
}

/*
 * main
 */
int main(int argc, char **argv)
{
    int     i;
    int     unit = 0;
    int     timeouts = 0, first_timeouts = 0, last_timeouts = 0;
    char   *progname = argv[0];
    char   *cameratype;
    char    filename[2048];
    FILE   *outfile;
    int     numbufs = 4;
    int     started;
    u_char *image_p;
    PdvDev *pdv_p;
    char    errstr[64];
    int     loops = 1;
    int     width, height, depth;
    char    devname[128];
    int     channel = 0;
    double  dtime;
    int	    skipframes = 1;	//< Skip frames to reduce required disk bandwidth
    bool    swaplines = false;	//< Swap every pair of lines (bug fix for EDT driver for Pulnix)

    devname[0] = '\0';
    *filename = '\0';

    /*
     * process command line arguments
     */
    --argc;
    ++argv;
    while (argc && ((argv[0][0] == '-') || (argv[0][0] == '/')))
    {
	switch (argv[0][1])
	{
	case 'u':		/* device unit number */
	    ++argv;
	    --argc;
	    if ((argv[0][0] >= '0') && (argv[0][0] <= '9'))
		unit = atoi(argv[0]);
	    else
		strncpy(devname, argv[0], sizeof(devname) - 1);
	    break;

	case 'c':		/* device unit number */
	    ++argv;
	    --argc;
	    if ((argv[0][0] >= '0') && (argv[0][0] <= '9'))
		channel = atoi(argv[0]);
	    break;

	case 'N':
	    ++argv;
	    --argc;
	    numbufs = atoi(argv[0]);
	    break;

	case 's':
	    ++argv;
	    --argc;
	    skipframes = atoi(argv[0]);
	    break;

	case '~':
	    swaplines = true;
	    break;

	case 'b':		/* bitmap save filename */
	    ++argv;
	    --argc;
	    strcpy(filename, argv[0]);
	    break;

	case 'l':
	    ++argv;
	    --argc;
	    loops = atoi(argv[0]);
	    break;

	default:
	    fprintf(stderr, "unknown flag -'%c'\n", argv[0][1]);
	case '?':
	case 'h':
	    usage(progname);
	    return(-1);
	}
	argc--;
	argv++;
    }
    printf("%s version %s\n", progname, VERSION_STRING);

    /*
     * open the interface
     * 
     * EDT_INTERFACE is defined in edtdef.h (included via edtinc.h)
     *
     * edt_parse_unit_channel and pdv_open_channel) are equivalent to
     * edt_parse_unit and pdv_open except for the extra channel arg and
     * would normally be 0 unless there's another camera (or simulator)
     * on the second channel (camera link) or daisy-chained RCI (PCI FOI)
     */
    if (devname[0])
    {
	unit = edt_parse_unit_channel(devname, devname, EDT_INTERFACE, &channel);
    }
    else
    {
	strcpy(devname, EDT_INTERFACE);
    }

    if ((pdv_p = pdv_open_channel(devname, unit, channel)) == NULL)
    {
	sprintf(errstr, "pdv_open_channel(%s%d_%d)", devname, unit, channel);
	pdv_perror(errstr);
	return(-1);
    }

    /*
     * get image size and name for display, save, printfs, etc.
     */
    width = pdv_get_width(pdv_p);
    height = pdv_get_height(pdv_p);
    depth = pdv_get_depth(pdv_p);
    cameratype = pdv_get_cameratype(pdv_p);

    if (swaplines && (height % 2 != 0)) {
      fprintf(stderr,"Cannot swap lines with odd-height image (%d)\n", height);
      return(-1);
    }
    unsigned char *swapbuf = new unsigned char[width];
    if (swapbuf == NULL) {
      fprintf(stderr, "Out of memory trying to allocate swap buffer\n");
      return(-1);
    }

    // Make sure we have an 8-bit camera.  Otherwise, bail.
    if (depth != 8) {
      fprintf(stderr, "Cannot handle camera with other than 8 bits\n");
      pdv_close(pdv_p);
      return(-1);
    }

    /*
     * allocate four buffers for optimal pdv ring buffer pipeline (reduce if
     * memory is at a premium)
     */
    pdv_multibuf(pdv_p, numbufs);

    printf("reading %d image%s from '%s'\nwidth %d height %d depth %d\n",
	   loops, loops == 1 ? "" : "s", cameratype, width, height, depth);

    // Open the output file for writing, unless we don't have a filename.
    // If we don't have a filename, then exit with an error.
    if (strlen(filename) == 0) {
      fprintf(stderr,"No output filename specified!\n");
      return(-1);
    }
    if ( (outfile = fopen(filename, "wb")) == NULL) {
      perror("Cannot open output file");
      return(-1);
    }

    // Clear the timeouts value.
    first_timeouts = last_timeouts = pdv_timeouts(pdv_p);

    /*
     * prestart the first image or images outside the loop to get the
     * pipeline going. Start multiple images unless force_single set in
     * config file, since some cameras (e.g. ones that need a gap between
     * images or that take a serial command to start every image) don't
     * tolerate queueing of multiple images
     */
    if (pdv_p->dd_p->force_single)
    {
	pdv_start_image(pdv_p);
	started = 1;
    }
    else
    {
	pdv_start_images(pdv_p, numbufs);
	started = numbufs;
    }

    (void) edt_dtime();		/* initialize time so that the later check will be zero-based. */
    for (i = 0; i < loops; i++)
    {
	/*
	 * get the image and immediately start the next one (if not the last
	 * time through the loop). Processing (saving to a file in this case)
	 * can then occur in parallel with the next acquisition
	 */
	image_p = pdv_wait_image(pdv_p);

	if (i <= loops - started)
	{
	    pdv_start_image(pdv_p);
	}
	timeouts = pdv_timeouts(pdv_p);

	if (timeouts > last_timeouts)
	{
	    /*
	     * pdv_timeout_cleanup helps recover gracefully after a timeout,
	     * particularly if multiple buffers were prestarted
	     */
	    if (numbufs > 1)
	    {
		int     pending = pdv_timeout_cleanup(pdv_p);

		pdv_start_images(pdv_p, pending);
	    }
	    last_timeouts = timeouts;
	}

	// Add this image into the raw video file unless this is a frame
	// that we are skipping.
	if ( (i % skipframes) == 0) {

	  // If we're supposed to swap every other line, do that here.
	  // The image lines are swapped in place.
	  if (swaplines) {
	    int j;    // Indexes into the lines, skipping every other.

	    for (j = 0; j < height; j += 2) {
	      memcpy(swapbuf, image_p + j*width, width);
	      memcpy(image_p + (j+1)*width, image_p + j*width, width);
	      memcpy(image_p + (j+1)*width, swapbuf, width);
	    }
	  }

	  // Write the image to the file
	  if (fwrite(image_p, width*height, 1, outfile) != 1) {
	    perror("Failed to write image to file");
	    pdv_close(pdv_p);
	    fclose(outfile);
	    return(-1);
	  }
	}
    }
    dtime = edt_dtime();
    fclose(outfile);
    puts("");

    printf("%d images from camera, %lg seconds, %d timeouts\n", loops, dtime, timeouts-first_timeouts);
    if (loops > 3) {
	printf("Read %lf frames/sec from camera\n", (double) (loops) / dtime);
	printf("Wrote %lf frames/sec to disk\n", (double) (loops) / (dtime * skipframes));
    }

    /*
     * if we got timeouts it indicates there is a problem
     */
    if (last_timeouts) {
	printf("check camera and connections\n");
    }
    pdv_close(pdv_p);

    return(0);
}
