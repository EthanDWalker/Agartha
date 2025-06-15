cmake_minimum_required(VERSION 3.20)

project(project VERSION 0.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 40626af88bd7df9a5fb80be7b25ac85b122d6c21
)
FetchContent_MakeAvailable(fmt)

FetchContent_Declare(
    vk-bootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
    GIT_TAG        b934b6c76cf7c3b7bef76c0ddd43a0996f8b9f92
)
FetchContent_MakeAvailable(vk-bootstrap)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
    fastgltf
    GIT_REPOSITORY https://github.com/spnda/fastgltf.git
    GIT_TAG d4734a7e55d8d25155e755d895e018e423845cd0
)
FetchContent_MakeAvailable(fastgltf)

find_package(Vulkan REQUIRED)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG f5befd2d29e66809cd1110a152e375a7f1981f06
)
FetchContent_MakeAvailable(imgui)

file(GLOB IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/*.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCES})

target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${Vulkan_INCLUDE_DIR}
)

target_link_libraries(imgui PUBLIC glfw ${Vulkan_LIBRARY})

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp)

add_executable(engine ${SOURCES})

target_include_directories(engine PUBLIC 
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/ext/
    ${Vulkan_INCLUDE_DIR}
    imgui
)

target_link_libraries(engine PRIVATE imgui ${Vulkan_LIBRARY} fastgltf::fastgltf glfw fmt::fmt vk-bootstrap::vk-bootstrap)

file(GLOB SHADER_SRC
    ${CMAKE_SOURCE_DIR}/assets/shaders/*.frag 
    ${CMAKE_SOURCE_DIR}/assets/shaders/*.vert 
    ${CMAKE_SOURCE_DIR}/assets/shaders/*.comp

    ${CMAKE_SOURCE_DIR}/assets/shaders/*.geom
)

set(COMPILED_SHADERS)

set(SHADER_DEPENDS ${CMAKE_SOURCE_DIR}/assets/shaders/*.glsl)

foreach(SHADER ${SHADER_SRC})
  get_filename_component(FILE_NAME ${SHADER} NAME)
  set(COMPILED_FILE ${CMAKE_SOURCE_DIR}/assets/shaders/${FILE_NAME}.spv)

  add_custom_command(
      OUTPUT ${COMPILED_FILE}
      COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${SHADER} -o ${COMPILED_FILE}
      DEPENDS ${SHADER} ${SHADER_DEPENDS}
      COMMENT "Compiling shader ${FILE_NAME}"
      VERBATIM
  )

  list(APPEND COMPILED_SHADERS ${COMPILED_FILE})
endforeach()

file(GLOB RT_SHADER_SRC
    ${CMAKE_SOURCE_DIR}/assets/shaders/*.rchit
    ${CMAKE_SOURCE_DIR}/assets/shaders/*.rmiss
    ${CMAKE_SOURCE_DIR}/assets/shaders/*.rgen
)

foreach(SHADER ${RT_SHADER_SRC})
  get_filename_component(FILE_NAME ${SHADER} NAME)
  set(COMPILED_FILE ${CMAKE_SOURCE_DIR}/assets/shaders/${FILE_NAME}.spv)

  add_custom_command(
      OUTPUT ${COMPILED_FILE}
      COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} ${SHADER} -o ${COMPILED_FILE} --target-env vulkan1.2
      DEPENDS ${SHADER} ${SHADER_DEPENDS}
      COMMENT "Compiling shader ${FILE_NAME}"
      VERBATIM
  )

  list(APPEND COMPILED_SHADERS ${COMPILED_FILE})
endforeach()


add_custom_target(compile_shaders ALL DEPENDS ${COMPILED_SHADERS})
