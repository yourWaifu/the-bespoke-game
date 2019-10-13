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
#include "generated/resources/resources.h"
#include "connection.pb.h"
#include <nonstd/string_view.hpp>

#include "input.h"

//forward declear
template<class GameState>
class ServerCoreSystem;

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

//This is used for forward casting
class GeneraticRenderer {

};

//forward declear
template<class Platform>
class Renderer;

class GenericCore {
public:
	using UpdateFunction = std::function<void(const double&)>;
	using DrawFunction = std::function<void()>;

	inline InputComponent& getInputComponent() noexcept {
		return inputComponent;
	}

	//to do remove dup code
	template<typename Function>
	inline std::function<void(const double&)>& addUpdateFunction(Function function) noexcept {
		updateFunctions.emplace_front(function);
		return updateFunctions.front();
	}

	template<typename Function>
	inline std::function<void()>& addDrawFunction(Function function) noexcept {
		drawFunctions.emplace_front(function);
		return drawFunctions.front();
	}

	inline void setRenderer(GeneraticRenderer*& output) {
		renderer = std::unique_ptr<GeneraticRenderer>(output);
	}

	inline const bool hasRenderer() {
		return renderer != nullptr;
	}

	template<class Platform>
	inline Renderer<Platform>& getRenderer() {
		return static_cast<Renderer<Platform>&>(*(renderer.get()));
	}

protected:
	std::unique_ptr<GeneraticRenderer> renderer = nullptr;
	std::list<UpdateFunction> updateFunctions;
	std::list<std::function<void()>> drawFunctions;
	InputComponent inputComponent;
};

template<class Platform>
class GameCoreSystem : public GenericCore {
public:

	GameCoreSystem(unsigned int * _width, unsigned int * _height, Platform& _system, void * nativeWindow)
		: width(_width)
		, height(_height)
		, system(_system)
	{
		//set up filament
		engine = filament::Engine::create();
		swapChain = engine->createSwapChain(nativeWindow);
		renderer = engine->createRenderer();
		view = engine->createView();
		view->setViewport(filament::Viewport{ 0, 0, *_width, *_height });
		scene = engine->createScene();
		view->setScene(scene);
		view->setClearColor({ 0.1, 0.125, 0.25, 1.0 });
		view->setPostProcessingEnabled(false);
		camera = engine->createCamera();
		view->setCamera(camera);
		camera->setProjection(45, (float)* width / *height, 0.1, 50);
	}

	~GameCoreSystem() {
		filament::Fence::waitAndDestroy(engine->createFence());
		engine->destroy(camera);
		engine->destroy(scene);
		engine->destroy(view);
		engine->destroy(renderer);
		engine->destroy(swapChain);
		engine->destroy(engine);
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
		//deal with input/events
		for (sys::Event sEvent = getEvent(); sEvent.type != sys::None; sEvent = getEvent()) {
			switch (sEvent.type) {
			case sys::Key:
				inputComponent.processInput(
					InputAction{
						static_cast<sys::KeyCode>(sEvent.value),
						static_cast<sys::ButtonState>(sEvent.value2)
					},
					inputsToSend
				);
			default: break;
			}
		}

		//update
		for (std::function<void(const double&)>& function : updateFunctions) {
			function(timePassed);
		}

		//render
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

	inline InputComponent& getInputComponent() noexcept {
		return inputComponent;
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

	//to do remove dup code
	template<typename Function>
	inline std::function<void(const double&)>& addUpdateFunction(Function function) noexcept {
		updateFunctions.emplace_front(function);
		return updateFunctions.front();
	}

	template<typename Function>
	inline std::function<void()>& addDrawFunction(Function function) noexcept {
		drawFunctions.emplace_front(function);
		return drawFunctions.front();
	}

private:
	Platform& system;
	unsigned int * width;
	unsigned int * height;
	filament::Engine* engine;
	filament::SwapChain* swapChain;
	filament::Renderer* renderer;
	filament::Camera* camera;
	filament::View* view;
	filament::Scene* scene;

	//Events
	sys::Event eventQueue[MAX_NUM_OF_EVENT_QUEUE] = {};
	unsigned int eventQueueHead = 0;
	unsigned int eventQueueTail = 0;

	std::list<std::function<void(const double&)>> updateFunctions;
	std::list<std::function<void()>> drawFunctions;
	InputComponent inputComponent;
	PlayerInput inputsToSend;
};

template<class Platform>
class Renderer : public GeneraticRenderer {
public:
	Renderer(unsigned int _width, unsigned int _height, Platform& _system)
		: system(_system)
	{
		//set up filament
		engine = filament::Engine::create();
		swapChain = engine->createSwapChain(_system.getWindow());
		renderer = engine->createRenderer();
		view = engine->createView();
		view->setViewport(filament::Viewport{ 0, 0, _width, _height });
		scene = engine->createScene();
		view->setScene(scene);
		view->setClearColor({ 0.1, 0.125, 0.25, 1.0 });
		view->setPostProcessingEnabled(false);
		camera = engine->createCamera();
		view->setCamera(camera);
		camera->setProjection(45, (float)_width / _height, 0.1, 50);
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

	Platform& system;
	filament::Engine* engine;
	filament::SwapChain* swapChain;
	filament::Renderer* renderer;
	filament::Camera* camera;
	filament::View* view;
	filament::Scene* scene;
};

enum ConnectionType {
	UnknownConnectionType,
	uWebSockets,
};

class GenericConnection {
public:
	ConnectionType type;
};

template<ConnectionType type = ConnectionType::UnknownConnectionType>
class GenericConnectionT : public GenericConnection {
public:
	using ConnectionType = int;
	ConnectionType actualConnection = 0;
	inline ConnectionType& getConnection() {
		return actualConnection;
	}
	static void send(ConnectionType& connection, nonstd::string_view data) {

	}
};

template<class GameState>
class ServerCoreSystem : public GenericCore {
public:
	void update(const double timePassed) {
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

		for (std::unique_ptr<GenericConnection>& connection : connections) {
			switch (connection.get()->type) {
			case ConnectionType::uWebSockets: {
				GenericConnectionT<ConnectionType::uWebSockets>::ConnectionType& actualConnection =
					static_cast<GenericConnectionT<ConnectionType::uWebSockets>*>(connection.get())->getConnection();
				GenericConnectionT<ConnectionType::uWebSockets>::send(actualConnection, gameStates.getFromTail());
			}
			default: break;
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
	std::list<std::unique_ptr<GenericConnection>> connections;
};