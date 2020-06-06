#pragma once
#include "core.h"
#include <iostream>
#include <tuple>
#include <vector>
#include <cmath>
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
		uint8_t actionFlags = 0;
		char data[11] = {0};
	};
	using InputType = Command;

	static const int maxPlayerCount = 0b100;

	//4 * 32 btyes is 128 btyes
	Command inputs[maxPlayerCount] = {};

	struct Player {
		float position[Axis::NumOf] = {0};
		float health = 100;
		enum AttackStage : int8_t {
			NONE = 0,
			GettingReady = 1,
			Attacking = 2,
			ReturningToNone = 3
		};
		AttackStage attackStage = NONE;
	};
	Player players[maxPlayerCount] = {};

	struct Entity {
		enum class Type : int8_t {
			NONE = 0,
			Arrow = 1,
		};
		Type type = Type::NONE;
		float veocity[2];
		float position[2];
	};
	#define entitiesSize 64
	Entity entities[entitiesSize]; //to do make this variable
	int entitiesEnd = 0;

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

		const float playerSizeRadius = 0.5;
		//simple point to circle collistion detection
		const auto getDistance = [=](const Player& player, const float (&point)[2]) {
			double distenaceSquared = 0;
			for (int axis = 0; axis < 2; axis += 1) {
				const double distenceOnAxis =
					point[axis] - player.position[axis];
				distenaceSquared +=
					distenceOnAxis * distenceOnAxis;
			}
			return std::sqrt(distenaceSquared);
		};

		playerInputIndex = 0;
		for (Command& playerInput : inputs) {
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
				
				for (Player& otherPlayer : players) {
					if (0 < otherPlayer.health &&
						getDistance(otherPlayer, playerSwordLocation) < playerSizeRadius
					) {
						//point is inside otherPlayer
						otherPlayer.health -= 100.0f * deltaTime;
					}
				}

				//bow attack
				if (playerInput.actionFlags & 1) {
					switch (player.attackStage) {
					case Player::AttackStage::NONE:
						player.attackStage = Player::AttackStage::GettingReady;
						break;
					default: break;
					}
				} else {
					switch (player.attackStage) {
					case Player::AttackStage::GettingReady: {
						player.attackStage = Player::AttackStage::Attacking;
						//spawn arrow
						entitiesEnd = entitiesEnd % entitiesSize;
						Entity& arrow = entities[entitiesEnd];
						entitiesEnd = entitiesEnd + 1;

						arrow.type = Entity::Type::Arrow;
						arrow.veocity[Axis::X] = triangleSideLengths[Axis::X] * 12.0;
						arrow.veocity[Axis::Y] = triangleSideLengths[Axis::Y] * 12.0;
						arrow.position[Axis::X] =
							player.position[Axis::X] + triangleSideLengths[Axis::X];
						arrow.position[Axis::Y] =
							player.position[Axis::Y] + triangleSideLengths[Axis::Y];
					} break;
					case Player::AttackStage::Attacking:
						player.attackStage = Player::AttackStage::ReturningToNone;
						//reload arrow 
						break;
					case Player::AttackStage::ReturningToNone:
						player.attackStage = Player::AttackStage::NONE;
						break;
					default: break;
					}
				}
			}
			playerInputIndex += 1;
		}

		//calulate entities
		for (Entity& entity : entities) {
			if (entity.type == Entity::Type::NONE)
				continue;

			int axis = 0;
			for (float& positionAxis : entity.position) {
				positionAxis += entity.veocity[axis] * deltaTime;
				axis += 1;
			}

			for (Player& otherPlayer : players) {
				if (0 < otherPlayer.health &&
					getDistance(otherPlayer, entity.position) < playerSizeRadius
				) {
					//point is inside otherPlayer
					otherPlayer.health -= 100.0f * deltaTime;
				}
			}
		}
	}
	double time = 0;
	int tick = 0;
private:
};

using GameState = TheWarrenState;
using GameServer = Core<GameState, true>;
using GameClient = Core<GameState, false>;