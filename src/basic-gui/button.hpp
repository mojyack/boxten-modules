#pragma once
#include <QIcon>
#include <QPushButton>
#include <libboxten.hpp>

class PlayPauseButton : public QPushButton, public boxten::Widget {
  private:
    static inline i32   icons_ref_count = 0;
    static inline QIcon icons[2];
    void                update_icon(boxten::Events event, void* param = nullptr);
    void                send_control();

  public:
    QSize sizeHint() const override;
    PlayPauseButton(void* param);
    ~PlayPauseButton();
};