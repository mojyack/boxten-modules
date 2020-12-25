#pragma once
#include <libboxten.hpp>

#include <config.h>

class FlacInput : public boxten::StreamInput {
  private:

  public:
    boxten::PCMPacketUnit read_frames(boxten::AudioFile& file, u64 from, boxten::n_frames frames) override;
    boxten::n_frames      calc_total_frames(boxten::AudioFile& file) override;
    boxten::AudioTag      read_tags(boxten::AudioFile& file) override;
    FlacInput(void* param) : boxten::StreamInput(param) {}
    ~FlacInput() {}
};