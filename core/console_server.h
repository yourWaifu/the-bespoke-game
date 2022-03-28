#pragma once
#include "snowflake.h"
//Since the Game Engine's netcode is about predictions, it's common that
//inputs will predicted to be simalar but console commands don't really
//work like that. to prevent the server from retrying the same request
//over and over again the server needs to store some idempotency values
//that the netcode can predict

struct ConsoleCommand {
	struct IdempotencyKey {
		int tick;
		Snowflake::RawSnowflake userID;
			//should be empty when sent from the client
			//but can be filled as such, the server should
			//overwrite it
		bool operator==(IdempotencyKey& key) {
			key.tick == tick && key.userID == userID;
		}
	} key;
	char command[256];

	bool operator==(ConsoleCommand& command) {
		return command.key == key;
	}
};

template<class GameState, class ConsoleCommands = GameState>
struct ConsoleState {
	//to do store more then one key
	using Inputs = decltype(GameState::Input::getConsoleCommands());
	ConsoleCommand::IdempotencyKey lastKeyProcessed;
	void update(GameState& gameState) {
		auto& commands = gameState.inputs.getConsoleCommands();
		auto& command = commands[0];
		if (command == lastKeyProcessed || !command.command[0])
			return;
		
		
	}
};