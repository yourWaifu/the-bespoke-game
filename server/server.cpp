#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#include <string_view>
//#include "uwebsockets/App.h"

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

int main() {
	return Server::run();
}