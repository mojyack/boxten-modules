#include <filesystem>

#include <json.hpp>
#include <jsontest.hpp>

#include "playlist-util.hpp"

namespace{
class PlaylistUtil : public boxten::Module {
  public:
    PlaylistUtil(void* param);
    ~PlaylistUtil();
};
PlaylistUtil::PlaylistUtil(void* param):boxten::Module(param){
    if(nlohmann::json conf; load_configuration(conf)) {
        do {
            constexpr const char* key = "Playlists";
            if(!boxten::array_type_check(key, boxten::JSON_TYPE::OBJECT, conf)) {
                break;
            }
            auto playlist_conf = conf[key];
            for(auto& p : playlist_conf) {
                if(!boxten::type_check("name", boxten::JSON_TYPE::STRING, p) ||
                   !boxten::array_type_check("files", boxten::JSON_TYPE::STRING, p)) {
                    continue;
                }
                auto name         = p["name"].get<std::string>();
                auto files        = p["files"].get<std::vector<std::filesystem::path>>();
                auto new_playlist = new boxten::Playlist;
                new_playlist->set_name(name.data());
                for(auto& f : files) {
                    new_playlist->add(f);
                }
                playlists.data.emplace_back(new_playlist);
            }
        } while(0);
    }
    if(playlists.data.empty()) {
        playlists.data.emplace_back(new boxten::Playlist);
    }
    if(i64 last_active; get_number("Active playlist", last_active) && static_cast<std::size_t>(last_active) < playlists.data.size()) {
        playlists.playing_playlist = last_active;
    } else {
        playlists.playing_playlist = 0;
    }
    playlists.data[playlists.playing_playlist]
        ->activate();
}
PlaylistUtil::~PlaylistUtil(){
    nlohmann::json conf;
    load_configuration(conf);

    std::vector<nlohmann::json> playlist_conf;
    for(auto& p : playlists.data) {
        std::vector<std::string>    files;
        std::lock_guard<std::mutex> lock(p->mutex());
        for(auto m : *p) {
            files.emplace_back(m->get_path().string());
        }
        nlohmann::json playlist;
        playlist["name"]  = p->get_name();
        playlist["files"] = files;
        playlist_conf.emplace_back(playlist);
    }
    conf["Playlists"]       = playlist_conf;
    conf["Active playlist"] = playlists.playing_playlist;
    save_configuration(conf);

    for(auto p : playlists.data) {
        delete p;
    }
}
}
BOXTEN_MODULE(
    {"Playlist util", boxten::COMPONENT_TYPE::MODULE, CATALOGUE_CALLBACK(PlaylistUtil)},
    {"Playlist viewer", boxten::COMPONENT_TYPE::WIDGET, CATALOGUE_CALLBACK(PlaylistViewer)})