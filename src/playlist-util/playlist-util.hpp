#pragma once
#include <mutex>

#include <libboxten.hpp>

#include "playlist-viewer.hpp"
#include <config.h>


struct Playlists{
    i64                            playing_playlist;
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