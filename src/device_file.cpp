//#include <windows.h>
#include "device_file.h"
#include "utility.h"


namespace audiere {

  static const int RATE = 44100;
  char FileAudioDevice::m_pathname[];
  wchar_t FileAudioDevice::m_pathnameW[];
  bool FileAudioDevice::m_pathnameValid = false;
  bool FileAudioDevice::m_pathnameIsWav = false;
  int const WavHeaderBytes = 44;

  FileAudioDevice* global_device = NULL;

  ADR_EXPORT( void ) AdrAdvanceFileDevice( int ms ) {
      global_device->advance( ms );
  }

  ADR_EXPORT( long ) AdrCanAdvanceFileDevice( ) {
      return global_device->canAdvance( );
  }

  ADR_EXPORT( void ) AdrSetFileDevicePathname( char const * pn ) {
      global_device->setPathname( pn );
  }

  ADR_EXPORT( void ) AdrSetFileDevicePathnameW( wchar_t const * pn ) {
      global_device->setPathnameW( pn );
  }

  ADR_EXPORT( void ) AdrFinalizeFileDeviceHeader( ) {
      global_device->finalizeHeader( );
  }

  FileAudioDevice*
  FileAudioDevice::create(const ParameterList& parameters) {
    FILE * file = NULL;
    size_t slen = m_pathname ? strlen( m_pathname ) : 0;
    m_pathnameIsWav = slen >= 4 && ( strcmp( m_pathname + slen - 4, ".wav" ) == 0 || strcmp( m_pathname + slen - 4, ".WAV" ) == 0 );
    if( slen == 0 && m_pathnameW[ 0 ] ) {
      slen = wcslen( m_pathnameW );
      m_pathnameIsWav = slen >= 4 && ( wcscmp( m_pathnameW + slen - 4, L".wav" ) == 0 || wcscmp( m_pathnameW + slen - 4, L".WAV" ) == 0 );
    }
#ifdef __GNUC__
fwprintf(stdout, L"FileAudioDevice::create\n");
#endif
    if( m_pathnameValid ) {
      if( m_pathname[ 0 ] ) {
        file = fopen( m_pathname, "wb" );
      } else {

#ifdef WIN32
        file = _wfopen( m_pathnameW, L"wb" );
#else
        // FIXME: convert to UTF-8 from platform native wchar_t Unicode encoding (e.g., UTF-32 on Unix, UTF-16 on Windows)
        // FIXME: currently just slicing off the high bits, which doesn't work for international customers.
        // http://stackoverflow.com/questions/12319/wfopen-equivalent-under-mac-os-x
        unsigned int buflen = wcslen( m_pathnameW ) + 1;
        char * buf = new char[ buflen ];
        unsigned int j;
        for( j = 0; j < buflen; j++ ) {
          buf[ j ] = ( char ) m_pathnameW[ j ];
        }
        file = fopen( buf, "wb" );
        delete [] buf;
#endif
      }

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
    if(!file) return NULL;
    FileAudioDevice* result = new FileAudioDevice(file, RATE);
    global_device = result;
    return result;
  }


  FileAudioDevice::FileAudioDevice(FILE * file, int rate)
    : MixerDevice(rate)
  {
    ADR_GUARD("FileAudioDevice::FileAudioDevice");

    m_file = file;
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
  }

  void
  FileAudioDevice::finalizeHeader()
  {
#ifdef __GNUC__
fwprintf(stdout, L"FileAudioDevice::~FileAudioDevice START\n");
#endif
      if( m_file ) {

          // Now that we know the length of the file, come back and write the header.
          if( m_pathnameIsWav ) {
#ifdef __GNUC__
fwprintf(stdout, L"FileAudioDevice::~FileAudioDevice m_file && m_pathnameIsWav\n");
#endif
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
#ifdef __GNUC__
fwprintf(stdout, L"FileAudioDevice::~FileAudioDevice END\n");
#endif
  }

  void 
  FileAudioDevice::setPathname( char const * pn ) {
    sprintf( m_pathname, "%s", pn );
    m_pathnameW[ 0 ] = 0;
    m_pathnameValid = true;
  }

  void 
  FileAudioDevice::setPathnameW( wchar_t const * pn ) {
    unsigned int i;
    for( i = 0; i < PATHNAME_LENGTH_MAX; i++ ) {
      wchar_t c = pn[ i ];
      if( i == PATHNAME_LENGTH_MAX - 1 )  {
        c = 0;
      }
      m_pathnameW[ i ] = c;
      if( !c ) {
        break;
      }
    }
    m_pathname[ 0 ] = 0;
    m_pathnameValid = true;
  }

  void
  FileAudioDevice::advance( int ms ) {
      if( !m_file ) {
          return;
      }
      m_requestTime += ms;
      internal_update();
  }

  long 
  FileAudioDevice::canAdvance() {
      return m_file && (m_outputTime >= m_requestTime);
  }

  void
  FileAudioDevice::update() {
    ADR_GUARD("FileAudioDevice::update");
    internal_update();
  }

  void
  FileAudioDevice::internal_update() {
    ADR_GUARD("FileAudioDevice::internal_update");
    int timeDelta = m_requestTime - m_outputTime;
    double samples_d = timeDelta * RATE / 1000.0;
    int samples = int(samples_d);
    if(samples_d != samples) {
      ADR_LOG("rounding error");
      fprintf(stderr, "rounding error\n");
    }
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
