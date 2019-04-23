cmake_minimum_required(VERSION 2.8.12)

################################################################################
# Setup / Bootstrap
################################################################################
include(cmake/CMakeHelpers.cmake)
include(cmake/CMakeFindExtensions.cmake)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules")

project(libcaffeine)

math(EXPR BITS "8*${CMAKE_SIZEOF_VOID_P}")

################################################################################
# CMake / Compiler
################################################################################
# C++ Standard and Extensions
SET(CMAKE_CXX_STANDARD 14)
## Disable nonstandard extensions
SET(CMAKE_CXX_EXTENSIONS OFF)

################################################################################
# Options
################################################################################

# WebRTC
find_package(WebRTC REQUIRED)

# LibCURL
# TODO: use a C++ http library
find_package(Libcurl REQUIRED)

################################################################################
# Code
################################################################################
# External header distributed with compiled libcaffeine
set(libcaffeine_HEADERS
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/caffeine.h>  
    $<INSTALL_INTERFACE:caffeine.h>
)

# Source & internal headers
set(libcaffeine_SOURCES
	src/Api.hpp
	src/Api.cpp
	src/AudioDevice.cpp
	src/AudioDevice.hpp
	src/AudioDeviceDefaultImpl.hpp
	src/Broadcast.cpp
	src/Broadcast.hpp
	src/Caffeine.cpp
	src/ErrorLogging.hpp
	src/Instance.cpp
	src/Instance.hpp
	src/LogSink.cpp
	src/LogSink.hpp
	src/PeerConnectionObserver.cpp
	src/PeerConnectionObserver.hpp
	src/Serialization.cpp
	src/Serialization.hpp
	src/SessionDescriptionObserver.cpp
	src/SessionDescriptionObserver.hpp
	src/VideoCapturer.cpp
	src/VideoCapturer.hpp
	src/X264Encoder.cpp
	src/X264Encoder.hpp
)

if(WIN32)
	add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX)
endif()

if(MSVC)
	add_compile_options("/MP" "/WX" "/EHa")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")

	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")

	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /MT")
	set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} /MT")

	set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} /MT")
	set(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} /MT")
endif()

add_library(${PROJECT_NAME} SHARED)

set_target_properties(${PROJECT_NAME}
	PROPERTIES
		OUTPUT_NAME ${PROJECT_NAME}
		OUTPUT_NAME_DEBUG ${PROJECT_NAME}d
		OUTPUT_NAME_MINSIZEREL ${PROJECT_NAME}s
		OUTPUT_NAME_RELEASE ${PROJECT_NAME}r
)

target_sources(${PROJECT_NAME}
	PRIVATE
		${libcaffeine_SOURCES}
	INTERFACE
		${libcaffeine_HEADERS}
)		

target_link_libraries(${PROJECT_NAME}
	PRIVATE
		${PROJECT_SOURCE_DIR}/third_party/x264/lib/win64/libx264.lib #TODO find externally
		${LIBCURL_LIBRARIES}
		${WEBRTC_LIBRARIES}
		${WEBRTC_DEPENDENCIES}
)

target_include_directories(${PROJECT_NAME}
	PRIVATE
		include
		third_party/x264/include #TODO find externally
		third_party/nlohmann_json/single_include
		third_party/stb
		${LIBCURL_INCLUDE_DIRS}
		${WEBRTC_INCLUDE_DIR}
		${WEBRTC_INCLUDE_DIR}/third_party/abseil-cpp
		${WEBRTC_INCLUDE_DIR}/third_party/libyuv/include
	INTERFACE
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>  
		$<INSTALL_INTERFACE:include>
		
)

################################################################################
# Installing
################################################################################

# Binaries & Headers
install(
	TARGETS ${PROJECT_NAME}
	EXPORT CaffeineTarget
	RUNTIME DESTINATION "./bin/${BITS}" COMPONENT Caffeine
	LIBRARY DESTINATION "./lib/${BITS}" COMPONENT Caffeine
	ARCHIVE DESTINATION "./lib/${BITS}" COMPONENT Caffeine
)
install(
	FILES
	# FIXME: Use generator expressions instead.
		${libcaffeine_HEADERS}
	DESTINATION
		"./include"
	COMPONENT
		Caffeine
)

if(MSVC)
	# Debug Symbols (optional)
	install(
		FILES $<TARGET_PDB_FILE:${PROJECT_NAME}>
		DESTINATION "bin"
		COMPONENT "Caffeine"
		OPTIONAL
	)
endif()

################################################################################
# Packaging
################################################################################

# Package Configuration
include(CMakePackageConfigHelpers)
set(INCLUDE_INSTALL_DIR "./include")
set(LIBRARY_INSTALL_DIR "./lib/${BITS}")
set(BINARY_INSTALL_DIR "./bin/${BITS}")
if(MSVC)
	set(LIBRARY_PATH "${LIBRARY_INSTALL_DIR}/${PROJECT_NAME}.lib")
else()
	set(LIBRARY_PATH "${LIBRARY_INSTALL_DIR}/${PROJECT_NAME}.a")
endif()
if(WIN32)
	set(BINARY_PATH "${LIBRARY_INSTALL_DIR}/${PROJECT_NAME}.dll")
else()
	set(BINARY_PATH "${LIBRARY_INSTALL_DIR}/${PROJECT_NAME}.so")
endif()

configure_package_config_file(
	"cmake/PackageConfig.cmake.in"
	"${CMAKE_CURRENT_BINARY_DIR}/CaffeineConfig.cmake"
	INSTALL_DESTINATION cmake/${BITS}
	PATH_VARS INCLUDE_INSTALL_DIR LIBRARY_INSTALL_DIR BINARY_INSTALL_DIR LIBRARY_PATH BINARY_PATH
)

write_basic_package_version_file(
	"${CMAKE_CURRENT_BINARY_DIR}/CaffeineConfigVersion.cmake"
	VERSION 0.0.1
	COMPATIBILITY SameMajorVersion
)
 
install(
	FILES
		"${CMAKE_CURRENT_BINARY_DIR}/CaffeineConfig.cmake"
		"${CMAKE_CURRENT_BINARY_DIR}/CaffeineConfigVersion.cmake"
	DESTINATION
		cmake/${BITS}
	COMPONENT
		Caffeine
)

# Package Exporting
export(PACKAGE ${PROJECT_NAME})
install(
	EXPORT CaffeineTarget
	DESTINATION cmake/${BITS}
)

# CPack
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY FALSE)
set(CPACK_COMPONENT_INCLUDE_TOPLEVEL_DIRECTORY FALSE)
include(CPack)
