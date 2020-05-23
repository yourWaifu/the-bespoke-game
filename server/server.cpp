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
#include "game.h"

//to do get this working
class SteamNetworkingServer : private SteamNetworking, public ISteamNetworkingSocketsCallbacks {
public:
	using clientMap = std::unordered_map<HSteamNetConnection, Snowflake::RawSnowflake>;

	SteamNetworkingServer(asio::io_context& _iOContext, SteamNetworkingIPAddr& serverLocalAddr) :
		SteamNetworking(_iOContext, *this),
		port(serverLocalAddr.m_port),
		listenSocket(sockets, serverLocalAddr) {
		std::cout << "Listening to port " << port << "\n";
	}

	~SteamNetworkingServer() = default;

	int receiveMessageFunction(ISteamNetworkingMessage** messages, int maxMessages) {
		return sockets->ReceiveMessagesOnListenSocket(listenSocket.data, messages, maxMessages);
	}

	void onPollTick(const double deltaTime) {
		//to do update game only when target deltaTime is hit
		gameServer.update(deltaTime);
		PackagedData <
			GameServer::GameStateUpdate
		> message{
			{ 
				PacketHeader::GAME_STATE_UPDATE,
				0, //set later
				gameServer.getCurrentTick(),
				static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::system_clock::now().time_since_epoch()).count()),
			},
			{
				gameServer.getCurrentState(),
				GameServer::GameState::Player{} //fill later
			}
		};
		//std::cout << message.data.time << '\n';
		std::for_each(clients.begin(), clients.end(), [=](clientMap::value_type& connection) {
			PackagedData <
				GameServer::GameStateUpdate
			> personalMessage = message;
			//set ack tick data
			const int ackTick = gameServer.getAckTick(connection.second);
			personalMessage.header.acknowledgedTick = ackTick;
			gameServer.getUserPlayer(connection.second, gameServer.getState(ackTick),
				[&personalMessage](const GameServer::PlayerType& player, int) {
					personalMessage.data.ackPlayer = player;
				}
			);

			delayedQueue.emplace(iOContext);
			asio::steady_timer& sendTimer = delayedQueue.back();
			sendTimer.expires_after(std::chrono::milliseconds(0));
			sendTimer.async_wait([this, &connection, personalMessage](const asio::error_code& error) mutable {
				if (error) return;
				sockets->SendMessageToConnection(connection.first,
					&personalMessage, sizeof(personalMessage),
					k_nSteamNetworkingSend_NoDelay, nullptr);
				delayedQueue.pop();
			});
		});
	}

	void onMessage(nonstd::string_view message) {
		gameServer.onMessage(message);
	}

private:
	const int port;
	HSteamListenSocketRAII listenSocket;
	GameServer gameServer;
	std::unordered_map<HSteamNetConnection, Snowflake::RawSnowflake> clients;
	const std::chrono::system_clock::time_point startTime =
		std::chrono::system_clock::now();

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
			{
				PackagedData<Snowflake::RawSnowflake> helloMessage{
					{ PacketHeader::HELLO },
					{}
				};

				//create a new ID for the new user
				auto foundClient = clients.find(info->m_hConn);
				if (foundClient == clients.end()) {
					auto time = std::chrono::system_clock::now() - startTime;
					int64_t timestamp = std::chrono::duration_cast<
						std::chrono::duration<int64_t, std::chrono::milliseconds::period>>(time).count();
					helloMessage.data = Snowflake::generate<true>(timestamp);
					//add to clients map
					clients.insert({ info->m_hConn, helloMessage.data });
					gameServer.onNewPlayer(helloMessage.data);
				} else {
					helloMessage.data = foundClient->second;
				}

				sockets->SendMessageToConnection(info->m_hConn, &helloMessage, sizeof(helloMessage),
					k_nSteamNetworkingSend_Reliable, nullptr);
			}
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