cmake_minimum_required(VERSION 3.1)

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
option(libcaffeine_BuildTests "Build unit tests when BUILD_TESTING is enabled." ON)

# WebRTC
find_package(WebRTC REQUIRED)

# LibCURL
# TODO: use a C++ http library
find_package(Libcurl REQUIRED)

################################################################################
# Code
################################################################################
# External header distributed with compiled libcaffeine
set(libcaffeine_HEADER
    include/caffeine.h
)

# Source & internal headers
set(libcaffeine_SOURCES
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
    src/Policy.hpp
    src/Policy.cpp
    src/RestApi.hpp
    src/RestApi.cpp
    src/Serialization.cpp
    src/Serialization.hpp
    src/SessionDescriptionObserver.cpp
    src/SessionDescriptionObserver.hpp
    src/StatsObserver.cpp
    src/StatsObserver.hpp
    src/Utils.hpp
    src/VideoCapturer.cpp
    src/VideoCapturer.hpp
    src/WebsocketApi.cpp
    src/WebsocketApi.hpp
    src/X264Encoder.cpp
    src/X264Encoder.hpp
)

add_definitions(-DASIO_STANDALONE)

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
elseif(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
endif()

set(OBJLIB_NAME "${PROJECT_NAME}-objlib")
add_library(${OBJLIB_NAME} OBJECT)

target_sources(${OBJLIB_NAME}
    PRIVATE
        ${libcaffeine_SOURCES}
        ${libcaffeine_HEADER}
    INTERFACE
        ${libcaffeine_HEADER}
)

if(WIN32)
    set(X264_LIB "${PROJECT_SOURCE_DIR}/third_party/x264/lib/win${BITS}/libx264.lib")
elseif(APPLE)
    set(X264_LIB "${PROJECT_SOURCE_DIR}/third_party/x264/lib/mac64/libx264.a")
endif()

set(PRIVATE_INCLUDES
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/third_party/x264/include #TODO find externally
    ${PROJECT_SOURCE_DIR}/third_party/asio/include
    ${PROJECT_SOURCE_DIR}/third_party/nlohmann_json/single_include
    ${PROJECT_SOURCE_DIR}/third_party/stb
    ${PROJECT_SOURCE_DIR}/third_party/websocketpp/include
    ${LIBCURL_INCLUDE_DIRS}
    ${WEBRTC_INCLUDE_DIR}
    ${WEBRTC_INCLUDE_DIR}/third_party/abseil-cpp
    ${WEBRTC_INCLUDE_DIR}/third_party/libyuv/include
    ${WEBRTC_INCLUDE_DIR}/third_party/boringssl/src/include
)

set(PUBLIC_INCLUDES
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_include_directories(${OBJLIB_NAME}
    PRIVATE ${PRIVATE_INCLUDES}
    INTERFACE ${PUBLIC_INCLUDES}
)

set(STATIC_NAME "${PROJECT_NAME}-static")

function(set_output_name project_name)
    set_target_properties(${project_name}
        PROPERTIES
            PREFIX "" # Prevent naming the library `liblibcaffeine`
            OUTPUT_NAME ${project_name}
            OUTPUT_NAME_DEBUG ${project_name}d
            OUTPUT_NAME_MINSIZEREL ${project_name}s
            OUTPUT_NAME_RELEASE ${project_name}r
    )
endfunction(set_output_name)

# Add dummy file to library sources to workaround Xcode project generation issues
add_library(${STATIC_NAME} STATIC $<TARGET_OBJECTS:${OBJLIB_NAME}> src/Dummy.cpp)
add_library(${PROJECT_NAME} SHARED $<TARGET_OBJECTS:${OBJLIB_NAME}> src/Dummy.cpp)

function(set_target_link_libraries project_name)
    target_link_libraries(${project_name} PRIVATE
        ${X264_LIB}
        ${LIBCURL_LIBRARIES}
        ${WEBRTC_LIBRARIES}
        ${WEBRTC_DEPENDENCIES}
    )
endfunction(set_target_link_libraries)

set_output_name(${STATIC_NAME})
set_target_link_libraries(${STATIC_NAME})

set_output_name(${PROJECT_NAME})
set_target_link_libraries(${PROJECT_NAME})

set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER ${libcaffeine_HEADER})

################################################################################
# Tests
################################################################################
if(libcaffeine_BuildTests)
    include(CTest)
    if(BUILD_TESTING)
        enable_testing()
        add_subdirectory(tests)
    endif()
endif()

################################################################################
# Installing
################################################################################

function(do_install)
    install(
        TARGETS ${PROJECT_NAME}
        EXPORT ${PROJECT_NAME}Target
        RUNTIME DESTINATION "bin${BITS}" COMPONENT Caffeine
        LIBRARY DESTINATION "lib${BITS}" COMPONENT Caffeine
        ARCHIVE DESTINATION "lib${BITS}" COMPONENT Caffeine
        PUBLIC_HEADER DESTINATION "include" COMPONENT Caffeine
    )
    if(MSVC)
        # Debug Symbols (optional)
        install(
            FILES $<TARGET_PDB_FILE:${PROJECT_NAME}>
            DESTINATION "bin${BITS}"
            COMPONENT Caffeine
            OPTIONAL
        )
    endif()
endfunction()

do_install()

################################################################################
# Packaging
################################################################################

function(do_package)
    # Package Configuration
    include(CMakePackageConfigHelpers)
    set(INCLUDE_INSTALL_DIR "include")
    set(LIBRARY_INSTALL_DIR "lib${BITS}")
    set(BINARY_INSTALL_DIR "bin${BITS}")
    set(BINARY_PDB_PATH "")
    set(BINARY_PDB_RELWITHDEBINFO "")
    set(BINARY_PDB_DEBUG "")
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set(BIN_SUFFIX "dylib")
        set(LIB_SUFFIX "a")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(BIN_SUFFIX "so")
        set(LIB_SUFFIX "a")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
            set(BIN_SUFFIX "dll")
        if(MSVC)
            set(LIB_SUFFIX "lib")
            set(BINARY_PDB_RELWITHDEBINFO "${BINARY_INSTALL_DIR}/${PROJECT_NAME}.pdb")
            set(BINARY_PDB_PATH "${BINARY_PDB_RELWITHDEBINFO}")
            set(BINARY_PDB_DEBUG "${BINARY_INSTALL_DIR}/${PROJECT_NAME}d.pdb")
        else()
            set(LIB_SUFFIX "a")
        endif()
    endif()

    set(BINARY_PATH "${BINARY_INSTALL_DIR}/${PROJECT_NAME}.${BIN_SUFFIX}")
    set(LIBRARY_PATH "${LIBRARY_INSTALL_DIR}/${PROJECT_NAME}.${LIB_SUFFIX}")
    set(BINARY_RELEASE "${BINARY_INSTALL_DIR}/${PROJECT_NAME}r.${BIN_SUFFIX}")
    set(LIBRARY_RELEASE "${LIBRARY_INSTALL_DIR}/${PROJECT_NAME}r.${LIB_SUFFIX}")
    set(BINARY_RELWITHDEBINFO "${BINARY_INSTALL_DIR}/${PROJECT_NAME}.${BIN_SUFFIX}")
    set(LIBRARY_RELWITHDEBINFO "${LIBRARY_INSTALL_DIR}/${PROJECT_NAME}.${LIB_SUFFIX}")
    set(BINARY_MINSIZEREL "${BINARY_INSTALL_DIR}/${PROJECT_NAME}s.${BIN_SUFFIX}")
    set(LIBRARY_MINSIZEREL "${LIBRARY_INSTALL_DIR}/${PROJECT_NAME}s.${LIB_SUFFIX}")
    set(BINARY_DEBUG "${BINARY_INSTALL_DIR}/${PROJECT_NAME}d.${BIN_SUFFIX}")
    set(LIBRARY_DEBUG "${LIBRARY_INSTALL_DIR}/${PROJECT_NAME}d.${LIB_SUFFIX}")

    configure_package_config_file(
        "cmake/PackageConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
        INSTALL_DESTINATION "lib${BITS}/cmake"
        PATH_VARS
            INCLUDE_INSTALL_DIR
            LIBRARY_INSTALL_DIR
            BINARY_INSTALL_DIR

            BINARY_PATH
            BINARY_PDB_PATH
            LIBRARY_PATH
            BINARY_RELEASE
            LIBRARY_RELEASE
            BINARY_RELWITHDEBINFO
            BINARY_PDB_RELWITHDEBINFO
            LIBRARY_RELWITHDEBINFO
            BINARY_MINSIZEREL
            LIBRARY_MINSIZEREL
            BINARY_DEBUG
            BINARY_PDB_DEBUG
            LIBRARY_DEBUG
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
        VERSION 0.0.1
        COMPATIBILITY SameMajorVersion
    )

    install(
        FILES
            "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
        DESTINATION
            "lib${BITS}/cmake"
        COMPONENT
            Caffeine
    )

    # Package Exports
    install(
        EXPORT ${PROJECT_NAME}Target
        DESTINATION "lib${BITS}/cmake"
    )

    # CPack
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY FALSE)
    set(CPACK_COMPONENT_INCLUDE_TOPLEVEL_DIRECTORY FALSE)
    include(CPack)

    # Archive Packaging
    add_custom_target(
        PACKAGE_7Z
        ${CMAKE_COMMAND} -E tar cfv "${CMAKE_BINARY_DIR}/${CPACK_PACKAGE_NAME}.7z" --format=7zip --
            "${CMAKE_INSTALL_PREFIX}/bin${BITS}"
            "${CMAKE_INSTALL_PREFIX}/lib${BITS}"
            "${CMAKE_INSTALL_PREFIX}/include"
        WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}"
    )
    add_custom_target(
        PACKAGE_ZIP
        ${CMAKE_COMMAND} -E tar cfv "${CMAKE_BINARY_DIR}/${CPACK_PACKAGE_NAME}.zip" --format=zip --
            "${CMAKE_INSTALL_PREFIX}/bin${BITS}"
            "${CMAKE_INSTALL_PREFIX}/lib${BITS}"
            "${CMAKE_INSTALL_PREFIX}/include"
        WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}"
    )
endfunction()

do_package()

################################################################################
# Exporting
################################################################################

# TODO: Figure out how to make this work for the CMake package registry.
#function(do_export)
#    export(
#        TARGETS ${PROJECT_NAME}
#        FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Target.cmake"
#    )
#    export(PACKAGE "${PROJECT_NAME}")
#endfunction()
