#include "camera.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include "Backend/immediate_submit.h"
#include "GLFW/glfw3.h"
#include <fmt/base.h>
#include <glm/glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/transform.hpp>

void Camera::Create(VulkanContext &context,
                    DescriptorBuilder &descriptor_builder) {
  CreateBuffer(context, sizeof(CameraBuffer),
               VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
               VMA_MEMORY_USAGE_GPU_ONLY, buffer);

  descriptor_builder.Reset();
  descriptor_builder.BindUniformBuffer(0, buffer.buffer);
  descriptor_builder.Build(context, VK_SHADER_STAGE_ALL, descriptor_set,
                           descriptor_layout);
};

void Camera::Destroy(VulkanContext &context) {
  DestroyBuffer(context, buffer);
  vkDestroyDescriptorSetLayout(context.device, descriptor_layout, nullptr);
}

void Camera::Update(VulkanContext &context, ImmediateSubmit &immediate_submit,
                    GLFWwindow *window, float delta_time) {
  static double last_x, last_y;
  double pos_x, pos_y;
  int window_width, window_height;

  glfwGetWindowSize(window, &window_width, &window_height);
  glfwGetCursorPos(window, &pos_x, &pos_y);

  pos_x -= window_width / 2.0f;
  pos_y -= window_height / 2.0f;

  const double sensitivity = 0.003f;
  if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT)) {
    double dx, dy;
    dx = pos_x - last_x;
    dy = last_y - pos_y;
    dx *= sensitivity;
    dy *= sensitivity;

    yaw += static_cast<float>(dx);
    pitch += static_cast<float>(dy);
  }

  last_x = pos_x;
  last_y = pos_y;

  if (pitch < -89.0f)
    pitch = -89.0f;
  else if (pitch > 89.0f)
    pitch = 89.0f;

  const float speed = 50.0f * delta_time;

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

  {
    const float z_near = 0.01f;
    const float z_far = 10000.0f;
    const float aspect_ratio = window_width / static_cast<float>(window_height);
    const float fov_y = glm::radians(70.0f);

    glm::quat pitch_rotation = glm::angleAxis(pitch, glm::vec3{1.f, 0.f, 0.f});
    glm::quat yaw_rotation = glm::angleAxis(yaw, glm::vec3{0.f, -1.f, 0.f});
    glm::mat4 rotation_matrix =
        glm::toMat4(yaw_rotation) * glm::toMat4(pitch_rotation);

    glm::mat4 camera_translation = glm::translate(glm::mat4(1.f), position);

    glm::mat4 view_matrix = glm::inverse(camera_translation * rotation_matrix);

    buffer_data.view_matrix = view_matrix;

    glm::mat4 projection = glm::perspective(fov_y, aspect_ratio, z_near, z_far);

    projection[1][1] *= -1;

    buffer_data.projection_matrix = projection;

    position += glm::vec3(rotation_matrix * glm::vec4(velocity, 0.0f));

    buffer_data.view_pos = position;

    glm::mat4 clip = projection * view_matrix;

    Frustum frustum{};
    frustum.left = glm::normalize(clip[3] + clip[0]);
    frustum.right = glm::normalize(clip[3] - clip[0]);
    frustum.bottom = glm::normalize(clip[3] + clip[1]);
    frustum.top = glm::normalize(clip[3] - clip[1]);
    frustum.near = glm::normalize(clip[3] + clip[2]);
    frustum.far = glm::normalize(clip[3] - clip[2]);

    buffer_data.frustum = frustum;

    buffer_data.inv_view = glm::inverse(view_matrix);
    buffer_data.inv_proj = glm::inverse(projection);

    buffer_data.near = z_near;
    buffer_data.far = z_far;

    UpdateBuffer(context, immediate_submit, &buffer_data, sizeof(CameraBuffer),
                 0, buffer);
  }
}
