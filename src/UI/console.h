#pragma once

#include <fmt/core.h>
#include <imgui.h>
#include <mutex>
#include <queue>

namespace ui {

struct Console {
  static const uint32_t MAX_LOGS = 100;
  static std::mutex log_mutex;
  static std::queue<std::string> logs;
  static size_t index;

  template <typename... T> static void Log(const fmt::format_string<T...> fmt_string, T &&...args) {
    std::lock_guard<std::mutex> lock(log_mutex);
    index++;
    logs.push(fmt::format(fmt_string, std::forward<T>(args)...));
    if (logs.size() > MAX_LOGS) {
      logs.pop();
    }
  }

  static const std::queue<std::string> &GetLogs() {
    std::lock_guard<std::mutex> lock(log_mutex);
    return logs;
  }

  static void Draw() {
    if (ImGui::Begin("Console", 0, ImGuiWindowFlags_NoScrollbar)) {
      if (ImGui::BeginChild("Logs", ImGui::GetContentRegionAvail(),
                            ImGuiChildFlags_NavFlattened | ImGuiChildFlags_FrameStyle,
                            ImGuiWindowFlags_HorizontalScrollbar)) {
        std::lock_guard<std::mutex> lock(log_mutex);
        for (int32_t i = logs.size() - 1; i >= 0; --i) {
          ImGui::TextWrapped("%zu %s", index, logs._Get_container().at(i).c_str());
        }
      }
      ImGui::EndChild();
    }
    ImGui::End();
  }
};
} // namespace ui
