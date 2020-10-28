#pragma once
#include <QIcon>
#include <QPushButton>
#include <libboxten.hpp>

#include "button.hpp"

class PlayPauseButton : public QPushButton, public boxten::Widget {
  private:
    static inline i32   icons_ref_count = 0;
    static inline QIcon icons[2];
    void                update_icon();
    void                send_control();
    bool                is_playing();

  public:
    QSize sizeHint() const override;
    PlayPauseButton(void* param);
    ~PlayPauseButton();
};