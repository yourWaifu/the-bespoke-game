#pragma once
#include "core.h"
#include <iostream>
#include <tuple>
#include <vector>
#include "nonstd/string_view.hpp"

//server and client side code
class TheWarrenState {
public:
	TheWarrenState() = default;

	//this will be the inputs to the game
	//each one is 32 btyes large or less
	struct Command {
		Snowflake::RawSnowflake author = {0};
		float movement[2] = {0}; //players only move on a plain
		float rotation = 0.0;
		char data[12] = {0};
	};
	using InputType = Command;

	static const int maxPlayerCount = 0b100;

	//4 * 32 btyes is 128 btyes
	Command inputs[maxPlayerCount] = {};

	struct Player {
		float position[Axis::NumOf] = {0};
		float health = 100;
		bool isAttacking;
	};
	Player players[maxPlayerCount] = {};

	void update(double deltaTime) {
		time += deltaTime;
		tick += 1;

		size_t playerInputIndex = 0;
		for (Command& playerInput : inputs) {
			if (Snowflake::isAvaiable(playerInput.author)) {
				//handle input
				int axisIndex = 0;
				for (auto& moveOnAxis : playerInput.movement) {
					//movement should never be more then 1 or -1
					//so we need to clamp it
					if (1.0 < moveOnAxis)
						moveOnAxis = 1.0;
					if (moveOnAxis < -1.0)
						moveOnAxis = -1.0;

					//to do maybe add physics
					players[playerInputIndex].position[axisIndex] +=
						moveOnAxis * 3.0 * deltaTime;
					axisIndex += 1;
				}
			}
			playerInputIndex += 1;
		}

		playerInputIndex = 0;
		for (Command& playerInput : inputs) {
			const float playerSizeRadius = 0.5;

			Player& player = players[playerInputIndex];
			if (0 < player.health) { //to do use isAttacking
				//get the point where the sword is
				const float& angleA = playerInput.rotation;
				const float hypotanose = 1.0;
				float triangleSideLengths[3] = {
					std::sin(angleA) * hypotanose, //A
					std::cos(angleA) * hypotanose, //B
					hypotanose //c : the length away from center of player
				};
				const float playerSwordLocation[2] = {
					player.position[Axis::X] + triangleSideLengths[0],
					player.position[Axis::Y] + triangleSideLengths[1]
				};

				//simple point to circle collistion detection
				const auto getDistance = [=](const Player& player) {
					double distenaceSquared = 0;
					for (int axis = 0; axis < 2; axis += 1) {
						const double distenceOnAxis =
							playerSwordLocation[axis] - player.position[axis];
						distenaceSquared +=
							distenceOnAxis * distenceOnAxis;
					}
					return std::sqrt(distenaceSquared);
				};
				
				for (Player& otherPlayer : players) {
					if (0 < otherPlayer.health && getDistance(otherPlayer) < playerSizeRadius) {
						//point is inside otherPlayer
						otherPlayer.health -= 100.0f * deltaTime;
					}
				}
			}
			playerInputIndex += 1;
		}
	}
	double time = 0;
	int tick = 0;
private:
};

using GameState = TheWarrenState;
using GameServer = Core<GameState, true>;
using GameClient = Core<GameState, false>;