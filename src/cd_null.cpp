#include "audiere.h"
#include "internal.h"


namespace audiere {

    namespace hidden {

  ADR_EXPORT(const char*) AdrEnumerateCDDevices() {
    return "";
  }

  ADR_EXPORT(CDDevice*) AdrOpenCDDevice(const char* /*name*/) {
    return 0;
  }

    }

}
