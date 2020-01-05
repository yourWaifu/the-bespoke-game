#include <iostream>
#include <core.h>

#include <SDL.h>
#include <SDL_syswm.h>
#include "asio.hpp"
#include "game.h"
#include "networking_client.h"

template<class StringType>
inline void assert_message(bool condition, StringType message) {
	if (!condition) {
		std::cerr << "Error : Assert : " << message << '\n';
		std::terminate();
	}
}

class GameApp {
public:
	GameApp() :
		newTime(time()),
		oldTime(newTime),
		tickTimer(iOContext),
		client(iOContext)
	{
		assert_message(SDL_Init(SDL_INIT_EVENTS) == 0, "SDL_Init Failure");
	}
	~GameApp() {
		SDL_Quit();
	}

	double time() {
		return static_cast<double>(SDL_GetPerformanceCounter()) / SDL_GetPerformanceFrequency();
	}

	void run() {
		client.setServerAddress("::1");
		//to do run on seperate thread or something
		client.start();

		//start ticking by doing the first tick
		tick();

		iOContext.run();
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
			SDL_SysWMinfo wmInfo;
			SDL_VERSION(&wmInfo.version);
			SDL_GetWindowWMInfo(window, &wmInfo);

#if defined(SDL_VIDEO_DRIVER_WINDOWS)
#define VID_DRIVER_INFO_WINDOW win.window
#elif defined(SDL_VIDEO_DRIVER_X11)
#define VID_DRIVER_INFO_WINDOW x11.window
#else
#error "No avaiable VID_DRIVER_INFO_WINDOW"
#endif

			return reinterpret_cast<void*>(wmInfo.info.VID_DRIVER_INFO_WINDOW);
		}
		unsigned int width = 1080;
		unsigned int height = 720;
	private:
		SDL_Window* window = nullptr;
	};
	asio::io_context iOContext;
	Window window;
	double newTime;
	double oldTime;
	double timePassed = 0;
	asio::steady_timer tickTimer;
	SteamNetworkingClient client;

	void tick() {
		oldTime = newTime;
		newTime = time();
		timePassed = newTime - oldTime;

		constexpr int maxNumOfEvents = 16;
		SDL_Event event;
		for (int eventNum = 0; eventNum < maxNumOfEvents && SDL_PollEvent(&event) != 0; ++eventNum) {
			switch (event.type) {
			case SDL_QUIT:
				
				return;
			case SDL_KEYDOWN: case SDL_KEYUP:
				client.send();
				break;
			default:
				break;
			}
		}
		
		constexpr int64_t targetTickTime = 1000000000 / 60; //1 second / 60
		tickTimer = asio::steady_timer{ iOContext };
		tickTimer.expires_after(std::chrono::nanoseconds(targetTickTime));
		tickTimer.async_wait([&](const asio::error_code& error) {
			if (!error) tick();
			else return;
		});
	}
};

int main(int argc, char** argv) {
	GameApp game;
	game.run();
	return 0;
}