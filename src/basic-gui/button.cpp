#include "button.hpp"

QSize PlayPauseButton::sizeHint() const {
    return QSize(24, 24);
}

void PlayPauseButton::update_icon() {
    setIcon(icons[is_playing()]);
}
void PlayPauseButton::send_control() {
    if(is_playing()){
        boxten::pause_playback();
    }else{
        boxten::start_playback();
    }
}
bool PlayPauseButton::is_playing() {
    auto state = boxten::get_playback_state();
    return state == boxten::PLAYBACK_STATE::PLAYING;
}
PlayPauseButton::PlayPauseButton(void* param) : boxten::Widget(param) {
    if(icons[0].isNull() || icons[1].isNull()) {
        auto        resource_dir  = get_resource_dir();
        const char* file_names[2] = {"play-icon.svg", "pause-icon.svg"};
        for(int i = 0; i < 2; ++i) {
            auto icon_path = resource_dir;
            icon_path /= file_names[i];
            icons[i] = QIcon(icon_path.string().data());
        }
    }
    icons_ref_count++;
    update_icon();
    connect(this, &QPushButton::clicked, this, &PlayPauseButton::send_control);
    install_eventhook(std::bind(&PlayPauseButton::update_icon, this),
                      {boxten::EVENT::PLAYBACK_START,
                       boxten::EVENT::PLAYBACK_STOP,
                       boxten::EVENT::PLAYBACK_PAUSE,
                       boxten::EVENT::PLAYBACK_RESUME});
    setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    resize(sizeHint());
}
PlayPauseButton::~PlayPauseButton() {
    icons_ref_count--;
    if(icons_ref_count == 0) icons[0] = icons[1] = QIcon();
}
