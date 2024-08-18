#include <cstdint>
#include <fmt/core.h>
#include <vector>
#include <vulkan/vk_enum_string_helper.h>

namespace Engine {
const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const std::vector<const char*> c_validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
const bool c_enableValidationLayers = false;
#else
const bool c_enableValidationLayers = true;
#endif

#define VK_CHECK(x)                                                     \
do {                                                                \
	VkResult err = x;                                               \
	if (err) {                                                      \
		fmt::println(fmt::runtime("Detected Vulkan error: {}"), string_VkResult(err)); \
		abort();                                                    \
	}                                                               \
} while (0)

}
