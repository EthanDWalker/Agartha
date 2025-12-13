#include "event.h"

void Event::AddListener(std::function<bool()> listener) {
  listeners.push_back(listener);
}

void Event::TriggerAll() {
  for (auto &layer : listeners) {
    if (layer) {
      layer();
    }
  }
}

void Event::Trigger() {
  for (auto &layer : listeners) {
    if (layer) {
      if (layer()) {
        break;
      }
    }
  }
}
