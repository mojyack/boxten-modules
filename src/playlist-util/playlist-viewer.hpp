#pragma once
#include <QTableView>
#include <libboxten.hpp>

struct Playlists;
class PlaylistViewer : public QTableView, public boxten::Widget {
  public:
    PlaylistViewer(void* param);
    ~PlaylistViewer() {}
};