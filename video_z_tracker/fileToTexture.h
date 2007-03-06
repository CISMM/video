#pragma once


#include "file_stack_server.h"
#include "image_wrapper.h"
#include "edt_server.h"

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


class Pulnix_Controllable_Video : public Controllable_Video, public edt_pulnix_raw_file_server {
public:
  Pulnix_Controllable_Video(const char *filename) : edt_pulnix_raw_file_server(filename) {};
  virtual ~Pulnix_Controllable_Video() {};
  void play(void) { edt_pulnix_raw_file_server::play(); }
  void pause(void) { edt_pulnix_raw_file_server::pause(); }
  void rewind(void) { pause(); edt_pulnix_raw_file_server::rewind(); }
  void single_step(void) { edt_pulnix_raw_file_server::single_step(); }
};

class FileStack_Controllable_Video : public Controllable_Video, public file_stack_server {
public:
  FileStack_Controllable_Video(const char *filename) : file_stack_server(filename) {};
  virtual ~FileStack_Controllable_Video() {};
  void play(void) { file_stack_server::play(); }
  void pause(void) { file_stack_server::pause(); }
  void rewind(void) { pause(); file_stack_server::rewind(); }
  void single_step(void) { file_stack_server::single_step(); }
};