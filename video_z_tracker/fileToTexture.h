#pragma once


#include "file_stack_server.h"
#include "image_wrapper.h"


class Controllable_Video {
public:
  /// Start the stored video playing.
  virtual void play(void) = 0;

  /// Pause the stored video
  virtual void pause(void) = 0;

  /// Rewind the stored video to the beginning (also pauses).
  virtual void rewind(void) = 0;

  /// Single-step the stored video for one frame.
  virtual void single_step() = 0;
};

class FileToTexture : public Controllable_Video, public file_stack_server {
public:
  FileToTexture(const char *filename) : file_stack_server(filename) {};
  virtual ~FileToTexture() {};
  void play(void) { file_stack_server::play(); }
  void pause(void) { file_stack_server::pause(); }
  void rewind(void) { pause(); file_stack_server::rewind(); }
  void single_step(void) { file_stack_server::single_step(); }
  bool write_memory_to_ppm_file(const char *filename, int gain = 1, bool sixteen_bits = false)
  {	  return file_stack_server::write_memory_to_ppm_file(filename, gain, sixteen_bits);  }
};