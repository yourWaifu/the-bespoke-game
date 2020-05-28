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
		UNAVAILABLE,
		GAME_STATE_UPDATE,
		IDENTIFY,
		HELLO,
		PLAYER_INPUT,
	};
	OperatorCode opCode = UNAVAILABLE;
	int acknowledgedTick = 0; //the last tick acknowledged by senter
	int tick = 0; //the tick when this data was sent
	int timestamp = 0; //needed to calulate ping
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
	using InputsType = decltype(GameState::inputs);
	using PlayerType = typename GameState::Player;

	const static int numOfStoredStates = 0b100000;
	const static int storedStatesMask = 0b11111;
	const GameState& getCurrentState() {
		return states[currentStateIndex];
	}

	const GameState& getState(int tick) {
		const int index = (tick) & storedStatesMask;
		return states[index];
	}

	struct GameStateUpdate {
		GameState state;
		PlayerType ackPlayer; //the known player state at ackTick
	};

	void onMessage(const nonstd::string_view message) {
		//message should be a packaged data, which starts with a header
		//c++ stores the first value in a struct first in memory.
		PacketHeader header;
		memcpy(&header, message.data(), sizeof(header));

		switch (header.opCode) {
		case PacketHeader::GAME_STATE_UPDATE: {
			PackagedData<GameStateUpdate> newState;
			memcpy(&newState, message.data(), sizeof(newState));
			updateClient(header, newState.data);
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
			//set the last ack tick so that we can send it later
			auto foundData = playersData.find(newInput.data.author);
			if (foundData != playersData.end()) {
				foundData->second.ackTick = header.tick;
			}
		} break;
		}
	}

	inline const int getCurrentTick() {
			return lastTick + currentTickOffset;
	}

	inline const int getAckTick(Snowflake::RawSnowflake iD = 0) {
		if (iD == Snowflake::RawSnowflake{ 0 })
			//for the client
			return lastTick;

		//for the server
		int tick = 0;
		auto foundData = playersData.find(iD);
		if (foundData != playersData.end()) {
			tick = foundData->second.ackTick;
		}
		return tick;
	}

	void update(double deltaTime) {
		const auto targetDeltaTime = 1.0 / tickRate;
		if constexpr (isServer) {
			const int currentTick = getCurrentTick();

			int previousStateIndex = currentStateIndex;
			//keep currentState under 0b100000
			currentStateIndex = currentTick & storedStatesMask;

			//copy previous state to current state
			GameState& currentState = states[currentStateIndex];
			currentState = states[previousStateIndex];
			currentState.update(targetDeltaTime);

			lastTick = currentState.tick; //this also sets the object's currect tick
		} else {
			//we need to check if the next tick has pasted
			static double timeSinceLastTick = targetDeltaTime;
			timeSinceLastTick += deltaTime;
			if (targetDeltaTime <= timeSinceLastTick) {
				//when the client goes to the next tick, we only need to set the
				//currentStateIndex and copy over the previous state so that when
				//updates the state, it updates the new current state.

				const int ticks = timeSinceLastTick / targetDeltaTime;
				int previousStateIndex = currentStateIndex;
				const int oldState = previousStateIndex;
				const double oldStateTick = states[previousStateIndex].tick;

				//before we set the current tick, we need to get the final state
				//for what will be used as the previousState
				//to do remove dup code
				InputsType inputs;
				GameState& prevousState = states[(currentStateIndex - 1) & storedStatesMask];
				GameState& currentState = states[currentStateIndex];
				memcpy(&inputs, &currentState.inputs, sizeof(InputsType));
				currentState = prevousState;
				memcpy(&currentState.inputs, &inputs, sizeof(InputsType));
				currentState.update(targetDeltaTime);

				currentTickOffset += ticks;
				int nextStateIndex = getCurrentTick() & storedStatesMask;

				for (int index = (previousStateIndex + 1) & storedStatesMask;
					previousStateIndex != nextStateIndex;
					index = (index + 1) & storedStatesMask)
				{
					states[index] = states[previousStateIndex];
					previousStateIndex = index;
				}

				currentStateIndex = nextStateIndex;

				const int currentTick = states[previousStateIndex].time / targetDeltaTime;
				//sub 1 to get the tick's time before update
				const double time = oldStateTick * targetDeltaTime;

				auto lastStateIndex = oldState & storedStatesMask;
				auto stateIndex = (lastStateIndex + 1) & storedStatesMask;
				auto NextstateIndex = (stateIndex + 1) & storedStatesMask;
				for (
					;
					stateIndex != currentStateIndex;
					NextstateIndex = (NextstateIndex + 1) & storedStatesMask
					) {
					GameState& lastState = states[lastStateIndex];
					GameState& state = states[stateIndex];

					InputsType inputs;
					memcpy(&inputs, &state.inputs, sizeof(InputsType));
					state = lastState;
					memcpy(&state.inputs, &inputs, sizeof(InputsType));

					//state.time -= targetDeltaTime;
					state.update(targetDeltaTime);

					lastStateIndex = stateIndex;
					stateIndex = NextstateIndex;
				}
				timeSinceLastTick = fmod(timeSinceLastTick, targetDeltaTime);
				deltaTime = timeSinceLastTick; //for when we call update again
			}

			//for the client we will keep updating the current state
			//correction will be done when receving new data fromt the server

			InputsType inputs;
			GameState& prevousState = states[(currentStateIndex - 1) & storedStatesMask];
			GameState& currentState = states[currentStateIndex];
			memcpy(&inputs, &currentState.inputs, sizeof(InputsType));
			currentState = prevousState;
			memcpy(&currentState.inputs, &inputs, sizeof(InputsType));
			currentState.update(timeSinceLastTick);

			//the time of current state is misaligned with the targetDeltaTime
			//so we'll have issues when syncing the client with the server
			//remember to fix it before syncing with the server
		}
	}

	//call this when the client gets states from the server
	//this function should sync the server's and client's state
	void updateClient(const PacketHeader header, const GameStateUpdate& update) {
		if constexpr (isServer)
			return;

		//disable netcode
		//if (lastTick != 0)
		//	return;
		
		//disable lag compensation 
		//states[(currentStateIndex - 1) & storedStatesMask] = update.state;
		//states[currentStateIndex] = update.state;
		//return;

		const int ackTick = header.acknowledgedTick;
		const int tick = header.tick;
		const int timestamp = header.timestamp;

		const int deltaAckTick = tick - ackTick;

		//newState is the server's predicted game state
		const GameState& newState = update.state;
		//ack player is the current player's correct and known state
		const PlayerType& ackPlayer = update.ackPlayer;

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
		const int clientTimestamp = static_cast<int>(
			std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::system_clock::now().time_since_epoch()
				).count());
		const double pingTime = (clientTimestamp - timestamp) / 1000.0f;

		//calulate the number of resimlations for some reason, I don't remember why

		int tempNumOfReSim = 0;
		auto lastStateIndex = (currentStateIndex - 1) & storedStatesMask;
		auto stateIndex = currentStateIndex;
		if (deltaClientServerTime != 0) {
			for (
				;
				lastStateIndex != ((currentStateIndex + 1) & storedStatesMask);
				lastStateIndex = (stateIndex - 1) & storedStatesMask
				) {
				if ( states[stateIndex].tick <= newState.tick ) {
					break;
				}
				tempNumOfReSim += 1;
				stateIndex = lastStateIndex;
			}
		} else {
			tempNumOfReSim = pingTime / targetDeltaTime;
			stateIndex = getCurrentTick() - tempNumOfReSim - 1;
			stateIndex = stateIndex & storedStatesMask;
		}
		const int numOfReSim = tempNumOfReSim;

		const auto newStateIndex = newState.tick & storedStatesMask;

		const auto mergeInputs = [=](Snowflake::RawSnowflake iD, GameState& destination, const GameState& source) {
			//we need to get the user's inputs so that when we overwrite the
			//inputs, we can put back the user's input
			GameState::InputType input;
			getUserInput(iD, destination,
				[&input](InputType& oldStateInput, int index) {
					input = oldStateInput;
				}
			);

			memcpy(&destination, &source, sizeof(GameState));

			//merge the user's inputs
			getUserInput(iD, destination,
				[=](InputType& newStateInput, int index) {
					newStateInput = input;
				}
			);
		};

		//update current state info
		const auto lastTick = this->lastTick;
		const auto oldCurrentStateIndex = currentStateIndex;
		this->lastTick = newState.tick;
		//add one to be ahead of the server
		currentTickOffset = (pingTime / targetDeltaTime) + 1;
		const int nextCurrentStateIndex = getCurrentTick() & storedStatesMask;

		//before we make more changes to the states array, we need to fix it.
		//the client's updateState function updates the game with a veriable
		//delta time. However, the server does not and uses a constant delta-
		//time. so we need to decided the current state would be if the client
		//used a constant deltaTime before making possiable changes to it in
		//the future.

		//this code is reused a lot, you might want to make is this a funciton.
		InputsType inputs;
		GameState& prevousState = states[(oldCurrentStateIndex - 1) & storedStatesMask];
		GameState& currentState = states[oldCurrentStateIndex];
		memcpy(&inputs, &currentState.inputs, sizeof(InputsType));
		currentState = prevousState;
		memcpy(&currentState.inputs, &inputs, sizeof(InputsType));
		currentState.update(targetDeltaTime);

		//now that the states array is fix, we can start making changes to it
		//and the current state.
		currentStateIndex = nextCurrentStateIndex;

		//verify that ackPlayer is the same as the one client in states array
		//sub one because ackPlayer is from before the ackTick
		const int arkStateIndex = ackTick & storedStatesMask;
		GameState& arkState = states[arkStateIndex];
		getUserPlayer(iD, arkState,
			[=](PlayerType& player, int index) {
				//verfy player state
				if (memcmp(&player, &ackPlayer, sizeof(PlayerType)) != 0) {
					//not the same, we need to correct
					memcpy(&player, &ackPlayer, sizeof(PlayerType));
					auto lastStateIndex = arkStateIndex;
					for (int index = (lastStateIndex + 1) & storedStatesMask;
						lastStateIndex != newStateIndex;
						index = (index + 1) & storedStatesMask)
					{
						GameState& stateBefore = states[lastStateIndex];
						GameState& state = states[stateIndex];
						//we'll need delta time to get the updated state
						double deltaTime = state.time - stateBefore.time;

						//merge inputs and states
						InputsType inputs;
						memcpy(&inputs, &state.inputs, sizeof(InputsType));
						state = stateBefore;
						memcpy(&state.inputs, &inputs, sizeof(InputsType));

						state.update(deltaTime);
						lastStateIndex = index;
					}
				}
			}
		);

		//now we have the correct player state for the tick that the server sent
		//now we need to merge that to into the server tick
		GameState tempCorrectedState = newState;
		getUserPlayer(iD, tempCorrectedState,
			[=](PlayerType& player, int index) {
				const PlayerType& correctedPlayer = states[newStateIndex].players[index];
				memcpy(&player, &correctedPlayer, sizeof(GameState::Player));
			}
		);
		const GameState correctedState = tempCorrectedState;
		states[newStateIndex] = correctedState;

		int numOfTicksResimulated = 0;
		lastStateIndex = newStateIndex;
		stateIndex = (lastStateIndex + 1) & storedStatesMask;
		auto NextstateIndex = (stateIndex + 1) & storedStatesMask;
		const GameState* replacement = &correctedState;
		double replacementTime = replacement->time;
		auto stateTick = newState.tick;
		for (
			;
			//for some reason, not updating the currentState causes more problems
			lastStateIndex != currentStateIndex;
			NextstateIndex = (NextstateIndex + 1) & storedStatesMask
		) {
			//we can assume the delta time because the server should be using a
			//constant deltaTime
			const double deltaTime = targetDeltaTime;

			mergeInputs(iD, states[stateIndex], *replacement);

			states[stateIndex].time = replacementTime;
			states[stateIndex].update(targetDeltaTime);

			numOfTicksResimulated += 1;
			replacement = &states[stateIndex];
			replacementTime = states[stateIndex].time;
			lastStateIndex = stateIndex;
			stateTick += 1;
			stateIndex = NextstateIndex;
		}
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
		auto foundIndex = playersData.find(iD);
		if (foundIndex != playersData.end()) {
			auto index = foundIndex->second.inputIndex;
			callback(state.inputs[index], index);
		}
		else {
			size_t i = 0;
			for (auto& currentStateInput : state.inputs) {
				if (currentStateInput.author == iD) {
					playersData[iD] = { i, 0 };
					callback(currentStateInput, i);
				}
				i += 1;
			}
		}
	}

	template<class Callback, class StateType>
	void getUserPlayer(Snowflake::RawSnowflake iD, StateType& state, Callback callback) {
		getUserInput(iD, state, [&state, &callback](const InputType&, int index) {
			callback(state.players[index], index);
		});
	}

	void onInput(int tick, Snowflake::RawSnowflake iD, InputType input) {
		const auto setInput = [=](GameState& state) {
			getUserInput(iD, state, [&input](InputType& UserInput, int index) {
				UserInput = input;
			});
		};

		if constexpr (isServer) {
			//remove one to that we don't update the current state
			const int currentTick = states[currentStateIndex].tick;
			//reject any that's too late
			if (tick < currentTick - storedStatesMask)
				return;

			if (currentTick < tick)
				tick = currentTick;

			const int currentStateIndex = currentTick & storedStatesMask;
			const int numOfReSim = currentTick - tick;
			if (storedStatesMask < numOfReSim)
				//not enough states stored to handle
				return;

			//since there a delay between the client senting the input and the server
			//receiving it, we need correct the state that was the current state when
			//the client sent the input and all the states after that.
			int stateIndex = tick & storedStatesMask;
			for (
				int lastStateIndex = (stateIndex - 1) & storedStatesMask;
				lastStateIndex != currentStateIndex;
				stateIndex = (stateIndex + 1) & storedStatesMask
				) {
				GameState& stateBefore = states[lastStateIndex];
				GameState& state = states[stateIndex];
				setInput(state);
				//we'll need delta time to get the updated state
				double deltaTime = state.time - stateBefore.time;

				//merge inputs and states
				InputsType inputs;
				memcpy(&inputs, &state.inputs, sizeof(InputsType));
				state = stateBefore;
				memcpy(&state.inputs, &inputs, sizeof(InputsType));

				state.update(deltaTime);
				lastStateIndex = stateIndex;
			}
		}
		else {
			//we don't need to update the current state as that'll be done later
			GameState& currentState = states[currentStateIndex];
			setInput(currentState);
		}
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
				playersData[iD] = { i, 0 };
				stateInput.author = iD;
				return;
			}
			i += 1;
		}
	}

	inline const Snowflake::RawSnowflake getID() {
		return iD;
	}

	const GameState getDelayedState(const double delay) {
		double targetTime = states[currentStateIndex].time - delay;

		int stateIndex = currentStateIndex;
		for (
			;
			stateIndex != ((currentStateIndex + 1) & storedStatesMask);
			stateIndex = (stateIndex - 1) & storedStatesMask
			) {
			if (states[stateIndex].time <= targetTime) {
				//GameState delayedState;
				//delayedState = states[stateIndex];
				//const double deltaTime = targetTime - states[stateIndex].time;
				//delayedState.update(deltaTime);
				//return delayedState;
				break;
			}
		}
		return states[stateIndex];
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

	struct playerData {
		size_t inputIndex;
		size_t ackTick;
	};

	std::unordered_map<Snowflake::RawSnowflake, playerData> playersData;
};