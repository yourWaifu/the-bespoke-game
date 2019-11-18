#pragma once
#include <functional>

template<class Base, class ReturnType, class ... ParmaTypes>
using BaseDelagate = std::function<ReturnType(Base&, ParmaTypes...)>;

template<class Base, class D, class ReturnType, class ... ParmaTypes, class Function>
BaseDelagate<Base, ReturnType, ParmaTypes...> bind(Function f) {
	return [&](Base& b, ParmaTypes... t) { return f(static_cast<D&>(b), t...); };
}

class EntityHandler {
public:
	struct State {};
	using Delagate = BaseDelagate<State, void, const double&>;
	inline constexpr void onNewState(State& newState) noexcept {
		state = &newState;
	}
	Delagate onTick;
protected:
	State* state = nullptr;
};