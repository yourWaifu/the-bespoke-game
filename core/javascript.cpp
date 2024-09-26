#include "javascript.h"
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

v8::MaybeLocal<v8::String> ScriptRuntime::readFile(const std::string& name)
{
	fs::path path{ name };
	std::ifstream file(path, std::ios::in | std::ios::binary);
	if (!file) { return v8::MaybeLocal<v8::String>(); } // Failed
	const auto size = fs::file_size(path);
	std::string buffer( size, '\0' );
	file.read(buffer.data(), size);

	v8::MaybeLocal<v8::String> result = v8::String::NewFromUtf8(isolate, buffer.data(), v8::NewStringType::kNormal, static_cast<int>(size));
	return result;
}

const char* ScriptRuntime::stringify_source = R"D8(
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