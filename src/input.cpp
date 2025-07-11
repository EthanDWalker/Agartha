#include "input.h"
#include <GLFW/glfw3.h>

__uint128_t InputContext::_pressed_input = 0;
__uint128_t InputContext::_held_input = 0;
__uint128_t InputContext::_released_input = 0;
glm::ivec2 InputContext::_window_size = glm::ivec2(1);

glm::vec2 InputContext::mouse_position = glm::vec2(0.0f);

void InputContext::Update(GLFWwindow *window) {
  glfwPollEvents();

  glfwGetWindowSize(window, &_window_size.x, &_window_size.y);
  glm::dvec2 mouse_position_double;
  glfwGetCursorPos(window, &mouse_position_double.x, &mouse_position_double.y);

  mouse_position =
      (static_cast<glm::vec2>(mouse_position_double) + glm::vec2(0.5f)) /
      static_cast<glm::vec2>(_window_size);
  mouse_position = mouse_position * 2.0f - 1.0f;

  _pressed_input = 0;
  _released_input = 0;
  for (uint8_t i = 0; i < Input::KEYBOARD_COUNT; i++) {
    __uint128_t input_down = glfwGetKey(window, KEY_TO_GLFW_KEY[i]);

    _pressed_input |= __uint128_t(!GetInputHeld((Input)i) && bool(input_down))
                      << i;

    _released_input |= __uint128_t(GetInputHeld((Input)i) && !bool(input_down))
                       << i;
  }

  for (uint8_t i = Input::KEYBOARD_COUNT; i < Input::COUNT; i++) {
    __uint128_t input_down = glfwGetMouseButton(
        window, MOUSE_TO_GLFW_MOUSE[i - Input::KEYBOARD_COUNT]);

    _pressed_input |= __uint128_t(!GetInputHeld((Input)i) && bool(input_down))
                      << i;

    _released_input |= __uint128_t(GetInputHeld((Input)i) && !bool(input_down))
                       << i;
  }

  _held_input = 0;
  for (uint8_t i = 0; i < Input::KEYBOARD_COUNT; i++) {
    __uint128_t input_down = glfwGetKey(window, KEY_TO_GLFW_KEY[i]);

    _held_input |= input_down << i;
  }

  for (uint8_t i = Input::KEYBOARD_COUNT; i < Input::COUNT; i++) {
    __uint128_t input_down = glfwGetMouseButton(
        window, MOUSE_TO_GLFW_MOUSE[i - Input::KEYBOARD_COUNT]);

    _held_input |= input_down << i;
  }
}
