#include "input.h"

void InputComponent::processInput(InputAction action, PlayerInput& inputsToSend) {
	auto foundKeyState = keyStates.find(action.key);
	if (foundKeyState == keyStates.end() || foundKeyState->second == action.pressed)
		//the system can send the same event, causing issue since the game logic expects
		//the action binding function to be only called when the button state changed.
		return;
	foundKeyState->second = action.pressed;

	BoundActionsMap::iterator foundActionBinding = boundActions.find(action);
	if (foundActionBinding == boundActions.end())
		return;
	InputActionBinding& actionBinding = *(foundActionBinding->second);
	if (actionBinding.function == nullptr)
		return;
	
	actionBinding.function();

	//add to input buffer so that we can send it to the server
	auto foundCommandIndex = commandIndices.find(actionBinding.name);
	if (foundKeyState == keyStates.end())
		//only seletected actions are in the commandIndices map, and only those
		//will be sent to the server.
		return;
	inputsToSend.addAction(foundCommandIndex->second, action.pressed ? 1.0 : 0.0);
}

void InputComponent::processInput(PlayerInput& actions) {
	for (Action& action : actions.actions) {
		boundCommands[action.command](action.param);
	}
}