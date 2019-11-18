cmake_minimum_required (VERSION 3.8)

set(LIBS core)

find_package(Protobuf REQUIRED)
set(PROTO_FILES
	state.proto
)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

if(WIN32)
	add_executable (scalable-game WIN32 "window_process_windows.cpp" "video_windows.cpp" "main_windows.cpp")

	set(COMPILER_FLAGS ${COMPILER_FLAGS} $<$<CONFIG:Release>:/fp:fast>)

	find_path(WIL_INCLUDE_DIR NAMES wil)

	target_include_directories(
		scalable-game
		PRIVATE
			.
			${WIL_INCLUDE_DIR}
			${CMAKE_CURRENT_BINARY_DIR}
	)
else()
	add_executable(scalable-game main_sdl.cpp sdl_system_linux.cpp ${PROTO_SRCS})
	list(APPEND LIBS sdl2)
	target_include_directories(
		scalable-game
		PRIVATE
			"../deps/filament/third_party/libsdl2/include"
			${CMAKE_CURRENT_BINARY_DIR}
	)
endif()

target_link_libraries(scalable-game PRIVATE ${LIBS})

file(COPY ../assets DESTINATION ${CMAKE_CURRENT_BINARY_DIR})