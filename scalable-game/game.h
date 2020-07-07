#pragma once
#include "core.h"
#include <iostream>
#include <tuple>
#include <vector>
#include <cmath>
#include <bitset>
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
		float health = 0;
		enum AttackStage : int8_t {
			NONE = 0,
			GettingReady = 1,
			Attacking = 2,
			ReturningToNone = 3
		};
		AttackStage attackStage = NONE;
		int8_t royleHealth = 0;
		size_t index = 0;
	};
	Player players[maxPlayerCount] = {};
	int playersLeft = 0;

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
	
	//warmup -> 1: dungeon -> royleWarmup -> royle -> go to 1
	enum class Phase {
		WarmUp         = 1 << 0,
		DungeonCrawl   = 1 << 1,
		BattleRoyle    = 1 << 2,
		EndGame        = 1 << 3,
		DisablePlayers = 1 << 4, //don't allow players to move or fight
		RoyleWarmUp    = BattleRoyle | WarmUp,
	};
	Phase phase = Phase::WarmUp;
	double phaseTimer = 30.0;
	int roundNum = 0;

	void update(double deltaTime) {
		size_t playerInputIndex = 0;
		for (Command& playerInput : inputs) {
			players[playerInputIndex].index = playerInputIndex; //for my santity
			if (Snowflake::isAvaiable(playerInput.author)) {
				int axisIndex = 0;
				for (auto& moveOnAxis : playerInput.movement) {
					//movement should never be more then 1 or -1
					//so we need to clamp it
					if (1.0 < moveOnAxis)
						moveOnAxis = 1.0;
					if (moveOnAxis < -1.0)
						moveOnAxis = -1.0;
					if (static_cast<int>(phase) & static_cast<int>(Phase::DisablePlayers))
						moveOnAxis = 0.0;

					//to do maybe add physics
					players[playerInputIndex].position[axisIndex] +=
						moveOnAxis * 3.0 * deltaTime;
					axisIndex += 1;
				}
			}
			playerInputIndex += 1;
		}

		time += deltaTime;
		tick += 1;

		const auto respawn = [=](Player& player) {
			const int spawnDistence = 
				static_cast<int>(phase) & static_cast<int>(Phase::BattleRoyle) ?
				2 / roundNum : 2 * roundNum;
			for (float& axis : player.position) {
				//to do find a better way to get spawn placement
				axis = spawnDistence * player.index;
			}
			if (
				static_cast<int>(phase) & static_cast<int>(Phase::WarmUp) ||
				0 < player.royleHealth
			)
				player.health = 100.0f;
			player.attackStage = Player::AttackStage::NONE;
		};

		phaseTimer -= deltaTime;
		if (phaseTimer <= 0.0) {
			//time to switch modes
			switch (phase) {
			case Phase::WarmUp:
				phase = Phase::DungeonCrawl;
				phaseTimer = 10.0; //placeholder values
				for (Player& player : players) {
					if (Snowflake::isAvaiable(inputs[player.index].author))
						player.royleHealth = 3;
				}
				roundNum = 1;
				break;
			break;
			case Phase::DungeonCrawl:
				//each dungeon should be bigger then the last
				phase = Phase::BattleRoyle;
				phaseTimer = 20.0;
			break;
			case Phase::BattleRoyle:
				//each battleRoyle should be more dense with players then the last
				phase = Phase::DungeonCrawl;
				phaseTimer = 15.0;
				roundNum += 1;
				break;
			}

			for (Player& player : players) {
				if (Snowflake::isAvaiable(inputs[player.index].author))
					respawn(player);
			}
			for (Entity& entity : entities) {
				entity.type = Entity::Type::NONE;
			}
		}

		const auto damagePlayer = [=](Player& player, const float amount) {
			player.health -= amount;
			if (player.health <= 0.0) {
				//on other player's death
				if (static_cast<int>(phase) & static_cast<int>(Phase::BattleRoyle))
					//after death, they get a head start on their next Dungeon
					//After dying in BattleRoyle a few times, they are out of the game.
					//after being out of the game, the value of their items are dropped
					//in keys and some money.
					player.royleHealth -= 1;
			}
		};

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

		int playersLeft = 0;
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
						damagePlayer(otherPlayer, 100.0f * deltaTime);
					}
				}

				//bow attack
				if (
					playerInput.actionFlags & 1 &&
					!(static_cast<int>(phase) & static_cast<int>(Phase::DisablePlayers))
				) {
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
			} else if (!Snowflake::isAvaiable(playerInput.author)) {
				//do nothing
			} else if (static_cast<int>(phase) & static_cast<int>(Phase::WarmUp)) {
				//died during warm up
				respawn(player);
			} else if (static_cast<int>(phase) & static_cast<int>(Phase::DungeonCrawl)) {
				//death in a dungeon
				//a new dungeon is given to the player, they keep most of their stuff
				//they however pay the cost of a new layout and respawn
				if (0 < player.royleHealth)
					respawn(player);
			} else if (static_cast<int>(phase) & static_cast<int>(Phase::BattleRoyle)) {
				//death during battle royle
				//notthing
			}

			playersLeft += 0 < player.royleHealth ? 1 : 0;

			playerInputIndex += 1;
		}

		if (
			!(static_cast<int>(phase) & static_cast<int>(Phase::WarmUp))
			&&
			playersLeft <= 1
		) {
			phase = static_cast<Phase> (
				static_cast<int>(Phase::EndGame) | static_cast<int>(Phase::DisablePlayers)
			);
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
					entity.type = Entity::Type::NONE;
					damagePlayer(otherPlayer, 50.0f);
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