#pragma once
#include <stdint.h>
#include <string>
#include <list>
#include <filament/Engine.h>
#include <filament/Scene.h>
#include <filament/View.h>
#include <filament/Renderer.h>
#include <filament/FilamentAPI.h>
#include <filament/Material.h>
#include <filament/MaterialInstance.h>
#include <filament/RenderableManager.h>
#include <filament/TransformManager.h>
#include <filament/Fence.h>
#include <stb_image.h>
#include <cmath>
#include <atomic>
#include "generated/resources/resources.h"
#include "connection.pb.h"
#include <nonstd/string_view.hpp>
#include "connection-type-list.h"

#include "input.h"

//forward declearion
class GenericCore;

struct Vertex {
	filament::math::float2 position;
	filament::math::float2 uv;
};

extern const Vertex QUAD_VERTICES[4];

static constexpr uint16_t QUAD_INDICES[6] = {
	0, 1, 2,
	3, 2, 1
};

static constexpr unsigned char textureNotFoundData[16] = {
	255, 0, 0, 255,    0, 255, 0, 255,
	0, 0, 0, 255,    0, 0, 255, 255
};

//to do move this to common
template<class _Type>
class CircularBuffer {
public:
	using Type = _Type;
	using Buffer = std::vector<Type>;
	using iterator = typename Buffer::iterator;
	using const_iterator = typename Buffer::const_iterator;
	CircularBuffer() = default;
	CircularBuffer(size_t _size) : maxSize(_size) {}
	inline iterator begin() {
		return values.begin() + (head % maxSize);
	}
	inline iterator end() {
		return values.begin() + (tail % maxSize);
	}
	inline Type* data() {
		return values.data();
	}
	inline bool empty() {
		return head == tail;
	}
	inline size_t size() {
		return tail - head;
	}
	inline void clear() {
		head = tail = 0;
	}
	inline _Type& getFromHead(size_t offset = 0) {
		return *(begin() + (offset % maxSize));
	}
	inline _Type& getFromTail(size_t offset = 0) {
		return *(end() - ((offset + 1) % maxSize));
	}

	template <class... Args>
	inline iterator emplaceBack(Args&& ... arguments) {
		const size_t index = newIndex();
		values[index] = std::move(Type{ std::forward<Args>(arguments)... });
		return begin() + index;
	}
	inline void pushBack(Type& value) {
		values[newIndex()] = value;
	}
private:
	inline size_t newIndex() {
		size_t index = tail % maxSize;
		if (maxSize <= tail - head) {
			//overflow
			++head;
		}

		++tail;
		return index;
	}
	std::vector<Type> values;
	size_t maxSize = 1;
	size_t head = 0;
	size_t tail = 0;
};

//snowflake stuff
template<class Identifable>
class Snowflake {
public:
	using RawType = //this should be int64
		//since decltype doesn't evaluate its arguments
		//we can use a nullptr and do a c style cast too
		decltype(((connection::Snowflake*)nullptr)->snow());
	constexpr Snowflake() = default;
	constexpr Snowflake(RawType& _snow)
		: snow(_snow) {}
	constexpr Snowflake(const RawType& _snow)
		: snow(_snow) {}
	inline constexpr operator const RawType&() const {
		return snow;
	}
private:
	RawType snow;
};

class SnowflakeGenerator {
	public:
	SnowflakeGenerator(std::time_t& _currentTimestamp) 
		: currentTimestamp(_currentTimestamp)
		, startTimestamp(currentTimestamp - startTimestampOffset) {}
	std::atomic<int> increment = {0};
	static constexpr size_t incrementSize = 12;

	std::time_t& currentTimestamp;
	std::time_t startTimestamp; //should be a few seconds before the game starts
	static const int startTimestampOffset = 3 << 9; //arbarity number

	template<int size, uint64_t recursion = 0, int i = size>
	class OpaqueBitMask {
	public:
		const static uint64_t get = OpaqueBitMask<size, (recursion << 1) | 1, i - 1>::get;
	};

	template<int size, uint64_t recursion>
	class OpaqueBitMask<size, recursion, 0> {
	public:
		const static uint64_t get = recursion;
	};

	template<class identifiable>
	Snowflake<identifiable> generate(identifiable& object) noexcept {
		typename Snowflake<identifiable>::RawType target;
		constexpr int incrementSize = 12;
		target = increment & OpaqueBitMask<incrementSize>::get;
		increment.fetch_add(1);
		target = target | ((currentTimestamp - startTimestamp) << incrementSize);
		return Snowflake<identifiable>(target);
	}
};

class Renderer {
public:
	Renderer(unsigned int& _width, unsigned int& _height, void*& windowHandle)
		: width(_width)
		, height(_height)
	{
		//set up filament
		engine = filament::Engine::create();
		swapChain = engine->createSwapChain(windowHandle);
		renderer = engine->createRenderer();
		view = engine->createView();
		view->setViewport(filament::Viewport{ 0, 0, _width, _height });
		scene = engine->createScene();
		view->setScene(scene);
		view->setClearColor({ 0.1, 0.125, 0.25, 1.0 });
		view->setPostProcessingEnabled(false);
		camera = engine->createCamera();
		view->setCamera(camera);
		camera->setProjection(45, (float) width / height, 0.1, 50);
	}

	~Renderer() {
		filament::Fence::waitAndDestroy(engine->createFence());
		engine->destroy(camera);
		engine->destroy(scene);
		engine->destroy(view);
		engine->destroy(renderer);
		engine->destroy(swapChain);
		engine->destroy(engine);
	}

	inline filament::Engine*& getRenderEngine() noexcept {
		return engine;
	}

	inline filament::Scene*& getRenderScene() noexcept {
		return scene;
	}

	inline filament::Camera*& getRenderCamera() noexcept {
		return camera;
	}

	template<typename Function>
	inline std::function<void()>& addDrawFunction(Function function) noexcept {
		drawFunctions.emplace_front(function);
		return drawFunctions.front();
	}

	inline void render() {
		for (std::function<void()>& function : drawFunctions) {
			function();
		}

		//wait for the gpu
		//it's important that there's as little time between waitAndDestroy and render
		//as the gpu is idle
		filament::Fence::waitAndDestroy(engine->createFence());

		// beginFrame() returns false if we need to skip a frame
		if (renderer->beginFrame(swapChain)) {
			// for each View
			renderer->render(view);
			renderer->endFrame();
		}
	}

private:
	unsigned int& width;
	unsigned int& height;

	filament::Engine* engine;
	filament::SwapChain* swapChain;
	filament::Renderer* renderer;
	filament::Camera* camera;
	filament::View* view;
	filament::Scene* scene;

	std::list<std::function<void()>> drawFunctions;
};

class GenericCore {
public:
	using UpdateFunction = std::function<void(const double&)>;
	using DrawFunction = std::function<void()>;

	GenericCore(unsigned int& _width, unsigned int& _height, void*& windowHandle)
		: renderer(std::unique_ptr<Renderer>(new Renderer{ _width, _height, windowHandle }))
	{}

	inline InputComponent& getInputComponent() noexcept {
		return inputComponent;
	}

	//to do remove dup code
	template<typename Function>
	inline std::function<void(const double&)>& addUpdateFunction(Function function) noexcept {
		updateFunctions.emplace_front(function);
		return updateFunctions.front();
	}

	inline void setRenderer(Renderer*& output) {
		renderer = std::unique_ptr<Renderer>(output);
	}

	inline const bool hasRenderer() {
		return renderer != nullptr;
	}

	inline Renderer& getRenderer() {
		return static_cast<Renderer&>(*(renderer.get()));
	}

	inline SnowflakeGenerator& getSnowflakeGenerator() {
		return snowflakeGenerator;
	}

protected:
	std::unique_ptr<Renderer> renderer = nullptr;
	std::list<UpdateFunction> updateFunctions;
	InputComponent inputComponent;
	std::time_t timeSinceStart = 0;
	SnowflakeGenerator snowflakeGenerator = {timeSinceStart};
};

class GenericClientCore : public GenericCore {
public:
	using OnEntityFunction = std::function<void(const connection::Entity&)>;
	using GenericCore::GenericCore;
	template<class Function>
	void setOnEntity(Function function) {
		onEntity = function;
	}
protected:
	OnEntityFunction onEntity;
};

template<class Platform>
class GameCoreSystem : public GenericClientCore {
public:

	GameCoreSystem(unsigned int * _width, unsigned int * _height, Platform& _system, void* windowHandle)
		: GenericClientCore(*_width, *_height, windowHandle)
		, system(_system)
	{
	}

	~GameCoreSystem() {
	}

#define MAX_NUM_OF_EVENT_QUEUE 256
#define MAX_EVENT_MASK (MAX_NUM_OF_EVENT_QUEUE - 1)

	sys::Event getEvent() {
		if (eventQueueHead < eventQueueTail) {
			++eventQueueHead;
			return eventQueue[(eventQueueHead - 1) & MAX_EVENT_MASK];
		}
		eventQueueHead = eventQueueTail = 0;
		return { sys::None, 0, 0, 0 };
	}

	void addEventToQueue(sys::EventType type, int value, int value2, int deviceNumber) {
		sys::Event * newEvent = &eventQueue[eventQueueTail & MAX_EVENT_MASK];

		if (MAX_NUM_OF_EVENT_QUEUE <= eventQueueTail - eventQueueHead) {
			//overflow
			++eventQueueHead;
		}
		
		++eventQueueTail;
		*newEvent = { type, value, value2, deviceNumber };
	}

	void update(const double timePassed) {
		//
		//deal with input/events
		//

		//state input from server
		for (int i = 0; i < entities.entities_size(); ++i) {
			const connection::Entity& entity = entities.entities(i);
			onEntity(entity);
		}

		//state input from client
		for (sys::Event sEvent = getEvent(); sEvent.type != sys::None; sEvent = getEvent()) {
			switch (sEvent.type) {
			case sys::Key:
			/*
				inputComponent.processInput(
					InputAction{
						static_cast<sys::KeyCode>(sEvent.value),
						static_cast<sys::ButtonState>(sEvent.value2)
					},
					inputsToSend
				);
			*/
			default: break;
			}
		}

		//update
		timeSinceStart += timePassed;
		for (std::function<void(const double&)>& function : updateFunctions) {
			function(timePassed);
		}

		if (hasRenderer())
			getRenderer().render();
	}

	void onTick() {
		connection::Payload payload;
		connection::Operation* operation = payload.add_operations();
		connection::OperationCode opCode;
		opCode.set_code(connection::OperationCode::PING);
		operation->set_allocated_opcode(&opCode);
		std::string* parts = operation->add_parts();
		connection::Ping ping;
		ping.set_timestamp(timeSinceStart);
		ping.SerializeToString(parts);
	}

	void onMessage(nonstd::string_view message) {
		//note: life time of message is likely at the end of this scope
		connection::Payload payload;
		if (!payload.ParseFromArray(message.data(), message.size()))
			return;
		for (int i = 0; i < payload.operations_size(); ++i) {
			auto operation = payload.operations(i);
			switch(operation.opcode().code()) {
			case connection::OperationCode::PING: {
				connection::Ping ping;
				ping.ParseFromString(operation.parts(0));
				roundTripTime = timeSinceStart - ping.timestamp();
				break;
				}
			default:
				break;
			}
		}
	}

	inline InputComponent& getInputComponent() noexcept {
		return inputComponent;
	}

	//to do remove dup code
	template<typename Function>
	inline std::function<void(const double&)>& addUpdateFunction(Function function) noexcept {
		updateFunctions.emplace_front(function);
		return updateFunctions.front();
	}

	template<typename Type>
	inline Snowflake<Type> generateSnowflake() noexcept {
		return snowflakeGenerator.generate<Type>();
	}
private:
	Platform& system;

	//Events
	sys::Event eventQueue[MAX_NUM_OF_EVENT_QUEUE] = {};
	unsigned int eventQueueHead = 0;
	unsigned int eventQueueTail = 0;

	std::time_t roundTripTime = 0;
	PlayerInput inputsToSend;

	connection::EntityArray entities;
};

using ConnectionReceiveFunction = std::function<void(const nonstd::string_view&)>;

class UniqueConnectionPtrHandler {
public:
	using HandleType = std::unique_ptr<GenericConnection>;
	void send(nonstd::string_view data) {
		switch (connection.get()->type) {
		case ConnectionType::uWebSockets:
			//send current state
		default: break;
		}
	}
	void receive(ConnectionReceiveFunction& handler) {

	}
	HandleType connection;
};

template<class GameCore>
class InternalCommutationHandler {
	using HandleType = GameCore;
	//lifetime of the server core should be less then
	//the lifetime of the game core.
	void send(nonstd::string_view data) {

	}
	void receive(ConnectionReceiveFunction& handler) {

	}
};

template<class GameState, class ConnectionHandler>
class ServerCoreSystem : public GenericCore {
public:
	using Connection = ConnectionHandler;
	void update(const double timePassed) {
		static std::string sendBuffer(1000, '\0');
		sendBuffer.resize(0); //hope that resize doesn't change the capacity of the buffer
		nonstd::string_view whatToSend(sendBuffer.data(), 0);
		//copy last state
		//gameStates.pushBack(gameStates.getFromTail());

		//deal with data from clients
		for (PlayerInput& input : playerInputs) {
			inputComponent.processInput(input);
		}

		//update
		for (std::function<void(const double&)>& function : updateFunctions) {
			function(timePassed);
		}

		if(whatToSend.empty())
			return;

		for (std::unique_ptr<Connection>& connection : connections) {
			connection->send(whatToSend);
		}
	}

	void onMessage(nonstd::string_view message) {
		static std::string sendBuffer = {};
		connection::Payload payload;
		if (!payload.ParseFromArray(message.data(), message.size()))
			return;
		for (int i = 0; i < payload.operations_size(); ++i) {
			auto operation = payload.operations(i);
			switch(operation.opcode().code()) {
			case connection::OperationCode::PING:
				//send back ping
				break;
			default:
				break;
			}
		}
	}

	void onPlayerConnect() {
		//Say hello and send server info

	}

private:
	uint32_t currentTick = 0;
	//circular buffer
	CircularBuffer<GameState> gameStates;
	CircularBuffer<PlayerInput> playerInputs;
	std::list<std::unique_ptr<Connection>> connections;
	std::string receiveBuffer;
};