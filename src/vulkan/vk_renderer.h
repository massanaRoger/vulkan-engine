#include "SDL_video.h"
#include <vulkan/vulkan_core.h>

namespace Engine {
	namespace Vulkan {
		class Renderer {
		public:
			void init_vulkan(SDL_Window* window);
		private:
			VkInstance m_instance;
			void create_instance(SDL_Window* window);
		};
	}
}
