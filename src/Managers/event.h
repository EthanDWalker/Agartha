#pragma once
#include <functional>

struct Event {
  std::vector<std::function<bool()>> listeners;

  void Trigger();

  void TriggerAll();

  void AddListener(std::function<bool()> listener);
};
