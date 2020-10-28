#pragma once
#include <mutex>

#include <libboxten.hpp>

#include "playlist-viewer.hpp"
#include <config.h>


struct Playlists{
    u64                            playing_playlist = 0;
    std::vector<boxten::Playlist*> data;
    std::mutex                     lock;

    boxten::Playlist& operator[](u64 n) {
        return *data[n];
    }
    boxten::Playlist& playing() {
        return *data[playing_playlist];
    }
};

inline Playlists playlists;