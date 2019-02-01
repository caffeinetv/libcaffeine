cmake_minimum_required(VERSION 2.8.12)

include(cmake/CMakeHelpers.cmake)
include(cmake/CMakeFindExtensions.cmake)

project(libcaffeine)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules")

find_package(WebRTC REQUIRED)

include_directories(
    include
    x264/include #TODO find externally

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
    src/audiodevice.cpp
    src/audiodevice.hpp
    src/audiodevicedefaultimpl.hpp
    src/caffeine.cpp
    src/iceinfo.hpp
    src/interface.cpp
    src/interface.hpp
    src/logsink.cpp
    src/logsink.hpp
    src/peerconnectionobserver.cpp
    src/peerconnectionobserver.hpp
    src/sessiondescriptionobserver.cpp
    src/sessiondescriptionobserver.hpp
    src/stream.cpp
    src/stream.hpp
    src/videocapturer.cpp
    src/videocapturer.hpp
    src/x264encoder.cpp
    src/x264encoder.hpp
)

if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX)
endif()

if(MSVC)
    add_compile_options("/MP")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_RELEASE} /MTd")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_RELEASE} /MTd")
endif()

add_library(libcaffeine SHARED
    ${libcaffeine_HEADERS}
	${libcaffeine_SOURCES}
)

target_link_libraries(libcaffeine
    ${WEBRTC_LIBRARIES}
	${WEBRTC_DEPENDENCIES}
    ${PROJECT_SOURCE_DIR}/x264/lib/win64/libx264.lib #TODO find externally
)
