#pragma once
#include <array>
#include <string>
#include <vector>
#include <list>
#include "imgui.h"
#include "javascript.h"

struct Console {
	std::array<char, 256> inputBuffer;
	std::list<v8::UniquePersistent<v8::Value>> output; //to do replace with something from the game server
	//std::vector<std::string> history;

	ScriptRuntime& js;
	v8::Isolate::Scope isolate_scope{js.isolate};
	v8::HandleScope handle_scope{js.isolate};
	v8::Local<v8::Context> context{v8::Context::New(js.isolate)};
	v8::Context::Scope context_scope{context};

	struct TreePosition {
		TreePosition(uint _level, uint _depth = 0, uint _index = 0) :
			level(_level), depth(_depth), index(_index) {}
		uint level;
		uint depth = 0;
		uint index = 0;
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
					if (array->Get(context, i).ToLocal(&theValue))
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

				if (object->GetOwnPropertyNames(context).ToLocal(&names)) {
					const auto& listPairs = [&](v8::Local<v8::Array> names) {
						for (uint i = 0; i < names->Length(); i += 1) {
							v8::Local<v8::Name> name = names->Get(context, i).ToLocalChecked().As<v8::Name>();
							v8::Local<v8::Value> desc = object->GetOwnPropertyDescriptor(context, name).ToLocalChecked();
							if (desc->IsUndefined()) continue;
							if (name->IsSymbol()) {
								v8::Local<v8::String> left =
									v8::String::NewFromUtf8Literal(js.isolate, "[");
								v8::Local<v8::String> right =
									v8::String::NewFromUtf8Literal(js.isolate, "]");
								v8::Local<v8::String> one = v8::String::Concat(js.isolate, left,
									name->ToString(context).ToLocalChecked());
								name = v8::String::Concat(js.isolate, one, right);
							}
							v8::Local<v8::Value> value;
							if (desc.As<v8::Object>()->Get(context, name).ToLocal(&value)) {
								ImGui::Text("%s:", *v8::String::Utf8Value{js.isolate, name});
								ImGui::SameLine();
								drawObjectTree(object->Get(context, name).ToLocalChecked(),
									{ pos.level, pos.depth + 1, i });
							}
							v8::Local<v8::Value> getter;
							if (
								desc.As<v8::Object>()->Get(context,
									v8::String::NewFromUtf8Literal(js.isolate, "get")
								).ToLocal(&getter) &&
								!getter->IsUndefined()
							) {
								ImGui::Text(
									"get %s: %s",
									*v8::String::Utf8Value{js.isolate, name},
									*v8::String::Utf8Value{
										js.isolate, getter->ToString(context).ToLocalChecked()
									}
								);
							}
							v8::Local<v8::Value> setter;
							if (
								desc.As<v8::Object>()->Get(context,
									v8::String::NewFromUtf8Literal(js.isolate, "set")
								).ToLocal(&setter) &&
								!setter->IsUndefined()
							) {
								ImGui::Text(
									"set %s: %s",
									*v8::String::Utf8Value{js.isolate, name},
									*v8::String::Utf8Value{
										js.isolate, setter->ToString(context).ToLocalChecked()
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

//source code from v8
	//v8::Global<v8::Context> evaluation_context;
	v8::Global<v8::Function> stringify_function;
	const char* stringify_source = R"D8(
(function() {
"use strict";

// A more universal stringify that supports more types than JSON.
// Used by the d8 shell to output results.
var stringifyDepthLimit = 4;  // To avoid crashing on cyclic objects

// Hacky solution to circumvent forcing --allow-natives-syntax for d8
function isProxy(o) { return false };
function JSProxyGetTarget(proxy) { };
function JSProxyGetHandler(proxy) { };

try {
  isProxy = Function(['object'], 'return %IsJSProxy(object)');
  JSProxyGetTarget = Function(['proxy'],
    'return %JSProxyGetTarget(proxy)');
  JSProxyGetHandler = Function(['proxy'],
    'return %JSProxyGetHandler(proxy)');
} catch(e) {};


function Stringify(x, depth) {
  if (depth === undefined)
    depth = stringifyDepthLimit;
  else if (depth === 0)
    return "...";
  if (isProxy(x)) {
    return StringifyProxy(x, depth);
  }
  switch (typeof x) {
    case "undefined":
      return "undefined";
    case "boolean":
    case "number":
    case "function":
    case "symbol":
      return x.toString();
    case "string":
      return "\"" + x.toString() + "\"";
    case "bigint":
      return x.toString() + "n";
    case "object":
      if (x === null) return "null";
      if (x.constructor && x.constructor.name === "Array") {
        var elems = [];
        for (var i = 0; i < x.length; ++i) {
          elems.push(
            {}.hasOwnProperty.call(x, i) ? Stringify(x[i], depth - 1) : "");
        }
        return "[" + elems.join(", ") + "]";
      }
      try {
        var string = String(x);
        if (string && string !== "[object Object]") return string;
      } catch(e) {}
      var props = [];
      var names = Object.getOwnPropertyNames(x);
      names = names.concat(Object.getOwnPropertySymbols(x));
      for (var i in names) {
        var name = names[i];
        var desc = Object.getOwnPropertyDescriptor(x, name);
        if (desc === (void 0)) continue;
        if (typeof name === 'symbol') name = "[" + Stringify(name) + "]";
        if ("value" in desc) {
          props.push(name + ": " + Stringify(desc.value, depth - 1));
        }
        if (desc.get) {
          var getter = Stringify(desc.get);
          props.push("get " + name + getter.slice(getter.indexOf('(')));
        }
        if (desc.set) {
          var setter = Stringify(desc.set);
          props.push("set " + name + setter.slice(setter.indexOf('(')));
        }
      }
      return "{" + props.join(", ") + "}";
    default:
      return "[crazy non-standard value]";
  }
}

function StringifyProxy(proxy, depth) {
  var proxy_type = typeof proxy;
  var info_object = {
    target: JSProxyGetTarget(proxy),
    handler: JSProxyGetHandler(proxy)
  }
  return '[' + proxy_type + ' Proxy ' + Stringify(info_object, depth-1) + ']';
}

return Stringify;
})();

)D8";

	Console(ScriptRuntime& _js) : js(_js) {
		
	}
	~Console() {
		//evaluation_context.Reset();
		stringify_function.Reset();
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
				std::to_string(message->GetStartColumn(context).FromJust());
			errorMsg += ':';
			errorMsg += *exception;
			output.push_back(v8::UniquePersistent<v8::Value>{
				js.isolate,
				v8::String::NewFromUtf8(js.isolate, errorMsg.c_str())
				.ToLocalChecked()});
		}
	}

	// Turn a value into a human-readable string.
	v8::Local<v8::String> stringify(v8::Isolate* isolate, v8::Local<v8::Value> value) {
		/*v8::Local<v8::Context> context =
			v8::Local<v8::Context>::New(js.isolate, evaluation_context);*/
		if (stringify_function.IsEmpty()) {
			v8::Local<v8::String> source =
				v8::String::NewFromUtf8(js.isolate, stringify_source).ToLocalChecked();
			v8::Local<v8::String> name = v8::String::NewFromUtf8Literal(js.isolate, "d8-stringify");
			v8::ScriptOrigin origin(name);
			v8::Local<v8::Script> script =
				v8::Script::Compile(context, source, &origin).ToLocalChecked();
			stringify_function.Reset(
				isolate, script->Run(context).ToLocalChecked().As<v8::Function>());
		}
		v8::Local<v8::Function> fun = v8::Local<v8::Function>::New(isolate, stringify_function);
		v8::Local<v8::Value> argv[1] = {value};
		v8::TryCatch try_catch(isolate);
		v8::MaybeLocal<v8::Value> result = fun->Call(context, Undefined(isolate), 1, argv);
		if (result.IsEmpty()) return v8::String::Empty(isolate);
		return result.ToLocalChecked().As<v8::String>();
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
		
		uint count = 0;
		for (auto& value : output) {
			drawObjectTree(value.Get(js.isolate), count);
			count += 1;
		}
		
		ImGui::EndChild();
		ImGuiInputTextFlags inputFlags =
			ImGuiInputTextFlags_EnterReturnsTrue |
			ImGuiInputTextFlags_CallbackCompletion |
			ImGuiInputTextFlags_CallbackHistory;
		if (ImGui::InputText(">", inputBuffer.data(),
				inputBuffer.max_size(), inputFlags,
				[](ImGuiTextEditCallbackData* data) -> int {
					return 0;
				},
				static_cast<void*>(this)
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
				if (!v8::Script::Compile(context, source).ToLocal(&script)) {
					reportExeception(tryCatch);
				} else {
					v8::Local<v8::Value> result;
					if (!script->Run(context).ToLocal(&result)) {
						reportExeception(tryCatch);
					} else if (!result->IsUndefined()) {
						output.push_back(v8::UniquePersistent<v8::Value>{js.isolate, result});
					}
				}
			}
			inputBuffer = {0};
			//history.push_back(output);
			//output.clear();
		}
		ImGui::End();
	}
};