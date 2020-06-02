add_library(game STATIC ${CMAKE_CURRENT_SOURCE_DIR}/game.cpp)
target_include_directories(game
	PUBLIC
		${PROJECT_SOURCE_DIR}/scalable-game
		"${PROJECT_SOURCE_DIR}/deps/string-view-lite/include"
)
target_link_libraries(game core)