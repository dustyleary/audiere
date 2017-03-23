#ifndef DEVICE_FILE_H
#define DEVICE_FILE_H


//#include <windows.h>
#include <queue>
#include "internal.h"
#include "device_mixer.h"
#include "utility.h"


namespace audiere {

  class FileAudioDevice : public MixerDevice
  {
  public:
    static FileAudioDevice* create(const ParameterList& parameters);

    // Advances the request time by the given number of milliseconds.  Called from a separate thread, this
    // function advances the time up to which the FileAudioDevice will play forward the
    // audio streams, mixing and writing them to the file.  This call blocks, waiting
    // for the FileAudioDevice's output to reach the playhead.
    void advance( int ms );

    // Returns true if advance() will not block, i.e., if the device is all caught up.
    long canAdvance( );

    // Prior to creating the device, you need to set the output pathname.
    static void setPathname( char const * );
    static void setPathnameW( wchar_t const * );

    // When done, finalize the file 
    void finalizeHeader();

  private:
    FileAudioDevice(FILE *, int rate);
    ~FileAudioDevice();

  public:
    void ADR_CALL internal_update();
    void ADR_CALL update();
    const char* ADR_CALL getName();

  private:

    // 16000 bytes is 4000 frames at 44100 Hz is about 364 milliseconds of audio, the maximum.
    enum {
      BUFFER_SAMPLES = 16000,
      BUFFER_BYTES = BUFFER_SAMPLES * 4,
      PATHNAME_LENGTH_MAX = 2048,
    };

    volatile FILE * m_file = 0;

    // The time up to which the FileAudioDevice may output.  Incremented by the call
    // to ::advance().  Monotonically increases.  Never reset.  Stops working when wraps around but that 
    // isn't for 2^31ms, which is a long time (300hrs).
    volatile int m_requestTime = 0;

    // The time up to which the FileAudioDevice has already output.
    volatile int  m_outputTime = 0;

    // Set this before creating the device.
    static char m_pathname[ PATHNAME_LENGTH_MAX ];
    static wchar_t m_pathnameW[ PATHNAME_LENGTH_MAX ];
    static bool m_pathnameValid;
    static bool m_pathnameIsWav;
    int m_dataBytes;

    u8 m_samples[ BUFFER_BYTES ];
  };

}


#endif
