add_library(game game.cpp)
target_include_directories(game
	PUBLIC
		${PROJECT_SOURCE_DIR}/scalable-game
		"${PROJECT_SOURCE_DIR}/deps/string-view-lite/include"
)
target_link_libraries(game core)