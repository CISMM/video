#include  <math.h>
#include  <stdlib.h>
#include  <signal.h>
#include  <string.h>
#include  <stdio.h>
#include  <vrpn_Connection.h>
#include  <vrpn_TempImager.h>
#include  "roper_server.h"
#include  "directx_camera_server.h"

const int MAJOR_VERSION = 1;
const int MINOR_VERSION = 3;

//-----------------------------------------------------------------
// This section contains code to initialize the camera and read its
// values.  The server code reads values from the camera and sends
// them over the network.

base_camera_server  *g_camera;	//< The camera we're going to read from

/// Open the camera we want to use (Roper or DirectX)
bool  init_camera_code(const char *type)
{
  if (!strcmp(type, "roper")) {
    g_camera = new roper_server();
    if (!g_camera->working()) {
      fprintf(stderr,"init_camera_code(): Can't open roper camera server\n");
      return false;
    }
  } else if (!strcmp(type, "directx")) {
    g_camera = new directx_camera_server();
    if (!g_camera->working()) {
      fprintf(stderr,"init_camera_code(): Can't open DirectX camera server\n");
      return false;
    }
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

/// Cause the camera to read one frame.
// XXX Later, would like to put it into continuous mode, most likely
void  mainloop_camera_code(void)
{
  g_camera->read_image_to_memory();
}

//-----------------------------------------------------------------
// This section contains code that does what the server should do

vrpn_Synchronized_Connection  *svrcon;	//< Connection for server to talk on
vrpn_TempImager_Server	      *svr;	//< Image server to be used to send
int			      svrchan;	//< Server channel index for image data

bool  init_server_code(void)
{
  const int PORT = 4511;
  if ( (svrcon = new vrpn_Synchronized_Connection(PORT)) == NULL) {
    fprintf(stderr, "Could not open server connection\n");
    return false;
  }
  if ( (svr = new vrpn_TempImager_Server("TestImage", svrcon,
    g_camera->get_num_columns(), g_camera->get_num_rows())) == NULL) {
    fprintf(stderr, "Could not open TempImager Server\n");
    return false;
  }
  if ( (svrchan = svr->add_channel("value", "unsigned16bit", 0, (float)(pow(2,16)-1))) == -1) {
    fprintf(stderr, "Could not add channel to server image\n");
    return false;
  }
  printf("Waiting for video connections on %d\n", PORT);

  return true;
}

void  teardown_server_code(void)
{
  if (svr) { delete svr; };
  if (svrcon) { delete svrcon; };
}

void  mainloop_server_code(void)
{
  unsigned  x,y;
  unsigned  num_x = g_camera->get_num_columns();
  unsigned  num_y = g_camera->get_num_rows();
  vrpn_uint16 *data = new vrpn_uint16[num_x * num_y];

  if (data == NULL) {
    fprintf(stderr, "mainloop_server_code(): Out of memory\n");
    return;
  }

  // Loop through each line, fill them in, and send them to the client.
  for (y = 0; y < num_y; y++) {
    for (x = 0; x < num_x; x++) {
      g_camera->get_pixel_from_memory(x, y, data[x + y*num_x]);
    }

    // Send the current row over to the client.
    svr->fill_region(svrchan, 0, num_x-1, y,y, data);
    svr->send_region();
    svr->mainloop();
  }

  // Mainloop the server connection (once per server mainloop, not once per object)
  svrcon->mainloop();

  delete [] data;
}

//-----------------------------------------------------------------
// g_done gets set when the user presses ^C to exit the program.

static	bool  g_done = false;	//< Becomes true when time to exit

// WARNING: On Windows systems, this handler is called in a separate
// thread from the main program (this differs from Unix).  To avoid all
// sorts of chaos as the main program continues to handle packets, we
// set a done flag here and let the main program shut down in its own
// thread by calling shutdown() to do all of the stuff we used to do in
// this handler.

void handle_cntl_c(int) {
  g_done = true;
}

//-----------------------------------------------------------------
// Mostly just calls the above functions; split into client and
// server parts is done clearly to help people who want to use this
// as example code.  You could pull the above functions down into
// the main() program body without trouble (except for the callback
// handler, of course).

int main(unsigned argc, char *argv[])
{
  printf("video_tempImager_server version %02d.%02d\n", MAJOR_VERSION, MINOR_VERSION);

  if (!init_camera_code("directx")) { return -1; }
  if (!init_server_code()) { return -1; }

  // Set up handler for all these signals to set done
  signal(SIGINT, handle_cntl_c);

  while (!g_done) {
    mainloop_camera_code();
    mainloop_server_code();
    vrpn_SleepMsecs(1);
  }

  printf("Deleting camera and connection objects\n");
  teardown_server_code();
  teardown_camera_code();
  return 0;
}
