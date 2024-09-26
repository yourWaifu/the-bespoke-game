add_executable(scaleable-game-server server.cpp)
target_link_directories(scaleable-game-server PUBLIC "${uwebsockets_INCLUDE_DIR}/../lib")
target_link_libraries(scaleable-game-server GameNetworkingSockets game)
target_include_directories(
	scaleable-game-server
	PRIVATE
		#"${PROJECT_SOURCE_DIR}/deps/string-view-lite/include"
)
add_sanitizers(scaleable-game-server)

#copy dlls
if(WIN32)
add_custom_command(TARGET scaleable-game-server POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different
						${CMAKE_CURRENT_BINARY_DIR}/../deps/GameNetworkingSockets/src/GameNetworkingSockets.dll ${CMAKE_CURRENT_BINARY_DIR})
add_custom_command(TARGET scaleable-game-server POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different
						${CMAKE_CURRENT_BINARY_DIR}/../deps/GameNetworkingSockets/src/GameNetworkingSockets.pdb ${CMAKE_CURRENT_BINARY_DIR})
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR EXISTS ${CMAKE_CURRENT_BINARY_DIR}/../deps/GameNetworkingSockets/src/libprotobuf.dll)
	add_custom_command(TARGET scaleable-game-server POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different
						${CMAKE_CURRENT_BINARY_DIR}/../deps/GameNetworkingSockets/src/libprotobuf.dll ${CMAKE_CURRENT_BINARY_DIR})
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR EXISTS ${CMAKE_CURRENT_BINARY_DIR}/../deps/GameNetworkingSockets/src/libprotobufd.dll)
	add_custom_command(TARGET scaleable-game-server POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different
						${CMAKE_CURRENT_BINARY_DIR}/../deps/GameNetworkingSockets/src/libprotobufd.dll ${CMAKE_CURRENT_BINARY_DIR})
endif()
endif()

#copy JS files
set(GAME_JS_FILES ${PROJECT_SOURCE_DIR}/scalable-game/main.js)
add_custom_command(TARGET scaleable-game-server POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy
		${GAME_JS_FILES} ${CMAKE_CURRENT_BINARY_DIR})