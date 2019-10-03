#pragma once
#include <msgpack.hpp>

template<class DataType>
struct Payload {
	enum OperatoryCode {
		PlayerInput = 0,
		Identify = 1,
		Hello = 2,
		GameUpdate = 3,
		Example = 4, //purposely unused for the compiler to use
		InputAck = 5,
	} op = DataType::opCode;
	uint32_t tick = 0;
	DataType data;
	MSGPACK_DEFINE(op, tick, data);
};

template<class Type>
Type convert(msgpack::object_handle& handle) {
	Type data;
	handle.get().convert(data);
	return data;
};

template<Payload<void>::OperatoryCode op>
struct PayloadHelper {
	using DataType = int;
};

template<>
struct PayloadHelper<Payload<void>::OperatoryCode::Example> {
	using DataType = unsigned int;
};

template<Payload<void>::OperatoryCode op>
PayloadHelper<op>::DataType convert(msgpack::object_handle& handle) {
	return convert<PayloadHelper<op>::DataType>(handle);
};