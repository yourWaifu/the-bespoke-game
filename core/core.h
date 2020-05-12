#pragma once
#include "snowflake.h"
#include "input.h"
#include "nonstd/string_view.hpp"
#include <array>
#include <unordered_map>

enum Axis : int {
	X = 0,
	Y = 1,
	Z = 2,
	NumOf = 3,
};

struct PacketHeader {
	enum OperatorCode {
		GAME_STATE_UPDATE,
		IDENTIFY,
		HELLO,
		PLAYER_INPUT,
	};
	OperatorCode opCode;
};

template<class DataType>
struct PackagedData {
	PacketHeader header; //this needs to be first
	DataType data;
};

template<class _GameState, bool isServer = true>
class Core {
public:
	using GameState = _GameState;
	using InputType = typename GameState::InputType;

	//The client only needs the last state from the server, and the
	//current state.
	//While the server needs more states to be used for analying or
	//other features that down the line.
	const static int numOfStoredStates = isServer ? 0b100000 : 0b10;
	const static int storedStatesMask = isServer ? 0b11111 : 0b1;
	const GameState getCurrentState() {
		return states[currentStateIndex];
	}

	void onMessage(const nonstd::string_view message) {
		//message should be a packaged data, which starts with a header
		//c++ stores the first value in a struct first in memory.
		PacketHeader header;
		memcpy(&header, message.data(), sizeof(header));

		switch (header.opCode) {
		case PacketHeader::GAME_STATE_UPDATE: {
			PackagedData<GameServer::GameState> newState;
			memcpy(&newState, message.data(), sizeof(newState));
			updateClient(newState.data);
		} break;
		case PacketHeader::HELLO: {
			PackagedData<Snowflake::RawSnowflake> newID;
			memcpy(&newID, message.data(), sizeof(newID));
			onClientReady(newID.data);
		} break;
		case PacketHeader::PLAYER_INPUT: {
			PackagedData<GameState::InputType> newInput;
			memcpy(&newInput, message.data(), sizeof(newInput));
			onInput(newInput.data.author, newInput.data);
		} break;
		}
	}

	void update(double deltaTime) {
		if constexpr (isServer) {
			int currentTick = lastTick + 1;

			int previousStateIndex = currentStateIndex;
			currentStateIndex += 1;
			//keep currentState under 0b100000
			currentStateIndex = currentStateIndex & storedStatesMask;

			//copy previous state to current state
			GameState& currentState = states[currentStateIndex];
			currentState = states[previousStateIndex];
			currentState.update(deltaTime);

			lastTick = currentTick;
		} else {
			//for the client we will keep updating the current state
			//correction will be done when receving new data fromt the server
			states[currentStateIndex].update(deltaTime);
		}
	}

	//call this when the client gets states from the server
	void updateClient(const GameState& newState) {
		if constexpr (isServer)
			return;
		//the client should be a bit ahead by running preditcions
		//and we need to keep track of how much has pasted since the last tick
		//take the timestamp of the client and the timestamp from the server
		//get the difference and that should be where the current state is

		const auto previousIndex = (currentStateIndex - 1) & storedStatesMask;

		//get client's current time and compare to server's
		const auto clientTime = states[currentStateIndex].time;
		const auto serverTime = newState.time;
		const auto previousTime = states[previousIndex].time;
		double deltaTime = serverTime < clientTime ?
			clientTime - serverTime : 0; //zero is a placeholder
		//to do replace the zero with a better way to calulate deltaTime

		//since packets are given out of order, we need to reject any that
		//are older then that last packet from the server
		if (serverTime < previousTime)
			return;

		//in case the deltaTime is too large, we need to set it to something
		//that is less then or equal to the target deltaTime
		const auto lastServerDeltaTime = serverTime - previousTime;
		if (lastServerDeltaTime < deltaTime) {
			const int numOfLaps =
				static_cast<int>(deltaTime / lastServerDeltaTime);
			deltaTime -= numOfLaps * lastServerDeltaTime;
		}

		GameState& previousState = states[currentStateIndex];

		//go to the next index
		currentStateIndex += 1;
		currentStateIndex = currentStateIndex & storedStatesMask;

		GameState& currentState = states[currentStateIndex];
		previousState = newState;
		currentState = newState;
		currentState.update(deltaTime);
	}
	
	void onClientReady(const Snowflake::RawSnowflake _iD) {
		if constexpr (isServer)
			return;
		//when the client connects to the server, the server should send back
		//an ID that the client stores to identify itself when sending inputs
		iD = _iD;
	}

	void onInput(Snowflake::RawSnowflake iD, InputType input) {
		GameState& currentState = states[currentStateIndex];
		//We don't know what the index of the player input that the game will
		//use to read the inputs of a player, so we need to get that. The game
		//will loop though all the indexes and when it founds a player's input
		//that's the player's input index. However, that can be done during a
		//state update and not when input is given.
		auto foundIndex = inputIndexCache.find(iD);
		if (foundIndex != inputIndexCache.end()) {
			currentState.inputs[foundIndex->second] = input;
		} else {
			size_t i = 0;
			for (InputType& currentStateInput : currentState.inputs) {
				if (currentStateInput.author == iD) {
					inputIndexCache[iD] = i;
					currentStateInput = input;
				}
				i += 1;
			}
		}
	}

	//for client use
	inline void onInput(InputType input) {
		return onInput(iD, input);
	}

	void onNewPlayer(Snowflake::RawSnowflake iD) {
		GameState& currentState = states[currentStateIndex];
		//find an avaiable slot for the player
		size_t i = 0;
		for (InputType& stateInput : currentState.inputs) {
			//isAvaiable tells if the author is avaiable and accessable
			//when this is false, it means it's not being used up because
			//that means there's no author
			if (!Snowflake::isAvaiable(stateInput.author)) {
				inputIndexCache[iD] = i;
				stateInput.author = iD;
				return;
			}
			i += 1;
		}
	}

	inline const Snowflake::RawSnowflake getID() {
		return iD;
	}
private:
	int currentStateIndex = 0;
	std::array<GameState, numOfStoredStates> states;
	//current tick is unknown by the last tick from the server is known
	int lastTick = 0; //if the game is at 60 ticks, it should take over a 1 year to overflow+
	Snowflake::RawSnowflake iD = 0;
	std::unordered_map<Snowflake::RawSnowflake, uint64_t> inputIndexCache;
};