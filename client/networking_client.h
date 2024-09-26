#pragma once
#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#include <iostream>
#include <array>
#include <string_view>
#include "asio.hpp"
#include "../server/networking.h"

class SteamNetworkingClient : public SteamNetworking {
public:
	SteamNetworkingClient(asio::io_context& _iOContext, std::shared_ptr<GameClient>& _gameClient):
		SteamNetworking(_iOContext, *this),
		gameClient(_gameClient),
		networkingOptions(*this)
	{}

	~SteamNetworkingClient() = default;

	void setServerAddress(const std::string address) {
		serverAdderess.Clear();
		serverAdderess.ParseString(address.c_str());
		if (serverAdderess.m_port == 0)
			serverAdderess.m_port = defaultServerPort;
	}

	void start() {
		std::array<char, SteamNetworkingIPAddr::k_cchMaxString> addressBuffer;
		serverAdderess.ToString(addressBuffer.data(), addressBuffer.size(), true);
		std::string_view serverAddressView{ addressBuffer.data() };
		std::cout << "Connecting to server: " << serverAddressView << '\n';
		connection = sockets->ConnectByIPAddress(serverAdderess,
			networkingOptions.data.size(), networkingOptions.data.data());

		if (connection == k_HSteamNetConnection_Invalid)
			std::cout << "Failed to create a connection\n";

		SteamNetConnectionInfo_t connectionInfo; 
		bool wasOK = 
			sockets->GetConnectionInfo(connection, &connectionInfo);
		if (wasOK) {
			setHStreamListenSocket(connectionInfo.m_hListenSocket);
		} else {
			std::cout << "Failed to get connection info\n";
		}
	}

	//to do make it so that you only need to give the data, and it'll automaticly fill the other data.
	template<class DataType>
	void send(PackagedData<DataType> packet) {
		//simulate delays in networking
		delayedQueue.emplace(iOContext);
		asio::steady_timer& sendTimer = delayedQueue.back();
		sendTimer.expires_after(std::chrono::milliseconds(0));
		sendTimer.async_wait([this, packet](const asio::error_code& error) {
			if (error) return;
			sockets->SendMessageToConnection(connection,
				&packet, sizeof(packet),
				k_nSteamNetworkingSend_NoDelay, nullptr);
			delayedQueue.pop();
		});
	}

	void close() {
		pollTimer.cancel();
	}

	int receiveMessageFunction(ISteamNetworkingMessage** messages, int maxMessages) {
		return sockets->ReceiveMessagesOnConnection(connection, messages, maxMessages);
	}

	void onPollTick(const double deltaTime) {

	}

	void onMessage(std::string_view message) {
		gameClient->onMessage(message);
	}

	void update(double deltaTime) {
		gameClient->update(deltaTime);
	}

	void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info) {
		switch (info->m_info.m_eState) {
		case k_ESteamNetworkingConnectionState_ClosedByPeer:
		case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
			isRunning = false;
			if (info->m_eOldState == k_ESteamNetworkingConnectionState_Connecting) {
				std::cout << "Couldn't connect\n";
			}
			else {
				std::cout << "RIP, lost connection\n";
			}

			//important, this cleans up the closed connection
			sockets->CloseConnection(info->m_hConn, 0, nullptr, false);
			connection = k_HSteamNetConnection_Invalid;
			break;
		case k_ESteamNetworkingConnectionState_Connecting:
			break;
		case k_ESteamNetworkingConnectionState_Connected:
			//on open event
			std::cout << "connected\n";
			break;
		default:break;
		}
	};

	std::shared_ptr<GameClient> gameClient;
private:
	HSteamNetConnection connection;
	bool isRunning = true;
	SteamNetworkingIPAddr serverAdderess;
	SteamNetworkConfigValue networkingOptions;
};