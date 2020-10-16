#pragma once
#include "core.h"
#include <iostream>
#include <tuple>
#include <vector>
#include <cmath>
#include <bitset>
#include <algorithm>
#include <optional>
#include <tuple>
#include "nonstd/string_view.hpp"
#include "box2d/box2d.h"
//v8
#if 1
	#include "javascript.h"
#endif
//#include "console_server.h"

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

//my version of dynamic span from c++20 with different features
//to do add a max size limit or make it circular
template<class ArrayType>
struct ArrayRegin {
	using array_type = ArrayType;
	using value_type = typename ArrayType::value_type;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;

	using iterator = int;

	iterator first = 0;
	iterator last = static_cast<iterator>(std::tuple_size<ArrayType>::value);

	constexpr const size_type size() const {
		return last - first;
	}
};

template<class ArrayRegin>
struct SubArray {
	using size_type = std::size_t;
	using array_type = typename ArrayRegin::array_type;
	using pointer = typename ArrayRegin::array_type::pointer;
	using const_pointer = typename ArrayRegin::array_type::const_pointer;
	using reference = typename ArrayRegin::array_type::reference;
	using const_reference = typename ArrayRegin::array_type::const_reference;
	using value_type = typename ArrayRegin::value_type;
	using iterator = typename ArrayRegin::array_type::iterator;
	using const_iterator = typename ArrayRegin::array_type::const_iterator;

	array_type& array;
	ArrayRegin& regin;

	constexpr reference operator[](size_type pos) {
		return array[regin.first + pos];
	}
	constexpr const_reference operator[](size_type pos) const {
		return array[regin.first + pos];
	}

	constexpr reference back() {
		return operator[](regin.size() - 1);
	}
	constexpr const_reference back() const {
		return operator[](regin.size() - 1);
	}

	constexpr pointer data() {
		return &array[regin.first];
	}
	constexpr const_pointer data() const {
		return &array[regin.first];
	}

	constexpr iterator begin() {
		return array.begin() + regin.first;
	}
	constexpr const_iterator begin() const {
		return array.begin() + regin.first;
	}

	constexpr iterator end() {
		return array.begin() + regin.last;
	}
	constexpr const_iterator end() const {
		return array.begin() + regin.last;
	}

	constexpr bool empty() const {
		return regin.size() == 0;
	}

	constexpr size_type size() const {
		return regin.size();
	}

	constexpr void pop() {
		reference value = back();
		value = {};
		regin.last -= 1;
	}

	void push(value_type&& value) {
		//move to last and change size
		reference newValue = *end();
		newValue = std::move(value);
		regin.last += 1;
	}
};

using PlayerIndex = int8_t;
constexpr PlayerIndex maxPlayerCount = 0b100;

struct Inputs {
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

	//4 * 32 btyes is 128 btyes
	Command userInput[maxPlayerCount] = {};
	//static const int maxConsoleCommands = 0b1;
	//using ConsoleCommands = TheWarrenState::Array<
	//	ConsoleCommand, maxConsoleCommands>;
	//ConsoleCommands consoleCommands;

	constexpr Command& operator[](std::size_t pos) {
		return userInput[pos];
	}

	constexpr const Command& operator[](std::size_t pos) const {
		return userInput[pos];
	}

	constexpr Command* begin() noexcept {
		return userInput;
	}

	constexpr const Command* begin() const noexcept {
		return userInput;
	}

	constexpr Command* end() noexcept {
		return begin() + maxPlayerCount;
	}

	constexpr const Command* end() const noexcept {
		return begin() + maxPlayerCount;
	}

	//constexpr ConsoleCommands& getConsoleCommands() {
	//	return consoleCommands;
	//}
};

struct Player {
	using InputType = Inputs::Command;
	static constexpr PlayerIndex maxPlayerCount = ::maxPlayerCount;

	int index = 0;
	
	float position[Axis::NumOf] = {0};
	//int dimension = 0; //to do add this

	float health = 0;
	int8_t royleHealth = 0;

	const bool isDead() {
		return health <= 0.0;
	}
	
	const void applyDamage(const float amount) {
		health -= amount;
	}

	void applyLostRound() {
		royleHealth -= 1;
	}

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
	static constexpr InventorySlotsRaw handBitMask = static_cast<InventorySlotsRaw>(
		fourBitMask << handBitOffset);

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
		InventorySlots whichHand, InputType& input,
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
		InventorySlots whichHand, InputType& input,
		InventorySlotsRaw slot
	) {
		input.handSlots = getNewHandSlot(whichHand, input, slot);
	}

	static constexpr InventorySlotsRaw getHandSlot(
		InventorySlots whichHand, const InputType& input
	) {
		const FourBitMask handBitMask{whichHand};
		return (input.handSlots & handBitMask.value) >> handBitMask.offset;
	}

	static constexpr void setDominantHandSlot(
		InputType& input, InventorySlotsRaw slot
	) {
		setHandSlot(InventorySlots::DominantHand, input, slot);
	}

	static constexpr InventorySlotsRaw getDominantHandSlot(InputType& input) {
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

	bool canPay(const std::optional<int>& cost) const;
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
	int rerollCount = 0;

	//old unused function
	//to do remove this funciton, is being replaced by ShopUIWheel::getSelectionIndex
	static std::size_t getShopSelectionIndex(const Inputs::Command& input) {
		//map atan2 to 0 to 2 pi
		constexpr double doublePi = (2 * M_PI);
		constexpr double startAngle = -1 * M_PI;
		double mapedAngle = std::fmod(
			input.rotation + startAngle + doublePi,
			doublePi);
		return static_cast<int>((mapedAngle) /
			(doublePi / static_cast<double>(Player::maxShopSize)));
	}

	void rollShop(TheWarrenState&);

	//random
	//server should set the first random num to something from mt19937 
	uint64_t lastRandomNum = 0;
	uint32_t generateRandomNum(const int tick) {
		//another random prime
		lastRandomNum += (tick + index) * 2301121489U;
		lastRandomNum *= 11313288804317771849U;
		return static_cast<uint32_t>(lastRandomNum);
	}

	//experience and levels
	using Level = int8_t;
	using Exp = int32_t;
	static constexpr Level maxLevel = 10;
	Level level = 1;
	Exp exp = 0;

	bool gainExp(Exp gain);
};

struct ShopUIWheel {
	using Index = int;
	constexpr static Index begin = 0;

	constexpr static Index ItemsBegin = begin;
	constexpr static Index ItemsCount = Player::maxShopSize;
	constexpr static Index ItemsEnd = ItemsBegin + ItemsCount;
	//list actions here
	enum Actions : Index {
		ActionBegin = ItemsEnd,
		Roll = ActionBegin,
		ActionEnd,
	};
	constexpr static Index end = ActionEnd;

	struct SlotUI {
		const char* header;
		const char* description;
		std::optional<int> keyCount;
	};

	struct SlotEvent {
		static bool emptyGetUI(const Player&, Index, SlotUI&) {
			return false;
		}
		using GetUIFun = decltype(&emptyGetUI);
		GetUIFun getUI;
		
		static void emptyExecute(Player&, Index, TheWarrenState&) {}
		using ExecuteFun = decltype(&emptyExecute);
		ExecuteFun execute;
	};

	using SlotEvents = std::array<SlotEvent, end>;
	
	const std::array<SlotEvent, end> slotEvents;

	static constexpr Index getSelectionIndex(const float rotation) {
		//map atan2 to 0 to 1
		constexpr double startAngle = -1 * M_PI;
		constexpr double endAngle = M_PI;
		const double mapedAngle = (rotation - startAngle) / (endAngle - startAngle);
		return static_cast<int>((mapedAngle) /
			(1.0 / static_cast<double>(std::tuple_size<SlotEvents>::value))) %
			std::tuple_size<SlotEvents>::value;
	}

	static constexpr std::size_t getSize() {
		return std::tuple_size<SlotEvents>::value;
	}
};

struct ItemStats {
	const char * name = "Untitled Item";
	static constexpr auto priceless = std::optional<int> {};
	static constexpr uint tierCount = 5;
	std::optional<int> costKeys = priceless; //also used as item tier
	const char * description = "";
	static constexpr int handsFree = 0;
	static constexpr int oneHanded = 1;
	static constexpr int twoHanded = 2;
	int numOfHandsRequiredToUse = 0;
	using Flags = int;
	static constexpr Flags isInStock = 1 << 0;
	Flags flags = 0;

	constexpr int getTierIndex() const {
		return (*costKeys) - 1;
	}
};

using ItemData = std::array<ItemStats, 
	static_cast<std::size_t>(Player::ItemType::ItemEnd)>;

constexpr ItemData generateItemData() {
	ItemData data;
	//to do add operator overloads to itemtype
	data[static_cast<std::size_t>(Player::ItemType::NONE)].name
		= "Empty";
	data[static_cast<std::size_t>(Player::ItemType::Bow)] = {
		"Basic Bow", 1, "Launch arrows at targets. The longer fire button is held, "
		"the faster the arrow flys and the more damage it'll do.",
		//to do experament with one hand for bow and another for different types of arrows
		ItemStats::twoHanded, ItemStats::isInStock
	};
	data[static_cast<std::size_t>(Player::ItemType::ShortSword)] = {
		"Basic Short Sword", 1, "Does damage when in contant with another.",
		ItemStats::oneHanded, ItemStats::isInStock
	};
	data[static_cast<std::size_t>(Player::ItemType::Bomb)] = {
		"Basic Explosive Grenade", 1, "5 seconds after thrown, damage will be dealt "
		"in an aera.", ItemStats::oneHanded, ItemStats::isInStock
	};
	data[static_cast<std::size_t>(Player::ItemType::Boots)] = {
		"Basic Boots", 1, "+5 movement speed", ItemStats::handsFree,
		ItemStats::isInStock
	};
	return data;
}

constexpr ItemData itemData = generateItemData();

bool Player::canPay(const std::optional<int>& cost) const {
	return cost != ItemStats::priceless && *cost <= keys;
}

bool Player::canPay(Player::ItemType item) const {
	const ItemStats& itemStats = itemData[static_cast<std::size_t>(item)];
	return canPay(itemStats.costKeys);
}

void Player::buyItem(std::size_t index) {
	Player::ItemType& item = shop[index];
	const ItemStats& itemStats = itemData[static_cast<std::size_t>(item)];
	if(canPay(item)) {
		moveIntoInventory(item, [&](bool canMove) {
			if (canMove)
				keys -= *itemStats.costKeys;
		});
	}
}

const auto itemGetUI = [](const Player& player, ShopUIWheel::Index selection, ShopUIWheel::SlotUI& ui) {
	const auto itemID = player.shop[selection];
	if (itemID == Player::ItemType::NONE)
		return false;
	auto& data = itemData[int(itemID)];
	ui = ShopUIWheel::SlotUI{
		data.name,
		data.description,
		data.costKeys
	};
	return true;
};

const auto itemExecute = [](Player& player, ShopUIWheel::Index selection, TheWarrenState&) {
	player.buyItem(selection);
};

using ShopOdds = std::array<
	std::array<uint, ItemStats::tierCount>,
	Player::maxLevel>;
static constexpr ShopOdds shopOdds{{
	//placeholders taken from dota underlords
//Tiers
	//1   2   3   4   5
	{ 70, 30, 0,  0,  0 }, //Level 1
	{ 65, 35, 0,  0,  0 }, //2
	{ 50, 35, 15, 0,  0 }, //3
	{ 40, 35, 20, 0,  0 }, //4
	{ 35, 30, 30, 5,  0 }, //5
	{ 25, 30, 35, 10, 0 }, //6
	{ 23, 27, 35, 15, 0 }, //7
	{ 22, 25, 30, 20, 3 }, //8
	{ 15, 21, 28, 30, 6 }, //9
	{ 15, 20, 25, 30, 10}  //10
}};

using ShopOddsOffsets = std::array<
	std::array<uint, ItemStats::tierCount + 1>,
	Player::maxLevel>;

constexpr ShopOddsOffsets generateShopOddsOffsets() {
	ShopOddsOffsets target = {0};
	for (int levelIndex = 0; levelIndex < shopOdds.size(); levelIndex += 1) {
		auto& tierOdds = shopOdds[levelIndex];
		auto& tierOddsOffsets = target[levelIndex];
		ShopOdds::value_type::value_type offset = 0;
		tierOddsOffsets[0] = offset;
		for (int tierIndex = 0; tierIndex < tierOdds.size(); tierIndex += 1) {
			offset += tierOdds[tierIndex];
			tierOddsOffsets[tierIndex + 1] = offset;
		}
	}
	return target;
}

static constexpr ShopOddsOffsets shopOddsOffsets = generateShopOddsOffsets();

using ShopSupplyForEachItemTier = std::array<uint, ItemStats::tierCount>;

static constexpr ShopSupplyForEachItemTier initialTierSupplyPerUnit = {
	15, 10, 9, 6, 5 // this is per unit, so x copies of every tier[index] unit
};

constexpr ShopSupplyForEachItemTier generateShopSupplyForEachTier() {
	ShopSupplyForEachItemTier target = { 0 };
	for (const ItemStats& item : itemData) {
		if (item.costKeys == ItemStats::priceless)
			continue;
		const std::size_t index = item.getTierIndex(); //sub 1 as tier 1 is index 0
		target[index] += initialTierSupplyPerUnit[index];
	}
	return target;
}

static constexpr ShopSupplyForEachItemTier initialTierSupply = generateShopSupplyForEachTier();

constexpr std::size_t generateShopInventorySize() {
	std::size_t size = 0;
	for (const auto tierSupply : initialTierSupply) {
		size += tierSupply;
	}
	return size;
}

using StoreInventory = std::array<Player::ItemType, generateShopInventorySize()>;

using StoreInventoryTierBorders = std::array<ArrayRegin<StoreInventory>, initialTierSupply.size()>;

constexpr StoreInventoryTierBorders generateStoreInventoryTierBorders() {
	ArrayRegin<StoreInventory>::iterator prevousLast = 0;
	StoreInventoryTierBorders target = {};
	static_assert(target.size() == initialTierSupply.size());
	for(int index = 0; index < target.size(); index += 1) {
		ArrayRegin<StoreInventory>& regin = target[index];
		uint size = initialTierSupply[index];
		regin.first = prevousLast;
		regin.last = regin.first + size;
		prevousLast = regin.last;
	}
	return target;
}

constexpr StoreInventoryTierBorders storeInventoryTierBorders = generateStoreInventoryTierBorders();

constexpr StoreInventory generateInitialStoreInventory() {
	StoreInventory target = { Player::ItemType::NONE };
	std::array<uint, storeInventoryTierBorders.size()> currentPosIndies = {0};
	for (int index = 0; index < itemData.size(); index += 1) {
		const Player::ItemType itemType =
			static_cast<Player::ItemType>(index);
		const ItemStats& itemStats = itemData[index];
		if (itemStats.costKeys == itemStats.priceless)
			continue;
		const int tier = *(itemStats.costKeys);
		const int tierIndex = itemStats.getTierIndex();
		const int numOfCopies = initialTierSupplyPerUnit[tierIndex];
		uint& currentPosIndex = currentPosIndies[tierIndex];
		auto& tierArrayRegin = storeInventoryTierBorders[tierIndex];
		auto tierSubArray = SubArray<const StoreInventoryTierBorders::value_type>{
			target, tierArrayRegin };
		for (uint index = 0; index < numOfCopies; index += 1) {
			tierSubArray[currentPosIndex] = itemType;
			currentPosIndex += 1;
		}
	}
	return target;
}

constexpr StoreInventory initialStoreInventory = generateInitialStoreInventory();

const auto getRollUI = [](const Player&, ShopUIWheel::Index, ShopUIWheel::SlotUI& ui) {
	ui = ShopUIWheel::SlotUI{"Reroll Shop", ""};
	return true;
};

const auto rollExecute = [](Player& player, ShopUIWheel::Index, TheWarrenState& state) {
	player.rerollCount += 1;
	player.rollShop(state);
};

constexpr ShopUIWheel::SlotEvents generateSlotEvents() {
	ShopUIWheel::SlotEvents events = {};

	for (int i = 0; i < ShopUIWheel::ItemsCount; i += 1) {
		events[i] = ShopUIWheel::SlotEvent{itemGetUI, itemExecute};
	}

	events[ShopUIWheel::Actions::Roll] =
		ShopUIWheel::SlotEvent{getRollUI, rollExecute};
	
	return events;
}

constexpr ShopUIWheel shopInterface = { generateSlotEvents() };

using LevelRequirements = std::array<int, Player::maxLevel + 1>;
constexpr LevelRequirements levelRequirements {
	//min
	//1  2  3  4  5  6   7   8   9   10  mx
	{ 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20 }
	//mn 1  2  3  4  5   6   7   8   9   10
	//max
};

bool Player::gainExp(Player::Exp gain)  {
	exp += gain;

	//update level
	//min level is 1
	const auto oldLevel = level;
	if (level <= 0)
		level = 1;
	do {
		const Exp levelLength = levelRequirements[level] - levelRequirements[level - 1];
		if (levelLength <= exp) {
			if (Player::maxLevel <= level) {
				constexpr Exp lastLevelLength =
					levelRequirements[Player::maxLevel] -
					levelRequirements[Player::maxLevel - 1];
				exp = lastLevelLength;
				level = Player::maxLevel;
				break;
			}
			exp -= levelLength;
			level += 1;
		} else {
			break;
		}
	} while (true);
	
	return oldLevel < level;
}

//server and client side code
class TheWarrenState {
public:
	TheWarrenState() = default;

	//to do switch to std::array when better serilzation is avaiable
	template<class Type, std::size_t MaxLength>
	struct Array {
		using value_type = Type;
		using pointer = value_type*;
		using const_pointer = const value_type*;
		using reference = value_type&;
		using const_reference = const value_type&;
		using iterator = value_type*;
		using const_iterator = const value_type*;
		using size_type = std::size_t;
		using difference_type = std::ptrdiff_t;
		using reverse_iterator = std::reverse_iterator<iterator>;
		using const_reverse_iterator = std::reverse_iterator<const_iterator>;

		Type elements[MaxLength];

		constexpr reference operator[](std::size_t pos) {
			return elements[pos];
		}
		constexpr const_reference operator[](std::size_t pos) const {
			return elements[pos];
		}

		constexpr iterator begin() noexcept {
			return elements;
		}
		constexpr const_iterator begin() const noexcept {
			return elements;
		}

		constexpr pointer end() noexcept {
			return begin() + MaxLength;
		}
		constexpr const_pointer end() const noexcept {
			return begin() + MaxLength;
		}

		constexpr size_type size() const noexcept {
			return MaxLength;
		}
		constexpr size_type max_size() const noexcept {
			return MaxLength;
		}

		constexpr bool empty() const noexcept {
			return size() == 0;
		}

		constexpr reference front() noexcept {
			return *begin();
		}
		constexpr const_reference front() const noexcept {
			return elements[0];
		}

		constexpr reference back() noexcept {
			return MaxLength ? *(end() - 1) : *end();
		}
		constexpr const_reference back() const noexcept {
			return MaxLength ? elements[MaxLength - 1] :
				elements[0];
		}

		constexpr pointer data() noexcept {
			return elements;
		}
		constexpr const_pointer data() const noexcept {
			return elements;
		}
	};

	using InputType = Inputs::Command;
	using Inputs = ::Inputs;

	Inputs inputs;

	using Player = ::Player;
	Player players[maxPlayerCount] = {};
	PlayerIndex playerRankings[maxPlayerCount] = { 0 };
	int playersLeft = 0;

	void applyDamageToPlayer(Player& player, const float amount) {
		player.applyDamage(amount);
		if(player.isDead()) {
			//on other player's death
			if (static_cast<int>(phase) & static_cast<int>(Phase::BattleRoyle))
				//after death, they get a head start on their next Dungeon
				//After dying in BattleRoyle a few times, they are out of the game.
				//after being out of the game, the value of their items are dropped
				//in keys and some money.
				player.applyLostRound();
		}
	}

	struct Entity {
		enum class Type : int8_t {
			NONE = 0,
			Arrow = 1,
			PickUp = 2,
			Bomb = 3,
			Explosion = 4,
		};
		Type type = Type::NONE;
		float veocity[2];
		float position[2];
		//int dimension = 0; //to do add this
		float height = 3.5f; //to use half float
		float birthTime = 0.0f; //time of creation
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

	//StoreInventory
	StoreInventory storeInventory;
	StoreInventoryTierBorders storeInventorySections;

	template<class Core>
	void start(Core& core) {
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

		for (auto& player : players) {
			//getting 0xFFFFFFFF causes overflow issues
			player.lastRandomNum = core.template genRandInt<int>(0, 0xFFFFFFF0);
		}

		//set up Shop
		storeInventory = initialStoreInventory;
		storeInventorySections = storeInventoryTierBorders;
	}

	void update(const CoreInfo& coreInfo, double deltaTime);

	void restartShopInventory();

	bool hasGameEnded() {
		return false;
	} //can be called anyTime

//v8 related stuff
#if 1
	v8::Local<v8::ObjectTemplate> createStateObjectTemplate(v8::Isolate* isolate) {
		v8::Local<v8::ObjectTemplate> gameWrapper = v8::ObjectTemplate::New(isolate);
		gameWrapper->SetInternalFieldCount(1);
		gameWrapper->SetAccessor(v8::String::NewFromUtf8Literal(isolate, "phase"),
			[](v8::Local<v8::String> property,
			   const v8::PropertyCallbackInfo<v8::Value>& info) {
				v8::Local<v8::Object> self = info.Holder();
				v8::Local<v8::External> wrap =
					v8::Local<v8::External>::Cast(self->GetInternalField(0));
				TheWarrenState& gameState = *static_cast<TheWarrenState*>(
					wrap->Value());
				info.GetReturnValue().Set(static_cast<int>(gameState.phase));
			},
			[](v8::Local<v8::String> property, v8::Local<v8::Value> value,
			   const v8::PropertyCallbackInfo<void>& info) {
				v8::Local<v8::Object> self = info.Holder();
				v8::Local<v8::External> wrap =
					v8::Local<v8::External>::Cast(self->GetInternalField(0));
				TheWarrenState& gameState = *static_cast<TheWarrenState*>(
					wrap->Value());
				//to do write the rest of this function
			});
		return gameWrapper;
	}

	void updateStateObjectInstance(v8::Isolate* isolate, v8::Local<v8::Object> stateObject) {
		stateObject->SetInternalField(0, v8::External::New(isolate, this));
	}
#endif

	double time = 0;
	int tick = 0;
private:
};

template<>
struct metaB2Data<B2DataType::Character> {
	using Type = Player;
};

template<>
struct metaB2Data<B2DataType::Entity> {
	using Type = TheWarrenState::Entity;
};

void Player::rollShop(TheWarrenState& state) {
	auto& tick = state.tick;
	auto& storeInventorySections = state.storeInventorySections;
	auto& storeInventory = state.storeInventory;

	for (Player::ItemType& item : shop) {
		//pick two random numbers no matter what
		const auto firstRandomNum = generateRandomNum(tick);
		const auto secondRandomNum = generateRandomNum(tick);
		//if there is an item put it back
		auto itemIndex = static_cast<uint>(item);
		const ItemStats& itemStats = itemData[itemIndex];
		if (
			item != Player::ItemType::NONE &&
			itemStats.costKeys != ItemStats::priceless &&
			storeInventorySections[itemStats.getTierIndex()].size() <
				storeInventoryTierBorders[itemStats.getTierIndex()].size()
		) {
			auto tierSubArray = SubArray<StoreInventoryTierBorders::value_type>{
				storeInventory, storeInventorySections[itemStats.getTierIndex()]};
			tierSubArray.push(std::move(item));
		}
		//pick a tier based on level
		auto level = 0;
		const auto tierOddsOffsets = shopOddsOffsets[level]; //to do use player level
		auto randomOffset = firstRandomNum % tierOddsOffsets.back();
		//we could do a binary search here but it's just 5 tiers
		uint tempChosenTier = 0;
		const uint cantChooseTier = ItemStats::tierCount;
		for (uint index = 0; index < ( tierOddsOffsets.size() - 1 ); index += 1) {
			if(tierOddsOffsets[index] <= randomOffset && randomOffset < tierOddsOffsets[index + 1]) {
				tempChosenTier = index;
				break;
			}
		}
		//check the size of each tier and pick a differnt tier if empty
		{
			int direction = -1;
			const auto oldChosenTier = tempChosenTier;
			while(storeInventorySections[tempChosenTier].size() == 0) {
				if (tempChosenTier == 0) {
					direction = 1;
					tempChosenTier = oldChosenTier;
				} else if ((tempChosenTier + 1) == storeInventorySections.size()) {
					tempChosenTier = cantChooseTier; //can't choose
					break;
				}
				tempChosenTier += direction;
			}
		}

		if (tempChosenTier == cantChooseTier) {
			item = Player::ItemType::NONE;
		} else {
			const uint chosenTier = tempChosenTier;
			//create regin
			auto tierSubArray = SubArray<StoreInventoryTierBorders::value_type>{
				storeInventory, storeInventorySections[chosenTier]};
			//swap a random item with the last item in regin
			const auto chosenItemIndex = tierSubArray.size() == 1 ? // don't devide by 0
				0 : secondRandomNum % (tierSubArray.size() - 1);
			std::swap(tierSubArray.back(), tierSubArray[chosenItemIndex]);
			item = tierSubArray.back();
			//pop off from end of regin
			tierSubArray.pop();
		}
	}
}

void TheWarrenState::update(const CoreInfo& coreInfo, double deltaTime) {
	const auto isPlayerDead = [&](Player& player) {
		return player.isDead();
	};

	B2Data<Player> b2PlayerData[maxPlayerCount];
	b2Vec2 playerVelocities[maxPlayerCount];
	size_t playerInputIndex = 0;
	for (Inputs::Command& playerInput : inputs) {
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
					default: break;
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

					velocity[axisIndex] = moveOnAxis;
					axisIndex += 1;
				}
				//clamp to a circle of distence 1.0
				const float moveDistence = std::sqrt(
					(velocity[0]*velocity[0]) + (velocity[1]*velocity[1])
				);
				if (1.0 < moveDistence) {
					for (auto& axis : velocity) {
						axis = axis / moveDistence;
					}
				}
				//apply speed stats
				for (auto& axis : velocity) {
					axis *= player.movementSpeed;
				}
				break;
			}
			
			case Player::InputMode::Shop: {
				if (0 < player.royleHealth && !(playerInput.actionFlags & 1) && player.actionFlags & 1) {
					int selection = ShopUIWheel::getSelectionIndex(playerInput.rotation);
					shopInterface.slotEvents[selection].execute(player, selection, *this);
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

		//save last input to reference next tick
		player.actionFlags = playerInput.actionFlags;

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
			//to do make applyGravity and elasticity variables in item data
			//apply gravity
			switch (entity.type) {
			case Entity::Type::Arrow: case Entity::Type::Bomb:
				if (0.0f < entity.height && entity.veocity[Axis::X] != 0.0f &&
					entity.veocity[Axis::Y] != 0.0f)
					//to do needs better way of detecting if it should apply gravity
					entity.height -= 4.0f * deltaTime;
				else 
					entity.height = 0.0f; // 0.0 would cause z fighting
			break;
			default: break;
			}
			//apply elasticity
			switch (entity.type) {
			case Entity::Type::Bomb: fixture.SetRestitution(0.75); break;
			default: fixture.SetRestitution(0.0); break;
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
	for (Inputs::Command& playerInput : inputs) {
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
		applyDamageToPlayer(player, amount);
	};

	for (
		b2Contact* contact = world.GetContactList();
		contact != nullptr; contact = contact->GetNext()
	) {
		if (contact->IsTouching()) {
			const auto internalContactHandler = [&](b2Fixture* fixtureA, b2Fixture* fixtureB) {
				void* userDataA = fixtureA->GetBody()->GetUserData();
				void* userDataB = fixtureB->GetBody()->GetUserData();
				if (userDataA == nullptr)
					return;

				BaseB2Data& baseUserDataA = *static_cast<BaseB2Data*>(userDataA);

				switch(baseUserDataA.type) {
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
								otherContact->contact->GetFixtureA()->
									GetBody()->GetUserData());
							if (isOnWall) break;
							isOnWall = check(
								otherContact->contact->GetFixtureB()->
									GetBody()->GetUserData());
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
					
					default: break;
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
			player.gainExp(1);
			player.rollShop(*this);
			player.inputMode = Player::InputMode::Shop;
		}
	};

	//update game phase
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

	//update passive entity effects
	for (Entity& entity : entities) {
		switch (entity.type) {
		case Entity::Type::Bomb: {
			const float detonateTimerLength = 3.0;
			float detonateTime = entity.birthTime + detonateTimerLength;
			if (detonateTime < time) {
				//Detonate Bomb
				const b2Vec2 bombCenterPoint{entity.position[Axis::X], entity.position[Axis::Y]};
				const float blastRadius = 3.0;
				const b2Vec2 halfBlastScale{ blastRadius, blastRadius };
				b2AABB explosionBoundingBox;
				explosionBoundingBox.lowerBound = bombCenterPoint - halfBlastScale;
				explosionBoundingBox.upperBound = bombCenterPoint + halfBlastScale;

				struct DetonateBombCallback : public b2QueryCallback {
					const float blastRadius;
					//captured data
					const b2Vec2& bombCenterPoint;
					TheWarrenState& gameState;

					DetonateBombCallback(const float _radius, const b2Vec2& _center, TheWarrenState& _state) :
						blastRadius(_radius), bombCenterPoint(_center), gameState(_state) {}

					bool ReportFixture(b2Fixture* fixture) override {
						b2Body& body = *fixture->GetBody();
						if (body.GetUserData() == nullptr)
							return true; //can't dereference

						b2Vec2 bodyCenterPoint = body.GetWorldCenter();
						auto distenceFromBomb = (bodyCenterPoint - bombCenterPoint).Length();
						
						if (blastRadius <= distenceFromBomb)
							return true; //not in the blast circle
						
						BaseB2Data& base = *static_cast<BaseB2Data*>(body.GetUserData());
						switch (base.type) {
						case B2DataType::Character: {
							auto& data = static_cast<B2Data<Player>&>(base);
							auto& player = data.getData();
							//damage based on distence
							const float baseDamage = 120.0;
							const float damage = (1.0 - (distenceFromBomb/blastRadius)) * baseDamage;
							gameState.applyDamageToPlayer(player, damage);
							break;
						}
						default: break;
						}
						return true;
					}
				} queryCallback {blastRadius, bombCenterPoint, *this};
				world.QueryAABB(&queryCallback, explosionBoundingBox);
				entity.type = Entity::Type::Explosion; //for the renderer
				entity.birthTime = time;
				entity.veocity[Axis::X] = entity.veocity[Axis::Y] = 0.0;
			}
			break;
		}
		
		default:
			break;
		}
	}

	//update players
	int& oldPlayersLeft = playersLeft;
	int playersLeft = 0;
	int playerLeftInRound = 0;
	playerInputIndex = 0;
	for (Inputs::Command& playerInput : inputs) {
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
							default: break;
						}
					};
					break;
				}
				default: break;
				}
				break;
			}

			case Player::ItemType::Bomb: {
				switch (player.attackStage)
				{
				case Player::AttackStage::Attacking:
					//to do clean up dup code
					if (0 < player.ammo) {
						//spawn arrow
						Entity& arrow = getNewEntity();
						constexpr float maxReadyLength = 1.0f;
						float force = (maxReadyLength < player.readyLength) ?
							maxReadyLength : player.readyLength;
						force *= 2.0f;

						arrow.type = Entity::Type::Bomb;
						arrow.veocity[Axis::X] = triangleSideLengths[Axis::X] * force * 4.0;
						arrow.veocity[Axis::Y] = triangleSideLengths[Axis::Y] * force * 4.0;
						//add veocity of player because that's how it works in real life
						//and prevents bugs
						b2Vec2& playerVeocity = playerVelocities[player.index];
						arrow.veocity[Axis::X] += playerVeocity.x;
						arrow.veocity[Axis::Y] += playerVeocity.y;
						//spawn arrow from the player
						//spawning from the bow causes issues if the bow is inside something
						arrow.position[Axis::X] =
							player.position[Axis::X] + (triangleSideLengths[Axis::X]
							* (/*player radius*/0.5 + /*arrow size*/0.004f));
						arrow.position[Axis::Y] =
							player.position[Axis::Y] + (triangleSideLengths[Axis::Y]
							* (/*player radius*/0.5 + /*arrow size*/0.004f));

						arrow.birthTime = time;

						player.ammo -= 10;
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
							default: break;
						}
					};
					break;
				
				default:
					break;
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