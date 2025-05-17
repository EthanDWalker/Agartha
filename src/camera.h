#pragma once
#include <GLFW/glfw3.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

class Camera {
public:
  glm::vec3 velocity;
  glm::vec3 position;

  float pitch{0.0f};
  float yaw{0.0f};

  glm::mat4 GetViewMatrix();
  glm::mat4 GetRotationMatrix();

  void ProcessInput(GLFWwindow *window, float delta_time);

  void Update();
};
