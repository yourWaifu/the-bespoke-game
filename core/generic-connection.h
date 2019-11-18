#pragma once
#include <nonstd/string_view.hpp>

enum ConnectionType {
	UnknownConnectionType,
	uWebSockets,
};

class GenericConnection {
public:
	ConnectionType type;
};

template<class ConnectionTypeHelper>
class ConnectionHelper : public ConnectionTypeHelper {
public:
	using CTH = ConnectionTypeHelper;
	static void send(GenericConnection*& connection, nonstd::string_view data) {
		typename CTH::ConnectionType& actualConnection = CTH::getConnection(connection);
		actualConnection.send(data);
	}
	template<class ReceiveHandler>
	static void receive(GenericConnection*& connection, ReceiveHandler handler) {
		typename CTH::ConnectionType& actualConnection = CTH::getConnection(connection);
		actualConnection.receive(handler);
	}
};

template<ConnectionType type = ConnectionType::UnknownConnectionType>
class ConnectionTypeHelper {
	public:
		using ConnectionType = int;
		static int num;
		static ConnectionType& getConnection(GenericConnection*& connection) {
			return num;
		} 
};

template<ConnectionType type = ConnectionType::UnknownConnectionType>
class GenericConnectionT : public ConnectionHelper<ConnectionTypeHelper<type>> {};