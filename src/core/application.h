#include "SDL_video.h"
namespace Engine {
	namespace Core {
		class Application {
		public:
			void run();
			void init();
		private:
			SDL_Window *m_window;
			VkExtent2D m_windowExtent;
		};
	}
}
