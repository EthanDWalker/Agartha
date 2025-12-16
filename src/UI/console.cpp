#include "console.h"

namespace ui {
std::mutex Console::log_mutex = {};
std::queue<std::string> Console::logs = {};
size_t Console::index = 0;
} // namespace ui
