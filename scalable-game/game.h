#pragma once
#include <core.h>
#include <cstdint>
#include <unordered_set>

class BaseCharacter {
public:
	using Core = GenericCore;

	inline void clampDirections() noexcept {
		//it's possiabe for direction to be higher then 1 or lower then -1
		//so they need to be clamped
		directionX = 1 < directionX  ?  1 : directionX;
		directionX = directionX < -1 ? -1 : directionX;
		directionY = 1 < directionY  ?  1 : directionY;
		directionY = directionY < -1 ? -1 : directionY;
	}

	template<int d>
	std::function<void()> createOnPressDirection(double& direction) {
		return [&]() {
			direction += d;
			clampDirections();
		};
	}

	template<int d>
	std::function<void()> createOnStopDirection(double& direction) {
		return [&]() {
			direction -= d;
			clampDirections();
		};
	}

	enum class ControllerType {
		KeyboardPlayer1,
		KeyboardPlayer2
	};

	BaseCharacter(Core& _core, ControllerType type)
		: core(_core) {
		filament::Engine*& engine = core.getRenderer().getRenderEngine();
		filament::Scene*& scene = core.getRenderer().getRenderScene();

		//load texture
		int w, h, n;
		const unsigned char* data = stbi_load("assets/textures/link.png", &w, &h, &n, 4);
		if (data == nullptr) {
			w = 2;
			h = 2;
			data = &textureNotFoundData[0];
		}
		filament::Texture::PixelBufferDescriptor buffer(data, size_t(w * h * 4),
			filament::Texture::Format::RGBA, filament::Texture::Type::UBYTE);
		texture = filament::Texture::Builder()
			.width(uint32_t(w))
			.height(uint32_t(h))
			.levels(1)
			.sampler(filament::Texture::Sampler::SAMPLER_2D)
			.format(filament::Texture::InternalFormat::RGBA8)
			.build(*engine);
		texture->setImage(*engine, 0, std::move(buffer));
		filament::TextureSampler sampler(filament::TextureSampler::MinFilter::LINEAR, filament::TextureSampler::MagFilter::NEAREST);

		// build a quad
		vertexBuffer = filament::VertexBuffer::Builder()
			.vertexCount(4)
			.bufferCount(1)
			.attribute(filament::VertexAttribute::POSITION, 0, filament::VertexBuffer::AttributeType::FLOAT2, 0, 16)
			.attribute(filament::VertexAttribute::UV0, 0, filament::VertexBuffer::AttributeType::FLOAT2, 8, 16)
			.build(*engine);
		vertexBuffer->setBufferAt(*engine, 0,
			filament::VertexBuffer::BufferDescriptor(QUAD_VERTICES, 64, nullptr));
		indexBuffer = filament::IndexBuffer::Builder()
			.indexCount(6)
			.bufferType(filament::IndexBuffer::IndexType::USHORT)
			.build(*engine);
		indexBuffer->setBuffer(*engine,
			filament::IndexBuffer::BufferDescriptor(QUAD_INDICES, sizeof(QUAD_INDICES), nullptr));
		material = filament::Material::Builder()
			.package(RESOURCES_BAKED_TEXTURE_DATA, RESOURCES_BAKED_TEXTURE_SIZE)
			.build(*engine);
		materialInstance = material->createInstance();
		materialInstance->setParameter("albedo", texture, sampler);
		renderable = utils::EntityManager::get().create();
		filament::RenderableManager::Builder(1)
			.boundingBox({ { -1, -1, -1 }, { 1, 1, 1 } })
			.material(0, materialInstance)
			.geometry(0, filament::RenderableManager::PrimitiveType::TRIANGLES, vertexBuffer, indexBuffer, 0, 6)
			.culling(true)
			.build(*engine, renderable);
		scene->addEntity(renderable);

		//set up update function
		core.addUpdateFunction(
			[&](const double& timePassed) {
				update(timePassed);
			}
		);
		core.addDrawFunction(
			[&]() {
				filament::Engine*& engine = core.getRenderer().getRenderEngine();
				auto rotation = filament::math::mat4f::rotation(
					(filament::math::details::PI * spin * 2), filament::math::float3{ 0, 1, 0 });
				auto translation = filament::math::mat4f::translation(
					filament::math::vec3<double>{ positionX, positionY, 0 });
				auto& transformManager = engine->getTransformManager();
				transformManager.setTransform(transformManager.getInstance(renderable), translation * rotation);
			}
		);

		//set up inputs
		sys::KeyCode actionBind;
		sys::KeyCode moveLeftBind;
		sys::KeyCode moveRightBind;
		sys::KeyCode moveUpBind;
		sys::KeyCode moveDownBind;

		switch (type) {
		case ControllerType::KeyboardPlayer1:
			actionBind = sys::K_SPACE;
			moveLeftBind = sys::K_a;
			moveRightBind = sys::K_d;
			moveUpBind = sys::K_w;
			moveDownBind = sys::K_s;
			break;
		case ControllerType::KeyboardPlayer2:
			actionBind = sys::K_o;
			moveLeftBind = sys::K_j;
			moveRightBind = sys::K_l;
			moveUpBind = sys::K_i;
			moveDownBind = sys::K_k;
			break;
		default:
			break;
		}

		InputComponent& inputComponent = core.getInputComponent();
		InputComponent::ActionFunctionPair action = inputComponent.bindAction(
			"spin", [&]() {
				spinButton = sys::DOWN;
			}, [&]() {
				spinButton = sys::UP;
			});
		inputComponent.bind(action, actionBind);
		InputComponent::ActionFunctionPair moveLeft = inputComponent.bindAction(
			"move left",
			createOnPressDirection<-1>(directionX),
			createOnStopDirection<-1>(directionX));
		inputComponent.bind(moveLeft, moveLeftBind);
		InputComponent::ActionFunctionPair moveRight = inputComponent.bindAction(
			"move right",
			createOnPressDirection<1>(directionX),
			createOnStopDirection<1>(directionX));
		inputComponent.bind(moveRight, moveRightBind);
		InputComponent::ActionFunctionPair moveUp = inputComponent.bindAction(
			"move up",
			createOnPressDirection<1>(directionY),
			createOnStopDirection<1>(directionY));
		inputComponent.bind(moveUp, moveUpBind);
		InputComponent::ActionFunctionPair moveDown = inputComponent.bindAction(
			"move down",
			createOnPressDirection<-1>(directionY),
			createOnStopDirection<-1>(directionY));
		inputComponent.bind(moveDown, moveDownBind);
	}

	void update(const double& timePassed) {
		const double frictionCoefficient = 0.4;
		const double frictionNormal = 1;
		velcoity += 8 * timePassed * static_cast<bool>(spinButton);
		velcoity = velcoity - (velcoity * (timePassed * (frictionCoefficient * frictionNormal)));
		spin += timePassed * velcoity;
		spin = spin - static_cast<int64_t>(spin);

		const double speed = 10.0;
		positionX += directionX * speed * timePassed;
		positionY += directionY * speed * timePassed;
	}

	~BaseCharacter() {
		filament::Engine*& engine = core.getRenderer().getRenderEngine();
		engine->destroy(renderable);
		engine->destroy(materialInstance);
		engine->destroy(material);
		engine->destroy(indexBuffer);
		engine->destroy(vertexBuffer);
		engine->destroy(texture);
	}
private:
	Core& core;
	filament::VertexBuffer* vertexBuffer;
	filament::IndexBuffer* indexBuffer;
	filament::Material* material;
	filament::MaterialInstance* materialInstance;
	utils::Entity renderable;
	filament::Texture* texture;

	double directionX = 0;
	double directionY = 0;
	bool spinButton = sys::UP;
	double velcoity = 0;
	double positionX = 0;
	double positionY = 0;
	double spin = 0;
};

class Game {
public:
	using Character = BaseCharacter;
	using Core = GenericCore;
	Game(GenericCore& _core) {
		characters.emplace_front(_core, BaseCharacter::ControllerType::KeyboardPlayer1);
		characters.emplace_front(_core, BaseCharacter::ControllerType::KeyboardPlayer2);
		_core.addDrawFunction(
			[&]() {
				_core.getRenderer().camera->lookAt({ 0, 0, 30 }, { 0, 0, 0 }, { 0, 1, 0 });
			}
		);
	}
private:
	std::list<Character> characters;
};