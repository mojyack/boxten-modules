#include <QAbstractTableModel>

#include "playlist-viewer.hpp"
#include "playlist-util.hpp"

namespace{
class PlaylistViewerModel : public QAbstractTableModel {
  private:
    Playlists& playlists;

  public:
    int      columnCount(const QModelIndex& parent = QModelIndex()) const override;
    int      rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    PlaylistViewerModel(Playlists& playlists);
    ~PlaylistViewerModel() {}
};
int PlaylistViewerModel::columnCount(const QModelIndex& parent) const {
    return 1;
}
int PlaylistViewerModel::rowCount(const QModelIndex& parent) const {
    std::lock_guard<std::mutex> lock(playlists.lock);
    auto& playing = playlists.playing();
    std::lock_guard<std::mutex> plock(playing.mutex());
    return playing.size();
}
QVariant PlaylistViewerModel::data(const QModelIndex& index, int role) const {
    if(!index.isValid() || role != Qt::DisplayRole) {
        return QVariant();
    }
    switch(index.column()) {
    case 0:
        return index.row() + 1000;
    default:
        return QVariant();
    }
}
QVariant PlaylistViewerModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if(role != Qt::DisplayRole) {
        return QVariant();
    }
    if(orientation == Qt::Horizontal) {
        switch(section) {
        case 0:
            return "Name";
        default:
            return QVariant();
        }
    } else {
        return QVariant(section + 1);
    }
}
PlaylistViewerModel::PlaylistViewerModel(Playlists& playlists) : playlists(playlists){}

} // namespace

PlaylistViewer::PlaylistViewer(void* param) : boxten::Widget(param) {
    setModel(new PlaylistViewerModel(playlists));
    setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred));
}