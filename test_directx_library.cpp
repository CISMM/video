#include <stdio.h>
#include "directx_camera_server.h"

void main(unsigned argc, char *argv[])
{
  directx_camera_server	server;

  // Read and image and write it to a PPM file.
  int i;
  for (i = 0; i < 300; i++) {
    printf("Taking snapshot %d\n", i);
    server.read_image_to_memory();
    server.write_memory_to_ppm_file("test.ppm");
  }
}