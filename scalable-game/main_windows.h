#pragma once
#include <Windows.h>
#include <stdexcept>
#include <stdint.h>
#include <Xinput.h>

#include <core.h>
#include "game.h"

namespace sys {
	class GameSystem;

	class Window {
	public:
		typedef LRESULT CALLBACK WindowProcess(HWND, UINT, WPARAM, LPARAM);
		Window(GameSystem& system, HINSTANCE & hInstance);
		HWND mainWindow;
		RECT windowRect;
	private:
		static LRESULT CALLBACK mainWindowProcess(
			HWND   windowHandle,
			UINT   message,
			WPARAM wParameter,
			LPARAM lParameter
		);
	};

	struct ScreenResolution {
		unsigned int width;
		unsigned int height;
	};

	class VideoSystem {
	public:
		VideoSystem(GameSystem& parentClass);
		void updateWindowFramebuffer();
		HBITMAP windowBitmap;	//I think this should be private
		ScreenResolution resolution;
		void *bitmapMemory;
	private:
		HDC windowDeviceContext;
		HDC mDeviceContext;
		void makeWindow();		//you should make this public later
	};

	class GameSystem {
		//order matters here
	private:
		Window window;
	public:
		bool isRunning;
		GameSystem(HINSTANCE& hInstance);
		double time();
		void sendEvents();
		VideoSystem video;
		GameCoreSystem<GameSystem> gameCore;
		Game game;
		Event getEvent();
		void addEventToQueue(EventType type, int value, int value2, int deviceNumber);
		HWND& getWindowHandle() { return window.mainWindow;  }
		RECT getWindowRect() { return window.windowRect; }
		ScreenResolution getScreenResolution() { return video.resolution;  }
		inline void* getWindow() { return (void*)window.mainWindow; }
	private:
		//Events
#define MAX_NUM_OF_EVENT_QUEUE 256
#define MAX_EVENT_MASK (MAX_NUM_OF_EVENT_QUEUE - 1)
		Event eventQueue[256] = {};
		unsigned int eventQueueHead = 0;
		unsigned int eventQueueTail = 0;

		
		const double secondsPerTick;
		const double initializeTime();
	};
}