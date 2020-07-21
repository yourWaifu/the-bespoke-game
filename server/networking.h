#pragma once
#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#include <cassert>
#include <string_view>
#include <iostream>
#include <optional>
#include <unordered_map>
#include <list>
#include <queue>
#include "nonstd/string_view.hpp"
#include "asio.hpp"
#include "snowflake.h"

class SteamNetworking;

constexpr int defaultServerPort = 27279;

using SteamNetworkingConfigValues_t =
	std::vector<SteamNetworkingConfigValue_t>;

using OnSteamNetConnectionStatusChangedCallbackFuncT =
		void (*)(SteamNetConnectionStatusChangedCallback_t*);

struct HSteamListenSocketRAII {
	HSteamListenSocketRAII() = default;
	HSteamListenSocketRAII(ISteamNetworkingSockets*& sock, SteamNetworkingIPAddr& localAddr, SteamNetworkingConfigValues_t options) :
		data(sock->CreateListenSocketIP(localAddr, options.size(), options.data())),
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

struct HSteamNetPollGroupRAII {
	HSteamNetPollGroupRAII() = default;
	HSteamNetPollGroupRAII(ISteamNetworkingSockets* _socket) :
		socket(_socket)
	{
		pollGroup = socket->CreatePollGroup();
		if (pollGroup == k_HSteamNetPollGroup_Invalid)
			std::cout << "Failed create poll group needed for listening\n";
	}
	~HSteamNetPollGroupRAII() {
		socket->DestroyPollGroup(pollGroup);
		pollGroup = k_HSteamNetPollGroup_Invalid;
	}
	HSteamNetPollGroup pollGroup;
private:
	ISteamNetworkingSockets* socket;
};

class SteamNetworking {
public:
	using ObjPtrList = std::list<std::weak_ptr<SteamNetworking>>;
	using ObjPtrMap = std::unordered_map<
		HSteamListenSocket, std::weak_ptr<SteamNetworking>&>;

	~SteamNetworking() {
		constexpr int64_t targetWaitTime = 1000000000 / 2; //1 second / 2
		asio::steady_timer timer{ iOContext };
		timer.expires_after(std::chrono::nanoseconds(targetWaitTime));
		timer.wait();
		GameNetworkingSockets_Kill();
		if (listIterator != allObjs.end())
			allObjs.erase(listIterator);
		if (mapIterator != objMap.end())
			objMap.erase(mapIterator);
	}

	template<class Child, class... Types>
	static std::shared_ptr<Child> makeObj(Types&&... arguments) {
		auto objPtr = std::make_shared<Child>(
			std::forward<Types>(arguments)...);
		auto basePtr = std::static_pointer_cast<SteamNetworking>(
			objPtr);
		allObjs.push_back(std::weak_ptr<SteamNetworking>(basePtr));
		basePtr->listIterator = std::prev(allObjs.end());
		return objPtr;
	}

	template<class Child>
	static OnSteamNetConnectionStatusChangedCallbackFuncT
	getOnSteamNetConnectionStatusChangedCallback() {
		return [](SteamNetConnectionStatusChangedCallback_t* info) {
			auto foundChild = objMap.find(info->m_info.m_hListenSocket);
			if (foundChild == objMap.end()) {
				std::cout << "Error couldn't find listen socket handle "
					"in object map\n";
				return;
			}
			if (auto shardPtrToChild = foundChild->second.lock()) {
				std::static_pointer_cast<Child>(shardPtrToChild)->
					OnSteamNetConnectionStatusChanged(info);
			} else {
				std::cout << "Error couldn't lock weak pointer "
					"pointing to a Steam Networking object\n";
				return;
			}
		};
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
		sockets->RunCallbacks();

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
	template<class Child>
	SteamNetworking(asio::io_context& _iOContext, Child& child) :
		iOContext(_iOContext),
		pollTimer(iOContext),
		listIterator(allObjs.end()), //end shouldn't ever change because of list
		                        //so it can be use to check if it was set before
		mapIterator(objMap.end())
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

	bool setHStreamListenSocket(HSteamListenSocket handle) {
		if (listIterator != allObjs.end()) {
			objMap.insert({handle, *listIterator});
			return true;
		}
		std::cout <<
			"Invalid Steam Networking Object List iterator "
			"found on object\n";
		return false;
	}

	ISteamNetworkingSockets* sockets = nullptr;
	asio::io_context& iOContext;
	asio::steady_timer pollTimer;
	ObjPtrList::iterator listIterator;
	ObjPtrMap::iterator mapIterator;

	//delayed messages queue for debugging
	std::queue<asio::steady_timer> delayedQueue;
	
	static ObjPtrList allObjs;
	static ObjPtrMap objMap;
};

SteamNetworking::ObjPtrList SteamNetworking::allObjs{};
SteamNetworking::ObjPtrMap SteamNetworking::objMap{};

struct SteamNetworkConfigValue {
	SteamNetworkConfigValue() = default;
	template<class CallbackHandler>
	SteamNetworkConfigValue(CallbackHandler& handler) {
		//to do hope valve's steam networking team fixes this BS
		SteamNetworkingConfigValue_t statusChangedOption;
		statusChangedOption.SetPtr(
			k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
			reinterpret_cast<void*>(SteamNetworking::
				getOnSteamNetConnectionStatusChangedCallback<
					CallbackHandler>())
		);
		data.push_back(statusChangedOption);
	}
	SteamNetworkingConfigValues_t data;
};