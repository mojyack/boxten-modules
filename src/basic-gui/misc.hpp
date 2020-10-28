#pragma once
#include <QWidget>
#include <libboxten.hpp>

class Spacer : public QWidget, public boxten::Widget {
  public:
    Spacer(void* param);
    ~Spacer(){}
};