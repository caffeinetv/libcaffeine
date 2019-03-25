cmake_minimum_required(VERSION 2.8.12)

include(cmake/CMakeHelpers.cmake)
include(cmake/CMakeFindExtensions.cmake)

project(libcaffeine)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules")

find_package(WebRTC REQUIRED)
find_package(Libcurl REQUIRED) # TODO: use a C++ http library

include_directories(
    include
    x264/include #TODO find externally
    third_party/nlohmann_json/single_include

    ${LIBCURL_INCLUDE_DIRS}

	${WEBRTC_INCLUDE_DIR}
	${WEBRTC_INCLUDE_DIR}/third_party/abseil-cpp
    ${WEBRTC_INCLUDE_DIR}/third_party/libyuv/include
)

# External header distributed with compiled libcaffeine
set(libcaffeine_HEADERS
    include/caffeine.h
    include/caffeine-api.h
)

# Source & internal headers
set(libcaffeine_SOURCES
    src/audiodevice.cpp
    src/audiodevice.hpp
    src/audiodevicedefaultimpl.hpp
    src/caffeine.cpp
    src/caffeine-api.cpp
    src/caffeine-helpers.hpp
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
    ${PROJECT_SOURCE_DIR}/x264/lib/win64/libx264.lib #TODO find externally
    ${LIBCURL_LIBRARIES}
    ${WEBRTC_LIBRARIES}
	${WEBRTC_DEPENDENCIES}
)
