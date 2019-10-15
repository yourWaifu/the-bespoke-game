#pragma once
#include <unordered_map>
#include <list>
#include <string>
#include <functional>
#include <nonstd/string_view.hpp>
#include <vector>
#include <set>

namespace sys {
	enum EventType {
		None,
		Key,            //value is a key code, and value2 is the down flag (controller buttons go here)
		Char,           //value is an Unicode char
		Mouse,          //value and value2 are relative signed x/y moves
		Mouse_Absolute, //value and value2 are absolute coordinates in the window
		Mouse_Leave,    //values are meaningless, the mouse has left the window
		Joystick_RightStick, //value and value2 are the x/y axis
		Joystick_LeftStick,
		Trigger         //value is the trigger code, value2 is the axis
	};

	enum KeyCode {      //I copied this from sdl sorry
		K_UNKNOWN = 0,

		K_RETURN = '\r',
		K_ESCAPE = '\033',
		K_BACKSPACE = '\b',
		K_TAB = '\t',
		K_SPACE = ' ',
		K_EXCLAIM = '!',
		K_QUOTEDBL = '"',
		K_HASH = '#',
		K_PERCENT = '%',
		K_DOLLAR = '$',
		K_AMPERSAND = '&',
		K_QUOTE = '\'',
		K_LEFTPAREN = '(',
		K_RIGHTPAREN = ')',
		K_ASTERISK = '*',
		K_PLUS = '+',
		K_COMMA = ',',
		K_MINUS = '-',
		K_PERIOD = '.',
		K_SLASH = '/',
		K_0 = '0',
		K_1 = '1',
		K_2 = '2',
		K_3 = '3',
		K_4 = '4',
		K_5 = '5',
		K_6 = '6',
		K_7 = '7',
		K_8 = '8',
		K_9 = '9',
		K_COLON = ':',
		K_SEMICOLON = ';',
		K_LESS = '<',
		K_EQUALS = '=',
		K_GREATER = '>',
		K_QUESTION = '?',
		K_AT = '@',
		/*
		Skip uppercase letters
		*/
		K_LEFTBRACKET = '[',
		K_BACKSLASH = '\\',
		K_RIGHTBRACKET = ']',
		K_CARET = '^',
		K_UNDERSCORE = '_',
		K_BACKQUOTE = '`',
		K_a = 'a',
		K_b = 'b',
		K_c = 'c',
		K_d = 'd',
		K_e = 'e',
		K_f = 'f',
		K_g = 'g',
		K_h = 'h',
		K_i = 'i',
		K_j = 'j',
		K_k = 'k',
		K_l = 'l',
		K_m = 'm',
		K_n = 'n',
		K_o = 'o',
		K_p = 'p',
		K_q = 'q',
		K_r = 'r',
		K_s = 's',
		K_t = 't',
		K_u = 'u',
		K_v = 'v',
		K_w = 'w',
		K_x = 'x',
		K_y = 'y',
		K_z = 'z',
	};

	enum ButtonState : bool {
		UP = false,
		DOWN = true
	};

	struct Event {
		EventType type;
		int value;
		int value2;
		int deviceNumber;   //in the case that there are more then one device

		bool isKeyEvent() const { return type == Key; }
		bool IsMouseEvent() const { return type == Mouse; }
	};
}

struct InputAxisBinding {
	std::string name;
	bool isBound = true;
	std::function<void(float)> function;

	InputAxisBinding(const std::string p_name, std::function<void(float)> p_function)
		: name(p_name)
		, function(p_function)
	{};
};

struct InputActionBinding {
	std::string name;
	bool isBound = true;
	std::function<void()> function;

	inline void operator()() { function(); }

	InputActionBinding(const std::string p_name, std::function<void()> p_funciton)
		: name(p_name)
		, function(p_funciton) {};
};

struct InputAction {
	sys::KeyCode key;
	sys::ButtonState pressed;

	inline bool operator==(const InputAction& right) const noexcept {
		return key == right.key && pressed == right.pressed;
	}
};

struct InputActionHash {
	std::size_t operator()(const InputAction& action) const noexcept {
		return std::hash<nonstd::string_view>{}(
			nonstd::string_view(
				reinterpret_cast<const char*>(&action), sizeof(InputAction)
			)
		);
	}
};

using CommandIndex = uint8_t;
struct Action {
	Action(CommandIndex _command, float _param) :
		command(_command), param(_param) {}
	Action() = default;
	CommandIndex command = 0;
	float param = 0.0;
};

struct PlayerInput {
public:
	unsigned int playerID = 0;
	std::vector<Action> actions;
	constexpr static uint8_t maxNumOfActions = 8;
	PlayerInput() {
		actions.reserve(maxNumOfActions);
	}
	void addAction(CommandIndex command, float value) {
		if (maxNumOfActions <= actions.size())
			return;
		actions.emplace_back(command, value);
	}
	void clearActionBuffer() {
		actions.clear();
	}
};

class InputComponent {
public:
	using BoundActionsMap = std::unordered_map<InputAction, InputActionBinding*, InputActionHash>;

	InputComponent() {
		int index = 0;
		for (const std::string& command : commands) {
			commandIndices.insert({ command, index });
			++index;
		}
		boundCommands.resize(index);
	}

	//InputAxisBinding& BindAxis(const std::string p_name, std::function<void(float)> p_funciton) {
	//	axisBindings.push_back(InputAxisBinding(p_name, p_funciton));
	//	return axisBindings.back();
	//}

	struct ActionFunctionPair {
		InputActionBinding& down;
		InputActionBinding& up;
	};

	template<class DownFunction, class UpFunction>
	ActionFunctionPair bindAction(const std::string name, DownFunction downFunction, UpFunction upFunction) {
		ActionFunctionPair bindings{ bindAction(name, downFunction), bindAction(name, upFunction) };
		
		auto foundCommandIndex = commandIndices.find(name);
		if (foundCommandIndex == commandIndices.end())
			return bindings;

		boundCommands[foundCommandIndex->second] = [bindings](float value) {
			sys::ButtonState isPressed = static_cast<sys::ButtonState>(static_cast<int>(value));
			if (isPressed == sys::DOWN) bindings.down();
			else bindings.up();
		};
		return bindings;
	}

	//inline std::list<InputAxisBinding>& getAxisBindings() {
	//	return axisBindings;
	//}

	inline BoundActionsMap& getBoundActions() {
		return boundActions;
	}

	inline void bind(ActionFunctionPair& action, sys::KeyCode key) {
		bind(action.up  , { key, sys::UP   });
		bind(action.down, { key, sys::DOWN });
	}

	void processInput(InputAction action, PlayerInput& inputsToSend);
	void processInput(PlayerInput& actions);
private:

	//to do move this into game code
	const std::set<std::string> commands = {
		"move left",
		"move right",
		"move up",
		"move down"
	};

	//std::unordered_map<InputAxisBinding> axisBindings;
	std::list<InputActionBinding> actionBindings;
	BoundActionsMap boundActions;
	std::unordered_map<sys::KeyCode, sys::ButtonState> keyStates;
	std::unordered_map<std::string, CommandIndex> commandIndices;
	std::vector<std::function<void(float)>> boundCommands;

	template<class Function>
	inline InputActionBinding& bindAction(const std::string p_name, Function function) {
		actionBindings.push_back(InputActionBinding(p_name, static_cast<std::function<void()>>(function)));
		return actionBindings.back();
	}

	inline void bind(InputActionBinding& action, InputAction input) {
		boundActions[input] = &action;
		keyStates[input.key] = sys::UP;
	}
};