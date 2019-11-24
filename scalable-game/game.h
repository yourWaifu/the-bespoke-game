#pragma once
#include <core.h>
#include <cstdint>
#include <unordered_set>

#include "state.pb.h"

class ClientCharacter;

class BaseCharacter : public EntityHandler {
public:
	using Core = GenericCore;

	enum class ControllerType {
		KeyboardPlayer1,
		KeyboardPlayer2
	};

	BaseCharacter(Core& _core)
		: core(_core) {

		id = core.getSnowflakeGenerator().generate(*this);

		//to do, it might be better to check if character was set up in core
		//instead of doing this
		static bool isSetUpDone = false;
		if(isSetUpDone == true)
			return;

		//set up update function
		onTick = bind<EntityHandler::State, CharacterState, void, const double&>(CharacterState::updateState);

		static constexpr const char actionKey[] = "spin";
		static constexpr const char moveLeftKey[] = "move left";
		static constexpr const char moveRightKey[] = "move right";
		static constexpr const char moveUpKey[] = "move up";
		static constexpr const char moveDownKey[] = "move down";

		InputComponent& inputComponent = core.getInputComponent();
		inputComponent.bindAction<CharacterState, void>(actionKey   , CharacterState::onSpinPress , CharacterState::onSpinDown );
		inputComponent.bindAction<CharacterState, void>(moveLeftKey , CharacterState::onLeftPress , CharacterState::onLeftStop );
		inputComponent.bindAction<CharacterState, void>(moveRightKey, CharacterState::onRightPress, CharacterState::onRightStop);
		inputComponent.bindAction<CharacterState, void>(moveUpKey   , CharacterState::onUpPress   , CharacterState::onUpStop   );
		inputComponent.bindAction<CharacterState, void>(moveDownKey , CharacterState::onDownPress , CharacterState::onDownStop );

		//set up inputs
		sys::KeyCode actionBind;
		sys::KeyCode moveLeftBind;
		sys::KeyCode moveRightBind;
		sys::KeyCode moveUpBind;
		sys::KeyCode moveDownBind;

		ControllerType type = ControllerType::KeyboardPlayer1;

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

		/*
		inputComponent.bind(action   , actionBind   );
		inputComponent.bind(moveLeft , moveLeftBind );
		inputComponent.bind(moveRight, moveRightBind);
		inputComponent.bind(moveUp   , moveUpBind   );
		inputComponent.bind(moveDown , moveDownBind );
		*/

		isSetUpDone = true;
	}

	void setUpControllerInputs(ControllerType type) {
		InputComponent& inputComponent = core.getInputComponent();
	}

	inline Snowflake<BaseCharacter>& getID() {
		return id;
	}

	inline Core& getCore() {
		return core;
	}

	~BaseCharacter() {
		
	}

	class CharacterState : public EntityHandler::State {
	public:
		constexpr CharacterState() = default;

		CharacterState(const GameProtocol::CharacterState& state)
			: id(state.id())
			, directionX(state.directionx())
			, directionY(state.directiony())
			, spinButton(state.spinbutton())
			, velcoity(state.velocity())
			, positionX(state.positionx())
			, positionY(state.positiony())
			, spin(state.spin())
		{}

		CharacterState(const std::string& data)
			: CharacterState(cast(data))
		{}

		static GameProtocol::CharacterState cast(const std::string& data) {
			GameProtocol::CharacterState state;
			state.ParseFromString(data);
			return state;
		}

		operator GameProtocol::CharacterState() const {
			GameProtocol::CharacterState data;
			data.set_id(id);
			data.set_directionx(directionX);
			data.set_directiony(directionY);
			data.set_spinbutton(spinButton);
			data.set_velocity(velcoity);
			data.set_positionx(positionX);
			data.set_positiony(positionY);
			return data;
		}

		constexpr void update(const double& timePassed) noexcept {
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

		constexpr static inline void updateState(CharacterState& state, const double& timePassed) noexcept {
			return state.update(timePassed);
		}

		constexpr static const CharacterState getNextState(const CharacterState& state, const double& timePassed) noexcept {
			CharacterState newState = state;
			newState.update(timePassed);
			return state;
		}

		inline constexpr void clampDirections() noexcept {
			//it's possiabe for direction to be higher then 1 or lower then -1
			//so they need to be clamped
			directionX = 1 < directionX  ?  1 : directionX;
			directionX = directionX < -1 ? -1 : directionX;
			directionY = 1 < directionY  ?  1 : directionY;
			directionY = directionY < -1 ? -1 : directionY;
		}

		inline constexpr static void clampDirections(CharacterState& state) {
			state.clampDirections();
		}

		template<int d>
		constexpr void onPressDirection(double& direction) noexcept {
			direction += d;
			clampDirections();
		}

		template<int d>
		constexpr void onStopDirection(double& direction) noexcept {
			direction -= d;
			clampDirections();
		}

		constexpr inline static void onLeftPress (CharacterState& state) noexcept {state.onPressDirection<-1>(state.directionX);}
		constexpr inline static void onRightPress(CharacterState& state) noexcept {state.onPressDirection< 1>(state.directionX);}
		constexpr inline static void onUpPress   (CharacterState& state) noexcept {state.onPressDirection< 1>(state.directionY);}
		constexpr inline static void onDownPress (CharacterState& state) noexcept {state.onPressDirection<-1>(state.directionY);}
		constexpr inline static void onSpinPress (CharacterState& state) noexcept {state.spinButton = sys::DOWN;}
		constexpr inline static void onLeftStop  (CharacterState& state) noexcept {state.onStopDirection <-1>(state.directionX);}
		constexpr inline static void onRightStop (CharacterState& state) noexcept {state.onStopDirection < 1>(state.directionX);}
		constexpr inline static void onUpStop    (CharacterState& state) noexcept {state.onStopDirection < 1>(state.directionY);}
		constexpr inline static void onDownStop  (CharacterState& state) noexcept {state.onStopDirection <-1>(state.directionY);}
		constexpr inline static void onSpinDown  (CharacterState& state) noexcept {state.spinButton = sys::UP;}

		Snowflake<BaseCharacter> id;
		double directionX = 0;
		double directionY = 0;
		bool spinButton = sys::UP;
		double velcoity = 0;
		double positionX = 0;
		double positionY = 0;
		double spin = 0;
	private:
	};

	struct TypeHelper {
		using State = BaseCharacter::CharacterState;
	};
private:
	Snowflake<BaseCharacter> id;
	Core& core;
};

class CharacterRenderable {
public:
	CharacterRenderable(Renderer& _renderer)
		: renderer(_renderer) {
		filament::Engine*& engine = renderer.getRenderEngine();
		filament::Scene*& scene = renderer.getRenderScene();

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
	}

	~CharacterRenderable() {
		filament::Engine*& engine = renderer.getRenderEngine();
		engine->destroy(renderable);
		engine->destroy(materialInstance);
		engine->destroy(material);
		engine->destroy(indexBuffer);
		engine->destroy(vertexBuffer);
		engine->destroy(texture);
	}
	
	inline utils::Entity& getRenderable() {
		return renderable;
	}

	friend ClientCharacter;
private:
	Renderer& renderer;
	filament::VertexBuffer* vertexBuffer;
	filament::IndexBuffer* indexBuffer;
	filament::Material* material;
	filament::MaterialInstance* materialInstance;
	utils::Entity renderable;
	filament::Texture* texture;
};

class ClientCharacter : public BaseCharacter {
public:
	ClientCharacter(BaseCharacter::Core& _core)
		: BaseCharacter(_core)
		, renderable(_core.getRenderer()) {
		renderable.renderer.addDrawFunction(
			[&]() {
				if (state == nullptr)
					return;
				auto characterState = static_cast<CharacterState&>(*state);
				filament::Engine*& engine = renderable.renderer.getRenderEngine();
				auto rotation = filament::math::mat4f::rotation(
					(filament::math::F_PI * characterState.spin * 2), filament::math::float3{ 0, 1, 0 });
				auto translation = filament::math::mat4f::translation(
					filament::math::vec3<double>{ characterState.positionX, characterState.positionY, 0 });
				auto& transformManager = engine->getTransformManager();
				transformManager.setTransform(transformManager.getInstance(renderable.getRenderable()), translation * rotation);
			}
		);
	}
private:
	CharacterRenderable renderable;
};

struct ServerTypeHelper {
	using Core = GenericCore;
	using Character = BaseCharacter;
};

struct ClientTypeHelper {
	using Core = GenericClientCore;
	using Character = ClientCharacter;
};

template<class TypeHelper = ServerTypeHelper>
class Game : public TypeHelper {
public:
	using TH = TypeHelper;
	using Character = typename TH::Character; 
	using Core = typename TH::Core;
	Game(GenericCore& _core)
		: core(static_cast<Core&>(_core)) {
	}

	Snowflake<BaseCharacter> createCharacter() {
		characters.emplace_front(core);
		characterMap.emplace(characters.front().getID(), characters.front());
		return characters.front().getID();
	}
protected:
	Core& core;
	std::list<Character> characters;
	std::unordered_map<Snowflake<BaseCharacter>::RawType, Character&> characterMap;
};

class GameClient : public Game<ClientTypeHelper> {
public:
	using Game = Game<ClientTypeHelper>;
	using TH = Game::TH;
	GameClient(TH::Core& _core)
		: Game(_core) {
		_core.getRenderer().addDrawFunction(
			[&]() {
				_core.getRenderer().getRenderCamera()->lookAt({ 0, 0, 30 }, { 0, 0, 0 }, { 0, 1, 0 });
			}
		);
		_core.setOnEntity([&](const connection::Entity& entity) {
			BaseCharacter::CharacterState characterState(entity.data());
			auto foundCharacter = characterMap.find(characterState.id);
			if (foundCharacter != characterMap.end())
				foundCharacter->second.onNewState(characterState);
		});
	}
private:
	Snowflake<TH::Character> playerID;
};