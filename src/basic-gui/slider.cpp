#include <chrono>
#include <thread>

#include <QProxyStyle>

#include "slider.hpp"

namespace{
class Style : public QProxyStyle {
  public:
    int styleHint(QStyle::StyleHint hint, const QStyleOption* option = 0, const QWidget* widget = 0, QStyleHintReturn* returnData = 0) const override{
        if(hint == QStyle::SH_Slider_AbsoluteSetButtons)
            return (Qt::LeftButton | Qt::MidButton | Qt::RightButton);
        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }
    Style(QStyle* style = nullptr) : QProxyStyle(style) {}
};
}

void SeekSlider::update_range() {
    setRange(0, boxten::get_playing_song_length());
}
void SeekSlider::update_loop() {
    while(!finish_update_loop){
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if(!is_slider_pressed) {
            setValue(boxten::get_playback_pos());
        }
    }
}
void SeekSlider::start_update_loop(){
    if(update_loop_thread) return;
    finish_update_loop = false;
    update_loop_thread = boxten::Worker(std::bind(&SeekSlider::update_loop, this));
}
void SeekSlider::exit_update_loop() {
    finish_update_loop = true;
    if(update_loop_thread)update_loop_thread.join();
}
void SeekSlider::slider_pressed(){
    is_slider_pressed = true;
}
void SeekSlider::slider_released(){
    is_slider_pressed = false;
    double pos     = static_cast<double>(value()) / static_cast<double>(maximum());
    boxten::seek_rate_abs(pos);
}
QSize SeekSlider::sizeHint() const {
    return QSize(320, 24);
}
SeekSlider::SeekSlider(void* param) : QSlider(Qt::Horizontal), boxten::Widget(param) {
    setStyle(new Style(this->style()));
    connect(this, &QSlider::sliderPressed, this, &SeekSlider::slider_pressed);
    connect(this, &QSlider::sliderReleased, this, &SeekSlider::slider_released);

    install_eventhook(std::bind(&SeekSlider::update_range, this),
                      {boxten::EVENT::PLAYBACK_START,
                       boxten::EVENT::SONG_CHANGE});
    install_eventhook(std::bind(&SeekSlider::start_update_loop, this),
                      {boxten::EVENT::PLAYBACK_START,
                       boxten::EVENT::PLAYBACK_RESUME});
    install_eventhook(std::bind(&SeekSlider::exit_update_loop, this),
                      {boxten::EVENT::PLAYBACK_STOP,
                       boxten::EVENT::PLAYBACK_PAUSE});

    setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed));
}
SeekSlider::~SeekSlider() {
    exit_update_loop();
}