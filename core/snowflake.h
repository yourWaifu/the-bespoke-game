#pragma once
#include <cstdint>

namespace Snowflake {
	//snowflake generator
	//structure left to right
	//Timestamp in ms : 42 bits
	//isUnoffical : 1 bit : tells the server if this needs to be replaced
	//zero bit : 1 bit : unused, for later perpose
	//increment : 20 bits
	using RawSnowflake = uint64_t;
	//note: this is a c++14 feature
	constexpr RawSnowflake timestampMask =
		0b1111111111111111111111111111111111111111110000000000000000000000;
	constexpr RawSnowflake isOfficalMask =
		0b0000000000000000000000000000000000000000001000000000000000000000;
	constexpr RawSnowflake theZeroBitMask =
		0b0000000000000000000000000000000000000000000100000000000000000000;
	constexpr RawSnowflake incrementMask =
		0b0000000000000000000000000000000000000000000011111111111111111111;

	constexpr int getStart(RawSnowflake mask, int i = 0) {
		return (mask & 0b1) == 0b0 ? getStart(mask >> 1, i + 1) : i;
	}

	constexpr RawSnowflake timestampStart  = getStart(timestampMask );
	constexpr RawSnowflake isOfficalStart  = getStart(isOfficalMask );
	constexpr RawSnowflake theZeroBitStart = getStart(theZeroBitMask);
	constexpr RawSnowflake incrementStart  = getStart(incrementMask );

	const bool isAvaiable(const RawSnowflake snowflake) {
		return snowflake != 0;
	}

	template<bool isServer = true>
	RawSnowflake generate(uint64_t timestampMS) {
		//start at 1 so that 0 can be used for unavailable
		static int increment = 1;
		RawSnowflake target = 0;
		target = target | ((timestampMS << timestampStart ) & timestampMask );
		target = target | ((isServer    << isOfficalStart ) & isOfficalMask );
		target = target | ((0           << theZeroBitStart) & theZeroBitMask);
		target = target | ((increment   << incrementStart ) & incrementMask );
		//always increment at the end or else
		increment += 1;
		return target;
	}
}