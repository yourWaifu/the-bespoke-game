#include <iostream>
#include <codecvt>
#include <core.h>

#include "javascript.h"
#include <SDL.h>
#include <SDL_syswm.h>
#include <math.h>
#include "asio.hpp"
#include "game.h"
#include "renderer_sdl.h"
#include "networking_client.h"
#include "IO_file.h"

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
		//load in settings
		std::string serverAddress = "::1";
		{
			std::string optionsJSON;
			{
				File optionsFile("settings.json");
				const std::size_t optionsSize = optionsFile.getSize();
				if (optionsSize != static_cast<std::size_t>(-1)) {
					optionsJSON.resize(optionsSize);
					optionsFile.get<std::string::value_type>(&optionsJSON[0]);
				}
			}

			//to do copy settings from default to settings.json
			
			if (!optionsJSON.empty()) {
				v8::Isolate::Scope isolate_scope(js.isolate);
				v8::HandleScope handle_scope(js.isolate);
				v8::Local<v8::Context> context = v8::Context::New(js.isolate);
				v8::Context::Scope context_scope(context);
				{
					//I'm sure there's a faster way to do this, but this is only done once
					//so speed shouldn't matter that much for now.
					context->Global()
						->Set(context, v8::String::NewFromUtf8Literal(js.isolate, "options"),
							v8::String::NewFromUtf8(js.isolate, optionsJSON.c_str()).ToLocalChecked());
					const char sourceText[] =
						//read json with comments
						"JSON.parse(options.replace(/\\/\\*[\\s\\S]*?\\*\\/|\\/\\/.*/g,''))";
					v8::Local<v8::String> source =
						v8::String::NewFromUtf8Literal(js.isolate, sourceText);
					v8::Local<v8::Script> script =
						v8::Script::Compile(context, source).ToLocalChecked();
					v8::Local<v8::Object> result = script->Run(context).ToLocalChecked()
						.As<v8::Object>();
					
					v8::Local<v8::Value> ipVal = result->Get(context,
						v8::String::NewFromUtf8Literal(js.isolate, "ip")).ToLocalChecked();
					if (!ipVal->IsUndefined()) {
						v8::String::Utf8Value utf8(js.isolate, ipVal);
						serverAddress = *utf8;
					}

					v8::Local<v8::Object> controlsJSON = result->Get(context,
						v8::String::NewFromUtf8Literal(js.isolate, "controls")).ToLocalChecked()
						.As<v8::Object>();;
					if (!controlsJSON->IsUndefined()) {
						for (MovementKey& key : movementsKeys) {
							v8::Local<v8::Value> value = controlsJSON->Get(context,
								v8::String::NewFromUtf8(js.isolate, key.name).ToLocalChecked())
								.ToLocalChecked();
							if (!value->IsUndefined()) {
								v8::String::Utf8Value utf8(js.isolate, value);
								SDL_Keycode keycode = SDL_GetKeyFromName(*utf8);
								if (keycode != SDLK_UNKNOWN) {
									key.key = keycode;
								}
							}
						}
					}
				}
			}
		}

		client.setServerAddress(serverAddress);
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
	GameState::InputType input;
	ScriptRuntime js; //to do move this to the game client maybe

	enum class Movement : int {
		right = 1 << 0,
		left  = 1 << 1,
		up    = 1 << 2,
		down  = 1 << 3,
	};

	struct MovementKey {
		const char * name;
		SDL_Keycode key;
		Movement move;
		std::function<void(GameState::InputType&)> callback;
	};

	MovementKey movementsKeys[4] = {
		{"moveRight", SDLK_d, Movement::right, [](GameState::InputType& input) { input.movement[Axis::X] += 1.0; }},
		{"moveLeft" , SDLK_a, Movement::left , [](GameState::InputType& input) { input.movement[Axis::X] -= 1.0; }},
		{"moveUp"   , SDLK_w, Movement::up   , [](GameState::InputType& input) { input.movement[Axis::Y] += 1.0; }},
		{"moveDown" , SDLK_s, Movement::down , [](GameState::InputType& input) { input.movement[Axis::Y] -= 1.0; }},
	};

	void tick() {
		oldTime = newTime;
		newTime = time();
		timePassed = newTime - oldTime;

		//we need this to tell the server who we are
		input.author = client.gameClient.getID();
		//reset movement back to 0 to avoid some bugs
		float emptyMovement[2] = { 0 };
		memcpy(&input.movement, &emptyMovement, sizeof(input.movement));

		static Movement currentMovement;

		constexpr int maxNumOfEvents = 16;
		SDL_Event event;
		for (int eventNum = 0; eventNum < maxNumOfEvents && SDL_PollEvent(&event) != 0; ++eventNum) {
			switch (event.type) {
			case SDL_QUIT:
				std::cout << "quit\n";
				iOContext.stop();
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
			case SDL_MOUSEMOTION: {
				int sdlMouseCords[2] = { 0 };
				SDL_GetMouseState(&sdlMouseCords[0], &sdlMouseCords[1]);
				SDL_Rect viewport = renderer.getViewport();
				//mouse cords are cords relaive to the center of the screen
				//or where the player is. since they are always centered
				double mouseCords[2] = {
					static_cast<double>(sdlMouseCords[Axis::X]) - (viewport.w / 2.0),
					static_cast<double>(sdlMouseCords[Axis::Y]) * -1 + (viewport.h / 2.0),
				};
				//get the angle between the player and mouse
				input.rotation = std::atan2(mouseCords[Axis::X], mouseCords[Axis::Y]);
			} break;
			case SDL_MOUSEBUTTONDOWN:
				if (event.button.button == SDL_BUTTON_LEFT)
					input.actionFlags = 1;
			break;
			case SDL_MOUSEBUTTONUP:
				if (event.button.button == SDL_BUTTON_LEFT)
					input.actionFlags = 0;
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
		PacketHeader header;
		header.opCode = PacketHeader::PLAYER_INPUT;
		header.acknowledgedTick = client.gameClient.getAckTick();
		header.tick = client.gameClient.getCurrentState().tick;
		header.timestamp = static_cast<int>(
			std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count());
		client.send<GameState::InputType>({ header, input });

		//update game
		client.update(timePassed);

		constexpr int64_t targetTickTime = 1000000000 / 240; //1 second / 60
		tickTimer = asio::steady_timer{ iOContext };
		tickTimer.expires_after(std::chrono::nanoseconds(targetTickTime));
		tickTimer.async_wait([&](const asio::error_code& error) {
			if (!error) tick();
			else return;
		});

		renderer.draw(client.gameClient, client.gameClient.getCurrentState(), timePassed);
	}
};

int main(int argc, char** argv) {
	GameApp game;
	game.run();
	return 0;
}