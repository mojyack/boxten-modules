#pragma once
#include "console.hpp"
#include <optional>
#include <vector>

#include <FLAC++/decoder.h>

#include <libboxten.hpp>

class Decoder : public FLAC::Decoder::Stream {
  private:
    boxten::AudioFile&      file;
    uint64_t*               write_callback_position;
    std::optional<uint64_t> stream_position;
    std::vector<uint8_t>*   write_callback_buffer = nullptr;
    boxten::n_frames        total_frames          = 0;
    boxten::ConsoleSet&     console;

    FLAC__StreamDecoderWriteStatus  write_callback(const FLAC__Frame* frame, const FLAC__int32* const buffer[]) override;
    FLAC__StreamDecoderReadStatus   read_callback(FLAC__byte buffer[], size_t* bytes) override;
    FLAC__StreamDecoderSeekStatus   seek_callback(FLAC__uint64 absolute_byte_offset) override;
    FLAC__StreamDecoderTellStatus   tell_callback(FLAC__uint64* absolute_byte_offset) override;
    FLAC__StreamDecoderLengthStatus length_callback(FLAC__uint64* stream_length) override;
    void                            metadata_callback(const FLAC__StreamMetadata* metadata) override;
    void                            error_callback(FLAC__StreamDecoderErrorStatus status) override;

  public:
    uint64_t                read_frames(uint64_t from, boxten::n_frames frames, std::vector<uint8_t>& buffer);
    boxten::n_frames        get_total_frames();
    std::optional<uint64_t> get_current_frame_pos();
    Decoder(boxten::AudioFile& file, boxten::ConsoleSet& console);
    Decoder(const Decoder&) = delete;
    Decoder(Decoder&&)      = delete;
    Decoder& operator=(const Decoder&) = delete;
    Decoder& operator=(Decoder&&) = delete;
    Decoder()                     = delete;
};
