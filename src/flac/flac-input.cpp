#include "flac-input.hpp"
#include "decoder.hpp"

namespace{
    Decoder* get_decoder(boxten::AudioFile& file){
        Decoder* decoder = reinterpret_cast<Decoder*>(file.get_private_data());
        if(decoder == nullptr){
            decoder = new Decoder(file);
            auto init_error = decoder->init();
            auto decode_meta_error = decoder->process_until_end_of_metadata();
            if(init_error != FLAC__STREAM_DECODER_INIT_STATUS_OK || !decode_meta_error) {
                boxten::console << "failed to init FLAC decoder for: " << file.get_path();
                delete decoder;
                decoder = nullptr;
            } else {
                file.set_private_data(decoder, [](void* ptr) {
                    delete reinterpret_cast<Decoder*>(ptr);
                });
            }
        }
        return decoder;
    }
    boxten::FORMAT_SAMPLE_TYPE bps_to_sample_type(uint32_t bps){
        switch(bps) {
        case 8:
            return boxten::FORMAT_SAMPLE_TYPE::S8;
        case 16:
            return boxten::FORMAT_SAMPLE_TYPE::S16_LE;
        case 24:
            return boxten::FORMAT_SAMPLE_TYPE::S24_LE;
        case 32:
            return boxten::FORMAT_SAMPLE_TYPE::S32_LE;
        }
        return boxten::FORMAT_SAMPLE_TYPE::UNKNOWN;
    }
}

boxten::PCMPacketUnit FlacInput::read_frames(boxten::AudioFile& file, u64 from, boxten::n_frames frames) {
    boxten::PCMPacketUnit result;
    auto decoder = get_decoder(file);
    if(decoder == nullptr) return result;
    
    auto position = decoder->read_frames(from, frames, result.pcm);
    result.format.sample_type    = bps_to_sample_type(decoder->get_bits_per_sample());
    result.format.channels       = decoder->get_channels();
    result.format.sampling_rate  = decoder->get_sample_rate();
    result.original_frame_pos[0] = position;
    result.original_frame_pos[1] = position + result.get_frames() - 1;
    //printf("result %lu\n", result.get_frames());
    return result;
}
boxten::AudioTag FlacInput::read_tags(boxten::AudioFile& file) {
    boxten::AudioTag result;
    result["Title"] = "FLAC TITLE";
    return result;
}
boxten::n_frames FlacInput::calc_total_frames(boxten::AudioFile& file) {
    auto decoder = get_decoder(file);
    if(decoder == nullptr) return 0;

    return decoder->get_total_samples();
}

BOXTEN_MODULE({"FLAC input", boxten::COMPONENT_TYPE::STREAM_INPUT, CATALOGUE_CALLBACK(FlacInput)})