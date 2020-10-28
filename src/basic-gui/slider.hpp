#pragma once
#include <QSlider>
#include <libboxten.hpp>

class SeekSlider : public QSlider, public boxten::Widget {
  private:
    bool           is_slider_pressed = false;
    bool           finish_update_loop;
    boxten::Worker update_loop_thread;
    void           update_range();
    void           update_loop();
    void           start_update_loop();
    void           exit_update_loop();

    /* slots */
    void slider_pressed();
    void slider_released();

  public:
    QSize sizeHint() const override;
    SeekSlider(void* param);
    ~SeekSlider();
};