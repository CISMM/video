// Set this "if" to zero to run the non-lossy demo program.
// Set it to one to run the "fastest possible" demo program.

#if 1

#include <stdio.h>
#include <stdlib.h>
#include "master.h"
#include "pvcam.h"

static void FocusContinuous( int16 hCam );

int main(int argc, char **argv)
{
    char cam_name[CAM_NAME_LEN]; /* camera name */
    int16 hCam; /* camera handle */
    boolean avail_flag; /* ATTR_AVAIL, param is available */

    /* Initialize the PVCam Library and Open the First Camera */
    pl_pvcam_init();
    pl_cam_get_name( 0, cam_name );
    pl_cam_open(cam_name, &hCam, OPEN_EXCLUSIVE );

    /* check for circular buffer support */
    if( pl_get_param( hCam, PARAM_CIRC_BUFFER, ATTR_AVAIL, &avail_flag ) && avail_flag )
	  FocusContinuous( hCam );
    else
	  printf( "circular buffers not supported\n" );
    pl_cam_close( hCam );
    pl_pvcam_uninit();
    return 0;
}

void FocusContinuous( int16 hCam )
{
    rgn_type region = { 0, 511, 1, 0, 511, 1 };
    uns32 size;
    uns16 *buffer;
    int16 status;
    void_ptr address;
    uns16 numberframes = 5;
    static uns32 last_buffer_cnt = 0; //< How many buffers have been read?

    /* Init a sequence set the region, exposure mode and exposure time */
    pl_exp_init_seq();
    pl_exp_setup_cont( hCam, 1, &region, TIMED_MODE, 100, &size, CIRC_OVERWRITE );

    /* set up a circular buffer of 3 frames */
    buffer = (uns16*)malloc( size * 3 );

    /* Start the acquisition */
    printf( "Collecting %i Frames, using OVERWRITE\n", numberframes );
    pl_exp_start_cont(hCam, buffer, size );

    /* ACQUISITION LOOP */
    printf("Acquisition loop\n");
    while( numberframes ) {
      /* wait for data or error */
      printf("  Checking status\n");
      uns32 byte_cnt, buffer_cnt;
      while( pl_exp_check_cont_status( hCam, &status, &byte_cnt, &buffer_cnt) &&
	(status != READOUT_COMPLETE) && (status != READOUT_FAILED) ) {

	//XXX I was hoping that when we had enough bytes for the frame, we could
	// go on and get it.  This does not seem to work.  It doesn't work even if
	// we wait for the number of bytes to match
//	printf("%d.%d ",buffer_cnt, byte_cnt);
	if (byte_cnt == 512*512*2 * (6-numberframes)) { printf("%d.%d ",buffer_cnt, byte_cnt); break; }
      };

      /* Check Error Codes */
      if( status == READOUT_FAILED ) {
	printf( "Data collection error: %i\n", pl_error_code() );
	break;
      }

      printf("  Getting latest frame\n");
      if ( pl_exp_get_latest_frame( hCam, &address )) {
	/* address now points to valid data */
	printf( "Center Three Points: %i, %i, %i\n",
	*((uns16*)address + size/sizeof(uns16)/2 - 1),
	*((uns16*)address + size/sizeof(uns16)/2),
	*((uns16*)address + size/sizeof(uns16)/2 + 1) );
	numberframes--;
	printf( "Remaining Frames %i\n", numberframes );
      } else {
	fprintf(stderr, "Error getting latest frame\n");
	return;
      }
    } /* End while */

    /* Stop the acquisition */
    pl_exp_stop_cont(hCam,CCS_HALT);

    /* Finish the sequence */
    pl_exp_finish_seq( hCam, buffer, 0);

    /*Uninit the sequence */
    pl_exp_uninit_seq();
    free( buffer );
}

#else

#include <stdio.h>
#include <stdlib.h>
#include "master.h"
#include "pvcam.h"
static void AcquireContinuous( int16 hCam );
int main(int argc, char **argv)
{
char cam_name[CAM_NAME_LEN]; /* camera name */
int16 hCam; /* camera handle */
boolean avail_flag; /* ATTR_AVAIL, param is available */
/* Initialize the PVCam Library and Open the First Camera */
pl_pvcam_init();
pl_cam_get_name( 0, cam_name );
pl_cam_open(cam_name, &hCam, OPEN_EXCLUSIVE );
/* check for circular buffer support */
if( pl_get_param( hCam, PARAM_CIRC_BUFFER, ATTR_AVAIL, &avail_flag ) &&
avail_flag )
AcquireContinuous( hCam );
else
printf( "circular buffers not supported\n" );
pl_cam_close( hCam );
pl_pvcam_uninit();
return 0;
}
void AcquireContinuous( int16 hCam )
{
rgn_type region = { 0, 511, 1, 0, 511, 1 };
uns32 size;
uns16 *buffer;
int16 status;
uns32 not_needed;
void_ptr address;
uns16 numberframes = 5;
/* Init a sequence set the region, exposure mode and exposure time */
pl_exp_init_seq();
pl_exp_setup_cont( hCam, 1, &region, TIMED_MODE, 100, &size,
CIRC_NO_OVERWRITE );
/* set up a circular buffer of 3 frames */
buffer = (uns16*)malloc( size * 3 );
/* Start the acquisition */
printf( "Collecting %i Frames using NO_OVERWRITE\n", numberframes );
pl_exp_start_cont(hCam, buffer, size );
/* ACQUISITION LOOP */
while( numberframes ) {
/* wait for data or error */
while( pl_exp_check_cont_status( hCam, &status, &not_needed,
&not_needed ) &&
(status != READOUT_COMPLETE && status != READOUT_FAILED) );
/* Check Error Codes */
if( status == READOUT_FAILED ) {
printf( "Data collection error: %i\n", pl_error_code() );
break;
}
if ( pl_exp_get_oldest_frame( hCam, &address )) {
/* address now points to valid data */
printf( "Center Three Points: %i, %i, %i\n",
*((uns16*)address + size/sizeof(uns16)/2 - 1),
*((uns16*)address + size/sizeof(uns16)/2),
*((uns16*)address + size/sizeof(uns16)/2 + 1) );
numberframes--;
printf( "Remaining Frames %i\n", numberframes );
pl_exp_unlock_oldest_frame( hCam );
}
} /* End while */
/* Stop the acquisition */
pl_exp_stop_cont(hCam,CCS_HALT);
/* Finish the sequence */
pl_exp_finish_seq( hCam, buffer, 0);
/*Uninit the sequence */
pl_exp_uninit_seq();
free( buffer );
}

#endif