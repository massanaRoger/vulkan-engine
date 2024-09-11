#include "vk_utils.h"
#include "SDL_video.h"
#include "SDL_vulkan.h"
#include "fmt/core.h"
#include "glm/fwd.hpp"
#include "vk_types.h"
#include "fastgltf/util.hpp"
#include "vulkan/vk_renderer.h"
#include <cstddef>
#include <fstream>
#include <ios>
#include <iostream>
#include <ostream>
#include <vulkan/vulkan_core.h>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

namespace Engine {

VkResult create_debug_utils_messenger_EXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	} else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void destroy_debug_utils_messenger_EXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}

std::vector<const char*> get_required_extensions(SDL_Window* window) {
	uint32_t sdlExtensionCount = 0;
	if (SDL_Vulkan_GetInstanceExtensions(window, &sdlExtensionCount, nullptr) == SDL_FALSE) {
		std::cerr << "Failed to get the number of extensions." << std::endl;
		abort();
	}

	std::vector<const char*> sdlExtensions(sdlExtensionCount);
	if(SDL_Vulkan_GetInstanceExtensions(window, &sdlExtensionCount, sdlExtensions.data()) == SDL_FALSE) {
		std::cerr << "Failed to get the extensions \n";
		abort();
	}

	std::vector<const char*> extensions(sdlExtensions.data(), sdlExtensions.data() + sdlExtensionCount);

	if (c_enableValidationLayers) {
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	return extensions;
}

std::vector<char> read_file(const std::string& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		std::cerr << "Failed to open file" << std::endl;
		return std::vector<char>();
	}

	size_t fileSize = (size_t) file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();

	return buffer;
}

std::optional<std::vector<MeshAsset>> load_gltf_meshes(Renderer& renderer, std::filesystem::path filePath)
{
	std::cout << "Loading GLTF: " << filePath << std::endl;
	fastgltf::GltfDataBuffer data;
	data.loadFromFile(filePath);

	constexpr auto gltfOptions = fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;

	fastgltf::Asset gltf;
	fastgltf::Parser parser {};

	auto load = parser.loadBinaryGLTF(&data, filePath.parent_path(), gltfOptions);
	if (load) {
		gltf = std::move(load.get());
	} else {
		fmt::print(fmt::runtime("Failed to load gltf: {} \n"), fastgltf::to_underlying(load.error()));
		return{};
	}

	std::vector<MeshAsset> meshes;

	// use same vector for all meshes so there are less reallocations
	std::vector<uint32_t> indices;
	std::vector<Vertex> vertices;

	for (fastgltf::Mesh &mesh : gltf.meshes) {
		MeshAsset newMesh;

		newMesh.name = mesh.name;

		// Clear mesh arrays after each mesh
		for (auto &&p : mesh.primitives) {
			GeoSurface newSurface;
			newSurface.startIndex = (uint32_t)indices.size();
			newSurface.count = (uint32_t)gltf.accessors[p.indicesAccessor.value()].count;
			
			size_t initial_vtx = vertices.size();

			// Load indexes
			{
				fastgltf::Accessor &indexAccessor = gltf.accessors[p.indicesAccessor.value()];
				indices.reserve(indices.size() + indexAccessor.count);

				fastgltf::iterateAccessor<std::uint32_t>(gltf, indexAccessor, 
					     [&](std::uint32_t idx) {
					     indices.push_back(idx + initial_vtx);
					}
				);
			}

			// load vertex positions
			{
				fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->second];
				vertices.resize(vertices.size() + posAccessor.count);

				fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
					[&](glm::vec3 v, size_t index) {
						  Vertex newvtx;
						  newvtx.pos = v;
						  newvtx.normal = { 1, 0, 0 };
						  newvtx.color = glm::vec4 { 1.f };
						  newvtx.texCoord = glm::vec2(0.0f, 0.0f);
						  vertices[initial_vtx + index] = newvtx;
				});
			}

			// load vertex normals
			auto normals = p.findAttribute("NORMAL");
			if (normals != p.attributes.end()) {

				fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[(*normals).second],
						  [&](glm::vec3 v, size_t index) {
						  vertices[initial_vtx + index].normal = v;
						  });
			}

			// load UVs
			auto uv = p.findAttribute("TEXCOORD_0");
			if (uv != p.attributes.end()) {

				fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uv).second],
						  [&](glm::vec2 v, size_t index) {
						  vertices[initial_vtx + index].texCoord.x = v.x;
						  vertices[initial_vtx + index].texCoord.y = v.y;
				});
			}

			// load vertex colors
			auto colors = p.findAttribute("COLOR_0");
			if (colors != p.attributes.end()) {

				fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*colors).second],
						  [&](glm::vec4 v, size_t index) {
						  vertices[initial_vtx + index].color = v;
						  });
			}

			newMesh.surfaces.push_back(newSurface);
		}

		// Display vertex normals
		constexpr bool OverrideColors = true;
		if (OverrideColors) {
			for (Vertex &vtx : vertices) {
				vtx.color = glm::vec4(vtx.normal, 1.f);
			}
		}

		//newMesh.meshBuffers = renderer.upload_mesh(indices, vertices);
		renderer.upload_mesh(indices, vertices);
		meshes.emplace_back(newMesh);
	}

	return meshes;
}

VkShaderModule create_shader_module(VkDevice device, const std::vector<char>& code)
{

	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shaderModule;
	VK_CHECK(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule));

	return shaderModule;
}

}
