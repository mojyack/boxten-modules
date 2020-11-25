#include "button.hpp"

QSize PlayPauseButton::sizeHint() const {
    return QSize(24, 24);
}

void PlayPauseButton::update_icon(boxten::Events event, void* param) {
    auto state = reinterpret_cast<boxten::HookParameters::PlaybackChange*>(param)->new_state;
    bool playing = state == boxten::PlaybackState::PLAYING;
    setIcon(icons[playing]);
}
void PlayPauseButton::send_control() {
    if(boxten::get_playback_state() == boxten::PlaybackState::PLAYING) {
        boxten::pause_playback();
    } else {
        boxten::start_playback();
    }
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
    setIcon(icons[boxten::get_playback_state() == boxten::PlaybackState::PLAYING]);
    connect(this, &QPushButton::clicked, this, &PlayPauseButton::send_control);
    install_eventhook(std::bind(&PlayPauseButton::update_icon, this, std::placeholders::_1, std::placeholders::_2), boxten::Events::PLAYBACK_CHANGE);
    setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    resize(sizeHint());
}
PlayPauseButton::~PlayPauseButton() {
    icons_ref_count--;
    if(icons_ref_count == 0) icons[0] = icons[1] = QIcon();
}
