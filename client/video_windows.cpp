#include "main_windows.h"

namespace sys {

	VideoSystem::VideoSystem(GameSystem& parentClass)
	{
		//
		//make frame buffer
		//
		RECT windowRect = parentClass.getWindowRect();
		resolution = { 
			static_cast<unsigned int>(windowRect.right - windowRect.left),
			static_cast<unsigned int>(windowRect.bottom - windowRect.top)
		};
	}

	void VideoSystem::updateWindowFramebuffer() {
	}

	void VideoSystem::makeWindow() {
		
	}
}