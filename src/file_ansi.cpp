#include <stdio.h>
#include "debug.h"
#include "default_file.h"


namespace audiere {

  class CFile : public RefImplementation<File> {
  public:
    CFile(FILE* file) {
      ADR_ASSERT(file, "FILE* handle not valid");
      m_file = file;
    }

    ~CFile() {
      fclose(m_file);
    }

    int ADR_CALL read(void* buffer, int size) {
      ADR_ASSERT(buffer, "buffer pointer not valid");
      ADR_ASSERT(size >= 0, "can't read negative number of bytes");
      return fread(buffer, 1, size, m_file);
    }

    bool ADR_CALL seek(int position, SeekMode mode) {
      int m;
      switch (mode) {
        case BEGIN:   m = SEEK_SET; break;
        case CURRENT: m = SEEK_CUR; break;
        case END:     m = SEEK_END; break;
        default: return false;
      }

      return (fseek(m_file, position, m) == 0);
    }

    int ADR_CALL tell() {
      return ftell(m_file);
    }

  private:
    FILE* m_file;
  };


  ADR_EXPORT(File*) AdrOpenFile(const char* filename, bool writeable) {
    FILE* file = fopen(filename, writeable ? "wb" : "rb");
    return (file ? new CFile(file) : 0);
  }

  ADR_EXPORT(File*) AdrOpenFileW(const wchar_t * filename, bool writeable) {
#ifdef WIN32
    FILE* file = _wfopen(filename, writeable ? L"wb" : L"rb");
#else
    // FIXME: convert to UTF-8 from platform native wchar_t Unicode encoding (e.g., UTF-32 on Unix, UTF-16 on Windows)
    // FIXME: currently just slicing off the high bits, which doesn't work for international customers.
    // http://stackoverflow.com/questions/12319/wfopen-equivalent-under-mac-os-x
    unsigned int buflen = wcslen( pn ) + 1;
    char * buf = new char[ buflen ];
    unsigned int j;
    for( j = 0; j < buflen; j++ ) {
      buf[ j ] = ( char ) pn[ j ];
    }
    FILE* file = fopen( buf, writeable ? "wb" : "rb");
    delete [] buf;
#endif
    return (file ? new CFile(file) : 0);
  }

}
