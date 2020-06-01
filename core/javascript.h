#pragma once
#include "libplatform/libplatform.h"
#include "v8.h"

struct ScriptRuntime {
public:
	ScriptRuntime() {
		platform = v8::platform::NewDefaultPlatform();
		v8::V8::InitializePlatform(platform.get());
		v8::V8::Initialize();

		create_params.array_buffer_allocator =
			v8::ArrayBuffer::Allocator::NewDefaultAllocator();
		isolate = v8::Isolate::New(create_params);
	}
	~ScriptRuntime() {
		isolate->Dispose();
		v8::V8::Dispose();
		v8::V8::ShutdownPlatform();
		delete create_params.array_buffer_allocator;
	}
	std::unique_ptr<v8::Platform> platform;
	v8::Isolate::CreateParams create_params;
	v8::Isolate* isolate; //default isolate
};