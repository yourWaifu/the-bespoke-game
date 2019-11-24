#include <SDL_syswm.h>

void * getSystemWindow(SDL_Window*& window) {
	SDL_SysWMinfo wmInfo;
	SDL_VERSION(&wmInfo.version);
	SDL_GetWindowWMInfo(window, &wmInfo);
	return reinterpret_cast<void*>(wmInfo.info.x11.window);
}