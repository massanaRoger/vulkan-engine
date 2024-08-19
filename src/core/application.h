#include "SDL_video.h"
#include "vulkan/vk_renderer.h"

namespace Engine {
class Application {
	public:
	Application();
	void run();
	void init();
	private:
	SDL_Window *m_window;
	VkExtent2D m_windowExtent = { 1700 , 900 };
	Renderer m_renderer;
};
}
