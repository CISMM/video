/****************************************************************************

  GLUItake

  A modification of simple_take from the EDT examples and GLUT/GLUI examples 
  found in the GLUT/GLUI source tree. 

  This program allows one to preview a video stream from the EDT/Pulnix camera
  before and while writing video to disk at 120 frames per second.

  -----------------------------------------------------------------------
	   
  07/16/2004 Jeremy Cribb (jcribb@email.unc.edu)

****************************************************************************/

#include <string.h>
#include <process.h>
#include <windows.h>
#include <string>
#include "glui.h"
#include <GL/glut.h>
#include "edtinc.h"
#include "vrpn_shared.h"

const char VERSION_STRING[] = "1.00";
#define NUM_THREADS 2
#define COLLECT 1
#define DISPLAY 2


// function prototypes
void start_collection();
/*  This attempt to abort data collection is unstable  */
void abort_collection();
void collect(void *);
void display(void *);
void printLastError();
void displayFrame(void);
void setfilename(int);
void showCurrentFrame();
void open_camera();
void close_and_exit();
void close_camera();
 
/*** These are the live variables passed into GLUI ***/
int   main_window;

int   unit       = 0;
int   channel    = 0;
int   numbufs    = 300;
int   loops      = 1200;
int   gain = 128;
int   duration   = 5; // seconds
int   fps_idx = 0;
int   fpsid = 0;
int   skipframes = 1;
int   swaplines  = 1;
int   timestamps = 1;
//int   integrate = 0;

char  filename[2048];
char  devname[128];


/*** other global variables.  Maybe I'll fix that soon.  ***/
bool collecting = false;
bool showing = false;
bool abort_acquisition = false;
bool camera_open = false;
bool display_frames = true;

int i = 0;
char   *cameratype;
PdvDev *pdv_p;
struct timeval start_time;

int width = 648;
int height= 484;
int depth = 8;
int showframe = 4; // Display every i % showframe in GL window
char *fps_list[] = {"120", "60", "40","30","24","20","15", "12", "10", "8", "5", "1", "0.1", "0.01"};
int  skip_list[] = {   1,    2,    3,   4,   5,   6,   8,   10,   12,  15,  24,  120, 1200, 12000};
unsigned char *dispbuf = new unsigned char[height*width];
unsigned char *GLdispbuf = new unsigned char[4*(height*width)];
unsigned char *swapbuf = new unsigned char[width];


HANDLE hThread[NUM_THREADS];
CRITICAL_SECTION hUpdateMutex;



/***************************************** myGlutIdle() ***********/

void myGlutIdle( void )
{
  /* According to the GLUT specification, the current window is 
     undefined during an idle callback.  So we need to explicitly change
     it if necessary */
  if ( glutGetWindow() != main_window ) 
    glutSetWindow(main_window);  

  if (collecting == false){
    if(!camera_open)
      open_camera();

    showCurrentFrame();
  }
  
  glutPostRedisplay(); 
  
  // update the current desired framerate by changing value of skipframes via fps_idx
  skipframes = skip_list[fps_idx];

  // set camera parameters when idle runs  !*! Can cause unstable behavior !*!
  // We used a static var which is persistent and as such gain is updated on when it's value is changed.
  {
    static int old_gain = -5000;
    if (old_gain != gain) {
      pdv_set_gain(pdv_p, (255 - gain));
      old_gain = gain;
    }
  }
}


/**************************************** myGlutReshape() *************/

void myGlutReshape( int x, int y )
{
  
  float xy_aspect;

  xy_aspect = (float)x / (float)y;
  glViewport( 0, 0, width, height );

  
  glMatrixMode( GL_PROJECTION );
  glLoadIdentity();
  //glFrustum( -xy_aspect*.08, xy_aspect*.08, -.08, .08, .1, 15.0 );

  glutPostRedisplay();
  
}

/***************************************** myGlutDisplay() *****************/

void myGlutDisplay( void )
{
  glDrawBuffer(GL_BACK);
  glClearColor(0.0, 0.0, 0.0, 0.0);
  glClear(GL_COLOR_BUFFER_BIT);
  glViewport(0, 0, width, height);

  for(int r=height; r>=0; r--) {
    for(int c=0; c<width; c++) {
        GLdispbuf[0 + 4 * (c + width * r)] = dispbuf[(c + width * r)]; //red
		GLdispbuf[1 + 4 * (c + width * r)] = dispbuf[(c + width * r)];//green
		GLdispbuf[2 + 4 * (c + width * r)] = dispbuf[(c + width * r)];//blue
		GLdispbuf[3 + 4 * (c + width * r)] = 255;//alpha
    }
  }
  
  // Store the pixels from the image into the frame buffer
  // so that they cover the entire image (starting from lower-left
  // corner, which is at (-1,-1)).
  glRasterPos2i(-1,-1);

  glDrawPixels(width,height, GL_RGBA, GL_UNSIGNED_BYTE, GLdispbuf);

  glutSwapBuffers();

  //Sleep((long)(65));
}


/**************************************** main() ********************/

int main(int argc, char* argv[])
{
  // initial settings for device and file names (just to be safe).
    devname[0] = '\0';
    *filename = '\0';

    // open the camera interface
    open_camera();

    // report stuff
	fprintf(stderr, "\nGLUItake Pulnix Frame Grabber, version %s\n\n", VERSION_STRING);
	fprintf(stderr, "exposure: %i\n", pdv_get_exposure(pdv_p));
	fprintf(stderr, "width: %i\n", pdv_get_width(pdv_p));
	fprintf(stderr, "height: %i\n", pdv_get_height(pdv_p));
	fprintf(stderr, "depth: %i\n", pdv_get_depth(pdv_p));

  /****************************************/
  /*   Initialize GLUT and create window  */
  /****************************************/

  
  glutInitDisplayMode( GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH );
  glutInitWindowPosition( 200, 200 );
  glutInitWindowSize( 648, 484 );
 
  main_window = glutCreateWindow( "GLUItake Image" );
  glutPositionWindow(400,200);

  glutDisplayFunc( myGlutDisplay );
  glutReshapeFunc( myGlutReshape );  

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();

  /****************************************/
  /*         Here's the GLUI code         */
  /****************************************/

  GLUI *glui = GLUI_Master.create_glui( "GLUItake" );
  
  glui->add_checkbox( "Swap Lines", &swaplines );
  glui->add_checkbox( "TimeStamps", &timestamps );
//  glui->add_checkbox( "Integrate", &integrate );

  GLUI_Spinner *unit_spinner = 
    glui->add_spinner( "Unit:", GLUI_SPINNER_INT, &unit );
    unit_spinner->set_int_limits( 0, 9 ); 
  
  GLUI_Spinner *channel_spinner = 
    glui->add_spinner( "Channel:", GLUI_SPINNER_INT, &channel );
    channel_spinner->set_int_limits( 0, 9 ); 

  GLUI_Spinner *gain_spinner = 
    glui->add_spinner( "Gain:", GLUI_SPINNER_INT, &gain );
    gain_spinner->set_int_limits( 0, 255 ); 
    
  GLUI_Spinner *numbufs_spinner = 
    glui->add_spinner( "Buffers:", GLUI_SPINNER_INT, &numbufs );
    numbufs_spinner->set_int_limits( 0, 999 ); 

  GLUI_Spinner *duration_spinner = 
    glui->add_spinner( "Duration:", GLUI_SPINNER_INT, &duration );
    duration_spinner->set_int_limits( 0, 1000000 ); 

  GLUI_Listbox *fps_listbox =
	glui->add_listbox( "fps:", &fps_idx);
    for(int idx=0; idx<14; idx++ )
      fps_listbox->add_item( idx, fps_list[idx] );	

  GLUI_EditText *filename_EditText =
    glui->add_edittext( "filename:", GLUI_EDITTEXT_TEXT, filename);

  GLUI_Button *start_capture =
    glui->add_button( "Start Capture", 0, (GLUI_Update_CB) start_collection);

  /* */
  GLUI_Button *cancel_capture =
    glui->add_button( "Abort Capture", 0, (GLUI_Update_CB) abort_collection);
  

  GLUI_Button *quit =
    glui->add_button( "Quit", 0, (GLUI_Update_CB) close_and_exit);

  glui->set_main_gfx_window( main_window );

  // We register the idle callback with GLUI, *not* with GLUT 
  GLUI_Master.set_glutIdleFunc( myGlutIdle ); 

  glutMainLoop();
    
}


void start_collection()
{
  collecting = true;
  showing = false;
  abort_acquisition = false;
  hThread[COLLECT] = (HANDLE) _beginthread(collect, 0, NULL);
  close_camera();
}

/*  This attempt to abort data collection is unstable */
void abort_collection()
{
	abort_acquisition = true;
	fprintf(stderr, "\n** ACQUISITION ABORTED! **\n\n", VERSION_STRING);
}


////////////////////
void collect(void *)
{

    HANDLE me = GetCurrentThread();
    //SetPriorityClass(me, REALTIME_PRIORITY_CLASS);
    SetThreadPriority(me, THREAD_PRIORITY_TIME_CRITICAL);
    SetThreadAffinityMask(me, 0x1);

	// determine the number of frames to collect at full bandwidth (120 fps)
	int fps = 120;
	int frames_at_full_bandwidth = duration * fps;
	loops = frames_at_full_bandwidth;
	
	// time-related variables
    int     timeouts = 0;
    int     first_timeouts = 0;
    int	    last_timeouts = 0;
	struct timeval *pdv_time = new timeval[loops];
    double  dtime;
	u_int   timestamp[2];
	double  curtime = 0;
	double *timestack = new double[loops];
    
	FILE   *outfile;
    int     started;
    u_char *image_p;
    
	FILE   *out_time_file;
	char   *tfilename;


    open_camera();

	fprintf(stderr,"--------------------------------------------------------\n");

    if (swaplines && (height % 2 != 0)) {
      fprintf(stderr,"Cannot swap lines with odd-height image (%d)\n", height);
      return;
    }

    //unsigned char *swapbuf = new unsigned char[width];
   
    if (swapbuf == NULL) {
      fprintf(stderr, "Out of memory trying to allocate swap buffer\n");
      return;
    }

    // Make sure we have an 8-bit camera.  Otherwise, bail.
    if (depth != 8) {
      fprintf(stderr, "Cannot handle camera with other than 8 bits\n");
      pdv_close(pdv_p);
      return;
    }
	
	// get starting time as close as possible to frame capture
	gettimeofday(&start_time, NULL);
//	fprintf(stderr, "--STARTTIME--\n");
//	fprintf(stderr, " sec: %d\n", start_time.tv_sec);
//	fprintf(stderr, "usec: %d\n", start_time.tv_usec);
	
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
      return;
	}
	else {
	  fprintf(stderr,"Streaming data to %s\n", filename);
    }

    if ( (outfile = fopen(filename, "wb")) == NULL) {
      perror("Cannot open output file");
      return;
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
    
/*** this is where the output data is collected ***/
    for (i = 0; i < loops; i++)
    {
	/*
	 * get the image and immediately start the next one (if not the last
	 * time through the loop). Processing (saving to a file in this case)
	 * can then occur in parallel with the next acquisition
	 */

     //  EnterCriticalSection(&hUpdateMutex); // threading

/*  This attempt at abort functions is unstable */
		// Check if the user wants to cancel current data collection
		if (abort_acquisition)
			break;

      
		if (timestamps)  {
	       image_p = pdv_wait_image_timed(pdv_p, timestamp);
           curtime = (double) timestamp[0] * 1000000000L + timestamp[1];
	       curtime /= 1000000000.0;
	       timestack[i] = curtime;
		   pdv_time->tv_sec = (long) timestamp[0];  // value is in seconds
		   pdv_time->tv_usec = (long) (timestamp[1]/1000); // value is now in microseconds
		}
		else {
			image_p = pdv_wait_image(pdv_p);
		}


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
	  
	  // Are we displaying this frame?
	  if ( (i % showframe ) == 0) {
	    memcpy(dispbuf, image_p, width*height);
	    
	  }

	  // Write the image to the file
	  if (fwrite(image_p, width*height, 1, outfile) != 1) {
	    perror("Failed to write image to file");
	    pdv_close(pdv_p);
	    fclose(outfile);
	    return;
	  }

      //    LeaveCriticalSection(&hUpdateMutex);
	}
    }

    dtime = edt_dtime();

    fclose(outfile);
    puts("");

    // test time code 
/*  gettimeofday(&start_time, NULL);
	fprintf(stderr, "--ENDTIME--\n");
	fprintf(stderr, " sec: %d\n", start_time.tv_sec);
	fprintf(stderr, "usec: %d\n", start_time.tv_usec);
*/

    printf("%d images from camera, %lg seconds, %d timeouts\n", loops, dtime, timeouts-first_timeouts);
    if (loops > 3) {
	printf("Read %lf frames/sec from camera\n", (double) (loops) / dtime);
	printf("Wrote %lf frames/sec to disk\n", (double) (loops) / (dtime * skipframes));
    }


    // Write out the timestamps to a file	
	if (timestamps) {
		char buff[2048];
		int pdv_sec = 0;
		int pdv_usec= 0;
		long GLUItake_sec = 0L;
		long GLUItake_usec= 0L;
		double GLUItake_time = 0;
		//struct timeval our_time;

		strcpy(buff,filename);
		tfilename = strcat(buff, ".tstamp.txt");		
		if ( (out_time_file = fopen(tfilename, "wb")) == NULL) {
		  perror("Cannot open output timestamps file"); 
		  return;
		}
		
		for(i=0; i<loops; i+=skipframes) {
		   pdv_sec = (long) timestack[i];
		   pdv_usec = (long) ((timestack[i] - pdv_sec) * 1000000);
		   GLUItake_sec = pdv_sec + start_time.tv_sec;
		   GLUItake_usec = pdv_usec + start_time.tv_usec;
		   
		   // handle overflow in microseconds column
		   if (GLUItake_usec > 1000000L) {
				GLUItake_sec++;
				GLUItake_usec -= 1000000L;
		   }

		   fprintf(out_time_file, "%d\t%d\n", GLUItake_sec, GLUItake_usec);
		}
		fclose(out_time_file);
		printf("timestamps successfully written to: %s\n\n", tfilename);
	}
	else {
		printf("\n");
	}


    /*
     * if we got timeouts it indicates there is a problem
     */
    if (last_timeouts) {  printf("check camera and connections\n");    }
    close_camera();

    collecting = false; 

    return;

	// clear out old filename
/*	for(int j=0; j<2048; j++) {
		filename[j]='\0';
	} */

}


void showCurrentFrame() {
    if(camera_open == false)
      open_camera();

    u_char *preview_image;
    showing = true;

    if (swaplines && (height % 2 != 0)) {
      fprintf(stderr,"Cannot swap lines with odd-height image (%d)\n", height);
      return;
    }

    if (swapbuf == NULL) {
      fprintf(stderr, "Out of memory trying to allocate swap buffer\n");
      return;
    }

    // Make sure we have an 8-bit camera.  Otherwise, bail.
    if (depth != 8) {
      fprintf(stderr, "Cannot handle camera with other than 8 bits\n");
      pdv_close(pdv_p);
      return;
    }

    pdv_start_image(pdv_p);
    preview_image = pdv_wait_image(pdv_p);
    
    if (swaplines) {
      int j;    // Indexes into the lines, skipping every other.
      for (j = 0; j < height; j += 2) {
        memcpy(swapbuf, preview_image + j*width, width);
        memcpy(preview_image + (j+1)*width, preview_image + j*width, width);
        memcpy(preview_image + (j+1)*width, swapbuf, width);
      }
    }
	  	 
    // Copy the frame for displaying
    memcpy(dispbuf, preview_image, width*height);
	    
    Sleep((long)(32.0));
    
}


void open_camera() {
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
    char    errstr[64];

    if (devname[0])
    {
        unit = edt_parse_unit_channel(devname, devname, EDT_INTERFACE, &channel);
    }
    else
    {
		strcpy(devname, EDT_INTERFACE);
    }

    camera_open = true;
    
    if ((pdv_p = pdv_open_channel(devname, unit, channel)) == NULL)
    {
	sprintf(errstr, "pdv_open_channel(%s%d_%d)", devname, unit, channel);
	pdv_perror(errstr);
	return;
    }

    /*
     * get image size and name for display, save, printfs, etc.
     */
    width = pdv_get_width(pdv_p);
    height = pdv_get_height(pdv_p);
    depth = pdv_get_depth(pdv_p);
    cameratype = pdv_get_cameratype(pdv_p);
}

void close_camera() {
  pdv_close(pdv_p);
  camera_open = false;
}

void close_and_exit() {
  close_camera();
  exit(0);
}

