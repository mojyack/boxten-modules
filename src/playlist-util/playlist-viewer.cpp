#include <QAbstractTableModel>
#include <QHeaderView>

#include "playlist-viewer.hpp"
#include "playlist-util.hpp"

namespace{
class PlaylistViewerModel : public QAbstractTableModel {
  private:
    Playlists&         playlists;
    std::vector<Tags>& tag_config;

  public:
    int      columnCount(const QModelIndex& parent = QModelIndex()) const override;
    int      rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void playing_song_change_hook(boxten::Events event, void* param);
    PlaylistViewerModel(Playlists& playlists, std::vector<Tags>& tag_config);
    ~PlaylistViewerModel() {}
};
int PlaylistViewerModel::columnCount(const QModelIndex& /*parent*/) const {
    return tag_config.size();
}
int PlaylistViewerModel::rowCount(const QModelIndex& /*parent*/) const {
    std::lock_guard<std::mutex> lock(playlists.lock);
    auto& playing = playlists.playing();
    std::lock_guard<std::mutex> plock(playing.mutex());
    return playing.size();
}
QVariant PlaylistViewerModel::data(const QModelIndex& index, int role) const {
    if(!index.isValid()) return QVariant();
    switch(role) {
    case Qt::FontRole: {
        std::lock_guard<std::mutex> lock(playlists.lock);
        if(index.row() == static_cast<int>(boxten::get_playing_index())) {
            QFont font;
            font.setBold(true);
            return font;
        } else {
            return QVariant();
        }
    } break;
    case Qt::DisplayRole: {
        if(index.column() >= static_cast<int>(tag_config.size())) return QVariant();
        switch(auto tag = tag_config[index.column()]; tag) {
        case Tags::Title: {
        case Tags::Artist: {
            auto label = find_tag_label(tag);
            if(label == nullptr) return QVariant();
            std::lock_guard<std::mutex> lock(playlists.lock);
            std::lock_guard<std::mutex> plock(playlists.playing().mutex());
            auto&                       song = *(playlists.playing()[index.row()]);
            return song.get_tags()[label].data();
        } break;
        }
        }
    } break;
    default: {
        return QVariant();
    } break;
    }
    return QVariant();
}
QVariant PlaylistViewerModel::headerData(int section, Qt::Orientation orientation, int role) const {
    switch(role) {
    case Qt::DisplayRole: {
        if(orientation == Qt::Horizontal) {
            if(section >= static_cast<int>(tag_config.size())) return QVariant();
            return find_tag_label(tag_config[section]);
        } else {
            return QVariant(section + 1);
        }
    } break;
    }
    return QVariant();
}
void PlaylistViewerModel::playing_song_change_hook(boxten::Events event, void* param) {
    if(event == boxten::Events::SONG_CHANGE){
        auto p = reinterpret_cast<boxten::HookParameters::SongChange*>(param);
        emit dataChanged(index(0, p->old_index), index(columnCount(), p->old_index));
        emit dataChanged(index(0, p->new_index), index(columnCount(), p->new_index));
    }
}
PlaylistViewerModel::PlaylistViewerModel(Playlists& playlists, std::vector<Tags>& tag_config) : playlists(playlists), tag_config(tag_config) {}

} // namespace

PlaylistViewer::PlaylistViewer(void* param) : boxten::Widget(param) {
    tag_config = {Tags::Title, Tags::Artist};
    auto model = new PlaylistViewerModel(playlists, tag_config);
    install_eventhook(std::bind(&PlaylistViewerModel::playing_song_change_hook, model, std::placeholders::_1, std::placeholders::_2), boxten::Events::SONG_CHANGE);

    setModel(model);
    setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred));
    horizontalHeader()->setHighlightSections(false);
    verticalHeader()->hide();
    setSelectionBehavior(QAbstractItemView::SelectRows);
    resizeColumnsToContents();
}