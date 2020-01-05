add_executable(scaleable-game-server server.cpp)
find_package(unofficial-libuv CONFIG REQUIRED)
#find_path(uwebsockets_INCLUDE_DIR NAMES uwebsockets)
target_link_directories(scaleable-game-server PUBLIC "${uwebsockets_INCLUDE_DIR}/../lib")
target_link_libraries(scaleable-game-server GameNetworkingSockets unofficial::libuv::libuv)
target_include_directories(
	scaleable-game-server
	PRIVATE
		"${PROJECT_SOURCE_DIR}/deps/string-view-lite/include"
)
add_sanitizers(scaleable-game-server)

#copy dlls
add_custom_command(TARGET scaleable-game-server POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
						${CMAKE_CURRENT_BINARY_DIR}/../deps/GameNetworkingSockets/src/GameNetworkingSockets.dll ${CMAKE_CURRENT_BINARY_DIR})
add_custom_command(TARGET scaleable-game-server POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
						${CMAKE_CURRENT_BINARY_DIR}/../deps/GameNetworkingSockets/src/GameNetworkingSockets.pdb ${CMAKE_CURRENT_BINARY_DIR})
add_custom_command(TARGET scaleable-game-server POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
						${CMAKE_CURRENT_BINARY_DIR}/../deps/GameNetworkingSockets/src/libprotobuf.dll ${CMAKE_CURRENT_BINARY_DIR})