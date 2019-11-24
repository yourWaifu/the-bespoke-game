//#include <steam/steamnetworkingsockets.h>
//#include <steam/isteamnetworkingutils.h>
#include <string_view>
#include <chrono>
#include <iostream>
#include <thread> //temp
//#include "uwebsockets/App.h"
#include "core.h"
#include "game.h"
/*
constexpr int defaultServerPort = 27279;

//to do get this working
class SteamNetworkingServer : private ISteamNetworkingSocketsCallbacks {
public:
	SteamNetworkingServer(const int _port = defaultServerPort) :
		sockets(SteamNetworkingSockets()), port(_port)
	{}

	void run() {
		serverLocalAddr.Clear();
		serverLocalAddr.m_port = port;
		listenSocket = sockets->CreateListenSocketIP(serverLocalAddr);
	}
private:
	const int port;
	ISteamNetworkingSockets* sockets;
	SteamNetworkingIPAddr serverLocalAddr;
	HSteamListenSocket listenSocket;

	void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info) override {
		switch (info->m_info.m_eState) {
		case k_ESteamNetworkingConnectionState_ClosedByPeer:
		case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
			break;
		case k_ESteamNetworkingConnectionState_Connecting:
			break;
		case k_ESteamNetworkingConnectionState_Connected:
			break;
		default:
			break;
		}
	}
};

struct ServerData {

};

struct Server {
	static int run() {
		//uWS::App::WebSocketBehavior behavior;
		//behavior.message = [](auto* ws, std::string_view message, uWS::OpCode opCode) {
		//	
		//};
		//uWS::App().ws<ServerData>("/*", behavior);
		return 0;
	}
};
*/

class GameApp {
public:
	GameApp() = default;
	~GameApp() = default;
	double time() {
		using Clock = std::chrono::high_resolution_clock;
		using Duration = Clock::duration;
		using TimePoint = Clock::time_point;
		static const auto start = Clock::now();
		return std::chrono::duration_cast<std::chrono::seconds, double, Clock::period>(Clock::now() - start).count();
	}
	void run() {
		double tickTime = 1 /*seconds*/ / tickRate;
		double TickTimeMs = tickTime * 1000.0 /*milliseconds in a second*/;
		GenericCore core();
		Game game(core);
		double newTime = time();
		double timePassed;
		double oldTime = 0;

		while(true) {
			oldTime = newTime;
			newTime = time();
			timePassed = newTime - oldTime;
			//to do replace the sleep
			const double updateTime = time() - newTime;
			const double timeTilNextTick = TickTimeMs - updateTime;
			const double waitTime = timeTilNextTick < 0 ? 0 : timeTilNextTick;
			std::this_thread::sleep_for(std::chrono::seconds(static_cast<int>(waitTime)));
		}
	}
private:
	int tickRate = 60;
};

int main() {
	GameApp game;
	game.run();
}