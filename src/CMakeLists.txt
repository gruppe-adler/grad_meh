cmake_minimum_required (VERSION 3.0)


file(GLOB_RECURSE INTERCEPT_PLUGIN_SOURCES *.h *.hpp *.c *.cpp)
SOURCE_GROUP("src" FILES ${INTERCEPT_PLUGIN_SOURCES})

#If you want to split your source files into different directories you can do so here

#The SOURCE_GROUP string is the directory it will display as inside your visual studio.
#Here is a example of a "utilities" subdirectory.

#file(GLOB INTERCEPT_plugin_utilities_SOURCES "utilities/*.cpp" "utilities/*.hpp" "utilities/*.h")
#SOURCE_GROUP("src/utilities" FILES ${INTERCEPT_plugin_utilities_SOURCES})

#----Don't change anything below this line
if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    set(DEBUG_SUFFIX "d")
else()
    set(DEBUG_SUFFIX "")
endif()

# DXT
find_path(SQUISH_INCLUDE_DIR squish.h)
find_library(SQUISH_LIBRARY squish${DEBUG_SUFFIX})

# nlohmann json
find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME ON)

find_package(Boost 1.71.0 COMPONENTS system filesystem thread iostreams) 

# LZO
FIND_PATH(LZO2_INCLUDE_DIR lzo/lzoconf.h)
FIND_LIBRARY(LZO2_LIBRARY NAMES lzo2 liblzo2)

# OIIO
if("${INTERCEPT_LINK_TYPE}" STREQUAL "static")
	add_definitions(-DOIIO_STATIC_DEFINE)
	add_definitions(-DGRAD_AFF_STATIC_DEFINE)
endif()

find_package(OpenImageIO CONFIG REQUIRED)

FIND_PATH(GRAD_AFF_INCLUDE_DIR grad_aff/grad_aff.h HINTS ${GRAD_AFF_PATH} PATH_SUFFIXES include)
FIND_LIBRARY(GRAD_AFF_LIB_DIR NAMES grad_aff HINTS ${GRAD_AFF_PATH} PATH_SUFFIXES lib)
message(DBG  ${GRAD_AFF_INCLUDE_DIR})
message(DBG  ${GRAD_AFF_LIB_DIR})

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

include_directories(${Boost_INCLUDE_DIRS}) 
include_directories(${GRAD_AFF_INCLUDE_DIR})

add_library(${INTERCEPT_PLUGIN_NAME} SHARED ${INTERCEPT_PLUGIN_SOURCES} ${INTERCEPT_HOST_SOURCES})

target_include_directories(${INTERCEPT_PLUGIN_NAME} PUBLIC ${OPENIMAGEIO_INCLUDE_DIR})

target_link_libraries(${INTERCEPT_PLUGIN_NAME} OpenImageIO::OpenImageIO OpenImageIO::OpenImageIO_Util)
target_link_libraries(${INTERCEPT_PLUGIN_NAME} ${Boost_LIBRARIES})

target_link_libraries(${INTERCEPT_PLUGIN_NAME} nlohmann_json nlohmann_json::nlohmann_json)
target_link_libraries(${INTERCEPT_PLUGIN_NAME} ${LZO2_LIBRARY})
target_link_libraries(${INTERCEPT_PLUGIN_NAME} ${SQUISH_LIBRARY})
target_link_libraries(${INTERCEPT_PLUGIN_NAME} ${GRAD_AFF_LIB_DIR})

target_link_libraries(${INTERCEPT_PLUGIN_NAME} OpenSSL::SSL OpenSSL::Crypto)

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
	set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "/OPT:REF /DEBUG:FULL") 
endif()


if(NOT "${PLUGIN_FOLDER}" STREQUAL "")
	add_custom_command(TARGET ${INTERCEPT_PLUGIN_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${INTERCEPT_PLUGIN_NAME}> ${PLUGIN_FOLDER}/${INTERCEPT_PLUGIN_NAME}.dll
	)

	add_custom_command(TARGET ${INTERCEPT_PLUGIN_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_PDB_FILE:${INTERCEPT_PLUGIN_NAME}> ${PLUGIN_FOLDER}/${INTERCEPT_PLUGIN_NAME}.pdb
	)
endif()