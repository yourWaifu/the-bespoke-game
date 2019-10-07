# copied from filament source code
set(RESOURCE_DIR  "${CMAKE_CURRENT_BINARY_DIR}/generated/resources")
set(MATERIAL_DIR  "${CMAKE_CURRENT_BINARY_DIR}/generated/material")

# build materials
set(MATERIAL_SRCS
	materials/baked_texture.mat
)

file(MAKE_DIRECTORY ${MATERIAL_DIR})

foreach (mat_src ${MATERIAL_SRCS})
	get_filename_component(localname "${mat_src}" NAME_WE)
	get_filename_component(fullname "${mat_src}" ABSOLUTE)
	set(output_path "${MATERIAL_DIR}/${localname}.filamat")
	add_custom_command(
		OUTPUT ${output_path}
		COMMAND matc ${MATC_BASE_FLAGS} -m material -o ${output_path} ${fullname}
		MAIN_DEPENDENCY ${mat_src}
		DEPENDS matc
		COMMENT "Compiling material ${mat_src} to ${output_path}"
	)
	list(APPEND RESOURCE_BINS ${output_path})
endforeach()

# make resources
file(MAKE_DIRECTORY ${RESOURCE_DIR})

get_resgen_vars(${RESOURCE_DIR} resources)

add_custom_command(
        OUTPUT ${RESGEN_OUTPUTS}
        COMMAND resgen ${RESGEN_FLAGS} ${RESOURCE_BINS}
        DEPENDS resgen ${RESOURCE_BINS}
        COMMENT "Aggregating resources"
)

if (DEFINED RESGEN_SOURCE_FLAGS)
    set_source_files_properties(${RESGEN_SOURCE} PROPERTIES COMPILE_FLAGS ${RESGEN_SOURCE_FLAGS})
endif()

# CMake fails to invoke ar on Windows unless there is at least one C/C++ file in the library.
set(DUMMY_SRC "${RESOURCE_DIR}/dummy.c")
add_custom_command(OUTPUT ${DUMMY_SRC} COMMAND echo "//" > ${DUMMY_SRC})

add_library(our-resources ${DUMMY_SRC} ${RESGEN_SOURCE} ${RESOURCE_DIR}/resources.c)

set(COMPILER_FLAGS ${COMPILER_FLAGS} $<$<CONFIG:Release>:/fp:fast>)

# compile proto files into h and cc files
find_package(Protobuf REQUIRED)
set(PROTO_FILES
	connection.proto
)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

add_library(core
	events.cpp
	input.cpp
	core.cpp
	${PROTO_SRCS}
)

find_package(msgpack CONFIG REQUIRED)
find_path(MSG_INCLUDE_DIR NAMES msgpack.hpp)

target_include_directories(
	core
	PUBLIC
		${PROJECT_SOURCE_DIR}/core
		${CMAKE_CURRENT_BINARY_DIR}
		${MSG_INCLUDE_DIR}
)

target_link_libraries(core PUBLIC filament our-resources opengl32 gdi32 user32 bluegl stb msgpackc msgpackc-cxx)

target_compile_features(core PUBLIC cxx_std_17)