#pragma once
#include "libplatform/libplatform.h"
#include "v8.h"
#include "asio.hpp"


struct ScriptEngine {
public:
	ScriptEngine() {
		platform = v8::platform::NewDefaultPlatform();
		v8::V8::InitializePlatform(platform.get());
		v8::V8::Initialize();

		create_params.array_buffer_allocator =
			v8::ArrayBuffer::Allocator::NewDefaultAllocator();
		isolate = v8::Isolate::New(create_params);
	}
	~ScriptEngine() {
		isolate->Dispose();
		v8::V8::Dispose();
		v8::V8::ShutdownPlatform();
		delete create_params.array_buffer_allocator;
	}

	std::unique_ptr<v8::Platform> platform;
	v8::Isolate::CreateParams create_params;
	v8::Isolate* isolate; //default isolate
};

struct ScriptRuntime {
	ScriptRuntime(asio::io_context& _ioContext): ioContext(_ioContext) {
		engine.isolate->SetData(0, this);
		//create global template
		globalTemplate->Set(engine.isolate, "setTimer", v8::FunctionTemplate::New(engine.isolate, setTimeout));
		globalTemplate->Set(engine.isolate, "clearTimer", v8::FunctionTemplate::New(engine.isolate, clearTimeout));
	}
	~ScriptRuntime() {
		//evaluation_context.Reset();
		stringify_function.Reset();
	}
	
	ScriptEngine engine{};
	std::unique_ptr<v8::Platform>& platform = engine.platform;
	v8::Isolate::CreateParams& create_params = engine.create_params;
	v8::Isolate*& isolate = engine.isolate;

	asio::io_context& ioContext;
	v8::Isolate::Scope isolate_scope{ isolate };
	v8::HandleScope handle_scope{ isolate };
	v8::Local<v8::ObjectTemplate> globalTemplate{ v8::ObjectTemplate::New(isolate) };
	v8::Local<v8::Context> context{ v8::Context::New(isolate) };
	v8::Context::Scope context_scope{ context };

	//source code from v8
	//v8::Global<v8::Context> evaluation_context;
	v8::Global<v8::Function> stringify_function;
	static const char* stringify_source;

	// Turn a value into a human-readable string.
	v8::Local<v8::String> stringify(v8::Isolate* isolate, v8::Local<v8::Value> value) {
		/*v8::Local<v8::Context> context =
			v8::Local<v8::Context>::New(js.isolate, evaluation_context);*/
		if (stringify_function.IsEmpty()) {
			v8::Local<v8::String> source =
				v8::String::NewFromUtf8(engine.isolate, stringify_source).ToLocalChecked();
			v8::Local<v8::String> name = v8::String::NewFromUtf8Literal(engine.isolate, "d8-stringify");
			v8::ScriptOrigin origin(name);
			v8::Local<v8::Script> script =
				v8::Script::Compile(context, source, &origin).ToLocalChecked();
			stringify_function.Reset(
				isolate, script->Run(context).ToLocalChecked().As<v8::Function>());
		}
		v8::Local<v8::Function> fun = v8::Local<v8::Function>::New(isolate, stringify_function);
		v8::Local<v8::Value> argv[1] = { value };
		v8::TryCatch try_catch(isolate);
		v8::MaybeLocal<v8::Value> result = fun->Call(context, Undefined(isolate), 1, argv);
		if (result.IsEmpty()) return v8::String::Empty(isolate);
		return result.ToLocalChecked().As<v8::String>();
	}

	v8::MaybeLocal<v8::String> readFile(const std::string& name);

	v8::MaybeLocal<v8::Value> executeFile(const std::string& indexPath) {
		v8::Local<v8::String> jsFile;
		v8::Local<v8::String> jsOrigin;
		v8::MaybeLocal<v8::String> jsOriginMaybe = v8::String::NewFromUtf8(engine.isolate,
			indexPath.data(), v8::NewStringType::kNormal,
			static_cast<int>(indexPath.length()));
		if (!readFile(indexPath).ToLocal(&jsFile) && jsOriginMaybe.ToLocal(&jsOrigin)) {
			//std::cout << "no index.js found, continuing without one\n";
		}
		else {
			v8::ScriptOrigin origin();
			v8::Local<v8::Script> script;
			v8::HandleScope handleScope{ engine.isolate };
			v8::TryCatch tryCatch{ engine.isolate };
			if (!v8::Script::Compile(context, jsFile).ToLocal(&script)) {
				// to do make this generic or something
				//reportExeception(tryCatch);
				//std::cout << "index.js failed, look at imgui console for details\n";
			}
			else {
				v8::Local<v8::Value> result;
				if (!script->Run(context).ToLocal(&result)) {
					// to do generic error report
					//reportExeception(tryCatch);
				}
				else {
					return result;
				}
			}
		}
		return v8::Local<v8::Value>();
	}

	void startNewContext() {
		context = v8::Context::New(engine.isolate, nullptr, globalTemplate);
	}

	//ASIO stuff

	std::unordered_map<int32_t, asio::steady_timer> setTimeoutTimers;
	int32_t nextTimerID = 0;
	static void setTimeout(const v8::FunctionCallbackInfo<v8::Value>& args) {
		v8::Isolate* isolate = args.GetIsolate();
		ScriptRuntime& runtime = *reinterpret_cast<ScriptRuntime*>(isolate->GetData(0));
		if (args.Length() != 2 || !args[0]->IsFunction() || !args[1]->IsNumber()) {
			isolate->ThrowException(
				v8::String::NewFromUtf8Literal(args.GetIsolate(), "Bad parameters"));
			return;
		}
		v8::Local<v8::Function> callback = args[0].As<v8::Function>();
		v8::Local<v8::Context> context = isolate->GetCurrentContext();
		auto storedCallback = v8::Global<v8::Function>(runtime.engine.isolate, callback);
		auto storedContext = v8::Global<v8::Context>(runtime.engine.isolate, context);
		v8::Local<v8::Number> delayMilliseconds = args[1].As<v8::Number>();
		int32_t timerID = runtime.nextTimerID;
		runtime.nextTimerID += 1;
		auto insertResult = runtime.setTimeoutTimers.emplace(timerID, runtime.ioContext);
		if (!insertResult.second) {
			isolate->ThrowException(
				v8::String::NewFromUtf8Literal(args.GetIsolate(), "Failed to insert into timers map"));
			return;
		}
		auto iterator = insertResult.first;
		asio::steady_timer& timer = iterator->second;
		timer.expires_after(std::chrono::milliseconds(static_cast<int64_t>(delayMilliseconds->Value())));
		timer.async_wait([&runtime, iterator = std::move(iterator), storedCallback = std::move(storedCallback), storedContext = std::move(storedContext)](const asio::error_code& error) {
			if (!error) {
				v8::HandleScope handleScope(runtime.engine.isolate); //not sure if this is the correct isolate to use
				v8::Local<v8::Function> callback = storedCallback.Get(runtime.engine.isolate);
				v8::Local<v8::Context> context = storedContext.Get(runtime.engine.isolate);
				v8::TryCatch tryCatch(runtime.engine.isolate);
				v8::Context::Scope contextScope(context);
				runtime.setTimeoutTimers.erase(iterator);
				if (callback->Call(context, v8::Undefined(runtime.engine.isolate), 0, nullptr).IsEmpty()) {
					return;
				}
			}
			else return;
		});
		args.GetReturnValue().Set(v8::Int32::New(isolate, timerID));
	}

	static void clearTimeout(const v8::FunctionCallbackInfo<v8::Value>& args) {
		v8::Isolate* isolate = args.GetIsolate();
		ScriptRuntime& runtime = *reinterpret_cast<ScriptRuntime*>(isolate->GetData(0));
		if (args.Length() != 1 || !args[0]->IsInt32()) {
			isolate->ThrowException(
				v8::String::NewFromUtf8Literal(args.GetIsolate(), "Bad parameters"));
			return;
		}
		v8::Local<v8::Context> context = isolate->GetCurrentContext();
		auto iterator = runtime.setTimeoutTimers.find(args[0].As<v8::Int32>()->Value());
		if (iterator == runtime.setTimeoutTimers.end()) {
			isolate->ThrowException(
				v8::String::NewFromUtf8Literal(args.GetIsolate(), "ID not found"));
			return;
		}
		auto& timer = iterator->second;
		timer.cancel();
		runtime.setTimeoutTimers.erase(iterator);
	}
};