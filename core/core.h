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

	void mergeInputs(Snowflake::RawSnowflake iD, GameState& destination, const GameState& source) {
		//we need to get the user's inputs so that when we overwrite the
		//inputs, we can put back the user's input
		GameState::InputType input;
		getUserInput(iD, destination,
			[&input](InputType& oldStateInput, int index) {
				input = oldStateInput;
			}
		);

		//merge the other's input
		memcpy(&destination, &source, sizeof(GameState));

		getUserInput(iD, destination,
			[=](InputType& newStateInput, int index) {
				newStateInput = input;
			}
		);
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
			int currentTick = getCurrentTick();

			int previousStateIndex = currentStateIndex;
			//keep currentState under 0b100000
			currentStateIndex = currentTick & storedStatesMask;

			//copy previous state to current state
			GameState& currentState = states[currentStateIndex];
			currentState = states[previousStateIndex];
			currentState.tt = 4;
			currentState.update(targetDeltaTime);

			lastTick = currentTick; //this also sets the currect tick
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
				//for the previousState
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
					//states[index].update(targetDeltaTime);
					states[index].tt = 1;
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

					state.time -= targetDeltaTime;
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
		}
	}

	//call this when the client gets states from the server
	void updateClient(const PacketHeader header, const GameStateUpdate& update) {
		if constexpr (isServer)
			return;

		//if (lastTick != 0)
		//	return;

		const int ackTick = header.acknowledgedTick;
		const int tick = header.tick;
		const int timestamp = header.timestamp;

		const int deltaAckTick = tick - ackTick;

		//newState is the server's predicted game state
		const GameState& newState = update.state;
		//ack player is the current player's correct and known state
		const PlayerType& ackPlayer = update.ackPlayer;

		////the client should be a bit ahead by running preditcions
		////and we need to keep track of how much has pasted since the last tick
		////take the timestamp of the client and the timestamp from the server
		////get the difference and that should be where the current state is

		//const auto previousIndex = lastTick & storedStatesMask;
		//lastTick = tick;

		////get client's current time and compare to server's
		//const auto clientTime = states[currentStateIndex].time;
		//const auto serverTime = newState.time;
		//const auto previousTime = states[previousIndex].time;
		//const double deltaClientServerTime = serverTime < clientTime ?
		//	clientTime - serverTime : 0; //zero is a placeholder
		////to do replace the zero with a better way to calulate deltaTime

		////since packets are given out of order, we need to reject any that
		////are older then that last packet from the server
		////if (serverTime < previousTime && lastTick != 0)
		////	return;
		////or not

		//const auto targetDeltaTime = 1.0 / tickRate;

		////since there a delay between the server senting the state and the client
		////receiving it, we need correct the state that was the current state when
		////the server sent the state and all the states after that.
		//const double pingTime = (static_cast<int>(
		//	std::chrono::duration_cast<std::chrono::milliseconds>(
		//		std::chrono::system_clock::now().time_since_epoch()
		//		).count()) - timestamp) / 1000.0;

		////int tempNumOfReSim = 0;
		////auto lastStateIndex = (currentStateIndex - 1) & storedStatesMask;
		////auto stateIndex = currentStateIndex;
		////if (deltaClientServerTime != 0) {
		////	for (
		////		;
		////		lastStateIndex != ((currentStateIndex + 1) & storedStatesMask);
		////		lastStateIndex = (stateIndex - 1) & storedStatesMask
		////		) {
		////		if (states[stateIndex].time <= serverTime) {
		////			break;
		////		}
		////		tempNumOfReSim += 1;
		////		stateIndex = lastStateIndex;
		////	}
		////} else {
		////	tempNumOfReSim = pingTime / targetDeltaTime;
		////	stateIndex = getCurrentTick() - tempNumOfReSim - 1;
		////	stateIndex = stateIndex & storedStatesMask;
		////}
		////const int numOfReSim = tempNumOfReSim;

		//const int numOfReSim = static_cast<int>(
		//	deltaClientServerTime / targetDeltaTime
		//	//add to simulate being a head of the server
		//	) + 1;

		////const auto newStateIndex = lastStateIndex;
		//const auto newStateIndex = (currentStateIndex - numOfReSim)
		//	& storedStatesMask;
		//const auto ackStateIndex = (newStateIndex - deltaAckTick)
		//	& storedStatesMask;

		////to do come up with a better way to sync up the server and client
		////getUserInput(iD, states[newStateIndex],
		////	[=](InputType& input, int index) {
		////		GameState::InputType empty;
		////		if (memcmp(&empty, &input.movement, sizeof(empty.movement)) == 0 &&
		////			memcmp(&empty, &newState.inputs[index].movement, sizeof(empty.movement)) == 0
		////		) {
		////			//we might be able to update the state if the player isn't moving
		////			//however, we need to be careful as this cases rubberbanding
		////			states[newStateIndex] = newState;
		////		}
		////	}
		////);

		////now we have the correct player state for the tick that the server sent
		////now we need to merge that to into the server tick
		////GameState correctedState = newState;
		////getUserPlayer(iD, correctedState,
		////	[=](PlayerType& player, int index) {
		////		const PlayerType& correctedPlayer = states[newStateIndex].players[index];
		////		std::cout << correctedPlayer.position[Axis::X] << '\n';
		////		memcpy(&player, &correctedPlayer, sizeof(GameState::Player));
		////	}
		////);
		////states[newStateIndex] = correctedState;

		//const auto mergeInputs = [=](Snowflake::RawSnowflake iD, GameState& destination, const GameState& source) {
		//	//we need to get the user's inputs so that when we overwrite the
		//	//inputs, we can put back the user's input
		//	GameState::InputType input;
		//	getUserInput(iD, destination,
		//		[&input](InputType& oldStateInput, int index) {
		//			input = oldStateInput;
		//		}
		//	);

		//	//merge the other's input
		//	memcpy(&destination.inputs, &source.inputs, sizeof(GameState::inputs));

		//	getUserInput(iD, destination,
		//		[=](InputType& newStateInput, int index) {
		//			newStateInput = input;
		//		}
		//	);
		//};

		//int currentTick;
		////currentTick = tick + (pingTime / targetDeltaTime);
		////currentTickOffset = currentTick - tick;
		////currentStateIndex = getCurrentTick() & storedStatesMask;

		//int numOfTicksResimulated = 0;
		///*auto lastStateIndex = newStateIndex;
		//auto stateIndex = (lastStateIndex + 1) & storedStatesMask;*/
		//auto lastStateIndex = (newStateIndex - 1) & storedStatesMask;
		//auto stateIndex = newStateIndex;
		//auto NextstateIndex = (stateIndex + 1) & storedStatesMask;
		//const GameState* replacement = &newState;
		//for (
		//	;
		//	stateIndex != currentStateIndex;
		//	NextstateIndex = (NextstateIndex + 1) & storedStatesMask
		//) {
		//	GameState& state = states[stateIndex];
		//	//if (serverTime + pingTime <= state.time)
		//	//	break;

		//	states[lastStateIndex] = *replacement;

		//	//we'll need delta time to get the state
		//	//double deltaTime = states[stateIndex].time - replacement->time;
		//	double deltaTime = targetDeltaTime;

		//	//for some reason, delta time can get really small, so limit it
		//	if (deltaTime < targetDeltaTime)
		//		deltaTime = targetDeltaTime;

		//	mergeInputs(iD, state, *replacement);

		//	//the time is replaced by mergeInput with replaceState set to true
		//	state.time = replacement->time;
		//	if (0 <= deltaTime) { //deltaTime less then zero might crash the game
		//		state.update(targetDeltaTime); //to do use delta time
		//	}

		//	numOfTicksResimulated += 1;
		//	lastStateIndex = stateIndex;
		//	replacement = &state;
		//	stateIndex = NextstateIndex;
		//}

		////set tick info to the new tick info
		////numOfTicksResimulated should be numOfReSim - 1;
		//currentTick = tick + numOfTicksResimulated + 1;
		//currentTickOffset = currentTick - tick;
		//currentStateIndex = getCurrentTick() & storedStatesMask;

		////set current state to the new current state
		//GameState& previousState = states[lastStateIndex];
		//GameState& currentState = states[currentStateIndex];
		//mergeInputs(iD, currentState, previousState);
		//currentState.time = previousState.time;

		////GameState::InputType input;
		////getUserInput(iD, currentState,
		////	[&input](InputType& oldStateInput, int index) {
		////		input = oldStateInput;
		////	}
		////);

		////merge the other's state
		////memcpy(&currentState, &previousState, sizeof(GameState));

		////getUserInput(iD, currentState,
		////	[=](InputType& newStateInput, int index) {
		////		newStateInput = input;
		////	}
		////);
		////currentState.update(deltaClientServerTime - ((numOfTicksResimulated + 1) * targetDeltaTime));

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
		const double pingTime = (static_cast<int>(
			std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::system_clock::now().time_since_epoch()
				).count()) - timestamp) / 1000.0;
		const int numOfReSim0 = static_cast<int>(
			deltaClientServerTime / targetDeltaTime
			//add to simulate being a head of the server
			) + 1;

		const auto newStateIndex0 = (currentStateIndex - numOfReSim0)
			& storedStatesMask;

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

		const auto newStateIndex = stateIndex;

		//to do come up with a better way to sync up the server and client
		//getUserInput(iD, states[newStateIndex],
		//	[=](InputType& input, int index) {
		//		GameState::InputType empty;
		//		if (memcmp(&empty, &input.movement, sizeof(empty.movement)) == 0 &&
		//			memcmp(&empty, &newState.inputs[index].movement, sizeof(empty.movement)) == 0
		//		) {
		//			//we might be able to update the state if the player isn't moving
		//			//however, we need to be careful as this cases rubberbanding
		//			states[newStateIndex] = newState;
		//		}
		//	}
		//);

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

		//update current state info
		lastTick = newState.tick;
		currentTickOffset = (pingTime / targetDeltaTime);
		currentStateIndex = getCurrentTick() & storedStatesMask;

		int numOfTicksResimulated = 0;
		lastStateIndex = newStateIndex;
		stateIndex = (lastStateIndex + 1) & storedStatesMask;
		auto NextstateIndex = (stateIndex + 1) & storedStatesMask;
		const GameState* replacement = &newState;
		//sub 1 because we want the previous state ti
		double replacementTime = replacement->time;
		auto stateTick = newState.tick;
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

			states[stateIndex].time = replacementTime;
			if (0 <= deltaTime) { //deltaTime less then zero might crash the game
				states[stateIndex].update(targetDeltaTime); //to do use delta time
				//states[stateIndex] = *replacement;
			}
			states[stateIndex].tt = 3;

			numOfTicksResimulated += 1;
			replacement = &states[stateIndex];
			replacementTime = states[stateIndex].time;
			lastStateIndex = stateIndex;
			stateTick += 1;
			stateIndex = NextstateIndex;
		}

		//set tick info to the new tick info
		//numOfTicksResimulated should be numOfReSim - 1;

		//set current state to the new current state
		GameState& previousState = states[lastStateIndex];
		GameState& currentState = states[currentStateIndex];
		mergeInputs(iD, currentState, previousState);
		currentState.time = previousState.time;
		currentState.tt = 5;
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
			const int currentTick = getCurrentTick() - 1;
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
		//GameState delayedState;
		double targetTime = states[currentStateIndex].time - delay;

		int stateIndex = currentStateIndex;
		for (
			;
			stateIndex != ((currentStateIndex + 1) & storedStatesMask);
			stateIndex = (stateIndex - 1) & storedStatesMask
			) {
			if (states[stateIndex].time <= targetTime) {
				//delayedState = states[stateIndex];
				//const double deltaTime = targetTime - states[stateIndex].time;
				//delayedState.update(deltaTime);
				//return delayedState;
				break;
			}
		}
		//return states[stateIndex];
		return states[((currentStateIndex - 1) & storedStatesMask)];
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