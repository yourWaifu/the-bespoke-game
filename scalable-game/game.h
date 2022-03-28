#pragma once
#include "core.h"
#include <iostream>
#include <tuple>
#include <vector>
#include <cmath>
#include <bitset>
#include <algorithm>
#include <optional>
#include "nonstd/string_view.hpp"
#include "box2d/box2d.h"

#ifndef M_PI
	#define M_PI 3.14159265358979323846
#endif

#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif

#ifndef M_PI_4
#define M_PI_4 0.78539816339744830961
#endif

class TheWarrenState;

//to do clean up global variables into a class

//to do make map random
b2World world(/*gravity*/b2Vec2{0.0f, 0.0f});
//float wall[4] = { /*xy*/0.0f, -5.0f, /*wh*/5.0f, 1.0f};
//b2Vec2 wallPos{ /*xy*/0.0f, -5.0f };
//b2Fixture* wallFixture;

//box 2d BS
enum class B2DataType : uint16_t {
	NA = 0,
	Character,
	Entity
};

template<B2DataType type>
struct metaB2Data {
	using Type = int;
};

struct BaseB2Data {
	B2DataType type;
	template<B2DataType _type>
	const bool isType() {
		return type == _type;
	}
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
	struct Ptr {
		Ptr() = default;
		using GetType = B2Data<Entity>;
		Ptr(void * _ptr) : ptr(static_cast<GetType*>(_ptr)) {}
		operator bool () {
			return ptr != nullptr && ptr->data != nullptr;
		}
		Entity& get() {
			return *ptr->data;
		}
		const bool isType(B2DataType type) {
			return ptr->type == type;
		}
	private:
		B2Data<Entity>* ptr = nullptr;
	};
};

template<B2DataType type>
typename B2Data<typename metaB2Data<type>::Type>::Ptr castB2Data(void* ptr) {
	using PTR = typename B2Data<typename metaB2Data<type>::Type>::Ptr;
	PTR castedPtr{ptr};
	if (!castedPtr) return castedPtr;
	if (!castedPtr.isType(type)) return PTR{};
	return castedPtr;
}

std::vector<b2Fixture*> playerB2Fixtures;
std::vector<b2Fixture*> entityB2Fixtures;

struct Wall {
	b2Vec2 position;
	b2Vec2 scale;
};

std::array<Wall, 9> walls = {
	//          x      y              w     h
	Wall{b2Vec2{ 0.0f, -5.0f}, b2Vec2{5.0f, 1.0f}},
	Wall{b2Vec2{ 0.0f,  5.0f}, b2Vec2{5.0f, 1.0f}},
	Wall{b2Vec2{-5.0f,  0.0f}, b2Vec2{1.0f, 5.0f}},
	Wall{b2Vec2{ 5.0f,  0.0f}, b2Vec2{1.0f, 5.0f}},
	Wall{b2Vec2{ 0.0f,  0.0f}, b2Vec2{2.0f, 2.0f}},
	Wall{b2Vec2{ 0.0f, -9.0f}, b2Vec2{19.0f, 1.0f}},
	Wall{b2Vec2{ 0.0f,  9.0f}, b2Vec2{19.0f, 1.0f}},
	Wall{b2Vec2{-9.0f,  0.0f}, b2Vec2{1.0f, 19.0f}},
	Wall{b2Vec2{ 9.0f,  0.0f}, b2Vec2{1.0f, 19.0f}},
};

float playerSpawns[4][Axis::NumOf] = {
	{ 0.0f, -6.0f, 0.0f},
	{ 0.0f,  6.0f, 0.0f},
	{-6.0f,  0.0f, 0.0f},
	{ 6.0f,  0.0f, 0.0f}
};

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
		//inventory weapon selction
		//left to right: 4 bits Dominant Hand, 4 bits Non-Dominant Hand.
		//both indexes
		int8_t handSlots = 0;
		char data[10] = {0};
	};
	using InputType = Command;

	using PlayerIndex = int8_t;
	static const PlayerIndex maxPlayerCount = 0b100;

	//4 * 32 btyes is 128 btyes
	Command inputs[maxPlayerCount] = {};

	struct Player {
		size_t index = 0;
		
		float position[Axis::NumOf] = {0};
		//int dimension = 0; //to do add this

		float health = 0;
		int8_t royleHealth = 0;
		
		enum class AttackStage : int8_t {
			NONE = 0,
			GettingReady = 1,
			Attacking = 2,
			ReturningToNone = 3
		};
		AttackStage attackStage = AttackStage::NONE;
		float attackStageTimer = 0.0f; //disables attackStage switch when over 0
		float readyLength = 0.0f; //how long the ready stage was in seconds
		
		//stats
		float movementSpeed = 3.0;
		
		//items
		int8_t ammo = 30; //to do move ammo into inventory
		int8_t keys = 3;
		enum class ItemType : uint8_t {
			NONE = 0, //Empty hand or fist
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
			ItemOne,    //item that does notthing. meant to be unused. to be useful for
			            //debuggig.
			PassiveItem,
			Ammo,
			Boots,
			Sneakers,
			ItemEnd,
			WeaponStart = NONE,
			WeaponEnd = ItemOne
		};

		using InventorySlotsRaw = int8_t;
		enum InventorySlots : InventorySlotsRaw {
			//note When a player picks up an item it goes to the lowest inventory slot
			//number that is empty
			//to do remove hands start and end, same for packets, keep NumOf tho
			//replace them with hot swapables
			//Note: largest 4bit num = 15 - 2 hands = 13 packets max.
			InventoryStart = 0,
			HotBarStart = 0,
			HandsStart = 0,
			NumOfHands = 2,
			HandsEnd = HandsStart + NumOfHands,
			DominantHand = HandsStart, //hand used for one handed weapons
			NonDominantHand = HandsStart + 1,
			PacketsStart = HandsEnd,
			MaxPacketsSize = 6,
			PacketsEnd = PacketsStart + MaxPacketsSize,
			HotBarSize = NumOfHands + MaxPacketsSize,
			HotBarEnd = HandsStart + HotBarSize,
			BackPackStart = PacketsEnd,
			BackPackSize = 8,
			BackPackEnd = BackPackStart + BackPackSize,
			MaxInventorySize = BackPackEnd,
			InventoryEnd = MaxInventorySize
		};

		ItemType inventory[InventorySlots::MaxInventorySize] = { ItemType::NONE };
		int8_t numOfPackets = 3;
		//item slots
		//the edges of item slots have advantages
		//n slots away from some items have advantages
		static constexpr InventorySlotsRaw fourBitMask = 0b1111;
		static constexpr int handBitOffset = 4;
		static constexpr InventorySlotsRaw handBitMask = fourBitMask << handBitOffset;

		//input's hand slot can be different then actual hand slot 
		uint8_t dominantHandSlot : 4;
		uint8_t nondominantHandSlot : 4;

		struct FourBitMask {
			const int offset;
			const InventorySlotsRaw value;
			constexpr FourBitMask(InventorySlots hand) :
				offset(hand == InventorySlots::DominantHand ? handBitOffset : 0),
				value(fourBitMask << offset)
			{}
		};

		static constexpr InventorySlotsRaw getNewHandSlot(
			InventorySlots whichHand, TheWarrenState::InputType& input,
			InventorySlotsRaw slot
		) {
			const FourBitMask handBitMask{whichHand};

			slot = (slot & fourBitMask) << handBitMask.offset; //shift into place
			InventorySlotsRaw slots = input.handSlots; //2 4-bit slots
			slots &= ~handBitMask.value; //set target bits to 0
			slots |= slot;
			return slots;
		}

		static constexpr void setHandSlot(
			InventorySlots whichHand, TheWarrenState::InputType& input,
			InventorySlotsRaw slot
		) {
			input.handSlots = getNewHandSlot(whichHand, input, slot);
		}

		static constexpr InventorySlotsRaw getHandSlot(
			InventorySlots whichHand, const TheWarrenState::InputType& input
		) {
			const FourBitMask handBitMask{whichHand};
			return (input.handSlots & handBitMask.value) >> handBitMask.offset;
		}

		static constexpr void setDominantHandSlot(
			TheWarrenState::InputType& input, InventorySlotsRaw slot
		) {
			setHandSlot(InventorySlots::DominantHand, input, slot);
		}

		static constexpr InventorySlotsRaw getDominantHandSlot(TheWarrenState::InputType& input) {
			return getHandSlot(InventorySlots::DominantHand, input);
		}

		const ItemType getMainWeapon() {
			return inventory[dominantHandSlot];
		}

		const ItemType getOtherWeapon() {
			return inventory[InventorySlots::NonDominantHand];
		}

		static void emptyAddToInvCallback(bool) {}
		template<class Callback = decltype(emptyAddToInvCallback)>
		//to do use a const reference to an item
		void moveIntoInventory(ItemType& item, Callback callback = emptyAddToInvCallback) {
			std::array<ItemType, 1> lookFor = { ItemType::NONE };
			auto firstAvailable = std::find_first_of(
				std::begin(inventory), std::end(inventory),
				lookFor.begin(), lookFor.end());
			bool canMove = firstAvailable != std::end(inventory);
			if (canMove) {
				//swap with empty
				*firstAvailable = item;
				item = ItemType::NONE;
			}
			callback(canMove);
		}

		bool canPay(ItemType item) const;
		void buyItem(std::size_t index);

		//input modes
		//makes inputs do different things when in different modes
		enum class InputMode : int8_t {
			NONE = 0,
			Inventory = 1,
			Shop = 2,
			end
		};
		InputMode inputMode = InputMode::NONE;
		//stores the last button flags
		//needed in order to know if they are holding a button and
		//when they let go of a button
		uint8_t actionFlags = 0;

		//shop
		static constexpr int maxShopSize = 3;
		ItemType shop[maxShopSize] = { ItemType::NONE };

		static std::size_t getShopSelectionIndex(const TheWarrenState::Command& input) {
			//map atan2 to 0 to 2 pi
			constexpr double doublePi = (2 * M_PI);
			constexpr double startAngle = -1 * M_PI;
			double mapedAngle = std::fmod(
				input.rotation + startAngle + doublePi,
				doublePi);
			return static_cast<int>((mapedAngle) /
				(doublePi / static_cast<double>(Player::maxShopSize)));
		}
	};
	Player players[maxPlayerCount] = {};
	PlayerIndex playerRankings[maxPlayerCount] = { 0 };
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
	static constexpr int entitiesSize = 64;
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
		for (auto wall : walls) {
			b2PolygonShape wallBox;
			wallBox.SetAsBox(wall.scale.x / 2, wall.scale.y / 2);
			b2BodyDef wallBodyDef;
			wallBodyDef.position.Set(wall.position.x, wall.position.y);
			world.CreateBody(&wallBodyDef)->
				CreateFixture(&wallBox, 0.0f);
		}

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

		//to do clean up dup code
		b2CircleShape entityShape;
		entityShape.m_radius = 0.001f;
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

		{
			int i = 0;
			for (auto& playerIndex : playerRankings) {
				playerIndex = i;
				i += 1;
			}
		}
	}

	void update(double deltaTime);

	double time = 0;
	int tick = 0;
private:
};

template<>
struct metaB2Data<B2DataType::Character> {
	using Type = TheWarrenState::Player;
};

template<>
struct metaB2Data<B2DataType::Entity> {
	using Type = TheWarrenState::Entity;
};

struct ItemStats {
	const char * name = "Untitled Item";
	static constexpr auto priceless = std::optional<int> {};
	std::optional<int> costKeys = priceless;
	const char * description = "";
	int numOfHandRequiredToUse = 0;
};

using ItemData = std::array<ItemStats, 
	static_cast<std::size_t>(TheWarrenState::Player::ItemType::ItemEnd)>;

constexpr ItemData generateItemData() {
	ItemData data;
	//to do add operator overloads to itemtype
	data[static_cast<std::size_t>(TheWarrenState::Player::ItemType::NONE)].name
		= "Empty";
	data[static_cast<std::size_t>(TheWarrenState::Player::ItemType::Bow)] = {
		"Basic Bow", 1, "Launch arrows at targets. The longer fire button is held, "
		"the faster the arrow flys and the more damage it'll do.",
		//to do experament with one hand for bow and another for different types of arrows
		2
	};
	data[static_cast<std::size_t>(TheWarrenState::Player::ItemType::ShortSword)] = {
		"Basic Short Sword", 1, "Does damage when in contant with another.", 1
	};
	data[static_cast<std::size_t>(TheWarrenState::Player::ItemType::Bomb)] = {
		"Basic Explosive Grenade", 1, "5 seconds after thrown, damage will be dealt "
		"in an aera.", 1
	};
	return data;
}

constexpr ItemData itemData = generateItemData();

bool TheWarrenState::Player::canPay(TheWarrenState::Player::ItemType item) const {
	const ItemStats& itemStats = itemData[static_cast<std::size_t>(item)];
	return itemStats.costKeys != ItemStats::priceless
		&& *itemStats.costKeys <= keys;
}

void TheWarrenState::Player::buyItem(std::size_t index) {
	TheWarrenState::Player::ItemType& item = shop[index];
	const ItemStats& itemStats = itemData[static_cast<std::size_t>(item)];
	if(canPay(item)) {
		moveIntoInventory(item, [&](bool canMove) {
			if (canMove)
				keys -= *itemStats.costKeys;
		});
	}
}

void TheWarrenState::update(double deltaTime) {
	const auto isPlayerDead = [&](Player& player) {
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
		b2Data.type = B2DataType::Character;
		b2Data.data = &player;
		body.SetUserData(&b2Data);

		if (Snowflake::isAvaiable(playerInput.author)) {
			float velocity[2] = { 0.0f };
			switch (player.inputMode) {
			case Player::InputMode::NONE: {
				//handle buttons
				//active button events
				//to do clean this up by placing it near other attack
				//stage code
				if (playerInput.actionFlags & 1) {
					switch (player.attackStage) {
					case Player::AttackStage::NONE:
						player.attackStage = Player::AttackStage::GettingReady;
						break;
					default: break;
					}
				} else {
					switch (player.attackStage) {
					case Player::AttackStage::GettingReady:
						player.attackStage = Player::AttackStage::Attacking;
						//to do have a begin attack event
						break;
					}
				}

				int axisIndex = 0;
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
				break;
			}
			
			case Player::InputMode::Shop: {
				if (0 < player.royleHealth && playerInput.actionFlags & 1) {
					int selction = Player::getShopSelectionIndex(playerInput);
					player.buyItem(selction);
				}
				break;
			}

			default:
				break;
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

			//set hand slots
			//to do disable when attacking and when switching change state to return
			player.dominantHandSlot = Player::getDominantHandSlot(playerInput);
			player.nondominantHandSlot = Player::getHandSlot(
				Player::InventorySlots::NonDominantHand, playerInput);
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
		b2Data.type = B2DataType::Entity;
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
				if (0.0f < entity.height && entity.veocity[Axis::X] != 0.0f &&
					entity.veocity[Axis::Y] != 0.0f)
					//to do needs better way of detecting if it should apply gravity
					entity.height -= 4.0f * deltaTime;
			break;
			default: break;
			}
			fixture.SetSensor(false);
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

	//handle contact events
	const auto damagePlayer = [&](Player& player, const float amount) {
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
			const auto internalContactHandler = [&](b2Fixture* fixtureA, b2Fixture* fixtureB) {
				void* userDataA = (void*)fixtureA->GetBody()->GetUserData().pointer;
				void* userDataB = (void*)fixtureB->GetBody()->GetUserData().pointer;
				if (userDataA == nullptr)
					return;

				BaseB2Data& baseUserDataA = *static_cast<BaseB2Data*>(userDataA);

				switch(baseUserDataA.type) {
				case B2DataType::NA: break;
				case B2DataType::Character:
					
				break;
				case B2DataType::Entity: {
					//It's important you get the pointer related stuff right
					//or else it will break everything. this stuff is pretty
					//dangerous, so be careful.
					auto& entityData = *static_cast<B2Data<TheWarrenState::Entity>*>(userDataA);
					auto& entity = entityData.getData();

					switch (entity.type) {
					case TheWarrenState::Entity::Type::Arrow: {
						//figure out weather or not the arrow was moving towards what it went
						//into contact wtih
						//to do move this into arrow or something
						bool wasMovingTowardsOther;
						const b2Vec2 velocity = fixtureA->GetBody()->GetLinearVelocity();
						b2Vec2 othersVelocity = fixtureB->GetBody()->GetLinearVelocity();
						if (othersVelocity.x == 0.0f && othersVelocity.y == 0.0f) {
							wasMovingTowardsOther = true;
						} else {
							b2WorldManifold worldManifold;
							contact->GetWorldManifold(&worldManifold);
							const float normalAngle = std::atan2(worldManifold.normal.x,
								worldManifold.normal.y);
							const float velocityAngle = std::atan2(velocity.x, velocity.y);
							float phi = std::fmod(std::abs(normalAngle - velocityAngle), M_PI);
							float angleDistance = phi > M_PI_2 ? M_PI - phi : phi;
							//velocityAngle should be the opposite of normalAngle if moving
							//towards other object. We should count anything greater then
							//half of pi or 90 degress to be moving towards objects with
							//a little big of give.
							wasMovingTowardsOther = M_PI_2 - 0.1 < angleDistance;
							//half of pi would be 90 degrees but we want 
						}

						//check if arrow is on a wall
						bool isOnWall = false;
						for (
							b2ContactEdge* otherContact =
								fixtureA->GetBody()->GetContactList();
							otherContact != nullptr; otherContact = otherContact->next
						) {
							if (!otherContact->contact->IsTouching())
								continue;
							const auto check = [&](void* userData) {
								if (userData == nullptr) {
									return true;
								}
								return false;
							};
							isOnWall = check(
								(void*)otherContact->contact->GetFixtureA()->
									GetBody()->GetUserData().pointer);
							if (isOnWall) break;
							isOnWall = check(
								(void*)otherContact->contact->GetFixtureB()->
									GetBody()->GetUserData().pointer);
							if (isOnWall) break;
						}

						if(auto characterPtr = castB2Data<B2DataType::Character>(userDataB)) {
							auto& character = characterPtr.get();
							//check if the arrow is moving at all
							int movingOnAxisCount = 0;
							for (float& axis : entity.veocity) {
								//it's possable for the veocity to be very small
								//but not appear moving on screen
								movingOnAxisCount += 0.1 < std::abs(axis) ? 1 : 0;
							}
							if (movingOnAxisCount == 0) {
								//pick up arrow
								character.ammo += 1;
								entity = TheWarrenState::Entity{};
							} else if (wasMovingTowardsOther && !isOnWall) {
								//planned feature, more damage from speed reletive to character
								/*const float damage = 5.0f * std::hypot(
									entity.veocity[Axis::X],
									entity.veocity[Axis::Y]);*/
								const float damage = 50.0f;
								damagePlayer(character, damage);
								entity = TheWarrenState::Entity{};
							}
							//to do check if the player hit the back/sides of the arrow
							//if they did, they should move the arrow with the player
						}

						//stops arrow without being stuck in wall
						if (!wasMovingTowardsOther)
							break;
						for (float& axis : entity.veocity) {
							axis = 0.0;
						}
					} break;

					case TheWarrenState::Entity::Type::PickUp:
						if(auto characterPtr = castB2Data<B2DataType::Character>(userDataB)) {
							auto& character = characterPtr.get();
							character.movementSpeed *= 2.0f;
						}
						entity = TheWarrenState::Entity{};
					break;
					
					default:
						entity = TheWarrenState::Entity{};
					break;
					}

				} break;

				default: break;
				}
			};
			internalContactHandler(contact->GetFixtureA(), contact->GetFixtureB());
			internalContactHandler(contact->GetFixtureB(), contact->GetFixtureA());
		}
	}

	const auto getNewEntity = [&]() -> Entity& {
		entitiesEnd = entitiesEnd % entitiesSize;
		Entity& target = entities[entitiesEnd];
		entitiesEnd = entitiesEnd + 1;
		return target;
	};

	const auto respawn = [&](Player& player) {
		const int spawnDistence = 
			static_cast<int>(phase) & static_cast<int>(Phase::BattleRoyle) ?
			2 / roundNum : 2 * roundNum;
		int axisIndex = 0;
		for (float& axis : player.position) {
			//to do find a better way to get spawn placement
			//planned feature spread spawns using the above
			//axis = spawnDistence * player.index;
			axis = playerSpawns[player.index][axisIndex];
			axisIndex += 1;
		}
		if (
			static_cast<int>(phase) & static_cast<int>(Phase::WarmUp) ||
			0 < player.royleHealth
		)
			player.health = 100.0f;
		player.attackStage = Player::AttackStage::NONE;
	};

	const auto switchToDungeonCrawl = [&]() {
		phase = Phase::DungeonCrawl;
		phaseTimer = 10.0; //placeholder values
		for (auto& player : players) {
			//to do use a bag system, where once a player buys an item
			//it's taken out of the bag and selling adds it back to the
			//bag
			player.shop[0] = Player::ItemType::ShortSword;
			player.shop[1] = Player::ItemType::Bow;
			player.shop[2] = Player::ItemType::Bomb;
			player.inputMode = Player::InputMode::Shop;
		}
	};

	phaseTimer -= deltaTime;
	if (phaseTimer <= 0.0) {
		for (Entity& entity : entities) {
			entity.type = Entity::Type::NONE;
		}

		//time to switch modes
		switch (phase) {
		case Phase::WarmUp:
			switchToDungeonCrawl();
			for (Player& player : players) {
				if (Snowflake::isAvaiable(inputs[player.index].author)) {
					player.royleHealth = 3;
					player.ammo = 30;
				}
			}
			roundNum = 1;
		break;
		case Phase::DungeonCrawl: {
			//each dungeon should be bigger then the last
			phase = Phase::BattleRoyle;
			phaseTimer = 30.0;
			for (Player& player : players) {
				if (Snowflake::isAvaiable(inputs[player.index].author)) {
					player.inputMode = Player::InputMode::NONE;
				}
			}
		}
		break;
		case Phase::BattleRoyle:
			//each battleRoyle should be more dense with players then the last
			switchToDungeonCrawl();
			//switch to Dungeon Crawl if dead or last player alive
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
	const auto getDistance = [&](const Player& player, const float (&point)[2], int dimension) {
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

	int& oldPlayersLeft = playersLeft;
	int playersLeft = 0;
	int playerLeftInRound = 0;
	playerInputIndex = 0;
	for (Command& playerInput : inputs) {
		Player& player = players[playerInputIndex];
		if (static_cast<int>(phase) & static_cast<int>(Phase::DisablePlayers)) {
			//notthing
		} else if (!isPlayerDead(player)) { //to do use isAttacking
			//to do clean up this code 
			switch (player.attackStage) {
			case Player::AttackStage::GettingReady:
				player.readyLength += deltaTime;
				break;
			default: break;
			}

			//band-aid solution
			//to do, replace this with functions with names
			auto onAttackStageTimer = +[](Player&) {};

			//get the point where the sword is
			const float& angleA = playerInput.rotation;
			const float hypotanose = 1.0;
			float triangleSideLengths[3] = {
				std::sin(angleA), //A
				std::cos(angleA), //B
				1.0 //c : the length away from center of player
			};
			//to do use a switch statment
			switch (player.getMainWeapon()) {
			case Player::ItemType::ShortSword: {
				//passive effects
				const float playerSwordLocation[2] = {
					player.position[Axis::X] + (triangleSideLengths[0] * 1.0f),
					player.position[Axis::Y] + (triangleSideLengths[1] * 1.0f)
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

			case Player::ItemType::Bow: {
				//passive events
				//none

				//active effects
				switch (player.attackStage) {
				case Player::AttackStage::Attacking: {
					if (0 < player.ammo) {
						//spawn arrow
						Entity& arrow = getNewEntity();
						constexpr float maxReadyLength = 1.0f;
						float force = (maxReadyLength < player.readyLength) ?
							maxReadyLength : player.readyLength;
						force *= 2.0f;

						arrow.type = Entity::Type::Arrow;
						arrow.veocity[Axis::X] = triangleSideLengths[Axis::X] * force * 12.0;
						arrow.veocity[Axis::Y] = triangleSideLengths[Axis::Y] * force * 12.0;
						//add veocity of player because that's how it works in real life
						//and prevents bugs
						b2Vec2& playerVeocity = playerVelocities[player.index];
						arrow.veocity[Axis::X] += playerVeocity.x;
						arrow.veocity[Axis::Y] += playerVeocity.y;
						//spawn arrow from the player
						//spawning from the bow causes issues if the bow is inside something
						arrow.position[Axis::X] =
							player.position[Axis::X] + (triangleSideLengths[Axis::X]
							* (/*player radius*/0.5 + /*arrow size*/0.002f));
						arrow.position[Axis::Y] =
							player.position[Axis::Y] + (triangleSideLengths[Axis::Y]
							* (/*player radius*/0.5 + /*arrow size*/0.002f));

						player.ammo -= 1;
					}

					onAttackStageTimer = [](Player& player) {
						switch(player.attackStage) {
							case Player::AttackStage::ReturningToNone:
							//reload arrow
							if (0 < player.ammo) {
								//can reload
								//player.attackStageTimer = 1.0f;
								player.attackStageTimer = 1.0f/3.0f;
							} else {
								//can't reload
								player.attackStageTimer = 0.0f;
							}
						}
					};
					break;
				}
				}
				break;
			}

			default: break;

			}

			player.attackStageTimer -= deltaTime;
			if(player.attackStageTimer <= 0.0f && 
				player.attackStage == Player::AttackStage::Attacking)
			{
				player.attackStage = Player::AttackStage::ReturningToNone;
				onAttackStageTimer(player);
			}
			//check attackStageTimer again as the above can change it
			if (player.attackStageTimer <= 0.0f &&
				player.attackStage == Player::AttackStage::ReturningToNone)
			{
				player.attackStage = Player::AttackStage::NONE;
				player.readyLength = 0.0;
				onAttackStageTimer(player);
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
		playerLeftInRound += 0 < player.health ? 1 : 0;

		playerInputIndex += 1;
	}

	oldPlayersLeft = playersLeft;

	const auto comparePlayers = [&](PlayerIndex& a, PlayerIndex& b) {
		return players[a].royleHealth < players[b].royleHealth;
	};

	if (
		!(static_cast<int>(phase) & static_cast<int>(Phase::WarmUp))
		&&
		playersLeft <= 1
	) {
		phase = static_cast<Phase> (
			static_cast<int>(Phase::EndGame) | static_cast<int>(Phase::DisablePlayers)
		);
	}
	if ((static_cast<int>(phase) & static_cast<int>(Phase::BattleRoyle))
		&&
		playerLeftInRound <= 1
	) {
		phaseTimer = 0; //switch phase in next tick
	}
}

using GameState = TheWarrenState;
using GameServer = Core<GameState, true>;
using GameClient = Core<GameState, false>;