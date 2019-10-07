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
		return *(begin() + (offset % maxSize))
	}
	inline _Type& getFromTail(size_t offset = 0) {
		return *(end() - ((offset + 1) % maxSize));
	}

	template <class... Args>
	inline iterator emplaceBack(Args&& ... arguments) {
		const size_t newIndex = newIndex();
		values[newIndex] = std::move(Type{ std::forward<Args>(arguments)... }));
		return begin() + newIndex;
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

	GameCoreSystem(unsigned int * _width, unsigned int * _height, void * _buffer, Platform& _system)
		: width(_width)
		, height(_height)
		, buffer(_buffer)
		, system(_system)
	{
		//set up filament
		engine = filament::Engine::create();
		swapChain = engine->createSwapChain(system.getWindow());
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

	void update(const double timePassed) {
		//deal with input/events
		for (sys::Event sEvent = system.getEvent(); sEvent.type != sys::None; sEvent = system.getEvent()) {
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
	void * buffer;
	filament::Engine* engine;
	filament::SwapChain* swapChain;
	filament::Renderer* renderer;
	filament::Camera* camera;
	filament::View* view;
	filament::Scene* scene;

	std::list<std::function<void(const double&)>> updateFunctions;
	std::list<std::function<void()>> drawFunctions;
	InputComponent inputComponent;
	PlayerInput inputsToSend;
};

template<class Platform>
class Renderer : public GeneraticRenderer {
public:
	Renderer(unsigned int _width, unsigned int _height, Platform& _system)
		: width(_width)
		, height(_height)
		, system(_system)
	{
		//set up filament
		engine = filament::Engine::create();
		swapChain = engine->createSwapChain(system.getWindow());
		renderer = engine->createRenderer();
		view = engine->createView();
		view->setViewport(filament::Viewport{ 0, 0, _width, _height });
		scene = engine->createScene();
		view->setScene(scene);
		view->setClearColor({ 0.1, 0.125, 0.25, 1.0 });
		view->setPostProcessingEnabled(false);
		camera = engine->createCamera();
		view->setCamera(camera);
		camera->setProjection(45, (float)* width / *height, 0.1, 50);
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

template<ConnectionType _type = ConnectionType::UnknownConnectionType>
class GenericConnection {
	const ConnectionType type = _type;
};

template<ConnectionType type = ConnectionType::UnknownConnectionType>
class GenericConnectionHelper {
	using ConnectionType = int;
	static void send(ConnectionType& connection, std::string_view data) {

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

		for (std::unique_ptr<GenericConnection<>>& connection : connections) {
			switch (connection.get()->type) {
			case ConnectionType::uWebSockets:
				GenericConnectionHelper<ConnectionType::uWebSockets>::ConnectionType* connection =
					static_cast<GenericConnectionHelper<ConnectionType::uWebSockets>::ConnectionType*>connection.get();
				GenericConnectionHelper<ConnectionType::uWebSockets>::send(connection, gameStates.getFromTail());
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
	std::list<std::unique_ptr<GenericConnection<>>> connections;
};