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
		char data[16];
	};
	using InputType = Command;

	static const int maxPlayerCount = 0b100;

	//4 * 32 btyes is 128 btyes
	Command inputs[maxPlayerCount] = {};

	struct Player {
		float position[Axis::NumOf] = {0};
	};
	Player players[maxPlayerCount] = {};

	void update(double deltaTime) {
		size_t playerInputIndex = 0;
		for (Command& playerInput : inputs) {
			if (Snowflake::isAvaiable(playerInput.author)) {
				//handle input
				for (int i = 0; i < Axis::NumOf; i += 1) {
					//movement should never be more then 1 or -1
					//so we need to clamp it
					if (1.0 < playerInput.movement[i])
						playerInput.movement[i] = 1.0;
					if (playerInput.movement[i] < -1.0)
						playerInput.movement[i] = -1.0;

					//to do maybe add physics
					players[playerInputIndex].position[i] +=
						playerInput.movement[i] * 120 * deltaTime;
				}
			}
			playerInputIndex += 1;
		}
		time += deltaTime;
	}
	double time = 0;
private:
};

using GameState = TheWarrenState;
using GameServer = Core<GameState, true>;
using GameClient = Core<GameState, false>;