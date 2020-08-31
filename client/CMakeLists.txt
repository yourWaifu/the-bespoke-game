cmake_minimum_required (VERSION 3.8)

set(LIBS core GameNetworkingSockets imgui filamentapp our-resources cube-resources quad-resources filameshio filagui)

add_executable(scalable-game-client ${CMAKE_CURRENT_SOURCE_DIR}/main_sdl.cpp  ${CMAKE_CURRENT_SOURCE_DIR}/../deps/imgui_sdl/imgui_sdl.cpp)
list(APPEND LIBS sdl2 game)
target_include_directories(scalable-game-client PRIVATE "../deps/filament/third_party/libsdl2/include" "../deps")

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
if(WIN32)
add_custom_command(TARGET scalable-game-client POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
						${CMAKE_CURRENT_BINARY_DIR}/../deps/GameNetworkingSockets/src/GameNetworkingSockets.dll ${CMAKE_CURRENT_BINARY_DIR})
add_custom_command(TARGET scalable-game-client POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
						${CMAKE_CURRENT_BINARY_DIR}/../deps/GameNetworkingSockets/src/GameNetworkingSockets.pdb ${CMAKE_CURRENT_BINARY_DIR})
if(EXISTS ${CMAKE_CURRENT_BINARY_DIR}/../deps/GameNetworkingSockets/src/libprotobuf.dll)
	add_custom_command(TARGET scalable-game-client POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
						${CMAKE_CURRENT_BINARY_DIR}/../deps/GameNetworkingSockets/src/libprotobuf.dll ${CMAKE_CURRENT_BINARY_DIR})
endif()
if(EXISTS ${CMAKE_CURRENT_BINARY_DIR}/../deps/GameNetworkingSockets/src/libprotobufd.dll)
	add_custom_command(TARGET scalable-game-client POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
						${CMAKE_CURRENT_BINARY_DIR}/../deps/GameNetworkingSockets/src/libprotobufd.dll ${CMAKE_CURRENT_BINARY_DIR})
endif()
endif()

#copy settings files
add_custom_command(TARGET scalable-game-client POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
						${CMAKE_CURRENT_SOURCE_DIR}/default-settings.json ${CMAKE_CURRENT_BINARY_DIR})
add_custom_command(TARGET scalable-game-client POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
						${CMAKE_CURRENT_SOURCE_DIR}/default-settings.json ${CMAKE_CURRENT_BINARY_DIR}/settings.json)