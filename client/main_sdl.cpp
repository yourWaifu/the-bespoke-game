#include <iostream>
#include <core.h>

#include <SDL.h>
#include <SDL_syswm.h>
#include "asio.hpp"
#include "game.h"
#include "renderer_sdl.h"
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

		SDL_Window* window = nullptr;
	};
	asio::io_context iOContext;
	Window window;
	Renderer renderer = { window.window };
	double newTime;
	double oldTime;
	double timePassed = 0;
	asio::steady_timer tickTimer;
	SteamNetworkingClient client;

	void tick() {
		oldTime = newTime;
		newTime = time();
		timePassed = newTime - oldTime;

		GameState::InputType input;
		//we need this to tell the server who we are
		input.author = client.gameClient.getID();

		enum class Movement : int {
			right = 1 << 0,
			left  = 1 << 1,
			up    = 1 << 2,
			down  = 1 << 3,
		};

		struct MovementKey {
			SDL_Keycode key;
			Movement move;
			std::function<void(GameState::InputType&)> callback;
		};

		MovementKey movementsKeys[] = {
			{SDLK_f, Movement::right, [](GameState::InputType& input) { input.movement[Axis::X] += 1.0; }},
			{SDLK_s, Movement::left , [](GameState::InputType& input) { input.movement[Axis::X] -= 1.0; }},
			{SDLK_e, Movement::up   , [](GameState::InputType& input) { input.movement[Axis::Y] += 1.0; }},
			{SDLK_d, Movement::down , [](GameState::InputType& input) { input.movement[Axis::Y] -= 1.0; }},
		};
		static Movement currentMovement;

		constexpr int maxNumOfEvents = 16;
		SDL_Event event;
		for (int eventNum = 0; eventNum < maxNumOfEvents && SDL_PollEvent(&event) != 0; ++eventNum) {
			switch (event.type) {
			case SDL_QUIT:
				
				return;
			case SDL_KEYDOWN:
				//to do move this to game state and fix dup code
				for (auto movementKey : movementsKeys) {
					if (event.key.keysym.sym == movementKey.key) {
						currentMovement = static_cast<Movement>(
							static_cast<int>(currentMovement)
							| static_cast<int>(movementKey.move));
					}
				}
				break;
			case SDL_KEYUP:
				for (auto movementKey : movementsKeys) {
					if (event.key.keysym.sym == movementKey.key) {
						currentMovement = static_cast<Movement>(
							static_cast<int>(currentMovement)
							& ~static_cast<int>(movementKey.move));
					}
				}
				break;
			default:
				break;
			}
		}

		for (auto movementKey : movementsKeys) {
			if (static_cast<int>(currentMovement) &
				static_cast<int>(movementKey.move)
			) {
				movementKey.callback(input);
			}
		}

		client.gameClient.onInput(input);
		//to do make it only send input once every tick
		client.send<GameState::InputType>({
			PacketHeader::PLAYER_INPUT, input });

		//update game
		client.update(timePassed);

		constexpr int64_t targetTickTime = 1000000000 / 240; //1 second / 60
		tickTimer = asio::steady_timer{ iOContext };
		tickTimer.expires_after(std::chrono::nanoseconds(targetTickTime));
		tickTimer.async_wait([&](const asio::error_code& error) {
			if (!error) tick();
			else return;
		});

		renderer.draw(client.gameClient.getCurrentState(), timePassed);
	}
};

int main(int argc, char** argv) {
	GameApp game;
	game.run();
	return 0;
}