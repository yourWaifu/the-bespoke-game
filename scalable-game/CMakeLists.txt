cmake_minimum_required (VERSION 3.8)

find_package(Protobuf REQUIRED)
set(PROTO_FILES
	state.proto
)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

add_library(scalable-game "game.cpp" ${PROTO_SRCS})

target_include_directories(
	scalable-game
	PUBLIC
		${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(scalable-game PUBLIC core)