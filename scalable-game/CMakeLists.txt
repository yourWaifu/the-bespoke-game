cmake_minimum_required (VERSION 3.8)

add_executable (scalable-game WIN32 "window_process_windows.cpp" "video_windows.cpp" "main_windows.cpp")

set(COMPILER_FLAGS ${COMPILER_FLAGS} $<$<CONFIG:Release>:/fp:fast>)

find_path(WIL_INCLUDE_DIR NAMES wil)
target_include_directories(
	scalable-game
	PRIVATE
		.
		${WIL_INCLUDE_DIR}
)
target_link_libraries(scalable-game PRIVATE core)

file(COPY ../assets DESTINATION ${CMAKE_CURRENT_BINARY_DIR})