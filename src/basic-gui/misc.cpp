#include <QLabel>

#include "misc.hpp"

Spacer::Spacer(void* param) : boxten::Widget(param) {
    new QLabel("Spacer", this);
    setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred));
}