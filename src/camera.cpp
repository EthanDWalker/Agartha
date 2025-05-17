#include "camera.h"
#include "GLFW/glfw3.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

void Camera::Update() {
  glm::mat4 camera_rotation = GetRotationMatrix();
  position += glm::vec3(camera_rotation * glm::vec4(velocity * 0.5f, 0.0f));
}

void Camera::ProcessInput(GLFWwindow *window, float delta_time) {
  static double last_x, last_y;

  double pos_x, pos_y;
  int width, height;

  glfwGetWindowSize(window, &width, &height);
  glfwGetCursorPos(window, &pos_x, &pos_y);

  pos_x -= width / 2.0f;
  pos_y -= height / 2.0f;

  double dx, dy;
  dx = pos_x - last_x;
  dy = last_y - pos_y;

  last_x = pos_x;
  last_y = pos_y;

  const double sensitivity = 0.003f;
  dx *= sensitivity;
  dy *= sensitivity;

  yaw += static_cast<float>(dx);
  pitch += static_cast<float>(dy);

  if (pitch < -89.0f)
    pitch = -89.0f;
  else if (pitch > 89.0f)
    pitch = 89.0f;

  const float speed = 5.0f * delta_time;

  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    velocity.z = -speed;
  else if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    velocity.z = speed;
  else
    velocity.z = 0.0f;

  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    velocity.x = -speed;
  else if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    velocity.x = speed;
  else
    velocity.x = 0.0f;

  if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
    velocity.y = -speed;
  else if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
    velocity.y = speed;
  else
    velocity.y = 0.0f;
}

glm::mat4 Camera::GetViewMatrix() {
  // to create a correct model view, we need to move the world in opposite
  // direction to the camera
  //  so we will create the camera model matrix and invert
  glm::mat4 camera_translation = glm::translate(glm::mat4(1.f), position);
  glm::mat4 camera_rotation = GetRotationMatrix();
  return glm::inverse(camera_translation * camera_rotation);
}

glm::mat4 Camera::GetRotationMatrix() {
  // fairly typical FPS style camera. we join the pitch and yaw rotations into
  // the final rotation matrix

  glm::quat pitch_rotation = glm::angleAxis(pitch, glm::vec3{1.f, 0.f, 0.f});
  glm::quat yaw_rotation = glm::angleAxis(yaw, glm::vec3{0.f, -1.f, 0.f});

  return glm::toMat4(yaw_rotation) * glm::toMat4(pitch_rotation);
}
