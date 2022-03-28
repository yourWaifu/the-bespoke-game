# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

project ("scalable-game" CXX C ASM)

set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address -fno-builtin")
set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address -fno-builtin")
set (CXX_STANDARD 17)

# Include sub-projects.
add_subdirectory("deps/filament")

if (WIN32)

endif()

add_subdirectory("deps/GameNetworkingSockets")
add_subdirectory("deps/v8-cmake")
set(BOX2D_BUILD_UNIT_TESTS OFF)
set(BOX2D_BUILD_TESTBED OFF)
add_subdirectory("deps/box2d")

add_subdirectory("core")
add_subdirectory("scalable-game")
add_subdirectory("server")
add_subdirectory ("client")
