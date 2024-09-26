#pragma once
#include <array>
#include <string>
#include <vector>
#include <list>
#include <unordered_map>
#include "imgui.h"
#include "javascript.h"
#include "asio.hpp"

struct Console;
static Console* gConsole = nullptr; //used to pass to static functions

struct Console {
	std::array<char, 256> inputBuffer = {0};
	std::list<v8::UniquePersistent<v8::Value>> output; //to do replace with something from the game server
	std::list<v8::Global<v8::String>> history;
	const std::size_t historyMaxSize = 2;
	std::list<v8::Global<v8::String>>::iterator currentHistory;
	v8::UniquePersistent<v8::Object> selfRef;

	ScriptRuntime& js;

	struct TreePosition {
		TreePosition(unsigned int _level, unsigned int _depth = 0, unsigned int _index = 0) :
			level(_level), depth(_depth), index(_index) {}
		unsigned int level;
		unsigned int depth = 0;
		unsigned int index = 0;
	};

	void drawObjectTree(v8::Local<v8::Value> value, const TreePosition pos) {
		if (value->IsUndefined()) {
			ImGui::TextUnformatted("undefined");
		} else if (
			value->IsBoolean() ||
			value->IsNumber() ||
			value->IsFunction() ||
			value->IsSymbol()
		) {
			ImGui::TextUnformatted(
				*v8::String::Utf8Value(js.isolate, value));
		} else if (value->IsString()) {
			ImGui::Text("\"%s\"",
				*v8::String::Utf8Value(js.isolate, value));
		} else if (value->IsBigInt()) {
			ImGui::Text("%sn",
				*v8::String::Utf8Value(js.isolate, value));
		} else if (value->IsNull()) {
			ImGui::TextUnformatted("null");
		} else if (value->IsArray()) {
			v8::Local<v8::Array> array = value.As<v8::Array>();
			const std::string treeName = "Array (" + std::to_string(array->Length()) + ") "
				+ std::to_string(pos.level) + ':' + std::to_string(pos.depth);
			if (ImGui::TreeNode(treeName.c_str())) {
				for (std::size_t i = 0; i < array->Length(); i += 1) {
					ImGui::Text("%lu: ", i);
					ImGui::SameLine();
					v8::Local<v8::Value> theValue;
					if (array->Get(js.context, i).ToLocal(&theValue))
						drawObjectTree(theValue, { pos.level, pos.depth + 1, pos.index });
				}
				ImGui::TreePop();
			}
		} else if (value->IsObject()) {
			const std::string treeName = "Obj { ... "
				+ std::to_string(pos.level) + ':' + std::to_string(pos.depth) + ':'
				+ std::to_string(pos.index);
			if (ImGui::TreeNode(treeName.c_str())) {
				v8::Local<v8::Object> object = value.As<v8::Object>();
				v8::Local<v8::Array> names;

				if (object->GetOwnPropertyNames(js.context).ToLocal(&names)) {
					const auto& listPairs = [&](v8::Local<v8::Array> names) {
						for (unsigned int i = 0; i < names->Length(); i += 1) {
							v8::Local<v8::Name> name = names->Get(js.context, i).ToLocalChecked().As<v8::Name>();
							v8::Local<v8::Value> desc = object->GetOwnPropertyDescriptor(js.context, name).ToLocalChecked();
							if (desc->IsUndefined()) continue;
							if (name->IsSymbol()) {
								v8::Local<v8::String> left =
									v8::String::NewFromUtf8Literal(js.isolate, "[");
								v8::Local<v8::String> right =
									v8::String::NewFromUtf8Literal(js.isolate, "]");
								v8::Local<v8::String> one = v8::String::Concat(js.isolate, left,
									name->ToString(js.context).ToLocalChecked());
								name = v8::String::Concat(js.isolate, one, right);
							}
							v8::Local<v8::Value> value;
							if (desc.As<v8::Object>()->Get(js.context, name).ToLocal(&value)) {
								ImGui::Text("%s:", *v8::String::Utf8Value{js.isolate, name});
								ImGui::SameLine();
								drawObjectTree(object->Get(js.context, name).ToLocalChecked(),
									{ pos.level, pos.depth + 1, i });
							}
							v8::Local<v8::Value> getter;
							if (
								desc.As<v8::Object>()->Get(js.context,
									v8::String::NewFromUtf8Literal(js.isolate, "get")
								).ToLocal(&getter) &&
								!getter->IsUndefined()
							) {
								ImGui::Text(
									"get %s: %s",
									*v8::String::Utf8Value{js.isolate, name},
									*v8::String::Utf8Value{
										js.isolate, getter->ToString(js.context).ToLocalChecked()
									}
								);
							}
							v8::Local<v8::Value> setter;
							if (
								desc.As<v8::Object>()->Get(js.context,
									v8::String::NewFromUtf8Literal(js.isolate, "set")
								).ToLocal(&setter) &&
								!setter->IsUndefined()
							) {
								ImGui::Text(
									"set %s: %s",
									*v8::String::Utf8Value{js.isolate, name},
									*v8::String::Utf8Value{
										js.isolate, setter->ToString(js.context).ToLocalChecked()
									}
								);
							}
						}
					};
					listPairs(names);
					//to do add symbols
					//maybe do this in javascript instead
				} else {
					ImGui::TextUnformatted("Error: Couldn't get object names or ");
					return;
				}
				ImGui::TreePop();
			}
		} else {
			ImGui::TextUnformatted("[crazy non-standard value]");
		}
	}

	Console(
		ScriptRuntime& _js,
		asio::io_context& _ioContext
	) :
		js(_js)
	{
		gConsole = this; //to do fix this pointer mess
		// we create a template that is usally used once because that's how you set internal fields
		v8::Local<v8::ObjectTemplate> console_templ = v8::ObjectTemplate::New(js.isolate);
		console_templ->SetInternalFieldCount(1);
		auto consoleObj = console_templ->NewInstance(js.context).ToLocalChecked();
		// create pointer to access console in static functions
		consoleObj->SetInternalField(0, v8::External::New(js.isolate, this));
		js.globalTemplate->Set(js.isolate, "log", v8::FunctionTemplate::New(js.isolate, log, consoleObj));

		selfRef = v8::UniquePersistent<v8::Object>{ js.isolate, consoleObj };
	}

	~Console() {
		
	}

	void reportExeception(v8::TryCatch& tryCatch) {
		v8::HandleScope handleScope{js.isolate};
		v8::String::Utf8Value exception{js.isolate, tryCatch.Exception()};
		v8::Local<v8::Message> message = tryCatch.Message();
		if (message.IsEmpty()) {
			output.push_back(v8::UniquePersistent<v8::Value>{
				js.isolate,
				v8::String::NewFromUtf8(js.isolate, *exception)
				.ToLocalChecked()});
		} else {
			std::string errorMsg =
				std::to_string(message->GetStartColumn(js.context).FromJust());
			errorMsg += ':';
			errorMsg += *exception;
			output.push_back(v8::UniquePersistent<v8::Value>{
				js.isolate,
				v8::String::NewFromUtf8(js.isolate, errorMsg.c_str())
				.ToLocalChecked()});
		}
	}

	void draw(bool& isOpen) {
		if (!ImGui::Begin("Console", &isOpen)) {
			ImGui::End(); return;
		}

		// Reserve enough left-over height for 1 separator + 1 input text
        const float footer_height_to_reserve =
			ImGui::GetFrameHeightWithSpacing();
		ImGui::BeginChild("ConsoleOutput",
			ImVec2(0, -footer_height_to_reserve), false,
			ImGuiWindowFlags_HorizontalScrollbar);
		
		unsigned int count = 0;
		for (auto& value : output) {
			drawObjectTree(value.Get(js.isolate), count);
			count += 1;
		}
		
		ImGui::EndChild();

		ImGuiInputTextCallback inputCallback = [](ImGuiInputTextCallbackData* data) -> int {
			if (data->EventFlag != ImGuiInputTextFlags_CallbackHistory)
				return 0;
			Console& console = *reinterpret_cast<Console*>(data->UserData);
			switch (data->EventKey) {
			case ImGuiKey_UpArrow:
				if (console.currentHistory == console.history.end()) {
					console.currentHistory = console.history.begin();
				}
				else if (std::prev(console.history.end()) == console.currentHistory) {
					return 0;
				}
				else {
					console.currentHistory = console.currentHistory.operator++();
				}
			break;
			case ImGuiKey_DownArrow:
				if (console.currentHistory == console.history.end() || console.currentHistory == console.history.begin())
					return 0;
				else
					console.currentHistory = console.currentHistory.operator--();
			break;
			default: return 0;
			}
			data->DeleteChars(0, data->BufTextLen);
			data->InsertChars(0, *v8::String::Utf8Value(console.js.isolate, console.currentHistory->Get(console.js.isolate)));
			return 0;
		};


		ImGuiInputTextFlags inputFlags =
			ImGuiInputTextFlags_EnterReturnsTrue |
			//ImGuiInputTextFlags_CallbackCompletion |
			ImGuiInputTextFlags_CallbackHistory;
		if (ImGui::InputText(">", inputBuffer.data(),
				inputBuffer.max_size(), inputFlags, inputCallback, this
			)
		) {
			if (inputBuffer[0]) {
				//execute
				v8::Local<v8::String> source = v8::String::NewFromUtf8(
					js.isolate, inputBuffer.data()).ToLocalChecked();
				output.push_back(
					v8::UniquePersistent<v8::Value>{
						js.isolate,
						v8::String::Concat(js.isolate,
							v8::String::NewFromUtf8Literal(js.isolate, ">"), source
						)
					}
				);
				v8::HandleScope handleScope{js.isolate};
				v8::TryCatch tryCatch{js.isolate};
				v8::Local<v8::Script> script;
				if (!v8::Script::Compile(js.context, source).ToLocal(&script)) {
					reportExeception(tryCatch);
				} else {
					v8::Local<v8::Value> result;
					if (!script->Run(js.context).ToLocal(&result)) {
						reportExeception(tryCatch);
					} else if (!result->IsUndefined()) {
						output.push_back(v8::UniquePersistent<v8::Value>{js.isolate, result});
					}
				}
				
				//manage command history
				history.emplace_front(js.isolate, source);
				if (historyMaxSize < history.size()) {
					history.resize(historyMaxSize);
				}
				currentHistory = history.end();
			}
			inputBuffer = {0};
			//output.clear();
		}
		ImGui::End();
	}

	static void log(const v8::FunctionCallbackInfo<v8::Value>& args) {
		v8::Isolate* isolate = args.GetIsolate();
		ScriptRuntime& runtime = *reinterpret_cast<ScriptRuntime*>(isolate->GetData(0));
		Console& console = *reinterpret_cast<Console*>(
			args.Data().As<v8::Object>()->GetInternalField(0).As<v8::External>()->Value());
		if (args.Length() == 0) {
			isolate->ThrowException(
				v8::String::NewFromUtf8Literal(args.GetIsolate(), "Bad parameters"));
			return;
		}
		console.output.push_back(v8::UniquePersistent<v8::Value>{runtime.isolate, args[0]});
	}
};