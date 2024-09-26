#pragma once
#include <SDL.h>
#include <SDL_syswm.h>
#include "console_imgui.h"

class GenericRenderer {
public:
	GenericRenderer(SDL_Window* window, void* windowHandle, Console& _console) {}
	~GenericRenderer() {}

	template<typename WindowT>
	static GenericRenderer create(const WindowT& window, Console& console) {
		return GenericRenderer{ window.window, window.getWindow(), console };
	}
};

using Renderer = GenericRenderer;