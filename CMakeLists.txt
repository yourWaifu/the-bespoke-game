# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

project ("scalable-game")

# Include sub-projects.
add_subdirectory("deps/filament")

if (WIN32)
    set(CRT_FLAGS_RELEASE "/MT")
    set(CRT_FLAGS_DEBUG "/MTd")

    # TODO: Figure out why pdb generation messes with incremental compilaton.
    # IN RELEASE_WITH_DEBUG_INFO, generate debug info in .obj, no in pdb.
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${CRT_FLAGS_RELEASE} /Z7")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} ${CRT_FLAGS_RELEASE} /Z7")

    # In RELEASE, also generate PDBs.
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${CRT_FLAGS_RELEASE} /Zi")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${CRT_FLAGS_RELEASE} /Zi")

    # In DEBUG, avoid generating a PDB file which seems to mess with incremental compilation.
    # Instead generate debug info directly inside obj files.
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${CRT_FLAGS_DEBUG} /Z7")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${CRT_FLAGS_DEBUG} /Z7")
endif()

add_subdirectory("deps/GameNetworkingSockets")

add_subdirectory("core")
add_subdirectory("server")
add_subdirectory ("scalable-game")
