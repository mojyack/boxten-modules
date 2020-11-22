#pragma once
#include <cstring>
#include <string>

#include <libboxten.hpp>

#include <config.h>

struct PCMInfo {
    boxten::FORMAT_SAMPLE_TYPE format;
    u32                        channels;
    u32                        samplerate;

    std::streampos             data_pos; // "data" chunk position
    boxten::n_frames           total_frames;

    std::streampos             info_pos = -1;
    std::streamoff             info_limit;
    std::streampos             id3_pos = -1;
};

class WavInput : public boxten::StreamInput {
  private:
    bool     read_pcm_info(std::ifstream& handle, PCMInfo& pcm_info);
    PCMInfo* get_pcm_info(boxten::AudioFile& file);

  public:
    boxten::PCMPacketUnit read_frames(boxten::AudioFile& file, u64 from, boxten::n_frames frames) override;
    boxten::n_frames      calc_total_frames(boxten::AudioFile& file) override;
    boxten::AudioTag      read_tags(boxten::AudioFile& file) override;
    WavInput(void* param) : boxten::StreamInput(param) {}
    ~WavInput() {}
};