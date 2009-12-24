//#include <windows.h>
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
  bool FileAudioDevice::m_pathnameIsWav = false;
  int FileAudioDevice::m_dataBytes = 0;
  int const WavHeaderBytes = 44;

  ADR_EXPORT( void ) AdrAdvanceFileDevice( int ms ) {
      FileAudioDevice::advance( ms );
  }

  ADR_EXPORT( long ) AdrCanAdvanceFileDevice( ) {
      return FileAudioDevice::canAdvance( );
  }

  ADR_EXPORT( void ) AdrSetFileDevicePathname( char const * pn ) {
      FileAudioDevice::setPathname( pn );
  }

  FileAudioDevice*
  FileAudioDevice::create(const ParameterList& parameters) {
    FILE * file = NULL;
    m_requestTime = 0;
    m_outputTime = 0;
    m_dataBytes = 0;
    size_t slen = m_pathname ? strlen( m_pathname ) : 0;
    m_pathnameIsWav = slen >= 4 && ( strcmp( m_pathname + slen - 4, ".wav" ) == 0 || strcmp( m_pathname + slen - 4, ".WAV" ) == 0 );
    if( m_pathnameValid ) {
        file = fopen( m_pathname, "wb" );

        // If filename ends in ".wav" then write a wav header block to the file, which we'll come back
        // and fill in after writing the data.
        if( file && m_pathnameIsWav ) {
            char buf[ WavHeaderBytes ];
            int i;
            for( i = 0; i < WavHeaderBytes; i++ ) {
                buf[ i ] = 0;
            }
            size_t rl = fwrite( buf, 1, WavHeaderBytes, file );
            if( rl != WavHeaderBytes ) {
                fclose( file );
                file = NULL;
            }
        }
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

  void
  WriteChunkId( void * buf, char const * s )
  {
      char * bc = reinterpret_cast< char * >( buf );
      bc[ 0 ] = s[ 0 ];
      bc[ 1 ] = s[ 1 ];
      bc[ 2 ] = s[ 2 ];
      bc[ 3 ] = s[ 3 ];
  }

  void
  WriteUint32LittleEndian( void * buf, unsigned int p )
  {
      unsigned char * bc = reinterpret_cast< unsigned char * >( buf );
      bc[ 0 ] = ( unsigned char ) ( p & 0xff );
      bc[ 1 ] = ( unsigned char ) ( ( p >> 8 ) & 0xff );
      bc[ 2 ] = ( unsigned char ) ( ( p >> 16 ) & 0xff );
      bc[ 3 ] = ( unsigned char ) ( p >> 24 );
  }

  void
  WriteUint16LittleEndian( void * buf, unsigned int p )
  {
      unsigned char * bc = reinterpret_cast< unsigned char * >( buf );
      bc[ 0 ] = ( unsigned char ) ( p & 0xff );
      bc[ 1 ] = ( unsigned char ) ( ( p >> 8 ) & 0xff );
  }



  FileAudioDevice::~FileAudioDevice() 
  {
      if( m_file ) {

          // Now that we know the length of the file, come back and write the header.
          if( m_pathnameIsWav ) {
              char buf[ WavHeaderBytes ];
              WriteChunkId( buf, "RIFF" );
              WriteUint32LittleEndian( buf + 4, 36 + m_dataBytes );
              WriteChunkId( buf + 8, "WAVE" );
              WriteChunkId( buf + 12, "fmt " );
              WriteUint32LittleEndian( buf + 16, 16 ); // Subchunk1Size
              WriteUint16LittleEndian( buf + 20, 1 ); // AudioFormat
              WriteUint16LittleEndian( buf + 22, 2 ); // NumChannels
              WriteUint32LittleEndian( buf + 24, 44100 ); // SampleRate
              WriteUint32LittleEndian( buf + 28, 44100 * 2 * 2 ); // ByteRate
              WriteUint16LittleEndian( buf + 32, 4 ); // BlockAlign
              WriteUint16LittleEndian( buf + 34, 16 ); // BitsPerSample
              WriteChunkId( buf + 36, "data" );
              WriteUint32LittleEndian( buf + 40, m_dataBytes );
              int ri = fseek( ( FILE * ) m_file, 0, SEEK_SET );
              if( ri == 0 ) {
                  size_t rl = fwrite( buf, 1, WavHeaderBytes, ( FILE * ) m_file );
                  if( rl == WavHeaderBytes ) {
                      ri = fseek( ( FILE * ) m_file, 0, SEEK_END );
                  }
              }
          }
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

  long 
  FileAudioDevice::canAdvance() {
      return m_file && (m_outputTime >= m_requestTime);
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
            m_dataBytes += bytes;
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
