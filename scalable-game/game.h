#pragma once
#include "core.h"
#include "box2d/box2d.h"
#include "javascript.h"

struct Inputs {
	const static int8_t maxPlayerCount = 0b100;
	//each one is 32 btyes large or less
	struct Command {
		Snowflake::RawSnowflake author = { 0 };
		float movement[2] = { 0.0f };
		float rotation[2] = { 0.0f };
		uint8_t actionFlags = 0;
	};

	//4 * 32 btyes is 128 btyes
	Command userInput[maxPlayerCount] = {};

	constexpr Command& operator[](std::size_t pos) {
		return userInput[pos];
	}

	constexpr const Command& operator[](std::size_t pos) const {
		return userInput[pos];
	}

	constexpr Command* begin() noexcept {
		return userInput;
	}

	constexpr const Command* begin() const noexcept {
		return userInput;
	}

	constexpr Command* end() noexcept {
		return begin() + maxPlayerCount;
	}

	constexpr const Command* end() const noexcept {
		return begin() + maxPlayerCount;
	}
};

class Player {
public:
	int index = 0;

	//random
	//server should set the first random num to something from mt19937 
	uint64_t lastRandomNum = 0;
	uint32_t generateRandomNum(const int tick) {
		//another random prime
		lastRandomNum += (tick + index) * 2301121489U;
		lastRandomNum *= 11313288804317771849U;
		return static_cast<uint32_t>(lastRandomNum);
	}
};

class GlobalData {
public:
	v8::Global<v8::Function> updateCallback;

	static void setUpdate(const v8::FunctionCallbackInfo<v8::Value>& args) {
		v8::Isolate* isolate = args.GetIsolate();
		ScriptRuntime& runtime = *reinterpret_cast<ScriptRuntime*>(isolate->GetData(0));
		GlobalData& global = *reinterpret_cast<GlobalData*>(
			args.Data().As<v8::Object>()->GetInternalField(0).As<v8::External>()->Value());
		if (args.Length() == 0) {
			isolate->ThrowException(
				v8::String::NewFromUtf8Literal(args.GetIsolate(), "Bad parameters"));
			return;
		}
		v8::Local<v8::Function> callback = args[0].As<v8::Function>();
		global.updateCallback = v8::Global<v8::Function>(runtime.engine.isolate, callback);
	}
};

//server and client side code
// We currently don't support networking between computers with different architectures,
	// things like different type sizes or endians will cause game state to be unreadable.
	// It's simpiler to do this for prototyping reasons, but it's definally something to fix
class GenericState {
public:
	GenericState() = default;
	using InputType = Inputs::Command;
	using Inputs = ::Inputs;
	using GlobalState = GlobalData;

	Inputs inputs;

	using Player = ::Player;
	Player players[Inputs::maxPlayerCount] = {};

	template<class Core>
	void start(Core& core) {
		{
			int i = 0;
			for (auto& player : players) {
				player.index = i;
				//getting 0xFFFFFFFF causes overflow issues
				player.lastRandomNum = core.template genRandInt<int>(0, 0xFFFFFFF0);
				i += 1;
			}
		}

		// start up the JS API
		ScriptRuntime& js = core.scriptRuntime;
		v8::Local<v8::ObjectTemplate> gameGlobalTemp = v8::ObjectTemplate::New(js.isolate);
		gameGlobalTemp->SetInternalFieldCount(1);
		auto gameGlobalObj = gameGlobalTemp->NewInstance(js.context).ToLocalChecked();
		gameGlobalObj->SetInternalField(0, v8::External::New(js.isolate, &core.gameData));
		js.globalTemplate->Set(js.isolate, "setUpdate",
			v8::FunctionTemplate::New(js.isolate, GlobalData::setUpdate, gameGlobalObj));

		// start the script
		js.startNewContext();
		std::string path = "./main.js";
		js.executeFile(path);

	}

	void update(const CoreInfo& coreInfo, double deltaTime);

	double time = 0;
	int tick = 0;
};

void GenericState::update(const CoreInfo& coreInfo, double deltaTime) {
	GenericState::GlobalState& globals = coreInfo.getGlobalGameData<GenericState>();
	ScriptRuntime& js = coreInfo.scriptRuntime;
	v8::HandleScope handleScope(js.isolate);
	v8::Local<v8::Function> callback = globals.updateCallback.Get(js.isolate);
	v8::TryCatch tryCatch(js.isolate);
	v8::Context::Scope contextScope(js.context);
	callback->Call(js.context, js.context->Global(), 0, nullptr);
}

using GameState = GenericState;
using GameServer = Core<GameState, true>;
using GameClient = Core<GameState, false>;