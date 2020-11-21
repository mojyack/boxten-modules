#include "basic-gui.hpp"

BOXTEN_MODULE({"Spacer", boxten::COMPONENT_TYPE::WIDGET, CATALOGUE_CALLBACK(Spacer)},
              {"Play-Pause button", boxten::COMPONENT_TYPE::WIDGET, CATALOGUE_CALLBACK(PlayPauseButton)},
              {"SeekSlider", boxten::COMPONENT_TYPE::WIDGET, CATALOGUE_CALLBACK(SeekSlider)}, )