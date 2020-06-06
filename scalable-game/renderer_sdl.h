#pragma once
#define _USE_MATH_DEFINES
#include <math.h>
#include "game.h"
#include <SDL.h>
#include <SDL_syswm.h>

//client side rendering code
class TheWarrenRenderer {
public:
	TheWarrenRenderer(SDL_Window* window) {
		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
		SDL_RenderGetViewport(renderer, &viewport);
		if (!renderer)
			throw "SDL_CreateRenderer failed";
	}

	~TheWarrenRenderer() {
		if (renderer)
			SDL_DestroyRenderer(renderer);
	}

	//maybe move this to a sdl common file
	template <class Type>
	inline static Type toSDLSpaceX(const Type x, const SDL_Rect& viewport) {
		return x + (viewport.w / 2);
	}

	template <class Type>
	inline static Type toSDLSpaceY(const Type y, const SDL_Rect& viewport) {
		return (y * -1) + (viewport.h / 2);
	}

	const SDL_Rect getViewport() {
		return viewport;
	}

	template<class Core>
	void draw(Core& core, const TheWarrenState& state, double deltaTime) {
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
		SDL_RenderClear(renderer);

		SDL_RenderGetViewport(renderer, &viewport);

		//get current player
		int thisPlayerIndex = -1;
		core.getUserInput(core.getID(), state, [&thisPlayerIndex](const typename Core::GameState::InputType&, size_t index) {
			thisPlayerIndex = index;
		});
		if (thisPlayerIndex == -1) {
			//draw notthing
			SDL_RenderPresent(renderer);
		}
		const typename TheWarrenState::Player& thisPlayer = state.players[thisPlayerIndex];

		//player stats
		float playerScale[2] = { 1.0, 1.0 };

		//center on player for now
		const float scale = 40.0;
		const float playerToCameraOffset[2] = {
			0, 0
		};

		//for some reason, using a conditional operator or array for this gives us a syntax error
		//so I did it this really dumb roundabout way.
		float cameraLocationX; float cameraLocationY;
		if (0 <= thisPlayerIndex) {
			cameraLocationX = thisPlayer.position[Axis::X] + playerToCameraOffset[Axis::X];
			cameraLocationY = thisPlayer.position[Axis::Y] + playerToCameraOffset[Axis::Y];
		} else {
			cameraLocationX = 0 + playerToCameraOffset[Axis::X];
			cameraLocationY = 0 + playerToCameraOffset[Axis::Y];
		};
		const float cameraLocation[2] = { cameraLocationX, cameraLocationY };

		const auto toScreenSpace = [=](const float* point, const Axis axis) {
			return (point[axis] - cameraLocation[axis]) * scale;
		};

		const auto toSDLScreenSpaceRect = [=](const float* point, const float* rectScale) {
			SDL_Rect rect;
			rect.w = rectScale[Axis::X] * scale;
			rect.h = rectScale[Axis::Y] * scale;
			//since SDL places the point on the upper left
			//and we want the point to be on the center
			//we need to sub half of the rect to both axis
			const int halfW = rect.w / 2;
			const int halfH = rect.h / 2;
			rect.x = toSDLSpaceX(toScreenSpace(point, Axis::X), viewport) - halfW;
			rect.y = toSDLSpaceY(toScreenSpace(point, Axis::Y), viewport) - halfH;
			return rect;
		};

		int index = 0;
		for (TheWarrenState::Player player : state.players) {
			/*if (player.health <= 0) {
				index += 1;
				continue;
			}*/
			
			if (player.health <= 0) {
				//debug
				SDL_SetRenderDrawColor(renderer, 51, 51, 51, SDL_ALPHA_OPAQUE);
			} else if (index == thisPlayerIndex) {
				//blend lower health color with player color
				const int numOfSubColors = 3;
				float lowHealthColor[numOfSubColors] = { 1.0, 0.0, 0.0 };
				float lowHealthAlpha = 1.0 - (player.health / 100);
				float playerColor[numOfSubColors] = { 1.0, 1.0, 1.0 };
				float playerAlpha = 1.0;
				int finalColor[numOfSubColors] = { 0 };
				for (int i = 0; i < numOfSubColors; i += 1) {
					const float color =
						(lowHealthColor[i] * lowHealthAlpha + playerColor[i] * playerAlpha * (1.0 - lowHealthAlpha))
						/ (lowHealthAlpha + playerAlpha * (1.0 - lowHealthAlpha));
					finalColor[i] = color * 255.0;
				}
				SDL_SetRenderDrawColor(renderer, finalColor[0], finalColor[1], finalColor[2], SDL_ALPHA_OPAQUE);
			} else {
				SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
			}

			const SDL_Rect playerRect = 
				toSDLScreenSpaceRect(player.position, playerScale);
			SDL_RenderDrawRect(renderer, &playerRect);

			//draw a rect representing the player's rotation
			//to get the cords of the rect, we need to create a triangle
			//from the angle of the player's rotation and the length away
			//from the player the rect will be
			//for the final game, we'll just rotate the player
			std::array<float, 3> tempTriangleAngles = {
				state.inputs[index].rotation, //a
				0.0, // we don't really need this
				0.0, // we also don't need this
			};
			const std::array<float, 3> triangleAngles = tempTriangleAngles;
			const float& angle = state.inputs[index].rotation;
			const float& angleA = tempTriangleAngles[0];
			
			const float hypotanose = 1.0;
			float triangleSideLengths[3] = {
				std::sin(angleA) * hypotanose, //A
				std::cos(angleA) * hypotanose, //B
				hypotanose //c : the length away from player
			};

			const float playerRotationRectLocation[2] = {
				player.position[Axis::X] + triangleSideLengths[0],
				player.position[Axis::Y] + triangleSideLengths[1]
			};

			const float playerRotationRectScale[2] = {
				playerScale[Axis::X] / 3.0f,
				playerScale[Axis::Y] / 3.0f,
			};

			const SDL_Rect playerRotationRect =
				toSDLScreenSpaceRect(playerRotationRectLocation, playerRotationRectScale);
			SDL_RenderDrawRect(renderer, &playerRotationRect);
			
			index += 1;
		}

		//entities
		SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
		for (const TheWarrenState::Entity& entity : state.entities) {
			if (entity.type == TheWarrenState::Entity::Type::NONE)
				continue;
			
			const float entityRectScale[2] = {
				1.0f / 8.0f,
				1.0f / 8.0f,
			};

			const SDL_Rect entityRect =
				toSDLScreenSpaceRect(entity.position, entityRectScale);
			SDL_RenderDrawRect(renderer, &entityRect);
		}

		SDL_RenderPresent(renderer);
	}
private:
	SDL_Renderer* renderer = nullptr;
	SDL_Rect viewport;
};

using Renderer = TheWarrenRenderer;