#include "main_windows.h"
#include <GL/gl.h> 
#include <GL/glu.h>

namespace sys {
	Window::Window(GameSystem& system, HINSTANCE & hInstance) {
		WNDCLASS window = { 0 };
		window.lpfnWndProc = mainWindowProcess;
		window.hInstance = hInstance;
		window.hCursor = LoadCursor(NULL, IDC_ARROW);	//load loading cursor
		window.lpszMenuName = "Scalable Game";
		window.lpszClassName = "Scalable Game";

		if (!RegisterClass(&window))
			throw std::exception("Couldn't make window when initializing the video system");

		DWORD mainWindowStyle = WS_VISIBLE | WS_SYSMENU | WS_MINIMIZEBOX;
		windowRect.left = 0;      //x
		windowRect.right = 1080;  //w
		windowRect.top = 0;       //y
		windowRect.bottom = 720;  //h
		AdjustWindowRectEx(&windowRect, mainWindowStyle, FALSE, 0);

		AdjustWindowRect(&windowRect, mainWindowStyle, FALSE);
		mainWindow = CreateWindow(
			"Scalable Game",
			"Scalable Game",
			mainWindowStyle,    //the style
			CW_USEDEFAULT,	    //default x
			CW_USEDEFAULT,      //default y
			windowRect.right,   //w
			windowRect.bottom,  //h
			NULL,               //parent
			NULL,               //menu
			hInstance,          //instance
			&system             //param
		);
		UpdateWindow(mainWindow);
	}

	//this is where windows send events to
	LRESULT CALLBACK Window::mainWindowProcess(HWND windowHandle, UINT message, WPARAM wParameter, LPARAM lParameter) {
		//code copied from https://docs.microsoft.com/en-us/windows/win32/opengl/the-program-ported-to-win32
		LONG    lRet = 1;
		PAINTSTRUCT    ps;
		RECT rect;

		if (message == WM_CREATE) {
			LPCREATESTRUCT pcs = (LPCREATESTRUCT)lParameter;
			GameSystem * pGameSystem = (GameSystem*)pcs->lpCreateParams;
			SetWindowLongPtrW(windowHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pGameSystem));
			return 1;
		}

		GameSystem * pGameSystem = reinterpret_cast<GameSystem*>(static_cast<LONG_PTR>(GetWindowLongPtrW(windowHandle, GWLP_USERDATA)));

		switch (message) {
		case WM_DESTROY:
			pGameSystem->isRunning = false;
			return 1;
		case WM_KEYDOWN: pGameSystem->addEventToQueue(Key, static_cast<int>(wParameter), sys::DOWN, 0); break;
		case WM_KEYUP:   pGameSystem->addEventToQueue(Key, static_cast<int>(wParameter), sys::UP  , 0); break;

		case WM_PAINT:
			BeginPaint(pGameSystem->getWindowHandle(), &ps);
			EndPaint(pGameSystem->getWindowHandle(), &ps);
			break;
		}

		return DefWindowProc(windowHandle, message, wParameter, lParameter);	//default behavior
	}
}