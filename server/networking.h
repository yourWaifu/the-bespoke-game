#pragma once
#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#include <cassert>
#include <string_view>
#include <iostream>
#include <optional>
#include <unordered_map>
#include <queue>
#include "nonstd/string_view.hpp"
#include "asio.hpp"
#include "snowflake.h"

constexpr int defaultServerPort = 27279;

struct SteamNetworkingConfigValues_t {
	SteamNetworkingConfigValues_t() = default;
	SteamNetworkingConfigValue_t* options = nullptr;
	int numOptions = 0;
};

struct HSteamListenSocketRAII {
	HSteamListenSocketRAII() = default;
	HSteamListenSocketRAII(ISteamNetworkingSockets*& sock, SteamNetworkingIPAddr& localAddr, SteamNetworkingConfigValues_t options) :
		data(sock->CreateListenSocketIP(localAddr, options.numOptions, options.options)),
		socket(sock)
	{
		if (data == k_HSteamListenSocket_Invalid)
			std::cout << "Failed to listen to port " << localAddr.m_port << "\n";
	}
	HSteamListenSocketRAII(ISteamNetworkingSockets*& sock, SteamNetworkingIPAddr& localAddr) :
		HSteamListenSocketRAII(sock, localAddr, SteamNetworkingConfigValues_t{}) {}
	~HSteamListenSocketRAII() {
		socket->CloseListenSocket(data);
		data = k_HSteamListenSocket_Invalid;
	}
	HSteamListenSocket data;
private:
	ISteamNetworkingSockets* socket;
};

template<int mexMessages = 1>
struct ISteamNetworkingMessageRAII {
	ISteamNetworkingMessageRAII() {

	}
	~ISteamNetworkingMessageRAII() {
		for (int i = 0; i < messageAmount; ++i) {
			data[i]->Release();
		}
	}
	template<class ReceiveMessageFunction>
	void receiveMessages(ReceiveMessageFunction function) {
		messageAmount = function(&data[0], mexMessages);
	}
	nonstd::string_view getMessage(int index = 0) {
		return nonstd::string_view{
			static_cast<const char*>(data[index]->m_pData),
			static_cast<nonstd::string_view::size_type>(data[index]->m_cbSize)
		};
	}
	const HSteamNetConnection getConnection(int index = 0) {
		return data[index]->m_conn;
	}
	bool empty() {
		return messageAmount == 0;
	}
	bool error() {
		return messageAmount < 0;
	}
	bool canRead() {
		if (empty()) return false;
		if (error()) {
			std::cout << "Error poll messages failed\n";
			return false;
		}
		assert(1 <= messageAmount && data[0] != nullptr);
		return true;
	}
	int messageAmount = -1;
	ISteamNetworkingMessage* data[mexMessages] = { };
};


class SteamNetworking {
public:
	template<class Child>
	SteamNetworking(asio::io_context& _iOContext, Child& child) :
		iOContext(_iOContext),
		pollTimer(iOContext)
	{
		SteamDatagramErrMsg errMsg;
		if (!GameNetworkingSockets_Init(nullptr, errMsg))
			std::cout << "GameNetworkingSockets_Init failed." << errMsg;

		sockets = SteamNetworkingSockets();
		if (sockets == nullptr)
			std::cout << "No instance to use\n";

		asio::post(iOContext, [&]() {
			poll(child);
		});
	}

	~SteamNetworking() {
		constexpr int64_t targetWaitTime = 1000000000 / 2; //1 second / 2
		asio::steady_timer timer{ iOContext };
		timer.expires_after(std::chrono::nanoseconds(targetWaitTime));
		timer.wait();
		GameNetworkingSockets_Kill();
	}

private:
	template<class Child>
	void poll(Child& child) {
		while (true) {
			//to do add RAII to incomingMessage
			ISteamNetworkingMessageRAII<> incommingMessage{};
			incommingMessage.receiveMessages([&](ISteamNetworkingMessage** messages, int maxMessages) {
				return child.receiveMessageFunction(messages, maxMessages);
			});
			if (!incommingMessage.canRead()) break;

			//on message
			child.onMessage(incommingMessage.getMessage());
		}
		sockets->RunCallbacks(&child);

		//set up wait timer
		constexpr int64_t targetPollTime = 1000000000 / 120; //1 second / 120
		pollTimer = asio::steady_timer{ iOContext };
		pollTimer.expires_after(std::chrono::nanoseconds(targetPollTime));
		pollTimer.async_wait([&](const asio::error_code& error) {
			if (!error) poll(child);
			else return;
		});

		static auto previousTime = std::chrono::system_clock::now();
		static auto currentTime = previousTime;
		currentTime = std::chrono::system_clock::now();
		const auto timeDifference = currentTime - previousTime;
		using DeltaTimeDuration = std::chrono::duration<double>;
		const double deltaTime = std::chrono::duration_cast<
			DeltaTimeDuration>(timeDifference).count();

		child.onPollTick(deltaTime);
		previousTime = currentTime;
	}

protected:
	ISteamNetworkingSockets* sockets = nullptr;
	asio::io_context& iOContext;
	asio::steady_timer pollTimer;

	//delayed messages queue for debugging
	std::queue<asio::steady_timer> delayedQueue;
};