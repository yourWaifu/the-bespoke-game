#pragma once
#include "game.h"
#include <SDL.h>
#include <SDL_syswm.h>

//client side rendering code
class TheWarrenRenderer {
public:
	TheWarrenRenderer(SDL_Window* window) {
		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
		if (!renderer)
			throw "SDL_CreateRenderer failed";
	}

	~TheWarrenRenderer() {
		if (renderer)
			SDL_DestroyRenderer(renderer);
	}

	template <class Type>
	inline Type toSDLSpaceX(Type x, const SDL_Rect& viewport) {
		return x + (viewport.w / 2);
	}

	template <class Type>
	inline Type toSDLSpaceY(Type y, const SDL_Rect& viewport) {
		return (y * -1) + (viewport.h / 2);
	}

	void draw(const TheWarrenState& state, double deltaTime) {
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
		SDL_RenderClear(renderer);

		SDL_Rect viewport;
		SDL_RenderGetViewport(renderer, &viewport);

		SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
		for (TheWarrenState::Player player : state.players) {
			SDL_RenderDrawPoint(renderer,
				toSDLSpaceX(player.position[Axis::X], viewport),
				toSDLSpaceY(player.position[Axis::Y], viewport));
		}

		SDL_RenderPresent(renderer);
	}
private:
	SDL_Renderer* renderer = nullptr;
};

using Renderer = TheWarrenRenderer;