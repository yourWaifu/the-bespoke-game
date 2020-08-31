#pragma once
#include "game.h"

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
#include <filament/Viewport.h>
#include <filament/Camera.h>
#include <filament/Texture.h>
#include <filament/TextureSampler.h>
#include <filament/VertexBuffer.h>
#include <filament/IndexBuffer.h>
#include <filament/IndirectLight.h>
#include <filament/LightManager.h>

#include "imgui.h"
#include <filagui/ImGuiHelper.h>

#include <filameshio/MeshReader.h>

#include <math/vec3.h>
#include <math/norm.h>

#include <utils/EntityManager.h>

#include <stb_image.h>

#include "generated/resources/resources.h"
#include "generated/resources/cube.h"
#include "generated/resources/quad.h"

#include <SDL.h>
#include <SDL_syswm.h>

class TheWarrenRenderer;
TheWarrenRenderer* gRenderer; //used to pass to static functions
const TheWarrenState* gState;
void* gCore;

class TheWarrenRenderer {
public:
	TheWarrenRenderer(SDL_Window* window, void* windowHandle) :
		filamentRAII(*this, window, windowHandle),
		filamentGUIRAII(*this, window),
		imGuiHelper(filamentRAII.engine, filamentGUIRAII.view,
			//to do use better path detector
			"/home/hao-qi/Documents/git-repos/the-warren-game/build/client/assets/fonts/Roboto-Medium.ttf"),
		quad(*this),
		cube(*this),
		playerPrimitives(*this),
		wallPrimitives(*this),
		arrowPrimitives(*this)
	{
		//to do figure out how to automaticly do this
		for (auto& player : players) {
			player.renderable = playerPrimitives.createRenderable();
		}
		auto& transformManager = filamentRAII.engine->getTransformManager();
		auto& rendererManager = filamentRAII.engine->getRenderableManager();
		std::size_t index = 0;
		for (auto& wallEntity : wallEntities) {
			wallEntity.mesh = wallPrimitives.createMesh();

			//static walls can be set here instead of every frame
			auto& wall = walls[index];
			auto translation = filament::math::mat4f::translation(
				filament::math::vec3<decltype(walls[0].position.x)>{
					wall.position.x, wall.position.y, 20.0
				}
			);
			auto scale = filament::math::mat4f::scaling(
				filament::math::vec3<decltype(walls[0].scale.x)>{
					wall.scale.x, wall.scale.y, 40.0
				}
			);
			
			transformManager.setTransform(
				transformManager.getInstance(wallEntity.mesh.renderable),
				translation * scale
			);

			auto rendererInstance = rendererManager.getInstance(wallEntity.mesh.renderable);
			rendererManager.setCastShadows(rendererInstance, true);
			rendererManager.setReceiveShadows(rendererInstance, true);

			index += 1;
		}

		floor.mesh = wallPrimitives.createMesh();
		transformManager.setTransform(
			transformManager.getInstance(floor.mesh.renderable),
			filament::math::mat4f::translation(
				filament::math::vec3<float>{
					0.0, 0.0, -0.5
				}
			) * filament::math::mat4f::scaling(
				filament::math::vec3<float>{
					50.0, 50.0, 1.0
				}
			)
		);
		auto floorRendererInstance = rendererManager.getInstance(floor.mesh.renderable);
		rendererManager.setCastShadows(floorRendererInstance, false);
		rendererManager.setReceiveShadows(floorRendererInstance, true);
		rendererManager.setScreenSpaceContactShadows(floorRendererInstance, true);

		for (auto& arrowEntity : ArrowEntites) {
			arrowEntity.renderable = arrowPrimitives.createRenderable();
		}

		//UI
		//google's filament code
		//to do move this so that the game can change
		//screen size much easier
		SDL_Rect viewport;
		SDL_GetWindowSize(window, &viewport.w, &viewport.h);
		const int& width = viewport.w;
		const int& height = viewport.h;
		int displayWidth, displayHeight;
		SDL_GL_GetDrawableSize(window, &displayWidth, &displayHeight);

		float dpiScaleX = 1.0f;
		float dpiScaleY = 1.0f;
		{
			int virtualWidth, virtualHeight;
			SDL_GetWindowSize(window, &virtualWidth, &virtualHeight);
			dpiScaleX = (float) displayWidth / virtualWidth;
			dpiScaleY = (float) displayHeight / virtualHeight;
		}

		filamentGUIRAII.camera->setProjection(filament::Camera::Projection::ORTHO,
			0.0, width/dpiScaleX, height/dpiScaleY, 0.0,
			0.0, 1.0);

		filamentGUIRAII.view->setViewport({ 0, 0,
			static_cast<uint32_t>(width), static_cast<uint32_t>(height) });

		imGuiHelper.setDisplaySize(width, height,
			width > 0 ? ((float)displayWidth / width) : 0,
			displayHeight > 0 ? ((float)displayHeight / height) : 0);
	}
    ~TheWarrenRenderer() {
		for (auto& player : players) {
			filamentRAII.engine->destroy(player.renderable);
		}
		for (auto& wallEntity : wallEntities) {
			filamentRAII.engine->destroy(wallEntity.mesh.renderable);
		}
	}

	template<typename WindowT>
	static TheWarrenRenderer create(const WindowT& window) {
		return TheWarrenRenderer{window.window, window.getWindow()};
	}

	//we init playerLight in FilamentRAII so we need to have it here
	//or for some reason, it'll get corrupt
	utils::Entity playerLight;

	//we need this to have the filament stuff set up first before other
	//objects that need the renderer and need to do be done before the
	//constructor's {}
	struct FilamentRAII {
		FilamentRAII(
			TheWarrenRenderer& _parent,
			SDL_Window* window, void* windowHandle
		) :
			parent(_parent)
		{
			SDL_Rect viewport;
			SDL_GetWindowSize(window, &viewport.w, &viewport.h);
			const int& width = viewport.w;
			const int& height = viewport.h;

			//to do move stuff here
			auto& swapChain = parent.swapChain;
			auto& renderer = parent.renderer;
			auto& camera = parent.camera;
			auto& view = parent.view;
			auto& scene = parent.scene;

			//set up filament
			engine = filament::Engine::create();
			swapChain = engine->createSwapChain(windowHandle);
			renderer = engine->createRenderer();
			view = engine->createView();
			view->setViewport(filament::Viewport{ 0, 0, 
				static_cast<uint32_t>(width), static_cast<uint32_t>(height) });
			scene = engine->createScene();
			view->setScene(scene);
			view->setPostProcessingEnabled(false);
			camera = engine->createCamera();
			view->setCamera(camera);
			camera->setProjection(45, (float) width / height, 0.1, 50);
			renderer->setClearOptions({
				.clearColor = { 0.0f, 0.0f, 0.0f, 1.0f },
				.clear = true });

			//view renderering settings
			view->setAmbientOcclusion(filament::View::AmbientOcclusion::SSAO);
			view->setAmbientOcclusionOptions({ .quality = filament::View::QualityLevel::HIGH });

			//basic materals
			parent.bakedTexture = filament::Material::Builder()
				.package(RESOURCES_BAKED_TEXTURE_DATA, RESOURCES_BAKED_TEXTURE_SIZE)
				.build(*engine);

			//lighting
			parent.sunlight = utils::EntityManager::get().create();
			filament::LightManager::Builder(filament::LightManager::Type::SUN)
				.color(filament::LinearColor{ 0.98, 0.92, 0.89 })
				.intensity(/*110000.*/0)
				.direction(filament::math::float3{ 0.6, -1.0, -0.8 })
				.sunAngularRadius(1.9).sunHaloSize(10.0).sunHaloFalloff(80.0)
				.build(*engine, parent.sunlight);
			scene->addEntity(parent.sunlight);

			parent.backlight = utils::EntityManager::get().create();
			filament::LightManager::Builder(filament::LightManager::Type::DIRECTIONAL)
				.direction(filament::math::float3{ -0.088, 0.01, -1.0 })
				.intensity(2000.0)
				.castShadows(true)
				.build(*engine, parent.backlight);
			scene->addEntity(parent.backlight);

			parent.playerLight = utils::EntityManager::get().create();
			filament::LightManager::Builder(filament::LightManager::Type::SPOT)
				.color(filament::Color::toLinear<filament::ACCURATE>(filament::sRGBColor(0.98f, 0.92f, 0.89f)))
				.intensity(30000.0f, filament::LightManager::EFFICIENCY_LED)
				.direction({0.0f, 0.0f, -1.0f})
				.spotLightCone(M_PI_4, M_PI_2)
				.falloff(50.0f)
				.build(*engine, parent.playerLight);
		}

		~FilamentRAII() {
			engine->destroy(parent.backlight);
			engine->destroy(parent.sunlight);
			engine->destroy(parent.bakedTexture);

			filament::Fence::waitAndDestroy(engine->createFence());
			engine->destroy(parent.camera);
			engine->destroy(parent.scene);
			engine->destroy(parent.view);
			engine->destroy(parent.renderer);
			engine->destroy(parent.swapChain);
			engine->destroy(engine);
		}
		TheWarrenRenderer& parent;
		filament::Engine* engine;
	} filamentRAII;

	struct FilamentGUIRAII {
		FilamentGUIRAII(TheWarrenRenderer& _parent,
			SDL_Window* window
		) :
			parent(_parent)
		{

			view = parent.filamentRAII.engine->createView();
			camera = parent.filamentRAII.engine->createCamera();
			view->setCamera(camera);
		}
		~FilamentGUIRAII() {
			parent.filamentRAII.engine->destroy(camera);
			parent.filamentRAII.engine->destroy(view);
		}
		TheWarrenRenderer& parent;

		filament::View* view;
		filament::Camera* camera;
	} filamentGUIRAII;
	filagui::ImGuiHelper imGuiHelper;

	static constexpr int nullTextureWidth = 2;
	static constexpr int nullTextureHeight = 2;
	static constexpr int nullTextureChannels = 4;
	static constexpr unsigned char nullTexture[
		nullTextureWidth * nullTextureHeight * nullTextureChannels
	] = {
		0x88, 0x0E, 0x4F, 255,	0xFF, 0x80, 0xAB, 255,
		0xEA, 0x80, 0xFC, 255,	0x4A, 0x14, 0x8C, 255
	};

	struct SimpleTextureData {
		int w; int h;
		int channels = 4;
		const unsigned char* data = nullptr;
	};

	template<class Loader>
	struct StaticTextureBuilder {
		static filament::Texture* load(filament::Engine& engine) {
			SimpleTextureData data = Loader::load();
			filament::Texture::PixelBufferDescriptor buffer(data.data,
				size_t(data.w * data.h * data.channels),
				filament::Texture::Format::RGBA, filament::Texture::Type::UBYTE,
				static_cast<filament::Texture::PixelBufferDescriptor::Callback>([](
					void* buffer, size_t size, void*
				) {
					Loader::destroy(buffer, size);
				}));
			filament::Texture* texture = filament::Texture::Builder()
				.width(uint32_t(data.w)).height(uint32_t(data.h)).levels(1)
				.sampler(filament::Texture::Sampler::SAMPLER_2D)
				.format(filament::Texture::InternalFormat::RGBA8)
				.build(engine);
			texture->setImage(engine, 0, std::move(buffer));
			return texture;
		}
	};

	struct TextureErrorLoader {
		static SimpleTextureData load() {
			return SimpleTextureData {
				TheWarrenRenderer::nullTextureWidth,
				TheWarrenRenderer::nullTextureHeight,
				TheWarrenRenderer::nullTextureChannels,
				&TheWarrenRenderer::nullTexture[0] };
		}
		static void destroy(void* buffer, size_t size) {}
	};

	struct TextureFileLoader {
		static SimpleTextureData load(const char* filePath) {
			SimpleTextureData target;
			target.data = stbi_load(filePath, &target.w, &target.h, &target.channels, 4);
			if (target.data == nullptr) {
				return TextureErrorLoader::load();
			}
			return target;
		}
		static void destroy(void* buffer, size_t size) {
			if(buffer != &TheWarrenRenderer::nullTexture[0])
				stbi_image_free(buffer);
		}
	};

	template<const char* filePath>
	struct StaticTextureFileLoader {
		static SimpleTextureData load() {
			return TextureFileLoader::load(filePath);
		}
		static void destroy(void* buffer, size_t size) {
			TextureFileLoader::destroy(buffer, size);
		}
	};

	template<class PosType = filament::math::float2, class UVType = filament::math::float2>
	struct Vertex {
		PosType position;
		UVType uv;
	};
	using Vertex2D = Vertex<>;
	using Vertex3D = Vertex<filament::math::float3>;

	struct Quad {
		Quad(TheWarrenRenderer& _renderer) :
			renderer(_renderer)
		{
			filament::Engine& engine = *renderer.filamentRAII.engine;
			filament::Scene& scene = *renderer.scene;

			texture = TheWarrenRenderer::StaticTextureBuilder<
				TheWarrenRenderer::TextureErrorLoader>::load(engine);
			filament::TextureSampler sampler(
				filament::TextureSampler::MinFilter::LINEAR,
				filament::TextureSampler::MagFilter::NEAREST);

			materialInstance = renderer.bakedTexture->createInstance();
			materialInstance->setParameter("albedo", texture, sampler);

			mesh = createMesh(materialInstance);
		}
		~Quad() {
			renderer.filamentRAII.engine->destroy(texture);
			renderer.filamentRAII.engine->destroy(materialInstance);
			renderer.filamentRAII.engine->destroy(mesh.renderable);
			renderer.filamentRAII.engine->destroy(mesh.vertexBuffer);
			renderer.filamentRAII.engine->destroy(mesh.indexBuffer);
		}

		filamesh::MeshReader::Mesh createMesh(filament::MaterialInstance* defaultMaterial) {
			return filamesh::MeshReader::loadMeshFromBuffer(
				renderer.filamentRAII.engine, QUAD_QUAD_DATA, nullptr,
				nullptr, defaultMaterial);
		}

		TheWarrenRenderer& renderer;

		filament::Texture* texture;
		filament::MaterialInstance* materialInstance;
		filamesh::MeshReader::Mesh mesh;
	};
	Quad quad;

	template<class TextureLoader>
	struct TexturedQuad {
		TexturedQuad(TheWarrenRenderer& _renderer) :
			renderer(_renderer)
		{
			filament::Engine& engine = *renderer.filamentRAII.engine;
			filament::Scene& scene = *renderer.scene;

			texture = TextureLoader::load(engine);
			filament::TextureSampler sampler(
				filament::TextureSampler::MinFilter::LINEAR,
				filament::TextureSampler::MagFilter::NEAREST);

			materialInstance = renderer.bakedTexture->createInstance();
			materialInstance->setParameter("albedo", texture, sampler);
		}

		utils::Entity createRenderable() {
			utils::Entity renderable = utils::EntityManager::get().create();
			filament::RenderableManager::Builder(1)
				.boundingBox({ { -0.5, -0.5, -0.1 }, { 0.5, 0.5, 0.1 } })
				.material(0, materialInstance)
				.geometry(0, filament::RenderableManager::PrimitiveType::TRIANGLES,
					renderer.quad.mesh.vertexBuffer, renderer.quad.mesh.indexBuffer,
					0, renderer.quad.mesh.indexBuffer->getIndexCount())
				.culling(true)
				.build(*renderer.filamentRAII.engine, renderable);
			renderer.scene->addEntity(renderable);
			return renderable;
		}

		~TexturedQuad() {
			renderer.filamentRAII.engine->destroy(materialInstance);
			renderer.filamentRAII.engine->destroy(texture);
		}

		TheWarrenRenderer& renderer;
		
		filament::Texture* texture;
		filament::MaterialInstance* materialInstance;
	};

	struct Cube {
		Cube(TheWarrenRenderer& _renderer) :
			renderer(_renderer)
		{
			filament::Engine& engine = *renderer.filamentRAII.engine;
		}
		~Cube() {}

		filamesh::MeshReader::Mesh createMesh(filament::MaterialInstance* defaultMaterial) {
			return filamesh::MeshReader::loadMeshFromBuffer(
				renderer.filamentRAII.engine, CUBE_CUBE_DATA, nullptr,
				nullptr, defaultMaterial);
		}

		TheWarrenRenderer& renderer;
	};
	Cube cube;

	template<class TextureLoader>
	struct TexturedCube {
		TexturedCube(TheWarrenRenderer& _renderer) :
			renderer(_renderer)
		{
			filament::Engine& engine = *renderer.filamentRAII.engine;
			filament::Scene& scene = *renderer.scene;

			texture = TextureLoader::load(engine);
			filament::TextureSampler sampler(filament::TextureSampler::MinFilter::LINEAR,
				filament::TextureSampler::MagFilter::NEAREST);

			materialInstance = renderer.bakedTexture->createInstance();
			materialInstance->setParameter("albedo", texture, sampler);
		}

		filamesh::MeshReader::Mesh createMesh() {
			auto mesh = renderer.cube.createMesh(materialInstance);
			renderer.scene->addEntity(mesh.renderable);
			return mesh;
		}

		~TexturedCube() {
			renderer.filamentRAII.engine->destroy(materialInstance);
			renderer.filamentRAII.engine->destroy(texture);
		}

		TheWarrenRenderer& renderer;
		filament::Texture* texture;
		filament::MaterialInstance* materialInstance;
	};

	template<const char* filePath>
	using StaticTextureFileBuilder = StaticTextureBuilder<StaticTextureFileLoader<filePath>>;

	static constexpr const char playerTextureFilePath[] = "assets/textures/link.png";
	TexturedQuad<StaticTextureFileBuilder<playerTextureFilePath>> playerPrimitives;

	struct PlayerEntity {
		utils::Entity renderable;
	};
	PlayerEntity players[TheWarrenState::maxPlayerCount];
	
	template<const unsigned char data[1 * 1 * 4]>
	struct StaticColorTextureLoader {
		static SimpleTextureData load() {
			constexpr int w = 1;
			constexpr int h = 1;
			constexpr int channels = 4;
			constexpr size_t imageSize = w * h * channels;
			return SimpleTextureData{w, h, channels, data};
		}
		static void destroy(void* buffer, size_t size) {}
	};

	static constexpr unsigned char wallColor[] = { 0xFF, 0xFF, 0xFF, 255 };
	TexturedCube<StaticTextureBuilder<StaticColorTextureLoader<wallColor>>> wallPrimitives;
	struct WallEntity {
		filamesh::MeshReader::Mesh mesh;
	};
	WallEntity wallEntities[walls.max_size()];
	WallEntity floor;

	static constexpr unsigned char arrowColor[] = { 0xF4, 0x43, 0x36, 255 };
	TexturedQuad<StaticTextureBuilder<StaticColorTextureLoader<arrowColor>>> arrowPrimitives;
	struct ArrowEntity {
		utils::Entity renderable;
	};
	//to do make this a vector or something
	ArrowEntity ArrowEntites[TheWarrenState::entitiesSize];

	utils::Entity sunlight;
	utils::Entity backlight;

	int thisPlayerIndex = -1;

	bool isPlayer() {
		return thisPlayerIndex != -1;
	}
	const typename TheWarrenState::Player& getPlayer(const TheWarrenState& state) {
		return state.players[thisPlayerIndex];
	}

	template<class Core>
	filagui::ImGuiHelper::Callback getImGUICallBack(Core& core, const TheWarrenState& state) {
		gRenderer = this; //dumb way to pass values
		gState = &state;
		gCore = &core;

		return [](filament::Engine* engine, filament::View*) {
			TheWarrenRenderer& renderer = *gRenderer;
			const TheWarrenState& state = *gState;
			Core& core = *static_cast<Core*>(gCore);
			auto imGuiIO = ImGui::GetIO();
			
			const auto showItemName = [=](const TheWarrenState::Player::ItemType& item) {
				ImGui::Text("%s", itemData[static_cast<int>(item)].name);
				if (ImGui::IsItemHovered()) {
					ImGui::BeginTooltip();
					ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
					ImGui::Text("%s", itemData[static_cast<int>(item)].description);
					ImGui::PopTextWrapPos();
					ImGui::EndTooltip();
				}
			};

			if (renderer.isPlayer()) {
				auto& player = renderer.getPlayer(state);

				ImGui::SetNextWindowPos(ImVec2{2.0, 2.0}, ImGuiCond_Always);
				ImGui::Begin("Player Info", nullptr,
					ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar |
					ImGuiWindowFlags_NoFocusOnAppearing);

				ImGui::Columns(6, "playerStatecolumns");
				ImGui::Text("Health"); ImGui::NextColumn();
				ImGui::Text("Mana"); ImGui::NextColumn();
				ImGui::Text("Ammo"); ImGui::NextColumn();
				ImGui::Text("Level"); ImGui::NextColumn();
				ImGui::Text("Keys"); ImGui::NextColumn();
				ImGui::Text("Lives"); ImGui::NextColumn();

				ImGui::Separator();
				ImGui::Text("%d", static_cast<int>(player.health)); ImGui::NextColumn();
				ImGui::Text("%d", 0); ImGui::NextColumn();
				ImGui::Text("%d", player.ammo); ImGui::NextColumn();
				ImGui::Text("%d", 0); ImGui::NextColumn();
				ImGui::Text("%d", player.keys); ImGui::NextColumn();
				ImGui::Text("%d", player.royleHealth); ImGui::NextColumn();

				ImGui::Columns(1);
				ImGui::Separator();

				ImGui::Text("DHand slot: %d\tNDHand slot: %d",
					player.dominantHandSlot,
					player.nondominantHandSlot);
				int i = 0;
				for (const auto item : player.inventory) {
					ImGui::PushID(i);

					const char * startText = "Slot";
					const bool isInDominantHand = i == player.dominantHandSlot;
					const bool isInNonDominantHand = i == player.nondominantHandSlot;

					ImGui::Selectable("", isInDominantHand, ImGuiSelectableFlags_SpanAllColumns);
					ImGui::SameLine();
					if (isInNonDominantHand) {
						ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s", startText);
					} else {
						ImGui::Text("%s", startText);
					}
					ImGui::SameLine();
					ImGui::Text("%i:", i);
					ImGui::SameLine();
					showItemName(item);

					ImGui::PopID();
					i += 1;
				}

				ImGui::End();

				//shop UI
				if (player.inputMode == TheWarrenState::Player::InputMode::Shop) {
					constexpr auto shopSize = TheWarrenState::Player::maxShopSize;
					constexpr double doublePi = (2 * M_PI);
					const double buttonCircularSectorSize = doublePi / shopSize;
					const double halfButtonCircularSectorSize = buttonCircularSectorSize / 2;
					const double startAngle = -1 * M_PI;
					for (int i = 0; i < shopSize; i += 1) {
						if (player.shop[i] == TheWarrenState::Player::ItemType::NONE)
							continue;

						double direction = startAngle + halfButtonCircularSectorSize +
							(i * buttonCircularSectorSize);
						constexpr double distanceAwayFromOrgin = 200.0;
						const auto shopItemIconPos = ImVec2{
							(imGuiIO.DisplaySize.x / 2) + (static_cast<float>(std::sin(direction) *
								distanceAwayFromOrgin)), //x
							(imGuiIO.DisplaySize.y / 2) - (static_cast<float>(std::cos(direction) *
								distanceAwayFromOrgin))  //y
						};

						ImGui::SetNextWindowPos(shopItemIconPos, ImGuiCond_Always, ImVec2{0.5, 0.5});
						ImGui::Begin(("Shop item " + std::to_string(i)).c_str(), nullptr,
							ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar |
							ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
							ImGuiWindowFlags_NoFocusOnAppearing);
						showItemName(player.shop[i]);
						ImGui::End();
					}
				}
			}

			ImGui::SetNextWindowPos(ImVec2{imGuiIO.DisplaySize.x / 2, 2.0},
				ImGuiCond_Always, ImVec2{ 0.5, 0.0 });
			ImGui::Begin("Round Info", nullptr,
				ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar |
				ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
				ImGuiWindowFlags_NoFocusOnAppearing);
			ImGui::Text("%s Round %d %d", "%Round Type%", state.roundNum,
				static_cast<int>(state.phaseTimer));
			ImGui::End();
		};
	}

    template<class Core>
    void draw(Core& core, const TheWarrenState& state, double deltaTime) {
		//get current player
		thisPlayerIndex = -1;
		core.getUserInput(core.getID(), state, [this](const typename Core::GameState::InputType&, size_t index) {
			thisPlayerIndex = index;
		});
		const auto getPlayer = [&]() -> const typename TheWarrenState::Player& {
			return this->getPlayer(state);
		};
		filament::math::float2 currentPlayerPosition = { 0.0, 0.0 };
		if (isPlayer()) {
			const typename TheWarrenState::Player& player = getPlayer();
			currentPlayerPosition = {
				player.position[Axis::X], player.position[Axis::Y]
			};
		}

		//lights
		if(isPlayer()) {
			const typename TheWarrenState::Player& player = getPlayer();
			bool isVisible = scene->hasEntity(playerLight);
			if (0 < player.health) {
				if (!isVisible)
					scene->addEntity(playerLight);
				//for future version of filament
				/*auto& transformManager = engine->getTransformManager();
				transformManager.setTransform(
					transformManager.getInstance(playerLight),
					filament::math::mat4f::translation(
						filament::math::vec3<double>{
							player.position[Axis::X], player.position[Axis::Y], 4.0
						})
				);*/
				auto& lightManager = filamentRAII.engine->getLightManager();
				auto lightInstence = lightManager.getInstance(playerLight);
				if (lightInstence.isValid()) {
					lightManager.setPosition(
						lightInstence,
						filament::math::vec3<float>{ 
							currentPlayerPosition.x, currentPlayerPosition.y, 5.0 });
					
					const TheWarrenState::Command& input = state.inputs[player.index];
					lightManager.setDirection(
						lightInstence,
						{ std::sin(input.rotation), std::cos(input.rotation), 0.0f });
				}
			} else if (isVisible == true) {
				scene->remove(playerLight);
			}
		}

		camera->lookAt(
			//22 was choosen because it's pretty close to the sdl renderer
			{ currentPlayerPosition.x, currentPlayerPosition.y, 21.5f },
			{ currentPlayerPosition.x, currentPlayerPosition.y, 0 },
			{ 0, 1, 0 });

		std::size_t index = 0;
		for (const auto& player : state.players) {
			const auto& asset = players[index];
			bool isVisible = scene->hasEntity(asset.renderable);
			if (0 < player.health) {
				if (isVisible == false) {
					scene->addEntity(asset.renderable);
				}
				auto translation = filament::math::mat4f::translation(
					filament::math::vec3<double>{
						player.position[Axis::X], player.position[Axis::Y], 3.5
					}
				);
				auto& transformManager = filamentRAII.engine->getTransformManager();
				transformManager.setTransform(
					transformManager.getInstance(asset.renderable),
					translation
				);
			} else if (isVisible == true) {
				scene->remove(asset.renderable);
			}

			index += 1;
		}

		index = 0;
		for (const auto& arrow : state.entities) {
			const auto& asset = ArrowEntites[index];
			bool isVisible = scene->hasEntity(asset.renderable);
			if (arrow.type != TheWarrenState::Entity::Type::NONE) {
				if (isVisible == false) {
					scene->addEntity(asset.renderable);
				}
				auto translation = filament::math::mat4f::translation(
					filament::math::vec3<decltype(arrow.height)>{
						arrow.position[Axis::X], arrow.position[Axis::Y], arrow.height
					}
				);
				auto scale = filament::math::mat4f::scaling(
					filament::math::vec3<double>{
						1.0/8.0, 1.0/8.0, 1.0
					}
				);
				auto& transformManager = filamentRAII.engine->getTransformManager();
				transformManager.setTransform(
					transformManager.getInstance(asset.renderable),
					translation * scale
				);
			} else if (isVisible == true) {
				scene->remove(asset.renderable);
			}
			index += 1;
		}

		//imgui
		imGuiHelper.render(deltaTime, getImGUICallBack<Core>(core, state));

        //wait for the gpu
		//it's important that there's as little time between waitAndDestroy and render
		//as the gpu is idle
		filament::Fence::waitAndDestroy(filamentRAII.engine->createFence());

		// beginFrame() returns false if we need to skip a frame
		if (renderer->beginFrame(swapChain)) {
			// for each View
			renderer->render(view);
			renderer->render(filamentGUIRAII.view);
			renderer->endFrame();
		}
    }

	filament::SwapChain* swapChain;
	filament::Renderer* renderer;
	filament::Camera* camera;
	filament::View* view;
	filament::Scene* scene;

	filament::Material* bakedTexture;
};

using Renderer = TheWarrenRenderer;