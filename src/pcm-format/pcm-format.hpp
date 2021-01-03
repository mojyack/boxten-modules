#pragma once
#include "plugin.hpp"
#include <libboxten.hpp>

#include <config.h>

class PCMFormat : public boxten::SoundProcessor {
  private:
    boxten::SampleType to = boxten::SampleType::unknown;

  public:
    bool modify_packet(boxten::PCMPacketUnit& packet) override;
    PCMFormat(void* param);
    ~PCMFormat(); 
};
