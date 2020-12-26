#include "alsa-output.hpp"
#include "config.h"
#include <alsa/error.h>

#define TEST_ERROR(message)       \
    if(error < 0) {               \
        console.error << message; \
        break;                    \
    }

snd_pcm_format_t AlsaOutput::convert_alsa_format() {
    static const struct {
        boxten::FORMAT_SAMPLE_TYPE boxten_format;
        snd_pcm_format_t           alsa_format;
    } table[] =
        {
            {boxten::FORMAT_SAMPLE_TYPE::FLOAT, SND_PCM_FORMAT_FLOAT},
            {boxten::FORMAT_SAMPLE_TYPE::S8, SND_PCM_FORMAT_S8},
            {boxten::FORMAT_SAMPLE_TYPE::U8, SND_PCM_FORMAT_U8},
            {boxten::FORMAT_SAMPLE_TYPE::S16_LE, SND_PCM_FORMAT_S16_LE},
            {boxten::FORMAT_SAMPLE_TYPE::S16_BE, SND_PCM_FORMAT_S16_BE},
            {boxten::FORMAT_SAMPLE_TYPE::U16_LE, SND_PCM_FORMAT_U16_LE},
            {boxten::FORMAT_SAMPLE_TYPE::U16_BE, SND_PCM_FORMAT_U16_BE},
            {boxten::FORMAT_SAMPLE_TYPE::S24_LE, SND_PCM_FORMAT_S24_LE},
            {boxten::FORMAT_SAMPLE_TYPE::S24_BE, SND_PCM_FORMAT_S24_BE},
            {boxten::FORMAT_SAMPLE_TYPE::U24_LE, SND_PCM_FORMAT_U24_LE},
            {boxten::FORMAT_SAMPLE_TYPE::U24_BE, SND_PCM_FORMAT_U24_BE},
            {boxten::FORMAT_SAMPLE_TYPE::S32_LE, SND_PCM_FORMAT_S32_LE},
            {boxten::FORMAT_SAMPLE_TYPE::S32_BE, SND_PCM_FORMAT_S32_BE},
            {boxten::FORMAT_SAMPLE_TYPE::U32_LE, SND_PCM_FORMAT_U32_LE},
            {boxten::FORMAT_SAMPLE_TYPE::U32_BE, SND_PCM_FORMAT_U32_BE},
        };

    for(auto& f : table) {
        if(f.boxten_format == current_format.sample_type) {
            return f.alsa_format;
        }
    }
    return SND_PCM_FORMAT_UNKNOWN;
}
bool AlsaOutput::init_alsa_device() {
    snd_pcm_hw_params_t*  hw_params = nullptr;
    snd_pcm_sw_params_t*  sw_params = nullptr;
    int                   error;
    unsigned int          rate        = current_format.sampling_rate;
    bool                  success     = false;
    constexpr const char* DEVICE_NAME = "hw:0,0";
    constexpr u64         BUFFER_SIZE = boxten::PCMPACKET_PERIOD * 4;
    snd_pcm_uframes_t     buffer_size = BUFFER_SIZE;
    snd_pcm_uframes_t     period_size = boxten::PCMPACKET_PERIOD;
    do {
        error = snd_pcm_open(&playback_handle.data, DEVICE_NAME, SND_PCM_STREAM_PLAYBACK, 0);
        TEST_ERROR("cannot open audio device \"" << DEVICE_NAME << "\" (" << snd_strerror(error) << ").");

        error = snd_pcm_hw_params_malloc(&hw_params);
        TEST_ERROR("cannot allocate hardware parameter structure (" << snd_strerror(error) << ").");

        error = snd_pcm_hw_params_any(playback_handle, hw_params);
        TEST_ERROR("cannot initialize hardware parameter structure (" << snd_strerror(error) << ").");

        error = snd_pcm_hw_params_set_access(playback_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
        TEST_ERROR("cannot set access type (" << snd_strerror(error) << ").");

        error = snd_pcm_hw_params_set_format(playback_handle, hw_params, convert_alsa_format());
        TEST_ERROR("cannot set sample format (" << snd_strerror(error) << ").");

        error = snd_pcm_hw_params_set_channels(playback_handle, hw_params, 2);
        TEST_ERROR("cannot set channel count (" << snd_strerror(error) << ").");

        error = snd_pcm_hw_params_set_rate_near(playback_handle, hw_params, &rate, 0);
        TEST_ERROR("cannot set sample rate (" << snd_strerror(error) << ").");

        error = snd_pcm_hw_params_set_buffer_size_near(playback_handle, hw_params, &buffer_size);
        TEST_ERROR("cannot set buffer size (" << snd_strerror(error) << ").");

        error = snd_pcm_hw_params_set_period_size_near(playback_handle, hw_params, &period_size, NULL);
        TEST_ERROR("cannot set period size (" << snd_strerror(error) << ").");

        error = snd_pcm_hw_params(playback_handle, hw_params);
        TEST_ERROR("cannot set parameters (" << snd_strerror(error) << ").");

        snd_pcm_hw_params_free(hw_params);
        hw_params = nullptr;

        /* 
            tell ALSA to wake us up whenever MIN_FRAME or more frames
            of playback data can be delivered. Also, tell
            ALSA that we'll start the device ourselves.
        */
        error = snd_pcm_sw_params_malloc(&sw_params);
        TEST_ERROR("cannot allocate software parameters structure (" << snd_strerror(error) << ").");

        error = snd_pcm_sw_params_current(playback_handle, sw_params);
        TEST_ERROR("cannot initialize software parameters structure (" << snd_strerror(error) << ").");

        error = snd_pcm_sw_params_set_avail_min(playback_handle, sw_params, boxten::PCMPACKET_PERIOD * 2);
        TEST_ERROR("cannot set minimum available count (" << snd_strerror(error) << ").");

        error = snd_pcm_sw_params_set_start_threshold(playback_handle, sw_params, boxten::PCMPACKET_PERIOD * 2);
        TEST_ERROR("cannot set start mode (" << snd_strerror(error) << ").");

        error = snd_pcm_sw_params(playback_handle, sw_params);
        TEST_ERROR("cannot set software parameters (" << snd_strerror(error) << ").");

        snd_pcm_sw_params_free(sw_params);
        sw_params = nullptr;

        /* 
            the interface will interrupt the kernel every MIN_FRAME frames, and ALSA
            will wake up this program very soon after that.
            */
        error = snd_pcm_prepare(playback_handle);
        TEST_ERROR("cannot prepare audio interface for use (" << snd_strerror(error) << ").");

        success = true;
    } while(0);

    if(success) {
        return true;
    } else {
        if(playback_handle != nullptr) {
            snd_pcm_close(playback_handle);
            playback_handle = nullptr;
        }
        if(hw_params != nullptr) {
            snd_pcm_hw_params_free(hw_params);
        }
        if(sw_params != nullptr) {
            snd_pcm_sw_params_free(sw_params);
        }
        console.error << "failed to init ALSA device.";
        return false;
    }
}
void AlsaOutput::close_alsa_device() {
    if(playback_handle == nullptr) return;
    snd_pcm_drop(playback_handle);
    snd_pcm_close(playback_handle);
    playback_handle = nullptr;
}
void AlsaOutput::write_pcm_data() {
    int               error;
    snd_pcm_sframes_t frames_to_deliver;
    while(!exit_loop) {
        std::lock_guard<std::mutex> lock(playback_handle.lock);
        if(playback_handle == nullptr) {
            auto next_format = get_buffer_pcm_format();
            if(next_format.sample_type == boxten::FORMAT_SAMPLE_TYPE::UNKNOWN) continue;
            current_format = next_format;
            if(!init_alsa_device()) {
                break;
            }
        }
        error = snd_pcm_wait(playback_handle, 300);
        if(exit_loop) break;

        TEST_ERROR("poll failed (" << snd_strerror(error) << ").");
        {
            snd_pcm_sframes_t delay;
            error = snd_pcm_delay(playback_handle, &delay);
            TEST_ERROR("snd_pcm_delay failed (" << snd_strerror(error) << ").");
            std::lock_guard<std::mutex> clock(calced_delay.lock);
            calced_delay = delay;
        }
        frames_to_deliver = snd_pcm_avail_update(playback_handle);
        if(frames_to_deliver < 0) {
            if(frames_to_deliver == -EPIPE) {
                console.error << "an xrun occured.";
                break;
            } else {
                console.error << "unknown ALSA avail update return value (" << frames_to_deliver << ").";
                break;
            }
        }
        frames_to_deliver = static_cast<u64>(frames_to_deliver) > boxten::PCMPACKET_PERIOD ? boxten::PCMPACKET_PERIOD : frames_to_deliver;

        auto packet = get_buffer_pcm_packet(frames_to_deliver);
        for(auto& p : packet) {
            if(p.format != current_format) {
                close_alsa_device();
                current_format = get_buffer_pcm_format();
                if(!init_alsa_device()) {
                    break;
                }
            }
            error = snd_pcm_writei(playback_handle, p.pcm.data(), p.get_frames());
            TEST_ERROR("write failed (" << snd_strerror(error) << ").");
        }
    }
    boxten::stop_playback();
}
boxten::n_frames AlsaOutput::output_delay() {
    std::lock_guard<std::mutex> lock(calced_delay.lock);
    return calced_delay;
}
void AlsaOutput::start_playback() {
    if(playback_handle != nullptr) return;
    exit_loop = false;
    loop      = boxten::Worker(std::bind(&AlsaOutput::write_pcm_data, this));
}
void AlsaOutput::stop_playback() {
    exit_loop = true;

    loop.join();
    std::lock_guard<std::mutex> lock(playback_handle.lock);
    close_alsa_device();
}
void AlsaOutput::pause_playback() {
    snd_pcm_pause(playback_handle, 1);
}
void AlsaOutput::resume_playback() {
    snd_pcm_pause(playback_handle, 0);
}

BOXTEN_MODULE({"ALSA output", boxten::COMPONENT_TYPE::STREAM_OUTPUT, CATALOGUE_CALLBACK(AlsaOutput)})

/*
            std::vector<i16> cnv(p.pcm.size() / p.format.get_bytewidth());
            for(u64 i = 0; i < cnv.size(); i++) {
                u64 offset = i * p.format.get_bytewidth();
                //  cnv[i] = *(i16*)((i8*)(&p.pcm[i]) + 2); // S32 to S16
                cnv[i] = *(f32*)(&p.pcm[offset]) * 32767; // FLOAT to S16
                if(i == 0){
                    for(int s = 0; s < 32;++s){
                    //    printf("%02X ", p.pcm[s]);
                    }
                    printf("\n");
                }
            }
            printf("\n%d~", p.original_frame_pos[0]);
            for(int s = 0; s < 32; ++s) {
                printf("%02X ", p.pcm[s]);
            }
            */
