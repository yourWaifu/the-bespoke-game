#include "input.h"

void InputComponent::processInput(EntityHandler::State& state, InputAction action, PlayerInput& inputsToSend) {
	auto foundKeyState = keyStates.find(action.key);
	if (foundKeyState == keyStates.end() || foundKeyState->second == action.pressed)
		//the system can send the same event, causing issue since the game logic expects
		//the action binding function to be only called when the button state changed.
		return;
	foundKeyState->second = action.pressed;

	BoundActionsMap::iterator foundActionBinding = boundActions.find(action);
	if (foundActionBinding == boundActions.end())
		return;
	const std::string& name = foundActionBinding->second;

	//add to input buffer so that we can send it to the server
	auto foundCommandIndex = commandIndices.find(name);
	if (foundCommandIndex == commandIndices.end())
		//only selected actions are in the commandIndices map, and only those
		//will be sent to the server.
		return;
	float inputValue = action.pressed ? 1.0 : 0.0;
	inputsToSend.addAction(foundCommandIndex->second, inputValue);
	boundCommands[foundCommandIndex->second](state, inputValue);
}

void InputComponent::processInput(PlayerInput& actions) {
	for (Action& action : actions.actions) {
		//to do add state here
		//boundCommands[action.command](action.param);
	}
}