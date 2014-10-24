

#include <iostream>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <audiere.h>
using namespace std;
using namespace audiere;


#ifdef WIN32

#include <windows.h>
inline void sleepSecond() {
  Sleep(1000);
}

#else  // assume POSIX

#include <unistd.h>
inline void sleepSecond() {
  sleep(1);
}

#endif

#undef ADR_SWITCH_PLAY

int main(int argc, const char** argv) {

  if (argc != 2 && argc != 3) {
    cerr << "usage: simple <filename> [<device>]" << endl;
    return EXIT_FAILURE;
  }

  cerr << "initializing..." << endl;

  const char* output_filename = "";
  if (argc == 3) {
    output_filename = argv[2];
  }

#ifdef ADR_SWITCH_PLAY
  AudioDevicePtr device = OpenDevice("");
  if (!device) {
    cerr << "OpenDevice() failed" << endl;
    return EXIT_FAILURE;
  }

  cerr << "opened device" << endl;

  const char * soundpn = argv[1];
  OutputStreamPtr sound = OpenSound(device, argv[1]);
  if (!sound) {
    cerr << "OpenSound() failed" << endl;
    return EXIT_FAILURE;
  }

  cerr << "opened sound" << endl;

  sound->play();

  cerr << "started playback" << endl;
  while (sound->isPlaying()) {
    Sleep( 100 );
  }

#else
  SetFileDevicePathname( "myfilename2.wav" );
  AudioDevicePtr device = OpenDevice("file");
  if (!device) {
    cerr << "OpenDevice() failed" << endl;
    return EXIT_FAILURE;
  }

  cerr << "opened device" << endl;

  const char * soundpn = argv[1];
  OutputStreamPtr sound = OpenSound(device, argv[1]);
  if (!sound) {
    cerr << "OpenSound() failed" << endl;
    return EXIT_FAILURE;
  }

  cerr << "opened sound" << endl;

  sound->play();

  cerr << "started playback" << endl;
  while (sound->isPlaying()) {
    AdvanceFileDevice( 50 );
    if (sound->isSeekable()) {
      cerr << "position: " << sound->getPosition() << endl;
    }
  }
	FinalizeFileDeviceHeader();

#endif
  return EXIT_SUCCESS;
}
