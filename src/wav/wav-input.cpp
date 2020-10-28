#include "wav-input.hpp"

namespace{
struct FMT {
    u16 format;     // 0 = unknown  1 = PCM 3 = FLOAT
    u16 channels;   // 1 = mono  2 = steleo
    u32 samplerate; // Hz
    u32 bytepersec; // samplerate * bitswidth * channels
    u16 blockalign; // unused if format is PCM
    u16 bitswidth;  // bits per sample
};
} // namespace

bool WavInput::read_pcm_info(std::ifstream& handle, PCMInfo& pcm_info){
    std::streampos riff_limit;
    { // check if file has RIFF chunk
        char riff_identifier[4] = {"\0"};
        handle.read(riff_identifier, 4);
        if(std::strncmp(riff_identifier, "RIFF", 4) != 0) return false;
    }

    {
        i32 riff_size;
        handle.read(reinterpret_cast<char*>(&riff_size), 4);
        riff_limit = handle.tellg() + static_cast<std::streamoff>(riff_size);
    }

    { // check format type
        char format_type[4];
        handle.read(format_type, 4);
        if(std::strncmp(format_type, "WAVE", 4) != 0) return false;
    }

    FMT fmt;
    i32 data_chunk_size = -1;
    {
        bool format_chunk_found = false, data_chunk_found = false;
        while(1) {
            char chunk_identifier[4];
            i32  chunk_limit;
            handle.read(chunk_identifier, 4);
            handle.read(reinterpret_cast<char*>(&chunk_limit), 4);
            auto current_pos = handle.tellg();

            if(std::strncmp(chunk_identifier, "fmt ", 4) == 0) {
                handle.read(reinterpret_cast<char*>(&fmt), sizeof(FMT));
                format_chunk_found = true;
            } else if(std::strncmp(chunk_identifier, "data", 4) == 0) {
                data_chunk_size   = chunk_limit;
                pcm_info.data_pos = handle.tellg();
                data_chunk_found  = true;
            } else if(std::strncmp(chunk_identifier, "LIST", 4) == 0) {
                u32  list_limit;
                char list_type[4];
                handle.read(reinterpret_cast<char*>(&list_limit), sizeof(u32));
                handle.read(list_type, 4);
                if(std::strncmp(list_type, "INFO", 4) == 0){
                    pcm_info.info_pos = handle.tellg();
                    pcm_info.info_limit = list_limit - 4; // - 4 "INFO"
                }
            }

            handle.seekg(current_pos, std::ios_base::beg);
            if(handle.tellg() + static_cast<std::streamoff>(chunk_limit) >= riff_limit){
                break;
            } else {
                handle.seekg(chunk_limit, std::ios_base::cur);
            }
        }
        if(!format_chunk_found || !data_chunk_found) return false;
    }

    pcm_info.format = boxten::FORMAT_SAMPLE_TYPE::UNKNOWN;
    switch(fmt.format) {
    case 1:
        switch(fmt.bitswidth) {
        case 8:
            pcm_info.format = boxten::FORMAT_SAMPLE_TYPE::U8;
            break;
        case 16:
            pcm_info.format = boxten::FORMAT_SAMPLE_TYPE::S16_LE;
            break;
        case 24:
            pcm_info.format = boxten::FORMAT_SAMPLE_TYPE::S24_LE;
            break;
        case 32:
            pcm_info.format = boxten::FORMAT_SAMPLE_TYPE::S32_LE;
            break;
        }
        break;
    case 3:
        switch(fmt.bitswidth) {
        case 32:
            pcm_info.format = boxten::FORMAT_SAMPLE_TYPE::FLOAT;
            break;
        }
        break;
    }
    if(pcm_info.format == boxten::FORMAT_SAMPLE_TYPE::UNKNOWN) return false;
    pcm_info.channels   = fmt.channels;
    pcm_info.samplerate = fmt.samplerate;
    pcm_info.total_frames = data_chunk_size / fmt.channels / (fmt.bitswidth / 8);
    return true;
}

PCMInfo* WavInput::get_pcm_info(boxten::AudioFile& file){
    auto pcm_info = reinterpret_cast<PCMInfo*>(file.get_private_data());
    if(pcm_info == nullptr) {
        PCMInfo pcm_info_tmp;
        if(!read_pcm_info(file.get_handle(), pcm_info_tmp)) {
            boxten::console << "error while loading " << file.get_path().filename() << std::endl;
            return nullptr;
        }
        pcm_info  = new PCMInfo;
        *pcm_info = pcm_info_tmp;
        file.set_private_data(pcm_info, [](void* pcm_info) {
            delete reinterpret_cast<PCMInfo*>(pcm_info);
        });
    }
    return pcm_info;
}

boxten::PCMPacketUnit WavInput::read_frames(boxten::AudioFile& file, u64 from, boxten::n_frames frames) {
    boxten::PCMPacketUnit result;
    auto pcm_info = get_pcm_info(file);
    if(pcm_info == nullptr) return result;

    result.format.sample_type    = pcm_info->format;
    result.format.channels       = pcm_info->channels;
    result.format.sampling_rate  = pcm_info->samplerate;
    u64 pcm_size                 = frames * result.format.get_bytewidth() * result.format.channels;
    u64 pcm_offset               = from * result.format.get_bytewidth() * result.format.channels;
    result.original_frame_pos[0] = from;
    result.original_frame_pos[1] = from + frames;
    result.pcm.resize(pcm_size);
    {
        auto& handle = file.get_handle();
        handle.seekg(pcm_info->data_pos);
        handle.seekg(pcm_offset, std::ios_base::cur);
        handle.read(reinterpret_cast<char*>(result.pcm.data()), pcm_size);
    }
    return result;
}
boxten::AudioTag WavInput::read_tags(boxten::AudioFile& file) {
    boxten::AudioTag result;
    auto             pcm_info = get_pcm_info(file);
    if(pcm_info == nullptr) return result;
    auto& handle = file.get_handle();
    while(handle.tellg() < (pcm_info->info_pos + pcm_info->info_limit)){
        char chunk_identifier[4];
        i32  chunk_limit;
        handle.read(chunk_identifier, 4);
        handle.read(reinterpret_cast<char*>(&chunk_limit), 4);
        std::streampos next_chunk = handle.tellg() + static_cast<std::streamoff>(chunk_limit);

        struct {
            const char* tag_id;
            const char* tag_name;
        } constexpr tag_table[] = {
            {"IART", "Artist"},
            {"ICMT", "Comment"},
            {"ICOP", "Copyright"},
            {"ICRD", "DateCreated"},
            {"ICRP", "Cropped"},
            {"IDIM", "Dimensions"},
            {"IENG", "Engineer"},
            {"IGNR", "Genre"},
            {"IKEY", "Keywords"},
            {"INAM", "Title"},
            {"IPRD", "Album"},
            {"ISFT", "Software"},
            {"ITRK", "TrackNumber"},
        };
        constexpr u64 tag_table_limit = sizeof(tag_table) / sizeof(tag_table[0]);
        for(u64 i = 0; i < tag_table_limit;++i){
            if(std::strncmp(tag_table[i].tag_id, chunk_identifier, 4) == 0) {
                std::string data(chunk_limit, '\0');
                handle.read(&data[0], chunk_limit);
                result[tag_table[i].tag_name] = data;
            }
        }
        handle.seekg(next_chunk, std::ios_base::beg);
    }
    return result;
}
boxten::n_frames WavInput::calc_total_frames(boxten::AudioFile& file){
    auto pcm_info = get_pcm_info(file);
    return pcm_info->total_frames;
}

BOXTEN_MODULE({"Wav input", boxten::COMPONENT_TYPE::STREAM_INPUT, CATALOGUE_CALLBACK(WavInput)});