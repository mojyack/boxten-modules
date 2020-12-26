#pragma once
#include "type.hpp"
#include <alsa/global.h>
#include <mutex>
#include <thread>
#include <vector>

#include <alsa/asoundlib.h>
#include <libboxten.hpp>

#include <config.h>

class AlsaOutput : public boxten::StreamOutput {
  private:
    boxten::SafeVar<snd_pcm_t*>           playback_handle = nullptr;
    boxten::SafeVar<snd_async_handler_t*> async_handle    = nullptr;
    boxten::PCMFormat                     current_format;
    boxten::SafeVar<boxten::n_frames>     calced_delay = 0;

    snd_pcm_format_t convert_alsa_format();
    bool             write_packats(size_t frames);
    bool             init_alsa_device();
    void             close_alsa_device();

  public:
    void             write_pcm_data();

    boxten::n_frames output_delay() override;
    void             start_playback() override;
    void             stop_playback() override;
    void             pause_playback() override;
    void             resume_playback() override;
    AlsaOutput(void* param) : boxten::StreamOutput(param) {}
    ~AlsaOutput() override {}
};
