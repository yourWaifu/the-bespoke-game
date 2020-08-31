add_library(game STATIC ${CMAKE_CURRENT_SOURCE_DIR}/game.cpp)
target_include_directories(game
	PUBLIC
		${PROJECT_SOURCE_DIR}/scalable-game
		"${PROJECT_SOURCE_DIR}/deps/string-view-lite/include"
)
target_compile_features(game PUBLIC cxx_std_17)
target_link_libraries(game core box2d)