#include <core.h>

#include <SDL.h>
#include <SDL_syswm.h>
#include "game.h"

extern void * getSystemWindow(SDL_Window*& window);

template<class StringType>
inline void assert_message(bool condition, StringType message) {
	if (!condition) {
		std::cerr << "Error : Assert : " << message << '\n';
		std::terminate();
	}
}

class GameApp {
public:
	GameApp() {
		assert_message(SDL_Init(SDL_INIT_EVENTS) == 0, "SDL_Init Failure");
	}
	~GameApp() {
		SDL_Quit();
	}

	double time() {
		return static_cast<double>(SDL_GetPerformanceCounter()) / SDL_GetPerformanceFrequency();
	}

	void run() {
		Window window;
		GameCoreSystem<GameApp> gameCore(&window.width, &window.height, *this, window.getWindow());
		GameClient game(gameCore);
		bool isRunning = true;
		double newTime = time();
		double timePassed;
		double oldTime = 0;

		while (isRunning) {
			oldTime = newTime;
			newTime = time();
			timePassed = newTime - oldTime;

			constexpr int maxNumOfEvents = 16;
			SDL_Event event;
			for (int eventNum = 0; eventNum < maxNumOfEvents && SDL_PollEvent(&event) != 0; ++eventNum) {
				switch(event.type) {
				case SDL_QUIT:
					isRunning = false;
					break;
				case SDL_KEYDOWN: case SDL_KEYUP:{
					gameCore.addEventToQueue(
						sys::Key,
						static_cast<int>(event.key.keysym.sym),
						event.type == SDL_KEYDOWN ? sys::DOWN : sys::UP,
						0);
					break;
				}
				default:
					break;
				}
			}

			gameCore.update(timePassed);
		}
	}

private:
	class Window {
	public:
		Window() {
			const int x = SDL_WINDOWPOS_CENTERED;
			const int y = SDL_WINDOWPOS_CENTERED;
			uint32_t windowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI;
			window = SDL_CreateWindow("Cool Game", x, y, width, height, windowFlags);
		}
		~Window() {
			SDL_DestroyWindow(window);
		}
		void * getWindow() {
			return ::getSystemWindow(window);
		}
		unsigned int width = 1080;
		unsigned int height = 720;
	private:
		SDL_Window* window = nullptr;
	};
};

int main(int argc, char** argv) {
	GameApp game;
	game.run();
}