#include "widget.h"
#include "Backend/context.h"
#include "Backend/util.h"
#include "GLFW/glfw3.h"
#include "Loaders/model.h"
#include "camera.h"

void Widget::Create(VulkanContext &vulkan_context, glm::mat4 matrix,
                    std::string model) {
  std::thread([&, model]() {
    std::vector<MeshData> gltf_data = LoadModel(model);
    MeshData mesh_data = gltf_data[0];

    for (auto &vertex : mesh_data.vertices) {
      vertex.position = glm::vec3(matrix * glm::vec4(vertex.position, 1.0f));
    }

    mesh_data.aabb_bounds.first =
        glm::vec3(matrix * glm::vec4(mesh_data.aabb_bounds.first, 1.0f));
    mesh_data.aabb_bounds.second =
        glm::vec3(matrix * glm::vec4(mesh_data.aabb_bounds.second, 1.0f));

    CreateBufferDataAsync(vulkan_context, mesh_data.indices.data(),
                          sizeof(uint32_t) * mesh_data.indices.size(),
                          VK_BUFFER_USAGE_INDEX_BUFFER_BIT, index_buffer);

    CreateBufferDataAsync(vulkan_context, mesh_data.vertices.data(),
                          sizeof(Vertex) * mesh_data.vertices.size(),
                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                              VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                          vertex_buffer);

    bound_min = mesh_data.aabb_bounds.first;
    bound_max = mesh_data.aabb_bounds.second;
    this->matrix = matrix;

    vertex_address = GetDeviceAddress(vulkan_context, vertex_buffer.buffer);
  }).detach();
}

void Widget::Update(GLFWwindow *window, Camera &camera) {
  const glm::mat4 view_proj =
      camera.buffer_data.projection_matrix * camera.buffer_data.view_matrix;

  auto Project = [view_proj](const glm::vec4 &point) {
    glm::vec4 clip = view_proj * point;
    return glm::vec2(clip) / clip.w * 0.5f + 0.5f;
  };

  glm::vec2 projected_bounds_min = Project(matrix * glm::vec4(bound_min, 1.0f));
  glm::vec2 projected_bounds_max = Project(matrix * glm::vec4(bound_max, 1.0f));
  glm::vec2 projected_position =
      Project(matrix * glm::vec4(glm::vec3(0.0f), 1.0f));

  glm::vec2 pmin = glm::min(projected_bounds_min, projected_bounds_max);
  glm::vec2 pmax = glm::max(projected_bounds_min, projected_bounds_max);

  glm::dvec2 mouse_pos;
  glfwGetCursorPos(window, &mouse_pos.x, &mouse_pos.y);

  glm::ivec2 window_size;
  glfwGetWindowSize(window, &window_size.x, &window_size.y);

  glm::vec2 projected_mouse_pos =
      static_cast<glm::vec2>(mouse_pos) / static_cast<glm::vec2>(window_size);

  if (glm::all(glm::lessThan(projected_mouse_pos, pmax)) &&
      glm::all(glm::greaterThan(projected_mouse_pos, pmin)) &&
      glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)) {
    selected = true;
  } else if (!glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)) {
    selected = false;
  }

  if (selected) {
    drag_position = projected_mouse_pos;
  }
}

void Widget::Destroy(VulkanContext &vulkan_context) {
  DestroyBuffer(vulkan_context, index_buffer);
  DestroyBuffer(vulkan_context, vertex_buffer);
}
