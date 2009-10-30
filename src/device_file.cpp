#include <windows.h>
#include "device_file.h"
#include "utility.h"


namespace audiere {

  static const int RATE = 44100;
  volatile FILE * FileAudioDevice::m_file = NULL;
  volatile int FileAudioDevice::m_requestTime = 0;
  volatile int FileAudioDevice::m_outputTime = 0;
  volatile bool FileAudioDevice::m_exists = false;
  char FileAudioDevice::m_pathname[];
  bool FileAudioDevice::m_pathnameValid = false;

  ADR_EXPORT( void ) AdrAdvanceFileDevice( int ms ) {
      FileAudioDevice::advance( ms );
  }

  ADR_EXPORT( void ) AdrSetFileDevicePathname( char const * pn ) {
      FileAudioDevice::setPathname( pn );
  }

  FileAudioDevice*
  FileAudioDevice::create(const ParameterList& parameters) {
    FILE * file = NULL;
    m_requestTime = 0;
    m_outputTime = 0;
    if( m_pathnameValid ) {
        file = fopen( m_pathname, "wb" );
    }
    return new FileAudioDevice(file, RATE);
  }


  FileAudioDevice::FileAudioDevice(FILE * file, int rate)
    : MixerDevice(rate)
  {
    ADR_GUARD("FileAudioDevice::FileAudioDevice");

    m_file = file;
    m_exists = true;
    m_outputTime = 0;
    m_requestTime = 0;
  }


  FileAudioDevice::~FileAudioDevice() 
  {
      if( m_file ) {
          fclose( ( FILE * ) m_file );
          m_file = NULL;
      }
  }

  void 
  FileAudioDevice::setPathname( char const * pn ) {
    sprintf( m_pathname, "%s", pn );
    m_pathnameValid = true;
  }

  void
  FileAudioDevice::advance( int ms ) {
      if( !m_file ) {
          return;
      }
      volatile int i = 0;
      while( m_outputTime < m_requestTime ) {
          i++; // Don't optimize out the loop, please.
      }
      m_requestTime += ms;
  }


  void
  FileAudioDevice::update() {
    ADR_GUARD("FileAudioDevice::update");
    int timeDelta = m_requestTime - m_outputTime;
    int samples = timeDelta * RATE / 1000; // If stepping by multiple of 10ms, then no rounding error.
    while( samples > 0 ) {
        int isamples = samples < BUFFER_SAMPLES ? samples : BUFFER_SAMPLES;

        // Read in isamples.
        read( isamples, m_samples );

        // Write out isamples.
        if( m_file ) {
            size_t bytes = isamples * 4;
            size_t rl = fwrite( m_samples, 1, bytes, ( FILE * ) m_file );
            if( rl != bytes ) {
                fclose( ( FILE * ) m_file );
                m_file = NULL;
            }
        }

        samples -= isamples;
    }
    m_outputTime += timeDelta;
  }


  const char*
  FileAudioDevice::getName() {
    return "file";
  }

}
