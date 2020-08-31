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
		client(SteamNetworking::makeObj<
			SteamNetworkingClient>(iOContext))
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

		client->setServerAddress(serverAddress);
		//to do run on seperate thread or something
		client->start();

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

	struct ImGUIWrapper {
		ImGUIWrapper() {
			ImGui::CreateContext();
			ImGuiIO& io = ImGui::GetIO();
			io.KeyMap[ImGuiKey_Tab] = SDL_SCANCODE_TAB;
			io.KeyMap[ImGuiKey_LeftArrow] = SDL_SCANCODE_LEFT;
			io.KeyMap[ImGuiKey_RightArrow] = SDL_SCANCODE_RIGHT;
			io.KeyMap[ImGuiKey_UpArrow] = SDL_SCANCODE_UP;
			io.KeyMap[ImGuiKey_DownArrow] = SDL_SCANCODE_DOWN;
			io.KeyMap[ImGuiKey_PageUp] = SDL_SCANCODE_PAGEUP;
			io.KeyMap[ImGuiKey_PageDown] = SDL_SCANCODE_PAGEDOWN;
			io.KeyMap[ImGuiKey_Home] = SDL_SCANCODE_HOME;
			io.KeyMap[ImGuiKey_End] = SDL_SCANCODE_END;
			io.KeyMap[ImGuiKey_Insert] = SDL_SCANCODE_INSERT;
			io.KeyMap[ImGuiKey_Delete] = SDL_SCANCODE_DELETE;
			io.KeyMap[ImGuiKey_Backspace] = SDL_SCANCODE_BACKSPACE;
			io.KeyMap[ImGuiKey_Space] = SDL_SCANCODE_SPACE;
			io.KeyMap[ImGuiKey_Enter] = SDL_SCANCODE_RETURN;
			io.KeyMap[ImGuiKey_Escape] = SDL_SCANCODE_ESCAPE;
			io.KeyMap[ImGuiKey_A] = SDL_SCANCODE_A;
			io.KeyMap[ImGuiKey_C] = SDL_SCANCODE_C;
			io.KeyMap[ImGuiKey_V] = SDL_SCANCODE_V;
			io.KeyMap[ImGuiKey_X] = SDL_SCANCODE_X;
			io.KeyMap[ImGuiKey_Y] = SDL_SCANCODE_Y;
			io.KeyMap[ImGuiKey_Z] = SDL_SCANCODE_Z;
			io.SetClipboardTextFn = [](void*, const char* text) {
				SDL_SetClipboardText(text);
			};
			io.GetClipboardTextFn = [](void*) -> const char* {
				return SDL_GetClipboardText();
			};
			io.ClipboardUserData = nullptr;
		}
		~ImGUIWrapper() {
			ImGui::DestroyContext();
		}	
	};

	asio::io_context iOContext;
	Window window;
	ImGUIWrapper ImGUIRAII;
	Renderer renderer = { window.window };
	double newTime;
	double oldTime;
	double timePassed = 0;
	asio::steady_timer tickTimer;
	std::shared_ptr<SteamNetworkingClient> client;
	GameState::InputType input;
	ScriptRuntime js; //to do move this to the game client maybe

	enum class KeyBitNum : int8_t {
		right,
		left,
		up,
		down,
		slot0,
		slot1,
		slot2,
		slot3,
		slot4,
		slot5,
		slot6,
		slot7,
		modifier,
		end,

		slotStart = slot0,
		slotEnd = slotStart + TheWarrenState::Player::InventorySlots::HotBarSize
	};

	static constexpr int getBitMask(KeyBitNum bitNum) {
		return 1 << static_cast<int>(bitNum);
	};

	//to do rename to buttons or something
	enum class Movement : int {
		right = 1 << 0,
		left  = 1 << 1,
		up    = 1 << 2,
		down  = 1 << 3
	};

	struct MovementKey {
		const char * name;
		SDL_Keycode key;
		KeyBitNum move;
		std::function<void(GameState::InputType&)> callback;
	};

	bool useModifer = false;
	//to do move this to game client code instead of just being in client
	void setHand(GameState::InputType& input, int slot) {
		TheWarrenState::Player::setHandSlot(
			useModifer ? TheWarrenState::Player::NonDominantHand : TheWarrenState::Player::DominantHand,
			input, slot);
	}

	MovementKey movementsKeys[static_cast<int>(KeyBitNum::end)] = {
		{"useModifer", SDLK_LSHIFT, KeyBitNum::modifier, [&](GameState::InputType&) { useModifer = true; }},
		{"moveRight", SDLK_d, KeyBitNum::right, [](GameState::InputType& input) { input.movement[Axis::X] += 1.0; }},
		{"moveLeft" , SDLK_a, KeyBitNum::left , [](GameState::InputType& input) { input.movement[Axis::X] -= 1.0; }},
		{"moveUp"   , SDLK_w, KeyBitNum::up   , [](GameState::InputType& input) { input.movement[Axis::Y] += 1.0; }},
		{"moveDown" , SDLK_s, KeyBitNum::down , [](GameState::InputType& input) { input.movement[Axis::Y] -= 1.0; }},
		//to do add a auto fill these slots
		{"useSlot0" , SDLK_1, KeyBitNum::slot0, [&](GameState::InputType& input) { setHand(input, 0); }},
		{"useSlot1" , SDLK_2, KeyBitNum::slot1, [&](GameState::InputType& input) { setHand(input, 1); }},
		{"useSlot2" , SDLK_3, KeyBitNum::slot2, [&](GameState::InputType& input) { setHand(input, 2); }},
		{"useSlot3" , SDLK_4, KeyBitNum::slot3, [&](GameState::InputType& input) { setHand(input, 3); }},
		{"useSlot4" , SDLK_5, KeyBitNum::slot4, [&](GameState::InputType& input) { setHand(input, 4); }},
		{"useSlot5" , SDLK_6, KeyBitNum::slot5, [&](GameState::InputType& input) { setHand(input, 5); }},
		{"useSlot6" , SDLK_7, KeyBitNum::slot6, [&](GameState::InputType& input) { setHand(input, 6); }},
		{"useSlot7" , SDLK_8, KeyBitNum::slot7, [&](GameState::InputType& input) { setHand(input, 7); }},
	};

	void tick() {
		oldTime = newTime;
		newTime = time();
		timePassed = newTime - oldTime;

		//we need this to tell the server who we are
		input.author = client->gameClient.getID();
		//reset movement back to 0 to avoid some bugs
		float emptyMovement[2] = { 0 };
		memcpy(&input.movement, &emptyMovement, sizeof(input.movement));

		static int currentKeys;
		useModifer = false;

		//to do seperate ImGUI io from Game io
		//to do seperate engine io from game io
		ImGuiIO& io = ImGui::GetIO();
		bool mousePressed[3] = { false };
		const auto imguiHandleKey = [](ImGuiIO& io, SDL_Event& event) {
			int key = event.key.keysym.scancode;
			IM_ASSERT(key >= 0 && key < IM_ARRAYSIZE(io.KeysDown));
			io.KeysDown[key] = (event.type == SDL_KEYDOWN);
			io.KeyShift = ((SDL_GetModState() & KMOD_SHIFT) != 0);
			io.KeyAlt = ((SDL_GetModState() & KMOD_ALT) != 0);
			io.KeyCtrl = ((SDL_GetModState() & KMOD_CTRL) != 0);
			io.KeySuper = ((SDL_GetModState() & KMOD_GUI) != 0);
		};

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
						currentKeys |= getBitMask(movementKey.move);
					}
				}
				//ImGui
				imguiHandleKey(io, event);
				break;
			case SDL_KEYUP:
				for (auto movementKey : movementsKeys) {
					if (event.key.keysym.sym == movementKey.key) {
						currentKeys &= ~getBitMask(movementKey.move);
					}
				}
				//ImGui
				imguiHandleKey(io, event);
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

				//ImGui
				io.MousePos = ImVec2(
					static_cast<float>(sdlMouseCords[Axis::X]),
					static_cast<float>(sdlMouseCords[Axis::Y]));
			} break;
			case SDL_MOUSEBUTTONDOWN:
				if (event.button.button == SDL_BUTTON_LEFT)
					input.actionFlags = 1;
				
				//ImGui
				if (event.button.button == SDL_BUTTON_LEFT) mousePressed[0] = true;
				if (event.button.button == SDL_BUTTON_RIGHT) mousePressed[1] = true;
				if (event.button.button == SDL_BUTTON_MIDDLE) mousePressed[2] = true;

			break;
			case SDL_MOUSEBUTTONUP:
				if (event.button.button == SDL_BUTTON_LEFT)
					input.actionFlags = 0;
			break;
			case SDL_MOUSEWHEEL: {
				//ImGui
				if (0 < event.wheel.x) io.MouseWheelH += 1;
				if (event.wheel.x < 0) io.MouseWheelH -= 1;
				if (0 < event.wheel.y) io.MouseWheel += 1;
				if (event.wheel.y < 0) io.MouseWheel -= 1;
				break;
			}
			case SDL_TEXTINPUT: {
				io.AddInputCharactersUTF8(event.text.text);
				break;
			}
			default:
				break;
			}
		}

		//ImGui credit to google
		io.DeltaTime = timePassed;
		int mx, my;
		Uint32 buttons = SDL_GetMouseState(&mx, &my);
		io.MouseDown[0] = mousePressed[0] || (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
		io.MouseDown[1] = mousePressed[1] || (buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
		io.MouseDown[2] = mousePressed[2] || (buttons & SDL_BUTTON(SDL_BUTTON_MIDDLE)) != 0;
		mousePressed[0] = mousePressed[1] = mousePressed[2] = false;

		for (auto movementKey : movementsKeys) {
			if (currentKeys &
				getBitMask(movementKey.move)
			) {
				movementKey.callback(input);
			}
		}

		client->gameClient.onInput(input);
		//to do make it only send input once every tick
		PacketHeader header;
		header.opCode = PacketHeader::PLAYER_INPUT;
		header.acknowledgedTick = client->gameClient.getAckTick();
		header.tick = client->gameClient.getCurrentState().tick;
		header.timestamp = static_cast<int>(
			std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count());
		client->send<GameState::InputType>({ header, input });

		//update game
		client->update(timePassed);

		constexpr int64_t targetTickTime = 1000000000 / 240; //1 second / 60
		tickTimer = asio::steady_timer{ iOContext };
		tickTimer.expires_after(std::chrono::nanoseconds(targetTickTime));
		tickTimer.async_wait([&](const asio::error_code& error) {
			if (!error) tick();
			else return;
		});

		renderer.draw(client->gameClient, client->gameClient.getCurrentState(), timePassed);
	}
};

int main(int argc, char** argv) {
	GameApp game;
	game.run();
	return 0;
}