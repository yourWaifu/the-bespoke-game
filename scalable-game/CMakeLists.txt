cmake_minimum_required (VERSION 3.8)

set(LIBS core GameNetworkingSockets)

add_executable(scalable-game-client main_sdl.cpp)
list(APPEND LIBS sdl2)
target_include_directories(scalable-game-client PRIVATE "../deps/filament/third_party/libsdl2/include")

if(WIN32)
	list(APPEND LIBS sdl2main)

	#after building, copy the sdl dll
	add_custom_command(
		TARGET scalable-game-client 
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/deps/filament/samples/sdl2.dll ${CMAKE_CURRENT_BINARY_DIR}
	)
endif()

target_link_libraries(scalable-game-client PRIVATE ${LIBS})

file(COPY ../assets DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

#copy dlls
add_custom_command(TARGET scalable-game-client POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
						${CMAKE_CURRENT_BINARY_DIR}/../deps/GameNetworkingSockets/src/GameNetworkingSockets.dll ${CMAKE_CURRENT_BINARY_DIR})
add_custom_command(TARGET scalable-game-client POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
						${CMAKE_CURRENT_BINARY_DIR}/../deps/GameNetworkingSockets/src/GameNetworkingSockets.pdb ${CMAKE_CURRENT_BINARY_DIR})
add_custom_command(TARGET scalable-game-client POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
						${CMAKE_CURRENT_BINARY_DIR}/../deps/GameNetworkingSockets/src/libprotobuf.dll ${CMAKE_CURRENT_BINARY_DIR})