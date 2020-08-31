#pragma once
#define _USE_MATH_DEFINES
#include <math.h>
#include "game.h"
#include <SDL.h>
#include <SDL_syswm.h>
#include "imgui.h"
#include "imgui_sdl/imgui_sdl.h"

//client side rendering code
class TheWarrenRenderer {
public:
	TheWarrenRenderer(SDL_Window* window) {
		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
		SDL_RenderGetViewport(renderer, &viewport);
		if (!renderer)
			throw "SDL_CreateRenderer failed";
		ImGuiSDL::Initialize(renderer, viewport.w, viewport.h);
	}

	~TheWarrenRenderer() {
		ImGuiSDL::Deinitialize();
		if (renderer)
			SDL_DestroyRenderer(renderer);
	}

	struct ImGUIWrapper {
		ImGUIWrapper() {
			ImGui::CreateContext();
		}
		~ImGUIWrapper() {
			ImGui::DestroyContext();
		}	
	} imGuiWrapper;

	template<typename WindowT>
	static TheWarrenRenderer create(const WindowT& window) {
		return TheWarrenRenderer{window.window};
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

		//waiting screen
		//to do clean this up by removing the if
		if (!core.isClientReady()) {
			//move a box in a circle
			const int screenSize = viewport.w < viewport.h ?
				viewport.w : viewport.h;
			const int screenCenter[2] {
				static_cast<int>(viewport.w / 2),
				static_cast<int>(viewport.h / 2),
			};
			
			const int animationSize = screenSize / 3;
			const int circleRadius = animationSize / 2;
			const int iconSize = animationSize / 9;
			const float animationLength = 1.0f;

			const float animationProgress = std::fmod(state.time, animationLength);
			const float direction = animationProgress * 2 * M_PI;

			const SDL_Rect icon = {
				static_cast<int>(std::cos(direction) * circleRadius) - (iconSize / 2)
					+ screenCenter[Axis::X],
				static_cast<int>(std::sin(direction) * circleRadius) - (iconSize / 2)
					+ screenCenter[Axis::Y],
				iconSize, iconSize
			};
			
			const int centerIconSize = animationSize - 
				(std::sin(state.time * 0.5) * (animationSize / 10));
			const SDL_Rect centerIcon = {
				0 - (centerIconSize / 2) + screenCenter[Axis::X],
				0 - (centerIconSize / 2) + screenCenter[Axis::Y],
				centerIconSize, centerIconSize
			};

			SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
			SDL_RenderDrawRect(renderer, &icon);
			SDL_RenderDrawRect(renderer, &centerIcon);

			ImGui::NewFrame();

			ImGui::Text("Please Wait");

			ImGui::ShowDemoWindow();

			ImGui::Render();
			ImGuiSDL::Render(ImGui::GetDrawData());
			SDL_RenderPresent(renderer);
			return;
		}

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

		//walls
		SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
		for (auto wall : walls) {
			const float wallPosition[2] = {
				wall.position.x, wall.position.y
			};
			const float wallScale[2] = {
				wall.scale.x, wall.scale.y
			};
			const SDL_Rect wallRect =
				toSDLScreenSpaceRect(wallPosition, wallScale);
			SDL_RenderDrawRect(renderer, &wallRect);
		}

		//HUD
		const float UIScale = 2.0;
		const float halfViewportW = (viewport.w / 2);
		const float halfViewportH = (viewport.h / 2);
		const float topLeft    [2] = {halfViewportW * -1, halfViewportH     };
		const float topRight   [2] = {halfViewportW     , halfViewportH     };
		const float bottomLeft [2] = {halfViewportW * -1, halfViewportH * -1};
		const float bottomRight[2] = {halfViewportW     , halfViewportH * -1};
		const float center     [2] = {                 0,                  0};

		//to do make a function to remove dup code
		const auto spaceToSDLRect =
			[=](const float* originPoint, const float* point, const float* rectScale) {
				SDL_Rect rect;
				rect.w = rectScale[Axis::X] * UIScale;
				rect.h = rectScale[Axis::Y] * UIScale;
				//since SDL places the point on the upper left
				//and we want the point to be on the center
				//we need to sub half of the rect to both axis
				const int halfW = rect.w / 2;
				const int halfH = rect.h / 2;
				rect.x = toSDLSpaceX(
					(point[Axis::X] * UIScale) + originPoint[Axis::X], viewport) - halfW;
				rect.y = toSDLSpaceY(
					(point[Axis::Y] * UIScale) + originPoint[Axis::Y], viewport) - halfH;
				return rect;
			}
		;

		const float healthIconScale[2] = { 5.0f, 5.0f };
		SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
		for (int i = 0; i < thisPlayer.royleHealth; i += 1) {
			const float healthIconPos[2] = {
				(i * healthIconScale[Axis::X]) + healthIconScale[Axis::X],
				healthIconScale[Axis::Y] * -1
			};
			const SDL_Rect healthIconRect = spaceToSDLRect(
				topLeft,
				healthIconPos,
				healthIconScale
			);
			SDL_RenderDrawRect(renderer, &healthIconRect);
		}

		constexpr float ammoIconScale[2] = { 4.0f, 4.0f };
		for (int i = 0; i < thisPlayer.ammo; i += 1) {
			const float ammoIconPos[2] = {
				((i * (ammoIconScale[Axis::X] / 2)) + (ammoIconScale[Axis::X] / 2)) * -1,
				ammoIconScale[Axis::X]
			};
			const SDL_Rect ammoIconRect = spaceToSDLRect(
				bottomRight,
				ammoIconPos,
				ammoIconScale
			);
			SDL_RenderDrawRect(renderer, &ammoIconRect);
		}

		int16_t phaseSecondsBits = static_cast<int>(state.phaseTimer);
		const int phaseTimeSize = std::numeric_limits<int16_t>::digits;
		for (int i = 0; i < phaseTimeSize && phaseSecondsBits != 0; i += 1) {
			if (phaseSecondsBits & 1) {
				const float healthIconPos[2] = {
					((i * healthIconScale[Axis::X]) + (healthIconScale[Axis::X] / 2)) * -1,
					healthIconScale[Axis::Y] * -1.0f * 0.5f
				};
				const SDL_Rect healthIconRect = spaceToSDLRect(
					topRight,
					healthIconPos,
					healthIconScale
				);
				SDL_RenderDrawRect(renderer, &healthIconRect);
			}
			phaseSecondsBits >>= 1;
		}

		//shop UI
		constexpr float shopItemIconScale[2] = { 10.0, 10.0 };
		if (thisPlayer.inputMode == TheWarrenState::Player::InputMode::Shop) {
			constexpr auto shopSize = TheWarrenState::Player::maxShopSize;
			constexpr double doublePi = (2 * M_PI);
			const double buttonCircularSectorSize = doublePi / shopSize;
			const double halfButtonCircularSectorSize = buttonCircularSectorSize / 2;
			const double startAngle = -1 * M_PI;
			for (int i = 0; i < shopSize; i += 1) {
				if (thisPlayer.shop[i] == TheWarrenState::Player::ItemType::NONE)
					continue;

				double direction = startAngle + halfButtonCircularSectorSize +
					(i * buttonCircularSectorSize);
				constexpr double distanceAwayFromOrgin = 30.0;
				const float shopItemIconPos[2] = {
					static_cast<float>(std::sin(direction) * distanceAwayFromOrgin), //x
					static_cast<float>(std::cos(direction) * distanceAwayFromOrgin)  //y
				};
				const SDL_Rect healthIconRect = spaceToSDLRect(
					center,
					shopItemIconPos,
					shopItemIconScale
				);
				SDL_RenderDrawRect(renderer, &healthIconRect);
			}
		}

		//ImGui
		ImGui::NewFrame();

		ImGui::Text("Round %d: %d", state.roundNum, static_cast<int>(state.phaseTimer));
		ImGui::Text("HP: %d", static_cast<int>(thisPlayer.health));
		ImGui::Text("Ammo/Mana: %d", thisPlayer.ammo);
		ImGui::Text("inputMode: %d", static_cast<int>(thisPlayer.inputMode));
		ImGui::Text("AttackStage: %d %f", static_cast<int>(thisPlayer.attackStage), thisPlayer.attackStageTimer);

		//to do fix dup code, also in filament renderer
		const auto showItemName = [=](const TheWarrenState::Player::ItemType& item) {
			ImGui::Text("%s", itemData[static_cast<int>(item)].name);
			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
				ImGui::Text("%s", itemData[static_cast<int>(item)].description);
				ImGui::PopTextWrapPos();
				ImGui::EndTooltip();
			}
		};

		if(ImGui::CollapsingHeader("Inventory")) {
			ImGui::Text("DHand slot: %d\tNDHand slot: %d",
				thisPlayer.dominantHandSlot,
				thisPlayer.nondominantHandSlot);
			int i = 0;
			for (const auto item : thisPlayer.inventory) {
				ImGui::PushID(i);

				const char * startText = "Slot";
				const bool isInDominantHand = i == thisPlayer.dominantHandSlot;
				const bool isInNonDominantHand = i == thisPlayer.nondominantHandSlot;

				ImGui::Selectable("", isInDominantHand, ImGuiSelectableFlags_SpanAllColumns);
				ImGui::SameLine();
				if (isInNonDominantHand) {
					ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s", startText);
				} else {
					ImGui::Text("%s", startText);
				}
				ImGui::SameLine();
				ImGui::Text("%i:", i);
				ImGui::SameLine();
				showItemName(item);

				ImGui::PopID();
				i += 1;
			}
		}
		if(ImGui::CollapsingHeader("Shop")) {
			ImGui::Text("%dK", thisPlayer.keys);
			ImGui::Columns(3, "shopcolumns");
			ImGui::Text("Item name"); ImGui::NextColumn();
			ImGui::Text("Cost in keys"); ImGui::NextColumn();
			ImGui::Text("Can Pay"); ImGui::NextColumn();
			ImGui::Separator();
			int selction = TheWarrenState::Player::getShopSelectionIndex(state.inputs[thisPlayer.index]);
			int i = 0;
			for (const auto item : thisPlayer.shop) {
				ImGui::Selectable(
					"", selction == i,
					ImGuiSelectableFlags_SpanAllColumns);
				ImGui::SameLine();
				showItemName(item); ImGui::NextColumn();
            	ImGui::Text("%dK", *itemData[static_cast<int>(item)].costKeys); ImGui::NextColumn();
            	ImGui::Text("%s", thisPlayer.canPay(item) ? "yes" : "no"); ImGui::NextColumn();
				i += 1;
			}
			ImGui::Columns(1);
			ImGui::Separator();
		}
		if(ImGui::CollapsingHeader("Scoreboard")) {
			ImGui::Columns(2, "scoreboardcolumns");
			ImGui::Text("player index"); ImGui::NextColumn();
			ImGui::Text("royle health"); ImGui::NextColumn();
			ImGui::Separator();
			for (auto& playerIndex : state.playerRankings) {
				auto& player = state.players[playerIndex];
				ImGui::Selectable(
					std::to_string(player.index).c_str(),
					thisPlayerIndex == playerIndex,
					ImGuiSelectableFlags_SpanAllColumns);
				ImGui::NextColumn();
				ImGui::Text("%d", player.royleHealth);
				ImGui::NextColumn();
			}
			ImGui::Columns(1);
			ImGui::Separator();
		}

		if(ImGui::CollapsingHeader("Client Core Data")) {
			const auto& pingTimes = core.getPingTimes();
			float chartMax = 0.0;
			int searchSize = pingTimes.size() < 128 ? pingTimes.size() : 128;
			float totalPing = 0.0;
			for (int i = 0; i < searchSize; i += 1) {
				const float& pingTime = pingTimes.getFromHead(i);
				if (chartMax < pingTime) {
					chartMax = pingTime;
				}
				totalPing += pingTime;
			}
			float averagePing = totalPing / static_cast<float>(searchSize);
			ImGui::Text("Ping: %fsec.\tavg.: %f", pingTimes.getFromTail(), averagePing);
			ImGui::Text("Ping Chart High: %fsec.", chartMax);
			ImGui::PlotLines("Ping", pingTimes.data(), pingTimes.size(), 0, nullptr, 0.0, chartMax, ImVec2(0, 80.0f));
		}

		ImGui::Render();
		ImGuiSDL::Render(ImGui::GetDrawData());

		SDL_RenderPresent(renderer);
	}
private:
	SDL_Renderer* renderer = nullptr;
	SDL_Rect viewport;
};

using Renderer = TheWarrenRenderer;