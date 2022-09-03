#include <stdio.h>
#include <stdlib.h>
#include "directx_camera_server.h"

void main(unsigned argc, char *argv[])
{
  directx_camera_server	server(1);

  if (!server.working()) {
    fprintf(stderr,"Cannot open camera\n");
    exit(-1);
  }

  // Read and image and write it to a PPM file.
  int i;
  vrpn_uint16 val;
  for (i = 0; i < 300; i++) {
    printf("Taking snapshot %03d: ", i);
    server.read_image_to_memory();
    server.get_pixel_from_memory(0, 0, val, 1);
    printf("First Pixel green = %03d\n", val);
    server.write_memory_to_ppm_file("test.ppm");
  }
}