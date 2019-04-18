cmake_minimum_required(VERSION 2.8.12)

################################################################################
# Setup / Bootstrap
################################################################################
include(cmake/CMakeHelpers.cmake)
include(cmake/CMakeFindExtensions.cmake)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules")

project(libcaffeine)

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
include_directories(
    include
    third_party/x264/include #TODO find externally
    third_party/nlohmann_json/single_include
    third_party/stb

    ${LIBCURL_INCLUDE_DIRS}

	${WEBRTC_INCLUDE_DIR}
	${WEBRTC_INCLUDE_DIR}/third_party/abseil-cpp
    ${WEBRTC_INCLUDE_DIR}/third_party/libyuv/include
)

# External header distributed with compiled libcaffeine
set(libcaffeine_HEADERS
    include/caffeine.h
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

add_library(libcaffeine SHARED
    ${libcaffeine_HEADERS}
	${libcaffeine_SOURCES}
)

target_link_libraries(libcaffeine PRIVATE
    ${PROJECT_SOURCE_DIR}/third_party/x264/lib/win64/libx264.lib #TODO find externally
    ${LIBCURL_LIBRARIES}
    ${WEBRTC_LIBRARIES}
	${WEBRTC_DEPENDENCIES}
)
