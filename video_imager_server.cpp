// Need this here to keep the DirectShow SDK from causing sprintf() to be
// undefined.
#define NO_DSHOW_STRSAFE

#include  <math.h>
#include  <stdlib.h>
#include  <signal.h>
#include  <string.h>
#include  <stdio.h>
#include  <vrpn_Connection.h>
#include  <vrpn_Imager.h>
#include  <vrpn_Imager_Stream_Buffer.h>
#include  "directx_camera_server.h"

#ifndef	DIRECTX_VIDEO_ONLY
#include  "roper_server.h"
#include  "diaginc_server.h"
#include  "edt_server.h"
#include  "cooke_server.h"
#endif

const int MAJOR_VERSION = 3;
const int MINOR_VERSION = 7;

//-----------------------------------------------------------------
// g_done gets set when the user presses ^C to exit the program.

static	bool  g_done = false;	//< Becomes true when time to exit

// WARNING: On Windows systems, this handler is called in a separate
// thread from the main program (this differs from Unix).  To avoid all
// sorts of chaos as the main program continues to handle packets, we
// set a done flag here and let the main program shut down in its own
// and do all of the stuff we used to do in this handler.
// This flag actually stops two threads: the imager-server thread and
// the main-program thread.  The main-program thread runs the imager
// forwarder (which actually has its own sub-thread, but that death
// is handled internally).

void handle_cntl_c(int) {
  g_done = true;
}

void  Usage(const char *s)
{
  fprintf(stderr,"Usage: %s [-expose msecs] [-every_nth_frame N] [-bin count] [-res x y] [-swap_edt] [-buffers N] [devicename [devicenum]]\n",s);
  fprintf(stderr,"       -expose: Exposure time in milliseconds (default 250)\n");
  fprintf(stderr,"       -every_nth_frame: Discard all but every Nth frame (default 1)\n");
  fprintf(stderr,"       -bin: How many pixels to average in x and y (default 1)\n");
  fprintf(stderr,"       -res: Resolution in x and y (default 320 200)\n");
  fprintf(stderr,"       -swap_edt: Swap lines to fix a bug in the EDT camera\n");
  fprintf(stderr,"       -buffers: use N camera (may only work with the EDT camera) (default 360)\n");
  fprintf(stderr,"       devicename: roper, edt, cooke, diaginc, or directx (default is directx)\n");
  fprintf(stderr,"       devicenum: Which (starting with 1) if there are multiple (default 1)\n");
  fprintf(stderr,"       logfilename: Name of file to store outgoing log in (default NULL)\n");
  exit(-1);
}

//-----------------------------------------------------------------
// This section contains code to initialize the camera and read its
// values.  The server code reads values from the camera and sends
// them over the network.

base_camera_server  *g_camera;	    //< The camera we're going to read from
int		    g_bincount = 1; //< How many pixels to average into one bin in X and Y
double		    g_exposure = 250.0;	//< How long to expose in milliseconds
int		    g_every_nth_frame = 1;	//< Discard all but every nth frame
unsigned	    g_width = 0, g_height = 0;  //< Resolution for DirectX cameras (use default)
int                 g_numchannels = 1;  //< How many channels to send (3 for RGB cameras, 1 otherwise)
int                 g_maxval = 4095;    //< Maximum value available in a channel for this device
bool                g_swap_edt = false; //< Swap lines in EDT to fix bug in driver
unsigned            g_camera_buffers = 360; //< How many camera buffers to ask for

/// Open the camera we want to use (the type is based on the name passed in)
bool  init_camera_code(const char *type, int which = 1)
{
  if (!strcmp(type, "directx")) {
    printf("Opening DirectX Camera %d\n", which);
    g_camera = new directx_camera_server(which, g_width, g_height);
    g_numchannels = 3; // Send RGB
    g_maxval = 255;
    printf("Making sure camera is working\n");
    if (!g_camera->working()) {
      fprintf(stderr,"init_camera_code(): Can't open DirectX camera server\n");
      return false;
    }
  } else if (!strcmp(type, "directx640x480")) {
    printf("Opening DirectX Camera %d\n", which);
    g_camera = new directx_camera_server(which, 640, 480);
    g_numchannels = 3; // Send RGB
    g_maxval = 255;
    printf("Making sure camera is working\n");
    if (!g_camera->working()) {
      fprintf(stderr,"init_camera_code(): Can't open DirectX camera server\n");
      return false;
    }
#ifndef DIRECTX_VIDEO_ONLY
  } else if (!strcmp(type, "roper")) {
    printf("Opening Roper Camera with binning at %d\n", g_bincount);
    g_camera = new roper_server(g_bincount);
    g_numchannels = 1;
    g_maxval = 4095;
    if (!g_camera->working()) {
      fprintf(stderr,"init_camera_code(): Can't open roper camera server\n");
      return false;
    }
  } else if (!strcmp(type, "diaginc")) {
    printf("Opening Diagnostics Inc Camera with binning at %d\n", g_bincount);
    g_camera = new diaginc_server(g_bincount);
    g_numchannels = 1;
    g_maxval = 4095;
    if (!g_camera->working()) {
      fprintf(stderr,"init_camera_code(): Can't open diaginc camera server\n");
      return false;
    }
  } else if (!strcmp(type, "edt")) {
    printf("Opening ETD Camera (using %d buffers)\n", g_camera_buffers);
    g_camera = new edt_server(g_swap_edt, g_camera_buffers);
    g_numchannels = 1;
    g_maxval = 255;
    if (!g_camera->working()) {
      fprintf(stderr,"init_camera_code(): Can't open EDT camera server\n");
      return false;
    }
  } else if (!strcmp(type, "cooke")) {
    printf("Opening Cooke Camera\n");
    g_camera = new cooke_server(g_bincount);
    g_numchannels = 1;
    g_maxval = 65535;
    if (!g_camera->working()) {
      fprintf(stderr,"init_camera_code(): Can't open Cooke camera server\n");
      return false;
    }
#endif
  } else {
    fprintf(stderr,"init_camera_code(): Unknown camera type (%s)\n", type);
    return false;
  }

  return true;
}

void  teardown_camera_code(void)
{
  if (g_camera) { delete g_camera; };
}

//-----------------------------------------------------------------
// This section contains code that does what the server should do

// Handled on a separate thread that continually runs the camera
vrpn_Thread                   *svrthread;//< Thread to handle mainlooping the imager server.
vrpn_Connection               *svrcon;  //< Local loopback connection for imager server
vrpn_Imager_Server	      *svr;	//< Image server to be used to send
int			      svrchan;	//< Server channel index for image data

// Handled in the main thread.
vrpn_Connection               *strcon;	//< Connection for stream server to talk on
vrpn_Imager_Stream_Buffer     *str;     //< Stream server that listens to the imager server

// The function that is called to become the logging thread.
// It reads from the camera and sends to the imager server as
// fast as it can.  It tears down the camera code and deletes
// the server object and connection when the global g_done flag
// is set.
void imager_server_thread_func(vrpn_ThreadData &threadData)
{
  // Mainloop the server and its connection until it is time to quit.
  while (!g_done) {
    int skip;

    // Throw away all but every Nth frame.  We do this by reading them and then
    // reading again.
    for (skip = 1; skip <= g_every_nth_frame; skip++) {
      // Setting the min to be larger than the max means "the whole image"
      g_camera->read_image_to_memory(1,0,1,0,g_exposure);
    }

    // Send the non-skipped frame to VRPN and log.
    if (!g_camera->send_vrpn_image(svr,svrcon,g_exposure,svrchan, g_numchannels)) {
      fprintf(stderr, "Could not send VRPN frame\n");
    }
    svr->mainloop();
    svrcon->mainloop();
    svrcon->save_log_so_far();
  }
  if (svr) { delete svr; svr = NULL; };
  if (svrcon) { delete svrcon; svrcon = NULL; };
  teardown_camera_code();
}

bool  init_server_code(const char *outgoing_logfile_name, bool do_color)
{
  const int svrPORT = 9999;
  const int strPORT = vrpn_DEFAULT_LISTEN_PORT_NO;
  if ( (svrcon = vrpn_create_server_connection(svrPORT, NULL, outgoing_logfile_name)) == NULL) {
    fprintf(stderr, "Could not open imager server connection\n");
    return false;
  }
  if ( (svr = new vrpn_Imager_Server("TestImage", svrcon,
    g_camera->get_num_columns(), g_camera->get_num_rows())) == NULL) {
    fprintf(stderr, "Could not open Imager Server\n");
    return false;
  }
  if (do_color) {
    if ( (svrchan = svr->add_channel("red", "unknown", 0, (float)(g_maxval) )) == -1) {
      fprintf(stderr, "Could not add channel to server image\n");
      return false;
    }
    // This relies on VRPN to hand us sequential channel numbers.  This might be
    // dangerous.
    if ( (svr->add_channel("green", "unknown", 0, (float)(g_maxval) )) == -1) {
      fprintf(stderr, "Could not add channel to server image\n");
      return false;
    }
    if ( (svr->add_channel("blue", "unknown", 0, (float)(g_maxval) )) == -1) {
      fprintf(stderr, "Could not add channel to server image\n");
      return false;
    }
  } else {
    if ( (svrchan = svr->add_channel("mono", "unknown", 0, (float)(g_maxval) )) == -1) {
      fprintf(stderr, "Could not add channel to server image\n");
      return false;
    }
  }
  vrpn_ThreadData td;
  td.pvUD = NULL;
  svrthread = new vrpn_Thread(imager_server_thread_func, td);
  if (svrthread == NULL) {
    fprintf(stderr,"Can't create server thread\n");
    return false;
  }
  if (!svrthread->go()) {
    fprintf(stderr,"Can't start server thread\n");
    return false;
  }
  printf("Local loopback server thread on %d\n", svrPORT);

  // Now handle the main thread's code, which will run an
  // imager forwarder.
  if ( (strcon = vrpn_create_server_connection(strPORT)) == NULL) {
    fprintf(stderr, "Could not open imager stream buffer connection\n");
    return false;
  }
  char  svrname[1024];
  sprintf(svrname,"TestImage@localhost:%d", svrPORT);
  if ( (str = new vrpn_Imager_Stream_Buffer("TestImage", svrname, strcon)) == NULL) {
    fprintf(stderr, "Could not open imager stream buffer\n");
    return false;
  }
  printf("Waiting for video connections on %d\n", strPORT);

  return true;
}

void  teardown_server_code(void)
{
  if (str) { delete str; str = NULL; };
  if (strcon) { delete strcon; strcon = NULL; };
}

//-----------------------------------------------------------------
// Mostly just calls the above functions; split into client and
// server parts is done clearly to help people who want to use this
// as example code.  You could pull the above functions down into
// the main() program body without trouble (except for the callback
// handler, of course).

int main(int argc, char *argv[])
{
  int	i, realparams;		  // How many non-flag command-line arguments
  char	*devicename = "directx";  // Name of the device to open
  int	devicenum = 1;		  // Which, if there are more than one, to open
  char	*logfilename = NULL;	  // Outgoing log file name.

  realparams = 0;
  for (i = 1; i < argc; i++) {
    if (!strncmp(argv[i], "-bin", strlen("-bin"))) {
      if (++i > argc) { Usage(argv[0]); }
      g_bincount = atoi(argv[i]);
      if ( (g_bincount < 1) || (g_bincount > 16) ) {
	fprintf(stderr,"Invalid bincount (1-16 allowed, %d entered)\n", g_bincount);
	exit(-1);
      }
    } else if (!strncmp(argv[i], "-every_nth_frame", strlen("-every_nth_frame"))) {
      if (++i > argc) { Usage(argv[0]); }
      g_every_nth_frame = atoi(argv[i]);
      printf("Only sending one frame in %d\n", g_every_nth_frame);
      if ( (g_every_nth_frame < 1) || (g_every_nth_frame > 4000) ) {
	fprintf(stderr,"Invalid frame skip (1-4000 allowed, %d entered)\n", g_every_nth_frame);
	exit(-1);
      }
    } else if (!strncmp(argv[i], "-expose", strlen("-expose"))) {
      if (++i > argc) { Usage(argv[0]); }
      g_exposure = atof(argv[i]);
      if ( (g_exposure < 1) || (g_exposure > 4000) ) {
	fprintf(stderr,"Invalid exposure (1-4000 allowed, %f entered)\n", g_exposure);
	exit(-1);
      }
    } else if (!strncmp(argv[i], "-buffers", strlen("-buffers"))) {
      if (++i > argc) { Usage(argv[0]); }
      g_camera_buffers = atoi(argv[i]);
      if ( (g_camera_buffers < 1) || (g_camera_buffers > 1000) ) {
	fprintf(stderr,"Invalid exposure (1-1000 allowed, %d entered)\n", g_camera_buffers);
	exit(-1);
      }
    } else if (!strncmp(argv[i], "-res", strlen("-res"))) {
      if (++i > argc) { Usage(argv[0]); }
      g_width = atoi(argv[i]);
      if ( (g_width < 1) || (g_width > 1600) ) {
	fprintf(stderr,"Invalid width (1-1600 allowed, %f entered)\n", g_width);
	exit(-1);
      }
      if (++i > argc) { Usage(argv[0]); }
      g_height = atoi(argv[i]);
      if ( (g_height < 1) || (g_height > 1200) ) {
	fprintf(stderr,"Invalid height (1-1200 allowed, %f entered)\n", g_height);
	exit(-1);
      }
    } else if (!strncmp(argv[i], "-swap_edt", strlen("-swap_edt"))) {
      g_swap_edt = true;
    } else {
      switch (++realparams) {
      case 1:
	devicename = argv[i];
	break;
      case 2:
	devicenum = atoi(argv[i]);
	break;
      case 3:
	logfilename = argv[i];
	break;
      default:
	Usage(argv[0]);
      }
    }
  }
    
  // Set up handler for all these signals to set done
  signal(SIGINT, handle_cntl_c);

  printf("video_vrpnImager_server version %02d.%02d\n", MAJOR_VERSION, MINOR_VERSION);

  if (!init_camera_code(devicename, devicenum)) { return -1; }
  printf("Opened camera\n");
  if (!init_server_code(logfilename, (g_numchannels > 1) )) { return -1; }

  while (!g_done) {
    str->mainloop();
    strcon->mainloop();

    // Sleep to avoid eating the whole processor.  The camera-watching thread
    // does not sleep, so that it gets frames as fast as it can.
    vrpn_SleepMsecs(1);
  }

  printf("Deleting camera and connection objects\n");
  teardown_server_code();
  return 0;
}
