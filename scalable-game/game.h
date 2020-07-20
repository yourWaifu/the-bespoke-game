#pragma once
#include "core.h"
#include <iostream>
#include <tuple>
#include <vector>
#include <cmath>
#include <bitset>
#include "nonstd/string_view.hpp"
#include "box2d/box2d.h"

class TheWarrenState;
class MyContactListener;

//to do make map random
b2World world(/*gravity*/b2Vec2{0.0f, 0.0f});
float wall[4] = { /*xy*/0.0f, -5.0f, /*hw*/5.0f, 1.0f};
b2Vec2 wallPos{ /*xy*/0.0f, -5.0f };
b2Fixture* wallFixture;

//box 2d BS
struct BaseB2Data {
		enum class Type : uint16_t {
		NA = 0,
		Character,
		Entity
	};
	Type type;
};

template<class Entity>
struct B2Data : BaseB2Data {
	//this class should only be created in the tick function
	//dangorous pointers
	//to do figure out how to not use pointers
	Entity* data;
	Entity& getData() {
		return *data;
	}
};

template<BaseB2Data::Type type>
struct metaB2Data {
	using Type = int;
};

std::vector<b2Fixture*> playerB2Fixtures;
std::vector<b2Fixture*> entityB2Fixtures;

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
		size_t index = 0;
		
		float position[Axis::NumOf] = {0};
		//int dimension = 0; //to do add this

		float health = 0;
		int8_t royleHealth = 0;
		
		enum AttackStage : int8_t {
			NONE = 0,
			GettingReady = 1,
			Attacking = 2,
			ReturningToNone = 3
		};
		AttackStage attackStage = NONE;
		float attackStageTimer = 0.0f; //disables attackStage switch when over 0
		
		//stats
		float movementSpeed = 3.0;
		
		//items
		int8_t ammo = 30;
		int8_t keys = 3;
		enum class WeaponType : int8_t {
			NONE = 0,
			//palyers can select to have something else in their left hand
			Bow,        //needs two hands to shoot
			            //can be used rapidly, or pulled back for a better shoot
						//counters BigSword
			CrossBow,   //can be used with two or one hands
			            //use two hands to have better aim
			            //some can hold more then one bolt
						//more powerful then bow but reloading is required
						//coutners are same as Bow
			ShortSword, //One handed offensive weapon
			            //only effective at close range
			Shield,     //One handed defenive weapon
			            //can't attack while blocking
						//can't block while attacking
			            //to block an arrow, they need to block after the arrow
						//was fired and facing the arrow. Same goes for swords
						//blocking swords need to be frame perfectly timed
			            //counters bows, crossbows, BigSword
			BigSword,   //Needs two hands just to hold it
                        //requires time to attack and time to be ready to attack again
			MagicSpell, //Can only be used by wizards
			MagicStaff, //Can only be used by witches
			MagicWond,  //Can be used by both wizards and witches
			TubeLiquid, //Can be thrown or drank, can be a potion or poison
			            //when dropped by other players, it'll be called mystery liquid
			Bomb,
			BallNChain, //Not sure if a good idea, since this will be hard to program
			ChainNHook, //Same with BallAndChain
		};
		WeaponType weapon = WeaponType::Bow; //to do, change to weapon model ID
		//item slots
		//the edges of item slots have advantages
		//n slots away from some items have advantages
	};
	Player players[maxPlayerCount] = {};
	int playersLeft = 0;

	struct Entity {
		enum class Type : int8_t {
			NONE = 0,
			Arrow = 1,
			PickUp = 2,
		};
		Type type = Type::NONE;
		float veocity[2];
		float position[2];
		//int dimension = 0; //to do add this
		float height = 3.5f; //to use half float
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

	void start() {
		//this is called only on the first tick during core init
		//set box 2d values
		b2PolygonShape wallBox;
		wallBox.SetAsBox(2.5f, 0.5f);
		b2BodyDef wallBodyDef;
		wallBodyDef.position.Set(wall[0], wall[1]);
		wallFixture = world.CreateBody(&wallBodyDef)->
			CreateFixture(&wallBox, 0.0f);

		b2CircleShape playerShape;
		playerShape.m_radius = 0.5f;
		b2FixtureDef playerfixtureDef;
		playerfixtureDef.shape = &playerShape;
		playerfixtureDef.density = 1.0f;
		playerfixtureDef.friction = 0.3f;
		b2BodyDef playerBodyDef;
		playerBodyDef.type = b2_dynamicBody;
		playerBodyDef.position.Set(0.0f, 0.0f);
		playerB2Fixtures.clear();
		for (auto player : players) {
			b2Body* body = world.CreateBody(&playerBodyDef);
			body->SetEnabled(false);
			b2Fixture* fixture = body->CreateFixture(&playerfixtureDef);
			playerB2Fixtures.push_back(fixture);
		}

		b2CircleShape entityShape;
		entityShape.m_radius = 0.0f;
		b2FixtureDef entityFixtureDef;
		entityFixtureDef.shape = &entityShape;
		entityFixtureDef.density = 1.0f;
		entityFixtureDef.friction = 0.3f;
		b2BodyDef entityBodyDef;
		entityBodyDef.type = b2_dynamicBody;
		entityBodyDef.position.Set(0.0f, 0.0f);
		entityB2Fixtures.clear();
		for (auto entity : entities) {
			b2Body* body = world.CreateBody(&entityBodyDef);
			body->SetEnabled(false);
			b2Fixture* fixture = body->CreateFixture(&entityFixtureDef);
			entityB2Fixtures.push_back(fixture);
		}
	}

	void update(double deltaTime) {
		const auto isPlayerDead = [=](Player& player) {
			return player.health <= 0.0;
		};

		B2Data<Player> b2PlayerData[maxPlayerCount];
		b2Vec2 playerVelocities[maxPlayerCount];
		size_t playerInputIndex = 0;
		for (Command& playerInput : inputs) {
			players[playerInputIndex].index = playerInputIndex; //for my santity
			Player& player = players[playerInputIndex];

			b2Fixture& fixture = *playerB2Fixtures[playerInputIndex];
			b2Body& body = *(fixture.GetBody());
			B2Data<Player>& b2Data = b2PlayerData[playerInputIndex];
			b2Data.type = BaseB2Data::Type::Character;
			b2Data.data = &player;
			body.SetUserData(&b2Data);

			if (Snowflake::isAvaiable(playerInput.author)) {
				int axisIndex = 0;
				float velocity[2];
				for (auto& moveOnAxis : playerInput.movement) {
					//movement should never be more then 1 or -1
					//so we need to clamp it
					//to do use std::clamp or something
					if (1.0 < moveOnAxis)
						moveOnAxis = 1.0;
					if (moveOnAxis < -1.0)
						moveOnAxis = -1.0;
					if (static_cast<int>(phase) & static_cast<int>(Phase::DisablePlayers))
						moveOnAxis = 0.0;

					velocity[axisIndex] = moveOnAxis * player.movementSpeed;
					axisIndex += 1;
				}
				//set values box2d needs to simlate the tick
				body.SetEnabled(true);
				body.SetTransform(
					b2Vec2{player.position[Axis::X], player.position[Axis::Y]},
					0.0f);
				b2Vec2 velocityVec{velocity[Axis::X], velocity[Axis::Y]};
				body.SetLinearVelocity(velocityVec);
				playerVelocities[playerInputIndex] = velocityVec;
				//disable collision for dead players
				fixture.SetSensor(isPlayerDead(player));
			} else {
				body.SetEnabled(false);
			}
			playerInputIndex += 1;
		}

		B2Data<Entity> b2EntityData[entitiesSize];
		size_t entityIndex = 0;
		for (Entity& entity : entities) {
			b2Fixture& fixture = *entityB2Fixtures[entityIndex];
			b2Body& body = *(fixture.GetBody());
			B2Data<Entity>& b2Data = b2EntityData[entityIndex];
			b2Data.type = BaseB2Data::Type::Entity;
			b2Data.data = &entity;
			body.SetUserData(&b2Data);

			if (entity.type != Entity::Type::NONE) {
				body.SetEnabled(true);
				body.SetTransform(
					b2Vec2{entity.position[Axis::X], entity.position[Axis::Y]},
					0.0f);
				body.SetLinearVelocity(b2Vec2{
					static_cast<float>(entity.veocity[Axis::X]),
					static_cast<float>(entity.veocity[Axis::Y])
				});
				switch (entity.type) {
				case Entity::Type::Arrow:
					//apply gravity
					entity.height -= 4.0f * deltaTime;
				break;
				default: break;
				}
				//fixture.SetSensor(true);
			} else {
				body.SetEnabled(false);
			}

			entityIndex += 1;
		}

		time += deltaTime;
		tick += 1;

		int32 velocityIterations = 6;
		int32 positionIterations = 2;
		world.Step(deltaTime, velocityIterations, positionIterations);

		playerInputIndex = 0;
		for (Command& playerInput : inputs) {
			b2Body& body = *(playerB2Fixtures[playerInputIndex]->GetBody());
			Player& player = players[playerInputIndex];
			b2Vec2 position = body.GetPosition();
			player.position[Axis::X] = position.x;
			player.position[Axis::Y] = position.y;
			playerInputIndex += 1;
		}

		entityIndex = 0;
		for (Entity& entity : entities) {
			b2Body& body = *(entityB2Fixtures[entityIndex]->GetBody());
			const b2Vec2& position = body.GetPosition();
			entity.position[Axis::X] = position.x;
			entity.position[Axis::Y] = position.y;
			const b2Vec2& velocity = body.GetLinearVelocity();
			entity.veocity[Axis::X] = velocity.x;
			entity.veocity[Axis::Y] = velocity.y;

			//stops arrows on the ground
			if (entity.height <= 0.0f) {
				for (float& axis : entity.veocity) {
					axis = 0.0f;
				}
			}

			entityIndex += 1;
		}

		//handle contect events
		const auto damagePlayer = [=](Player& player, const float amount) {
			player.health -= amount;
			if (isPlayerDead(player)) {
				//on other player's death
				if (static_cast<int>(phase) & static_cast<int>(Phase::BattleRoyle))
					//after death, they get a head start on their next Dungeon
					//After dying in BattleRoyle a few times, they are out of the game.
					//after being out of the game, the value of their items are dropped
					//in keys and some money.
					player.royleHealth -= 1;
			}
		};

		for (
			b2Contact* contact = world.GetContactList();
			contact != nullptr; contact = contact->GetNext()
		) {
			if (contact->IsTouching()) {
				const auto internalContactHandler = [=](b2Fixture* fixtureA, b2Fixture* fixtureB) {
					void* userDataA = fixtureA->GetBody()->GetUserData();
					void* userDataB = fixtureB->GetBody()->GetUserData();
					if (userDataA == nullptr)
						return;

					BaseB2Data& baseUserDataA = *static_cast<BaseB2Data*>(userDataA);

					switch(baseUserDataA.type) {
					case BaseB2Data::Type::NA: break;
					case BaseB2Data::Type::Character:
						
					break;
					case BaseB2Data::Type::Entity: {
						//It's important you get the pointer related stuff right
						//or else it will break everything. this stuff is pretty
						//dangerous, so be careful.
						auto& entityData = *static_cast<B2Data<TheWarrenState::Entity>*>(userDataA);
						auto& entity = entityData.getData();

						if (userDataB == nullptr) {
							entity = TheWarrenState::Entity{};
							break;
						}

						switch (static_cast<BaseB2Data*>(userDataB)->type) {
						case BaseB2Data::Type::Character: {
							auto& character = static_cast<B2Data<TheWarrenState::Player>*>(
								userDataB)->getData();
							switch (entity.type) {
							case TheWarrenState::Entity::Type::Arrow: {
								//check if the arrow is moving at all
								int movingOnAxisCount = 0;
								for (float& axis : entity.veocity) {
									movingOnAxisCount += axis != 0 ? 1 : 0;
								}
								if (movingOnAxisCount != 0) {
									damagePlayer(character, 50.0f);
								} else {
									//pick up arrow
									character.ammo += 1;
								}
								entity = TheWarrenState::Entity{};
								//to do check if the player hit the back/sides of the arrow
								//if they did, they should move the arrow with the player
							}
							break;
							
							case TheWarrenState::Entity::Type::PickUp:
								character.movementSpeed *= 2.0f;
								entity = TheWarrenState::Entity{};
							break;
							
							default:
								entity = TheWarrenState::Entity{};
							break;
							}
						} break;
						default: break;
						}
					} break;
					}
				};
				internalContactHandler(contact->GetFixtureA(), contact->GetFixtureB());
				internalContactHandler(contact->GetFixtureB(), contact->GetFixtureA());
			}
		}

		const auto getNewEntity = [=]() -> Entity& {
			entitiesEnd = entitiesEnd % entitiesSize;
			Entity& target = entities[entitiesEnd];
			entitiesEnd = entitiesEnd + 1;
			return target;
		};

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
			for (Entity& entity : entities) {
				entity.type = Entity::Type::NONE;
			}

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
			case Phase::DungeonCrawl: {
				//each dungeon should be bigger then the last
				phase = Phase::BattleRoyle;
				phaseTimer = 20.0;
			}
			break;
			case Phase::BattleRoyle:
				//each battleRoyle should be more dense with players then the last
				phase = Phase::DungeonCrawl;
				phaseTimer = 15.0;
				roundNum += 1;
			break;

			default: break;
			}

			for (Player& player : players) {
				if (Snowflake::isAvaiable(inputs[player.index].author))
					respawn(player);
			}
		}

		const float playerSizeRadius = 0.5;
		//simple point to circle collistion detection
		const auto getDistance = [=](const Player& player, const float (&point)[2], int dimension) {
			if (dimension != /*player.dimension*/0)
				return std::numeric_limits<double>::infinity();
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
			if (static_cast<int>(phase) & static_cast<int>(Phase::DisablePlayers)) {
				//notthing
			} else if (!isPlayerDead(player)) { //to do use isAttacking
				//get the point where the sword is
				const float& angleA = playerInput.rotation;
				const float hypotanose = 1.0;
				float triangleSideLengths[3] = {
					std::sin(angleA) * hypotanose, //A
					std::cos(angleA) * hypotanose, //B
					hypotanose //c : the length away from center of player
				};
				//to do use a switch statment
				switch (player.weapon) {
				case Player::WeaponType::ShortSword: {
					//passive effects
					const float playerSwordLocation[2] = {
						player.position[Axis::X] + triangleSideLengths[0],
						player.position[Axis::Y] + triangleSideLengths[1]
					};
					
					for (Player& otherPlayer : players) {
						if (!isPlayerDead(otherPlayer) &&
							getDistance(otherPlayer, playerSwordLocation, /*player.dimension*/0) < playerSizeRadius
						) {
							//point is inside otherPlayer
							damagePlayer(otherPlayer, 200.0f * deltaTime);
						}
					}
				}
				break;

				case Player::WeaponType::Bow: {
					//active effects
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
							if (0 < player.ammo) {
								//spawn arrow
								Entity& arrow = getNewEntity();

								arrow.type = Entity::Type::Arrow;
								arrow.veocity[Axis::X] = triangleSideLengths[Axis::X] * 12.0;
								arrow.veocity[Axis::Y] = triangleSideLengths[Axis::Y] * 12.0;
								//add veocity of player because that's how it works in real life
								//and prevents bugs
								b2Vec2& playerVeocity = playerVelocities[player.index];
								arrow.veocity[Axis::X] += playerVeocity.x;
								arrow.veocity[Axis::Y] += playerVeocity.y;
								arrow.position[Axis::X] =
									player.position[Axis::X] + triangleSideLengths[Axis::X];
								arrow.position[Axis::Y] =
									player.position[Axis::Y] + triangleSideLengths[Axis::Y];

								player.ammo -= 1;
							}
						} break;
						default: break;
						}
					}

					if (player.attackStageTimer <= 0.0f) {
						switch (player.attackStage) {
						case Player::AttackStage::Attacking:
							player.attackStage = Player::AttackStage::ReturningToNone;
							player.attackStageTimer = 1.0f;
							//reload arrow
							break;
						case Player::AttackStage::ReturningToNone:
							player.attackStage = Player::AttackStage::NONE;
							break;

						default: break;
						}
					} else {
						player.attackStageTimer -= deltaTime;
					}
				}
				break;

				default: break;

				}
			}
			//dead but not in disabled mode
			else if (!Snowflake::isAvaiable(playerInput.author)) {
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
	}

	double time = 0;
	int tick = 0;
private:
};

template<>
struct Serializer<TheWarrenState> {
	using DataType = TheWarrenState;
	static const size_t getSize(DataType& data) {
		size_t size = sizeof(DataType);
		return size;
	}
	static void serialize(
		std::string& target, DataType& data
	) {
		
	}
};

using GameState = TheWarrenState;
using GameServer = Core<GameState, true>;
using GameClient = Core<GameState, false>;