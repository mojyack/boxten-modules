#pragma once
#include <QTableView>
#include <libboxten.hpp>

enum class Tags {
  Title,
  Artist,
};
constexpr std::pair<Tags,const char*> tag_labels[] = {
    {Tags::Title, "Title"},
    {Tags::Artist, "Artist"},
};
inline const char* find_tag_label(Tags tag){
    constexpr size_t tag_labels_limit = sizeof(tag_labels) / sizeof(tag_labels[0]);
    for(size_t i = 0; i < tag_labels_limit; ++i) {
        if(tag_labels[i].first == tag) return tag_labels[i].second;
    }
    return nullptr;
}
class PlaylistViewer : public QTableView, public boxten::Widget {
  private:
    std::vector<Tags> tag_config;
  public:
    PlaylistViewer(void* param);
    ~PlaylistViewer() {}
};