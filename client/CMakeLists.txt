cmake_minimum_required (VERSION 3.8)

set(LIBS scalable-game)

find_package(Protobuf REQUIRED)
set(PROTO_FILES
)
#protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

if(WIN32)
	add_executable(scalable-game-client WIN32 "main_sdl.cpp" "sdl_system_windows.cpp" ${PROTO_SRCS})
	list(APPEND LIBS sdl2 sdl2main)

	set(COMPILER_FLAGS ${COMPILER_FLAGS} $<$<CONFIG:Release>:/fp:fast>)

	find_path(WIL_INCLUDE_DIR NAMES wil)

	target_include_directories(
		scalable-game-client
		PRIVATE
			.
			${WIL_INCLUDE_DIR}
			${CMAKE_CURRENT_BINARY_DIR}
	)
	#after building, copy the sdl dll
	add_custom_command(
		TARGET scalable-game-client 
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/deps/filament/samples/sdl2.dll ${CMAKE_CURRENT_BINARY_DIR}
	)
else()
	add_executable(scalable-game-client main_sdl.cpp sdl_system_linux.cpp ${PROTO_SRCS})
	list(APPEND LIBS sdl2)
	target_include_directories(
		scalable-game-client
		PRIVATE
			"../deps/filament/third_party/libsdl2/include"
			${CMAKE_CURRENT_BINARY_DIR}
	)
endif()

target_link_libraries(scalable-game-client PRIVATE ${LIBS})

file(COPY ../assets DESTINATION ${CMAKE_CURRENT_BINARY_DIR})