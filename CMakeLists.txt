# Integrate Caffeine into your software with ease!
# Copyright (C) 2018 Caffeine
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

################################################################################
# Setup / Bootstrap
################################################################################

# CMake Setup
cmake_minimum_required(VERSION 3.17.0)

# Search Paths
set(CMAKE_MODULE_PATH
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules"
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake"
)

# Automatic Versioning
set(VERSION_MAJOR 0)
set(VERSION_MINOR 7)
set(VERSION_PATCH 0)
set(VERSION_TWEAK 0)
set(VERSION_SUFFIX "")
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/.git")
	set(GIT_RESULT "")
	set(GIT_OUTPUT "")
	execute_process(
		COMMAND git rev-list --count --topo-order v${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}${VERSION_SUFFIX}..HEAD
		WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
		RESULT_VARIABLE GIT_RESULT
		OUTPUT_VARIABLE GIT_OUTPUT
		OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_STRIP_TRAILING_WHITESPACE ERROR_QUIET
	)
	if(GIT_RESULT EQUAL 0)
		set(VERSION_TWEAK ${GIT_OUTPUT})
	endif()
	execute_process(
		COMMAND git rev-parse --short=8 HEAD
		WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
		RESULT_VARIABLE GIT_RESULT
		OUTPUT_VARIABLE GIT_OUTPUT
		OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_STRIP_TRAILING_WHITESPACE ERROR_QUIET
	)
	if(GIT_RESULT EQUAL 0)
		set(PROJECT_COMMIT ${GIT_OUTPUT})
	endif()
endif()
if(PROJECT_COMMIT)
	set(VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}.${VERSION_TWEAK}${VERSION_SUFFIX}-${PROJECT_COMMIT}")
else()
	set(VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}.${VERSION_TWEAK}${VERSION_SUFFIX}")
endif()

################################################################################
# Project
################################################################################
# set(CMAKE_C_COMPILER $ENV{CC})
# set(CMAKE_CXX_COMPILER $ENV{CXX})
# set(-DCMAKE_C_COMPILER"C:/Program Files (x86)/Microsoft Visual Studio/2019/Enterprise/VC/Tools/Llvm/bin/clang-cl.exe")
# set(-DCMAKE_CXX_COMPILER="C:/Program Files (x86)/Microsoft Visual Studio/2019/Enterprise/VC/Tools/Llvm/bin/clang-cl.exe")
message(STATUS "C Compiler executable: ${CMAKE_C_COMPILER}")
message(STATUS "CXX Compiler executable: ${CMAKE_CXX_COMPILER}")
message(STATUS "Linker executable: ${CMAKE_LINKER}")

project(libcaffeine
	DESCRIPTION "Caffeine integration library"
	VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}.${VERSION_TWEAK}
	HOMEPAGE_URL "https://caffeine.tv/"
)

# Apply compiler specific changes.
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
	message(STATUS "Applying custom flags for MSVC style build.")

	# MSVC/ClangCL
	# - Statically link Microsoft C/C++ Redistributable.
	# - Enable /W3 and disable useless warnings.
	# - Enable C++ exceptions with SEH exceptions.
	# - Enable multi-processor compiling.

	# Enable most useful warnings.
	set(DISABLED_WARNINGS 
		"/wd4061" "/wd4100" "/wd4180" "/wd4201" "/wd4464" "/wd4505" "/wd4514"
		"/wd4571" "/wd4623" "/wd4625" "/wd4626" "/wd4668" "/wd4710" "/wd4774"
		"/wd4820" "/wd5026" "/wd5027" "/wd5039" "/wd5045" "/wd26812"
	)
	add_compile_options("/W3")
	foreach(WARN ${DISABLED_WARNINGS})
		add_compile_options("${WARN}")
	endforeach()

	# Build with static MSVC linkage.
    add_compile_options(
        $<$<CONFIG:>:/MT>
        $<$<CONFIG:Debug>:/MTd>
        $<$<CONFIG:Release>:/MT>
        $<$<CONFIG:RelWithDebInfo>:/MT>
        $<$<CONFIG:MinSizeRel>:/MT>
    )

	# C++ Exceptions & SEH
	add_compile_options("/EHa")

	# Multiprocessor compiling
	add_compile_options("/MP")

elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	message(STATUS "Applying custom flags for GNU style build.")
	
	# Clang/AppleClang/GNU
	# - Don't export by default.
	# - Enable all and extra warnings.
	
	add_compile_options("-Wall")
	add_compile_options("-Wextra")
	add_compile_options("-fvisibility=hidden")

endif()

# Detect Architecture
set(BITS "64")
math(EXPR BITS "8*${CMAKE_SIZEOF_VOID_P}")
set(ARCH "x64")
if("${BITS}" STREQUAL "32")
	set(ARCH "x86")
endif()

# C++ Standard and Extensions
## Use C++14 and no non-standard extensions.
set(_CXX_STANDARD 14)
set(_CXX_EXTENSIONS OFF)

# Includes
include(cmake/CMakeHelpers.cmake)
include(cmake/CMakeFindExtensions.cmake)

################################################################################
# Options
################################################################################
option(BUILD_TESTING "Build unit tests." OFF)

################################################################################
# Project Setup
################################################################################
set(PROJECT_PRIVATE )
set(PROJECT_PUBLIC )
set(PROJECT_INCLUDE_DIRS )
set(PROJECT_PUBLIC_INCLUDE_DIRS )
set(PROJECT_GENERATED )
set(PROJECT_LIBRARIES )
set(PROJECT_DEFINITIONS )

# Dependencies
find_package(libCURL REQUIRED)
#find_package(CURL REQUIRED) # Can't use this due to obsdeps adding a -DEV to the version, which is not handled by FindCURL.
find_package(WEBRTC REQUIRED)
if(WIN32)
	set(X264_LIB "${PROJECT_SOURCE_DIR}/third_party/x264/lib/win${BITS}/libx264.lib")
elseif(APPLE)
	set(X264_LIB "${PROJECT_SOURCE_DIR}/third_party/x264/lib/mac64/libx264.a")
endif()

# Set things up
list(APPEND PROJECT_PRIVATE
	"src/AudioDevice.cpp"
	"src/AudioDevice.hpp"
	"src/AudioDeviceDefaultImpl.hpp"
	"src/Broadcast.cpp"
	"src/Broadcast.hpp"
	"src/Caffeine.cpp"
	"src/CaffQL.hpp"
	"src/Configuration.hpp.in"
	"src/ErrorLogging.hpp"
	"src/Instance.cpp"
	"src/Instance.hpp"
	"src/LogSink.cpp"
	"src/LogSink.hpp"
	"src/PeerConnectionObserver.cpp"
	"src/PeerConnectionObserver.hpp"
	"src/Policy.hpp"
	"src/Policy.cpp"
	"src/RestApi.hpp"
	"src/RestApi.cpp"
	"src/Serialization.cpp"
	"src/Serialization.hpp"
	"src/SessionDescriptionObserver.cpp"
	"src/SessionDescriptionObserver.hpp"
	"src/StatsObserver.cpp"
	"src/StatsObserver.hpp"
	"src/Urls.cpp"
	"src/Urls.hpp"
	"src/Utils.hpp"
	"src/VideoCapturer.cpp"
	"src/VideoCapturer.hpp"
	"src/WebsocketApi.cpp"
	"src/WebsocketApi.hpp"
	"src/X264Encoder.cpp"
	"src/X264Encoder.hpp"
)
list(APPEND PROJECT_PUBLIC
	"include/caffeine.h"
)
list(APPEND PROJECT_GENERATED
	"${CMAKE_BINARY_DIR}/gen/Configuration.hpp"
)
list(APPEND PROJECT_LIBRARIES
	${X264_LIB}
	${LIBCURL_LIBRARIES}
	${WEBRTC_LIBRARIES}
	${WEBRTC_DEPENDENCIES}
)
list(APPEND PROJECT_INCLUDE_DIRS
	"${PROJECT_SOURCE_DIR}/include"
	"${PROJECT_SOURCE_DIR}/src"	
	"${CMAKE_BINARY_DIR}/gen"
	"${PROJECT_SOURCE_DIR}/third_party/x264/include"
	"${PROJECT_SOURCE_DIR}/third_party/asio/include"
	"${PROJECT_SOURCE_DIR}/third_party/nlohmann_json/single_include"
	"${PROJECT_SOURCE_DIR}/third_party/stb"
	"${PROJECT_SOURCE_DIR}/third_party/websocketpp/include"
	"${WEBRTC_INCLUDE_DIR}"
	"${WEBRTC_INCLUDE_DIR}/third_party/abseil-cpp"
	"${WEBRTC_INCLUDE_DIR}/third_party/libyuv/include"
	"${WEBRTC_INCLUDE_DIR}/third_party/boringssl/src/include"
	"${CURL_INCLUDE_DIRS}"
)
list(APPEND PROJECT_PUBLIC_INCLUDE_DIRS
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>
)
list(APPEND PROJECT_DEFINITIONS
	"ASIO_STANDALONE"
)
if(MSVC)
	list(APPEND PROJECT_DEFINITIONS
		_CRT_SECURE_NO_WARNINGS
		_ENABLE_EXTENDED_ALIGNED_STORAGE
	)
endif()
if(WIN32)
	# windows.h
	list(APPEND PROJECT_DEFINITIONS
		WIN32_LEAN_AND_MEAN
		NOGPICAPMASKS
		NOVIRTUALKEYCODES
		#NOWINMESSAGES
		NOWINSTYLES
		NOSYSMETRICS
		NOMENUS
		NOICONS
		NOKEYSTATES
		NOSYSCOMMANDS
		NORASTEROPS
		NOSHOWWINDOW
		NOATOM
		NOCLIPBOARD
		NOCOLOR
		#NOCTLMGR
		NODRAWTEXT
		#NOGDI
		NOKERNEL
		#NOUSER
		#NONLS
		NOMB
		NOMEMMGR
		NOMETAFILE
		NOMINMAX
		#NOMSG
		NOOPENFILE
		NOSCROLL
		NOSERVICE
		NOSOUND
		#NOTEXTMETRIC
		NOWH
		NOWINOFFSETS
		NOCOMM
		NOKANJI
		#NOHELP
		NOPROFILER
		NODEFERWINDOWPOS
		NOMCX
		NOIME
		NOMDI
		NOINOUT
	)
endif()

# Generate Files
configure_file(src/Configuration.hpp.in gen/Configuration.hpp)

# Helper functions
function(fix_properties project_name)
	set_target_properties(${project_name}
		PROPERTIES
			PREFIX ""
			OUTPUT_NAME ${project_name}
			OUTPUT_NAME_DEBUG ${project_name}d
			OUTPUT_NAME_MINSIZEREL ${project_name}s
			OUTPUT_NAME_RELEASE ${project_name}r
			PUBLIC_HEADER ${PROJECT_PUBLIC}
	)
endfunction()
function(fix_libraries project_name)
	target_link_libraries(${project_name} PRIVATE
		${PROJECT_LIBRARIES}
	)
endfunction()

# Groups
source_group("Generated" FILES ${PROJECT_GENERATED})

# Create Object Library
set(OBJLIB_NAME "${PROJECT_NAME}-objlib")
add_library(${OBJLIB_NAME} OBJECT)
target_sources(${OBJLIB_NAME}
	PRIVATE
		${PROJECT_PRIVATE}
#		${PROJECT_PUBLIC}
		${PROJECT_GENERATED}
	INTERFACE
		${PROJECT_PUBLIC}
)
target_compile_definitions(${OBJLIB_NAME}
	PRIVATE
		${PROJECT_DEFINITIONS}
)
target_include_directories(${OBJLIB_NAME}
	PRIVATE
		${PROJECT_INCLUDE_DIRS}
)
target_include_directories(${OBJLIB_NAME}
	INTERFACE
		${PROJECT_PUBLIC_INCLUDE_DIRS}
) # Kept separate due to weird behavior in CMake argument parsing.

# Create static library
set(STATIC_NAME "${PROJECT_NAME}-static")
add_library(${STATIC_NAME} STATIC $<TARGET_OBJECTS:${OBJLIB_NAME}> src/Dummy.cpp)
fix_libraries(${STATIC_NAME})
fix_properties(${STATIC_NAME})

# Create shared library
add_library(${PROJECT_NAME} SHARED $<TARGET_OBJECTS:${OBJLIB_NAME}> src/Dummy.cpp)
fix_libraries(${PROJECT_NAME})
fix_properties(${PROJECT_NAME})

################################################################################
# Tests
################################################################################
if(BUILD_TESTING)
	include(CTest)
	enable_testing()
	add_subdirectory(tests)
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

#do_install()

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
	elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
		if(MSVC)
			set(BINARY_PDB_RELWITHDEBINFO "${BINARY_INSTALL_DIR}/${PROJECT_NAME}.pdb")
			set(BINARY_PDB_PATH "${BINARY_PDB_RELWITHDEBINFO}")
			set(BINARY_PDB_DEBUG "${BINARY_INSTALL_DIR}/${PROJECT_NAME}d.pdb")
		endif()
	endif()

	set(BINARY_PATH				 "${BINARY_INSTALL_DIR}/${PROJECT_NAME}.${CMAKE_SHARED_LIBRARY_SUFFIX}")
	set(LIBRARY_PATH				"${LIBRARY_INSTALL_DIR}/${PROJECT_NAME}.${CMAKE_LINK_LIBRARY_SUFFIX}")
	set(BINARY_RELEASE			  "${BINARY_INSTALL_DIR}/${PROJECT_NAME}.${CMAKE_SHARED_LIBRARY_SUFFIX}")
	set(LIBRARY_RELEASE			 "${LIBRARY_INSTALL_DIR}/${PROJECT_NAME}.${CMAKE_LINK_LIBRARY_SUFFIX}")
	set(BINARY_RELWITHDEBINFO	   "${BINARY_INSTALL_DIR}/${PROJECT_NAME}.${CMAKE_SHARED_LIBRARY_SUFFIX}")
	set(LIBRARY_RELWITHDEBINFO	  "${LIBRARY_INSTALL_DIR}/${PROJECT_NAME}.${CMAKE_LINK_LIBRARY_SUFFIX}")
	set(BINARY_MINSIZEREL		   "${BINARY_INSTALL_DIR}/${PROJECT_NAME}.${CMAKE_SHARED_LIBRARY_SUFFIX}")
	set(LIBRARY_MINSIZEREL		  "${LIBRARY_INSTALL_DIR}/${PROJECT_NAME}.${CMAKE_LINK_LIBRARY_SUFFIX}")
	set(BINARY_DEBUG				"${BINARY_INSTALL_DIR}/${PROJECT_NAME}d.${CMAKE_SHARED_LIBRARY_SUFFIX}")
	set(LIBRARY_DEBUG			   "${LIBRARY_INSTALL_DIR}/${PROJECT_NAME}d.${CMAKE_LINK_LIBRARY_SUFFIX}")

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
		VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}
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
		${CMAKE_COMMAND} -E tar cfv "${CMAKE_BINARY_DIR}/${CPACK_PACKAGE_FILE_NAME}.7z" --format=7zip --
			"${CMAKE_INSTALL_PREFIX}/bin${BITS}"
			"${CMAKE_INSTALL_PREFIX}/lib${BITS}"
			"${CMAKE_INSTALL_PREFIX}/include"
		WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}"
	)
	add_custom_target(
		PACKAGE_ZIP
		${CMAKE_COMMAND} -E tar cfv "${CMAKE_BINARY_DIR}/${CPACK_PACKAGE_FILE_NAME}.zip" --format=zip --
			"${CMAKE_INSTALL_PREFIX}/bin${BITS}"
			"${CMAKE_INSTALL_PREFIX}/lib${BITS}"
			"${CMAKE_INSTALL_PREFIX}/include"
		WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}"
	)
endfunction()

#do_package()
