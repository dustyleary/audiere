//#include <windows.h>
#include "device_file.h"
#include "utility.h"


namespace audiere {

  static const int PATHNAME_LENGTH_MAX = 2048;
  static const int RATE = 44100;
  int const WavHeaderBytes = 44;

  wchar_t g_pathname[PATHNAME_LENGTH_MAX];

  FileAudioDevice* global_device = NULL;

  ADR_EXPORT( void ) AdrAdvanceFileDevice( int ms ) {
      global_device->advance( ms );
  }

  ADR_EXPORT( long ) AdrCanAdvanceFileDevice( ) {
      return global_device->canAdvance( );
  }

  ADR_EXPORT( void ) AdrSetFileDevicePathnameW( wchar_t const * pn ) {
    for( int i = 0; i < PATHNAME_LENGTH_MAX; i++ ) {
      wchar_t c = pn[ i ];
      if( i == PATHNAME_LENGTH_MAX - 1 )  {
        c = 0;
      }
      g_pathname[ i ] = c;
      if( !c ) {
        break;
      }
    }
  }

  ADR_EXPORT( void ) AdrFinalizeFileDeviceHeader( ) {
      global_device->finalizeHeader( );
  }

  bool ends_in_dot_wav(const wchar_t* pn) {
    size_t slen = wcslen( pn );
    return slen >= 4 && ( wcscmp( pn + slen - 4, L".wav" ) == 0 || wcscmp( pn + slen - 4, L".WAV" ) == 0 );
  }

  FileAudioDevice*
  FileAudioDevice::create(const ParameterList& parameters) {
    FILE * file = NULL;
    bool is_wav = false;

    if( g_pathname[0] ) {
#ifdef WIN32
      file = _wfopen( g_pathname, L"wb" );
#else
      // FIXME: convert to UTF-8 from platform native wchar_t Unicode encoding (e.g., UTF-32 on Unix, UTF-16 on Windows)
      // FIXME: currently just slicing off the high bits, which doesn't work for international customers.
      // http://stackoverflow.com/questions/12319/wfopen-equivalent-under-mac-os-x
      unsigned int buflen = wcslen( g_pathname ) + 1;
      char * buf = new char[ buflen ];
      unsigned int j;
      for( j = 0; j < buflen; j++ ) {
        buf[ j ] = ( char ) g_pathname[ j ];
      }
      file = fopen( buf, "wb" );
      delete [] buf;
#endif

      // If filename ends in ".wav" then write a wav header block to the file, which we'll come back
      // and fill in after writing the data.
      if( file && ends_in_dot_wav(g_pathname) ) {
        is_wav = true;
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
    FileAudioDevice* result = new FileAudioDevice(file, RATE, is_wav);
    global_device = result;
    return result;
  }

  FileAudioDevice::FileAudioDevice(FILE * file, int rate, bool is_wav)
    : MixerDevice(rate), is_wav_(is_wav)
  {
    ADR_GUARD("FileAudioDevice::FileAudioDevice");

    m_file = file;
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
    if(m_file) {
      fclose( m_file );
    }
  }

  void
  FileAudioDevice::finalizeHeader() {
    if( m_file ) {

      // Now that we know the length of the file, come back and write the header.
      if( is_wav_ ) {
        //logToFinale( strprintf( "FileAudioDevice::finalizeHeader() this %p m_dataBytes %d", this, m_dataBytes ));

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
      fclose( m_file );
      m_file = NULL;
    }
  }

  void
  FileAudioDevice::advance( int ms ) {
      if( !m_file ) { return; }
      m_requestMs += ms;
      m_requestSample = m_requestMs * RATE / 1000.0;
      internal_update();
  }

  long 
  FileAudioDevice::canAdvance() { return true; }

  void
  FileAudioDevice::update() {
    ADR_GUARD("FileAudioDevice::update");
  }

  void
  FileAudioDevice::internal_update() {
    ADR_GUARD("FileAudioDevice::internal_update");
    int samples = m_requestSample - m_outputSample;
    while( samples > 0 ) {
        int isamples = samples < BUFFER_SAMPLES ? samples : BUFFER_SAMPLES;

        // Read in isamples.
        read( isamples, m_samples );

        // Write out isamples.
        if( m_file ) {
            size_t bytes = isamples * 4;
            size_t rl = fwrite( m_samples, 1, bytes, ( FILE * ) m_file );
            m_dataBytes += bytes;
            //logToFinale( strprintf( "FileAudioDevice::internal_update() bytes %d m_dataBytes %d", bytes, m_dataBytes ));
            if( rl != bytes ) {
                fclose( ( FILE * ) m_file );
                m_file = NULL;
            }
        }

        samples -= isamples;
    }
    m_outputSample = m_requestSample;
  }


  const char*
  FileAudioDevice::getName() {
    return "file";
  }

}
