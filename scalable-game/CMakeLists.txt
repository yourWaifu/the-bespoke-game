add_library(game STATIC "game.cpp" "renderer_filament.h")
target_include_directories(game
	PUBLIC
		${PROJECT_SOURCE_DIR}/scalable-game
		#"${PROJECT_SOURCE_DIR}/deps/string-view-lite/include"
)
target_compile_features(game PUBLIC cxx_std_20)
target_link_libraries(game core box2d)
