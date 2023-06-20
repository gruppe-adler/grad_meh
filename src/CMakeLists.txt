cmake_minimum_required (VERSION 3.15)


file(GLOB_RECURSE INTERCEPT_PLUGIN_SOURCES *.h *.hpp *.c *.cpp)
SOURCE_GROUP("src" FILES ${INTERCEPT_PLUGIN_SOURCES})

#If you want to split your source files into different directories you can do so here

#The SOURCE_GROUP string is the directory it will display as inside your visual studio.
#Here is a example of a "utilities" subdirectory.

#----Don't change anything below this line
if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    set(DEBUG_SUFFIX "d")
else()
    set(DEBUG_SUFFIX "")
endif()

# fmt
find_package(fmt CONFIG REQUIRED)

# nlohmann json
find_package(nlohmann_json CONFIG REQUIRED)

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME ON)

find_package(Boost 1.82.0 REQUIRED COMPONENTS system filesystem thread iostreams) 

# OIIO
if("${INTERCEPT_LINK_TYPE}" STREQUAL "static")
	add_definitions(-DOIIO_STATIC_DEFINE)
	add_definitions(-DGRAD_AFF_STATIC_DEFINE)
	add_definitions(-DGRAD_AFF_USE_OIIO)
endif()

find_package(OpenEXR CONFIG)
find_package(PNG MODULE)

add_definitions(-DOIIO_STATIC_DEFINE)
find_package(OpenImageIO CONFIG REQUIRED)

# plog
find_package(plog CONFIG REQUIRED)

# GADL
find_package(GDAL CONFIG REQUIRED)

#PCL
find_package(PCL CONFIG REQUIRED)

# Clipper
find_package(polyclipping CONFIG REQUIRED)


#include the Intercept headers from the submodule
set(INTERCEPT_CLIENT_PATH "${CMAKE_SOURCE_DIR}/intercept/src/client")

set(INTERCEPT_INCLUDE_PATH "${INTERCEPT_CLIENT_PATH}/headers" "${INTERCEPT_CLIENT_PATH}/headers/shared" "${INTERCEPT_CLIENT_PATH}/headers/client/" "${INTERCEPT_CLIENT_PATH}/headers/client/sqf")

if(USE_64BIT_BUILD)
    set(INTERCEPT_PLUGIN_NAME "${CMAKE_PROJECT_NAME}_x64")
else()
    set(INTERCEPT_PLUGIN_NAME "${CMAKE_PROJECT_NAME}")
endif()

add_definitions(/DINTERCEPT_NO_THREAD_SAFETY)

if(USE_ENGINE_TYPES)
    add_definitions(/DINTERCEPT_SQF_STRTYPE_RSTRING)
endif()

file(GLOB INTERCEPT_HOST_SOURCES "${INTERCEPT_CLIENT_PATH}/intercept/client/*.cpp"  "${INTERCEPT_CLIENT_PATH}/intercept/client/sqf/*.cpp" "${INTERCEPT_CLIENT_PATH}/intercept/shared/*.cpp")
SOURCE_GROUP("intercept" FILES ${INTERCEPT_HOST_SOURCES})

# Version file
ADD_CUSTOM_COMMAND(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/version.cpp
           ${CMAKE_CURRENT_BINARY_DIR}/_version.cpp
    COMMAND ${CMAKE_COMMAND} -P
            ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/version.cmake)

include(FetchContent)

FetchContent_Declare(
    Corrosion
    GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
    GIT_TAG master # Optionally specify a commit hash, version tag or branch here
)
# Set any global configuration variables such as `Rust_TOOLCHAIN` before this line!
FetchContent_MakeAvailable(Corrosion)

# Import targets defined in a package or workspace manifest `Cargo.toml` file

corrosion_import_crate(MANIFEST_PATH rvff-cxx/Cargo.toml)
corrosion_add_cxxbridge(rust-lib CRATE rvff-cxx MANIFEST_PATH rvff-cxx FILES lib.rs)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_library(${INTERCEPT_PLUGIN_NAME} SHARED ${INTERCEPT_PLUGIN_SOURCES} ${INTERCEPT_HOST_SOURCES} ${CMAKE_CURRENT_BINARY_DIR}/version.cpp)

target_include_directories(${INTERCEPT_PLUGIN_NAME} PUBLIC ${OPENIMAGEIO_INCLUDE_DIR})

target_link_libraries(${INTERCEPT_PLUGIN_NAME} GDAL::GDAL)

target_link_libraries(${INTERCEPT_PLUGIN_NAME} OpenImageIO::OpenImageIO OpenImageIO::OpenImageIO_Util)

target_link_libraries(${INTERCEPT_PLUGIN_NAME} ${Boost_LIBRARIES})
target_link_libraries(${INTERCEPT_PLUGIN_NAME} nlohmann_json nlohmann_json::nlohmann_json)
target_link_libraries(${INTERCEPT_PLUGIN_NAME} fmt::fmt-header-only)
target_link_libraries(${INTERCEPT_PLUGIN_NAME} ${PCL_LIBRARIES})
target_link_libraries(${INTERCEPT_PLUGIN_NAME} polyclipping::polyclipping)

target_link_libraries(${INTERCEPT_PLUGIN_NAME} plog::plog)

target_link_libraries(${INTERCEPT_PLUGIN_NAME} rust-lib)

if(MSVC)
	target_link_libraries(${INTERCEPT_PLUGIN_NAME} ntdll.lib)
endif()

if(MSVC)
    # Note: This is required because we use `cxx` which uses `cc` to compile and link C++ code.
	if(CMAKE_BUILD_TYPE STREQUAL "Debug")
		corrosion_set_env_vars(rvff-cxx "CFLAGS=-MTd" "CXXFLAGS=-MTd")
	else()
		corrosion_set_env_vars(rvff-cxx "CFLAGS=-MT" "CXXFLAGS=-MT")
	endif()
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${INTERCEPT_INCLUDE_PATH})

set_target_properties(${INTERCEPT_PLUGIN_NAME} PROPERTIES PREFIX "")
set_target_properties(${INTERCEPT_PLUGIN_NAME} PROPERTIES FOLDER "${CMAKE_PROJECT_NAME}")

if(CMAKE_COMPILER_IS_GNUCXX)
	set(CMAKE_CXX_FLAGS "-std=c++1z -O2 -s -fPIC -fpermissive -static-libgcc -static-libstdc++")#-march=i686 -m32
	set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
	set(CMAKE_SHARED_LINKER_FLAGS "-shared -static-libgcc -static-libstdc++")
else()
	set(CMAKE_CXX_FLAGS_DEBUG "/D_DEBUG /MTd /Zi /Ob0 /Od /RTC1 /MP /EHsc")
	set(CMAKE_CXX_FLAGS_RELEASE "/MT /Zi /O2 /Ob1 /EHsc /MP") #with debug info
	# /FORCE:MULTIPLE requrired to ignore  second definition of tiff stuff in gdal (conflicting with oiio?)
	target_link_options(${INTERCEPT_PLUGIN_NAME} PUBLIC "/FORCE:MULTIPLE")
	set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "/DEBUG:FULL /NODEFAULT:LIBCMTD") 
	set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "/OPT:REF /DEBUG:FULL /NODEFAULT:LIBCMTD") 
endif()


if(NOT "${PLUGIN_FOLDER}" STREQUAL "")
	add_custom_command(TARGET ${INTERCEPT_PLUGIN_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${INTERCEPT_PLUGIN_NAME}> ${PLUGIN_FOLDER}/${INTERCEPT_PLUGIN_NAME}.dll
	)

	add_custom_command(TARGET ${INTERCEPT_PLUGIN_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_PDB_FILE:${INTERCEPT_PLUGIN_NAME}> ${PLUGIN_FOLDER}/${INTERCEPT_PLUGIN_NAME}.pdb
	)
endif()
