#include "console.h"

namespace ui {
std::mutex Console::log_mutex = {};
std::queue<std::string> Console::logs = {};
} // namespace ui
