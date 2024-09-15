# Add source to this project's executable.
add_executable (engine
  core/application.cpp
  core/application.h
  core/camera.h
  core/camera.cpp
  vulkan/vk_renderer.h
  vulkan/vk_renderer.cpp
  vulkan/vk_types.h
  vulkan/vk_utils.h
  vulkan/vk_utils.cpp
  vulkan/vk_descriptors.h
  vulkan/vk_descriptors.cpp
  vulkan/vk_asset_loader.h
  vulkan/vk_asset_loader.cpp
  main.cpp
)

set_property(TARGET engine PROPERTY CXX_STANDARD 20)
target_include_directories(engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(engine PUBLIC vma glm Vulkan::Vulkan fmt::fmt stb_image SDL2::SDL2 vkbootstrap imgui fastgltf::fastgltf tinyobj)
target_precompile_headers(engine PUBLIC <optional> <vector> <memory> <string> <vector> <unordered_map> <glm/mat4x4.hpp>  <glm/vec4.hpp> <vulkan/vulkan.h>)

add_custom_command(TARGET engine POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:engine> $<TARGET_FILE_DIR:engine>
  COMMAND_EXPAND_LISTS
  )
