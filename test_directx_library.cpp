#include <stdio.h>
#include "directx_camera_server.h"

void main(unsigned argc, char *argv[])
{
  directx_camera_server	server;

  // Do something using the server!
  server.read_image_to_memory();
  server.write_memory_to_ppm_file("test.ppm");
//  XXX;
}