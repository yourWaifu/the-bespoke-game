# copied from filament source code
set(RESOURCE_DIR  "${CMAKE_CURRENT_BINARY_DIR}/generated/resources")
set(MATERIAL_DIR  "${CMAKE_CURRENT_BINARY_DIR}/generated/material")
set(ASSETS_DIR    "${CMAKE_CURRENT_SOURCE_DIR}/../assets")

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
		COMMAND matc ${MATC_BASE_FLAGS} -o ${output_path} ${fullname}
		MAIN_DEPENDENCY ${mat_src}
		DEPENDS matc
		COMMENT "Compiling material ${mat_src} to ${output_path}"
	)
	list(APPEND RESOURCE_BINS ${output_path})
endforeach()

# make resources
file(MAKE_DIRECTORY ${RESOURCE_DIR})

# CMake fails to invoke ar on Windows unless there is at least one C/C++ file in the library.
set(DUMMY_SRC "${RESOURCE_DIR}/dummy.c")
add_custom_command(OUTPUT ${DUMMY_SRC} COMMAND echo "//" > ${DUMMY_SRC})

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

set(OUR_RESOURCES_SOURCES ${DUMMY_SRC})

if(WIN32)
	list(APPEND OUR_RESOURCES_SOURCES ${RESGEN_SOURCE} ${RESOURCE_DIR}/resources.c)
endif()

if(UNIX)
	set_property(SOURCE ${RESOURCE_DIR}/resources.S PROPERTY LANGUAGE C)
	list(APPEND OUR_RESOURCES_SOURCES ${RESOURCE_DIR}/resources.S)
endif()

message(STATUS ${OUR_RESOURCES_SOURCES})

add_library(our-resources ${OUR_RESOURCES_SOURCES})

#build models
function(add_mesh SOURCE TARGET)
    set(source_mesh "${ROOT_DIR}/${SOURCE}")
    set(target_mesh "${RESOURCE_DIR}/${TARGET}")
    set(MONKEY_FILES ${MESH_BINS} ${target_mesh} PARENT_SCOPE)
    add_custom_command(
        OUTPUT ${target_mesh}
        COMMAND filamesh --compress ${source_mesh} ${target_mesh}
        MAIN_DEPENDENCY ${source_mesh}
        DEPENDS filamesh)
endfunction()

add_mesh("${ASSETS_DIR}/models/cube.obj" "cube.filamesh")
get_resgen_vars(${RESOURCE_DIR} cube)

add_custom_command(
        OUTPUT ${RESGEN_OUTPUTS}
        COMMAND resgen ${RESGEN_FLAGS} ${MONKEY_FILES}
        DEPENDS resgen ${MONKEY_FILES}
        COMMENT "Aggregating cube resources"
)

add_library(cube-resources ${DUMMY_SRC} ${RESGEN_SOURCE})
target_link_libraries(cube-resources PUBLIC filameshio)
target_include_directories(cube-resources PUBLIC "${RESOURCE_DIR}")

add_mesh("${ASSETS_DIR}/models/quad.obj" "quad.filamesh")
get_resgen_vars(${RESOURCE_DIR} quad)

add_custom_command(
        OUTPUT ${RESGEN_OUTPUTS}
        COMMAND resgen ${RESGEN_FLAGS} ${MONKEY_FILES}
        DEPENDS resgen ${MONKEY_FILES}
        COMMENT "Aggregating cube resources"
)

add_library(quad-resources ${DUMMY_SRC} ${RESGEN_SOURCE})
target_include_directories(quad-resources PUBLIC "${RESOURCE_DIR}")

set(COMPILER_FLAGS ${COMPILER_FLAGS} $<$<CONFIG:Release>:/fp:fast>)

#find library
#find_package(asio REQUIRED)

set(V8_LIB_DIR ${PROJECT_SOURCE_DIR}/deps/v8/)
if(WIN32 AND CMAKE_BUILD_TYPE MATCHES "^[Dd][Ee][Bb][Uu][Gg]$")
	set(V8_LIB_DIR ${PROJECT_SOURCE_DIR}/deps/v8/debug/)
endif()

add_library(core STATIC
	${CMAKE_CURRENT_SOURCE_DIR}/events.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/input.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/core.cpp
	${PROTO_SRCS}
)

target_include_directories(
	core
	PUBLIC
		${PROJECT_SOURCE_DIR}/core
		${CMAKE_CURRENT_BINARY_DIR}
		${PROJECT_SOURCE_DIR}/deps/string-view-lite/include
		${PROJECT_SOURCE_DIR}/deps/v8-cmake/v8/include
		${PROJECT_SOURCE_DIR}/deps/asio/asio/include
)

#target_link_directories(
#	core
#	PUBLIC
#		${V8_LIB_DIR}
#)

#set(LINK_LIBS filament our-resources cube-resources bluegl stb asio)
#if(WIN32)
#	list(APPEND LINK_LIBS opengl32 gdi32 user32 winmm dbghelp shlwapi)
#endif()
set(LINK_LIBS v8_libplatform v8_compiler v8_initializers v8_snapshot)

target_link_libraries(core PUBLIC ${LINK_LIBS})
