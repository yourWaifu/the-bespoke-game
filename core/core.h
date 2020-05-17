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
	int tick; //the tick when this data was sent
	int timestamp; //needed to calulate ping
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

	const static int numOfStoredStates = 0b100000;
	const static int storedStatesMask = 0b11111;
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
			updateClient(header.tick, header.timestamp, newState.data);
		} break;
		case PacketHeader::HELLO: {
			PackagedData<Snowflake::RawSnowflake> newID;
			memcpy(&newID, message.data(), sizeof(newID));
			onClientReady(newID.data);
		} break;
		case PacketHeader::PLAYER_INPUT: {
			PackagedData<GameState::InputType> newInput;
			memcpy(&newInput, message.data(), sizeof(newInput));
			onInput(header.tick, newInput.data.author, newInput.data);
		} break;
		}
	}

	inline const int getCurrentTick() {
		return lastTick + currentTickOffset;
	}

	void update(double deltaTime) {
		if constexpr (isServer) {
			int currentTick = getCurrentTick();

			int previousStateIndex = currentStateIndex;
			//keep currentState under 0b100000
			currentStateIndex = currentTick & storedStatesMask;

			//copy previous state to current state
			GameState& currentState = states[currentStateIndex];
			currentState = states[previousStateIndex];
			currentState.update(deltaTime);

			lastTick = currentTick; //this also sets the currect tick
		} else {
			//we need to check if the next tick has pasted
			static double timeSinceLastTick = 0.0;
			timeSinceLastTick += deltaTime;
			if ((1.0 / tickRate) <= timeSinceLastTick) {
				//when the client goes to the next tick, we only need to set the
				//currentStateIndex and copy over the previous state so that when
				//updates the state, it updates the new current state.
				const int previousStateIndex = currentStateIndex;
				currentTickOffset += 1;
				currentStateIndex = getCurrentTick() & storedStatesMask;
				states[currentStateIndex] = states[previousStateIndex];
				timeSinceLastTick -=
					static_cast<int>(timeSinceLastTick / (1.0 / tickRate)) *
					(1.0 / tickRate);
			}

			//for the client we will keep updating the current state
			//correction will be done when receving new data fromt the server
			states[currentStateIndex].update(deltaTime);
		}
	}

	//call this when the client gets states from the server
	void updateClient(int tick, int timestamp, const GameState& newState) {
		if constexpr (isServer)
			return;
		//the client should be a bit ahead by running preditcions
		//and we need to keep track of how much has pasted since the last tick
		//take the timestamp of the client and the timestamp from the server
		//get the difference and that should be where the current state is

		const auto previousIndex = lastTick & storedStatesMask;

		//get client's current time and compare to server's
		const auto clientTime = states[currentStateIndex].time;
		const auto serverTime = newState.time;
		const auto previousTime = states[previousIndex].time;
		const double deltaClientServerTime = serverTime < clientTime ?
			clientTime - serverTime : 0; //zero is a placeholder
		//to do replace the zero with a better way to calulate deltaTime

		//since packets are given out of order, we need to reject any that
		//are older then that last packet from the server
		//if (serverTime < previousTime && lastTick != 0)
		//	return;
		//or not

		const auto targetDeltaTime = 1.0 / tickRate;

		//since there a delay between the server senting the state and the client
		//receiving it, we need correct the state that was the current state when
		//the server sent the state and all the states after that.
		const auto pingTime = static_cast<int>(
			std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::system_clock::now().time_since_epoch()
				).count()) - timestamp;
		const int numOfReSim = static_cast<int>(
			deltaClientServerTime / targetDeltaTime
			//add to simulate being a head of the server
			) + 1;

		const auto newStateIndex = (currentStateIndex - numOfReSim)
			& storedStatesMask;

		//to do come up with a better way to sync up the server and client
		getUserInput(iD, states[newStateIndex],
			[=](InputType& input, int index) {
				GameState::InputType empty;
				if (memcmp(&empty, &input.movement, sizeof(empty.movement)) == 0 &&
					memcmp(&empty, &newState.inputs[index].movement, sizeof(empty.movement)) == 0
				) {
					//we might be able to update the state if the player isn't moving
					//however, we need to be careful as this cases rubberbanding
					states[newStateIndex] = newState;
				}
			}
		);

		const auto mergeInputs = [=](Snowflake::RawSnowflake iD, GameState& destination, const GameState& source) {
			//we need to get the user's inputs so that when we overwrite the
			//inputs, we can put back the user's input
			GameState::InputType input;
			getUserInput(iD, destination,
				[&input](InputType& oldStateInput, int index) {
					input = oldStateInput;
				}
			);

			//merge the other's input
			memcpy(&destination.inputs, &source.inputs, sizeof(GameState::inputs));

			getUserInput(iD, destination,
				[=](InputType& newStateInput, int index) {
					newStateInput = input;
				}
			);
		};

		int numOfTicksResimulated = 0;
		auto lastStateIndex = (newStateIndex - 1) & storedStatesMask;
		auto stateIndex = newStateIndex;
		auto NextstateIndex = (stateIndex + 1) & storedStatesMask;
		const GameState* replacement = &newState;
		for (
			;
			stateIndex != currentStateIndex;
			NextstateIndex = (NextstateIndex + 1) & storedStatesMask
		) {
			//we'll need delta time to get the state
			//double deltaTime = states[stateIndex].time - replacement->time;
			double deltaTime = targetDeltaTime;

			//for some reason, delta time can get really small, so limit it
			if (deltaTime < targetDeltaTime)
				deltaTime = targetDeltaTime;

			mergeInputs(iD, states[stateIndex], *replacement);

			states[stateIndex].time = replacement->time;
			if (0 <= deltaTime) { //deltaTime less then zero might crash the game
				states[stateIndex].update(targetDeltaTime); //to do use delta time
			}

			numOfTicksResimulated += 1;
			lastStateIndex = stateIndex;
			replacement = &states[lastStateIndex];
			stateIndex = NextstateIndex;
		}

		//set tick info to the new tick info
		//numOfTicksResimulated should be numOfReSim - 1;
		int currentTick = tick + numOfTicksResimulated + 1;
		currentTickOffset = currentTick - tick;

		//set current state to the new current state
		GameState& previousState = states[lastStateIndex];
		GameState& currentState = states[currentStateIndex];
		mergeInputs(iD, currentState, previousState);
		currentState.time = previousState.time;

		lastTick = tick;
	}
	
	void onClientReady(const Snowflake::RawSnowflake _iD) {
		if constexpr (isServer)
			return;
		//when the client connects to the server, the server should send back
		//an ID that the client stores to identify itself when sending inputs
		iD = _iD;
	}

	template<class Callback, class StateType>
	void getUserInput(Snowflake::RawSnowflake iD, StateType& state, Callback callback) {
		//We don't know what the index of the player input that the game will
		//use to read the inputs of a player, so we need to get that. The game
		//will loop though all the indexes and when it founds a player's input
		//that's the player's input index. However, that can be done during a
		//state update and not when input is given.
		auto foundIndex = inputIndexCache.find(iD);
		if (foundIndex != inputIndexCache.end()) {
			callback(state.inputs[foundIndex->second], foundIndex->second);
		}
		else {
			size_t i = 0;
			for (auto& currentStateInput : state.inputs) {
				if (currentStateInput.author == iD) {
					inputIndexCache[iD] = i;
					callback(currentStateInput, i);
				}
				i += 1;
			}
		}
	}

	void onInput(int tick, Snowflake::RawSnowflake iD, InputType input) {
		const auto setInput = [=](GameState& state) {
			getUserInput(iD, state, [&input](InputType& UserInput, int index) {
				UserInput = input;
			});
		};

		if constexpr (isServer) {
			const int currentTick = getCurrentTick();
			if (currentTick < tick)
				tick = currentTick;

			const int numOfReSim = currentTick - tick;
			if (storedStatesMask < numOfReSim)
				//not enough states stored to handle
				return;

			//since there a delay between the client senting the input and the server
			//receiving it, we need correct the state that was the current state when
			//the client sent the input and all the states after that.
			int stateIndex = (currentTick - numOfReSim) & storedStatesMask;
			for (
				int lastStateIndex = (stateIndex - 1) & storedStatesMask;
				stateIndex != currentStateIndex;
				stateIndex = (stateIndex + 1) & storedStatesMask
				) {
				GameState& stateBefore = states[lastStateIndex];
				GameState& state = states[stateIndex];
				setInput(state);
				//we'll need delta time to get the updated state
				double deltaTime = stateBefore.time - state.time;
				state.time = stateBefore.time;
				state.update(deltaTime);
				lastStateIndex = stateIndex;
			}
		}

		//we don't need to do the current state as that'll be done later
		GameState& currentState = states[currentStateIndex];
		setInput(currentState);
	}

	//for client use
	inline void onInput(InputType& input) {
		return onInput(getCurrentTick(), iD, input);
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
	//we can guess the currentTick by using an offset. For the server, this will always be 1.
	int currentTickOffset = 1;
	int tickRate = 120.0;
	Snowflake::RawSnowflake iD = 0;
	std::unordered_map<Snowflake::RawSnowflake, uint64_t> inputIndexCache;
};