#include "main_windows.h"

namespace sys {
	double GameSystem::time() {
		LARGE_INTEGER counter;
		QueryPerformanceCounter(&counter);
		return (double)counter.QuadPart * secondsPerTick;
	}

	const double GameSystem::initializeTime() {
		LARGE_INTEGER frequency;
		QueryPerformanceFrequency(&frequency);
		return 1.0 / (double)frequency.QuadPart;
	}

	void GameSystem::sendEvents() {
		MSG message;

		while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&message);
			DispatchMessage(&message);
		}
	}

	Event GameSystem::getEvent() {
		if (eventQueueHead < eventQueueTail) {
			++eventQueueHead;
			return eventQueue[(eventQueueHead - 1) & MAX_EVENT_MASK];
		}
		eventQueueHead = eventQueueTail = 0;
		return { None, 0, 0, 0 };
	}

	void GameSystem::addEventToQueue(EventType type, int value, int value2, int deviceNumber) {
		Event * newEvent = &eventQueue[eventQueueTail & MAX_EVENT_MASK];

		if (MAX_NUM_OF_EVENT_QUEUE <= eventQueueTail - eventQueueHead) {
			//overflow
			++eventQueueHead;
		}
		
		++eventQueueTail;
		*newEvent = { type, value, value2, deviceNumber };
	}

	GameSystem::GameSystem(HINSTANCE& hInstance) :
		secondsPerTick(initializeTime()),
		window(*this, hInstance),
		video(*this),
		gameCore(&video.resolution.width, &video.resolution.height, *this, getWindowHandle()),
		game(gameCore),
		isRunning(true)
	{
		
	}
}

int CALLBACK WinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	sys::GameSystem game(hInstance);
	double newTime = game.time();
	double timePassed;
	double oldTime = 0;

	//
	//main game loop
	//
	while (game.isRunning) {
		//find time passed since the last cycle
		oldTime = newTime;
		newTime = game.time();
		timePassed = newTime - oldTime;

		game.sendEvents();		//this should be a private function, but I need to get this working how now
		
		////get input
		//for (DWORD controllerIndex = 0; controllerIndex < XUSER_MAX_COUNT; ++controllerIndex) {
		//	XINPUT_STATE controllerState;
		//	if (XInputGetState(controllerIndex, &controllerState) == ERROR_SUCCESS) {
		//		XINPUT_GAMEPAD *controller = &controllerState.Gamepad;
		//		//you should fix this
		//		bool DPadUp = (controller->wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0;
		//		bool DPadDown = (controller->wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
		//		bool DPadLeft = (controller->wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0;
		//		bool DPadRight = (controller->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0;
		//		bool start = (controller->wButtons & XINPUT_GAMEPAD_START) != 0;
		//		bool back = (controller->wButtons & XINPUT_GAMEPAD_BACK) != 0;
		//		bool leftThumbClick = (controller->wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0;
		//		bool rightThumbClick = (controller->wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0;
		//		bool leftThumb = (controller->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
		//		bool rightShoulder = (controller->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;
		//		bool AButton = (controller->wButtons & XINPUT_GAMEPAD_A) != 0;
		//		bool BButton = (controller->wButtons & XINPUT_GAMEPAD_B) != 0;
		//		bool XButton = (controller->wButtons & XINPUT_GAMEPAD_X) != 0;
		//		bool YButton = (controller->wButtons & XINPUT_GAMEPAD_Y) != 0;
		//		uint8_t leftTrigger = controller->bLeftTrigger;
		//		uint8_t rightTrigger = controller->bRightTrigger;
		//		int16_t leftThumbX = controller->sThumbLX;
		//		int16_t leftThumbY = controller->sThumbLY;
		//		int16_t rightThumbX = controller->sThumbRX;
		//		int16_t rightThumbY = controller->sThumbRY;
		//	} else {
		//		//controller not available
		//	}
		//}

		game.gameCore.update(timePassed);
	}
}