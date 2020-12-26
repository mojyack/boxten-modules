#include "decoder.hpp"
#include <FLAC/stream_decoder.h>
#include <stdexcept>

namespace {
inline size_t byte_to_frames(Decoder* self, size_t bytes) {
    return bytes / self->get_channels() / (self->get_bits_per_sample() / 8);
}
} // namespace

FLAC__StreamDecoderWriteStatus
Decoder::write_callback(const FLAC__Frame* frame, const FLAC__int32* const buffer[]) {
    const size_t bytewidth = frame->header.bits_per_sample / 8;
    if(write_callback_buffer == nullptr) {
        // maybe flushed
    } else {
        if(write_callback_position != nullptr) {
            *write_callback_position =
                frame->header.number_type == FLAC__FrameNumberType::FLAC__FRAME_NUMBER_TYPE_SAMPLE_NUMBER
                    ? frame->header.number.sample_number
                    : frame->header.number.frame_number;
            write_callback_position = nullptr;
        }
        for(uint32_t b = 0; b < frame->header.blocksize; ++b) {
            for(uint32_t c = 0; c < frame->header.channels; ++c) {
                const FLAC__int32& block = buffer[c][b];
                for(size_t l = 0; l < bytewidth; ++l) {
                    write_callback_buffer->emplace_back(reinterpret_cast<const uint8_t*>(&block)[l]);
                }
            }
        }
    }
    const size_t increased_bytes = frame->header.blocksize * frame->header.channels * bytewidth;
    stream_position              = frame->header.number.sample_number + byte_to_frames(this, increased_bytes);
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}
FLAC__StreamDecoderReadStatus Decoder::read_callback(FLAC__byte buffer[], size_t* bytes) {
    auto&                         handle = file.get_handle();
    FLAC__StreamDecoderReadStatus result;
    do {
        if(handle.eof()) {
            *bytes = 0;
            result = FLAC__StreamDecoderReadStatus::FLAC__STREAM_DECODER_READ_STATUS_ABORT;
            break;
        }
        if(bytes == 0) {
            result = FLAC__StreamDecoderReadStatus::FLAC__STREAM_DECODER_READ_STATUS_ABORT;
            break;
        }
        handle.read(reinterpret_cast<char*>(buffer), *bytes);
        *bytes = handle.gcount();

        if(*bytes == 0) {
            result = FLAC__STREAM_DECODER_READ_STATUS_ABORT;
        } else if(handle.eof()) {
            handle.clear();
            result = FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
        } else {
            result = FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
        }
    } while(0);
    return result;
}
FLAC__StreamDecoderSeekStatus Decoder::seek_callback(FLAC__uint64 absolute_byte_offset) {
    auto& handle = file.get_handle();
    if(!handle.seekg(absolute_byte_offset, std::ios_base::beg)) {
        throw std::runtime_error(get_state().as_cstring());
        return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
    } else {
        return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
    }
}
FLAC__StreamDecoderTellStatus Decoder::tell_callback(FLAC__uint64* absolute_byte_offset) {
    auto& handle          = file.get_handle();
    *absolute_byte_offset = static_cast<FLAC__uint64>(handle.tellg());
    return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}
FLAC__StreamDecoderLengthStatus Decoder::length_callback(FLAC__uint64* stream_length) {
    auto& handle = file.get_handle();
    auto  cp     = handle.tellg();
    handle.seekg(0, std::ios_base::end);
    *stream_length = static_cast<FLAC__uint64>(handle.tellg());
    handle.seekg(cp);
    return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}
void Decoder::metadata_callback(const FLAC__StreamMetadata* metadata) {
    if(metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
        total_frames = metadata->data.stream_info.total_samples;
    }
}
void Decoder::error_callback(FLAC__StreamDecoderErrorStatus status) {
     console.error << "FLAC: libflac error: " << FLAC__StreamDecoderErrorStatusString[status];
}

uint64_t Decoder::read_frames(uint64_t from, boxten::n_frames frames, std::vector<uint8_t>& buffer) {
    uint64_t position;
    write_callback_buffer   = &buffer;
    write_callback_position = &position;

    boxten::n_frames total_decoded = 0;
    if(auto pos = get_current_frame_pos(); !pos || pos.value() != from) {
        if(!seek_absolute(from)) {
            console.error << "FLAC: seek out of range: " << from << std::endl;
        }
        total_decoded += byte_to_frames(this, write_callback_buffer->size()); // seek_absolute() decodes some samples
    }

    while(total_decoded < frames) {
        if(!process_single()) {
            console.error << "FLAC: process_single() failed." << std::endl;
            break;
        }
        if(get_channels() == 0 || get_blocksize() == 0) {
            continue;
        }
        total_decoded += byte_to_frames(this, write_callback_buffer->size());
    }
    write_callback_buffer = nullptr;
    return position;
}
boxten::n_frames        Decoder::get_total_frames() { return total_frames; }
std::optional<uint64_t> Decoder::get_current_frame_pos() {
    return stream_position;
}
Decoder::Decoder(boxten::AudioFile& file, boxten::ConsoleSet& console) : file(file), console(console) {}
