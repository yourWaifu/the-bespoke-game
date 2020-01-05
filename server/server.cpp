#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#include <cassert>
#include <string_view>
#include <iostream>
#include <optional>
#include <unordered_map>
#include <memory>
#include "nonstd/string_view.hpp"
#include "networking.h"

//to do get this working
class SteamNetworkingServer : private SteamNetworking, public ISteamNetworkingSocketsCallbacks {
public:
	SteamNetworkingServer(asio::io_context& _iOContext, SteamNetworkingIPAddr& serverLocalAddr) :
		SteamNetworking(_iOContext, *this),
		listenSocket(sockets, serverLocalAddr),
		port(serverLocalAddr.m_port) {
		std::cout << "Listening to port " << port << "\n";
	}

	~SteamNetworkingServer() = default;

	int receiveMessageFunction(ISteamNetworkingMessage** messages, int maxMessages) {
		return sockets->ReceiveMessagesOnListenSocket(listenSocket.data, messages, maxMessages);
	}
private:
	const int port;
	HSteamListenSocketRAII listenSocket;

	void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info) override {
		switch (info->m_info.m_eState) {
		case k_ESteamNetworkingConnectionState_ClosedByPeer:
		case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
			if (info->m_eOldState == k_ESteamNetworkingConnectionState_Connected) {
				std::cout << "Problem detected on Connection" << info->m_info.m_szConnectionDescription << ", "
					<< info->m_info.m_eEndReason << ": " << info->m_info.m_szEndDebug << '\n';
				//on error
			}

			//on close

			//important, this cleans up the closed connection
			sockets->CloseConnection(info->m_hConn, 0, nullptr, false);
			break;
		case k_ESteamNetworkingConnectionState_Connecting:
			//to do new connection sanity cheak
			std::cout << "Connection request from " << info->m_info.m_szConnectionDescription << '\n';
			//try connecting
			if (sockets->AcceptConnection(info->m_hConn) != k_EResultOK) {
				//when this fails, it's likly because the user canceled it or something
				sockets->CloseConnection(info->m_hConn, 0, nullptr, false);
				std::cout << "Unacceptable Connection. Connection might be already closed\n";
				break;
			}
			break;
		case k_ESteamNetworkingConnectionState_Connected:
			//on open event
			std::cout << "New connected game client\n";
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
		SteamNetworkingIPAddr serverLocalAddr;
		serverLocalAddr.Clear();
		serverLocalAddr.m_port = defaultServerPort;
		asio::io_context iOContext;
		SteamNetworkingServer server{ iOContext, serverLocalAddr };
		iOContext.run();
		return 0;
	}
};

int main() {
	return Server::run();
}