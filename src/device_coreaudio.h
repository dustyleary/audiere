#ifndef DEVICE_COREAUDIO_H
#define DEVICE_COREAUDIO_H


#include "audiere.h"
#include "device_mixer.h"

#include <CoreAudio/CoreAudio.h>
#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>


namespace audiere {

  class CAAudioDevice : public MixerDevice {
  public:
    static CAAudioDevice* create(const ParameterList& parameters);

  private:
    CAAudioDevice(AudioComponentInstance output_audio_unit);
    ~CAAudioDevice();

  public:
    void ADR_CALL update();
    const char* ADR_CALL getName();

    static OSStatus fillInput(void            *inRefCon,
			      AudioUnitRenderActionFlags  *ioActionFlags,
			      const AudioTimeStamp        *inTimeStamp,
			      UInt32                      inBusNumber,
			      UInt32                      inNumberFrames,
			      AudioBufferList             *ioData);

  private:
      AudioComponentInstance m_output_audio_unit;
  };

}


#endif
