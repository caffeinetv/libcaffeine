########################################################################
# CMake module for finding WEBRTC
#
# The following variables will be defined:
#
#  WEBRTC_FOUND
#  WEBRTC_INCLUDE_DIRS
#  WEBRTC_LIBRARIES
#  WEBRTC_DEPENDENCIES
#

mark_as_advanced(
	WEBRTC_FOUND
	WEBRTC_INCLUDE_DIR
	WEBRTC_INCLUDE_DIRS
	WEBRTC_PATH_RELEASE
	WEBRTC_PATH_DEBUG
	WEBRTC_LIBRARY_RELEASE
	WEBRTC_LIBRARY_DEBUG
	WEBRTC_LIBRARIES_RELEASE
	WEBRTC_LIBRARIES_DEBUG
	WEBRTC_LIBRARIES
	WEBRTC_DEPENDENCIES
)

#set(WEBRTC_FOUND FALSE CACHE INTERNAL "Found WebRTC")
set(WEBRTC_INCLUDE_DIRS "" CACHE INTERNAL "WebRTC Include Directories")
set(WEBRTC_LIBRARIES_DEBUG "" CACHE INTERNAL "WebRTC Libraries (Debug)")
set(WEBRTC_LIBRARIES_RELEASE "" CACHE INTERNAL "WebRTC Libraries (Release)")
set(WEBRTC_LIBRARIES "" CACHE INTERNAL "WebRTC Libraries")

# Set required variables
if (NOT "$ENV{WEBRTC_ROOT_DIR}" STREQUAL "")
    set(WEBRTC_ROOT_DIR "$ENV{WEBRTC_ROOT_DIR}" CACHE INTERNAL "Copied from environment")
else()
    set(WEBRTC_ROOT_DIR "" CACHE PATH "Where is the WEBRTC root directory located?")
endif()

# ----------------------------------------------------------------------
# Find WEBRTC include directory.
# ----------------------------------------------------------------------
function(find_webrtc_includes search_dir)
	find_path(WEBRTC_INCLUDE_DIR
		NAMES
			api/peerconnectioninterface.h
		PATHS
			${search_dir}
	)

	# Add vendor include directories
	set(_include_dirs ${WEBRTC_INCLUDE_DIR})
	if(_include_dirs)
		list(APPEND _include_dirs ${WEBRTC_INCLUDE_DIR}/third_party/boringssl/src/include)
	endif()

	set(WEBRTC_INCLUDE_DIR ${WEBRTC_INCLUDE_DIR} PARENT_SCOPE)
	set(WEBRTC_INCLUDE_DIRS ${_include_dirs} PARENT_SCOPE)
endfunction()

# ----------------------------------------------------------------------
# Find WEBRTC libraries.
# ----------------------------------------------------------------------
function(find_webrtc_libraries search_dir)
	unset(_libs_debug)
	unset(_libs_release)
	unset(_libs)
	set(_libs "")

	# FIXME: convert to component search similar to ffmpeg?
	set(_WEBRTC_DEPENDENCY_INCLUDES ".*")

	math(EXPR BITS "8*${CMAKE_SIZEOF_VOID_P}")
	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(ARCH 64)
	else()
		set(ARCH 86)
	endif()

    if(MSVC)
		set(lib_suffix "lib")
    else()
		set(lib_suffix "a")
    endif()

	find_path(WEBRTC_PATH_DEBUG
		webrtc.${lib_suffix} libwebrtc.${lib_suffix}
		HINTS
			${search_dir}
			# Debug
			${search_dir}/out/Debug_${ARCH}
			${search_dir}/out/Debug-${ARCH}
			${search_dir}/out/Debug_x${ARCH}
			${search_dir}/out/Debug-x${ARCH}
			${search_dir}/out/Debug_${BITS}
			${search_dir}/out/Debug-${BITS}
			${search_dir}/out/Debug_x${BITS}
			${search_dir}/out/Debug-x${BITS}
			${search_dir}/out/${ARCH}/Debug_${ARCH}
			${search_dir}/out/${ARCH}/Debug-${ARCH}
			${search_dir}/out/${ARCH}/Debug_x${ARCH}
			${search_dir}/out/${ARCH}/Debug-x${ARCH}
			${search_dir}/out/${BITS}/Debug_${BITS}
			${search_dir}/out/${BITS}/Debug-${BITS}
			${search_dir}/out/${BITS}/Debug_x${BITS}
			${search_dir}/out/${BITS}/Debug-x${BITS}
			${search_dir}/out/x${ARCH}/Debug_${ARCH}
			${search_dir}/out/x${ARCH}/Debug-${ARCH}
			${search_dir}/out/x${ARCH}/Debug_x${ARCH}
			${search_dir}/out/x${ARCH}/Debug-x${ARCH}
			${search_dir}/out/x${BITS}/Debug_${BITS}
			${search_dir}/out/x${BITS}/Debug-${BITS}
			${search_dir}/out/x${BITS}/Debug_x${BITS}
			${search_dir}/out/x${BITS}/Debug-x${BITS}
			${search_dir}/out/${ARCH}/Debug
			${search_dir}/out/x${ARCH}/Debug
			${search_dir}/out/${BITS}/Debug
			${search_dir}/out/x${BITS}/Debug
			${search_dir}/out/Debug
			# Mac
			${search_dir}/out/macOS/Debug
			# Default
			${search_dir}/out/Default_${ARCH}
			${search_dir}/out/Default-${ARCH}
			${search_dir}/out/Default_x${ARCH}
			${search_dir}/out/Default-x${ARCH}
			${search_dir}/out/Default_${BITS}
			${search_dir}/out/Default-${BITS}
			${search_dir}/out/Default_x${BITS}
			${search_dir}/out/Default-x${BITS}
			${search_dir}/out/${ARCH}/Default_${ARCH}
			${search_dir}/out/${ARCH}/Default-${ARCH}
			${search_dir}/out/${ARCH}/Default_x${ARCH}
			${search_dir}/out/${ARCH}/Default-x${ARCH}
			${search_dir}/out/${BITS}/Default_${BITS}
			${search_dir}/out/${BITS}/Default-${BITS}
			${search_dir}/out/${BITS}/Default_x${BITS}
			${search_dir}/out/${BITS}/Default-x${BITS}
			${search_dir}/out/x${ARCH}/Default_${ARCH}
			${search_dir}/out/x${ARCH}/Default-${ARCH}
			${search_dir}/out/x${ARCH}/Default_x${ARCH}
			${search_dir}/out/x${ARCH}/Default-x${ARCH}
			${search_dir}/out/x${BITS}/Default_${BITS}
			${search_dir}/out/x${BITS}/Default-${BITS}
			${search_dir}/out/x${BITS}/Default_x${BITS}
			${search_dir}/out/x${BITS}/Default-x${BITS}
			${search_dir}/out/${ARCH}/Default
			${search_dir}/out/x${ARCH}/Default
			${search_dir}/out/${BITS}/Default
			${search_dir}/out/x${BITS}/Default
			${search_dir}/out/Default
		PATHS
			/usr/lib /usr/local/lib /opt/local/lib
		PATH_SUFFIXES
			obj${BITS} obj_${BITS} obj-${BITS}
			obj${ARCH} obj_${ARCH} obj-${ARCH}
			objx${BITS} obj_x${BITS} obj-x${BITS}
			objx${ARCH} obj_x${ARCH} obj-x${ARCH}
			obj
			lib${BITS} lib_${BITS} lib-${BITS}
			lib${ARCH} lib_${ARCH} lib-${ARCH}
			libx${BITS} lib_x${BITS} lib-x${BITS}
			libx${ARCH} lib_x${ARCH} lib-x${ARCH}
			lib

	)

	find_path(WEBRTC_PATH_RELEASE
		webrtc.${lib_suffix} libwebrtc.${lib_suffix}
		HINTS
			${search_dir}
			# Release
			${search_dir}/out/Release_${ARCH}
			${search_dir}/out/Release-${ARCH}
			${search_dir}/out/Release_x${ARCH}
			${search_dir}/out/Release-x${ARCH}
			${search_dir}/out/Release_${BITS}
			${search_dir}/out/Release-${BITS}
			${search_dir}/out/Release_x${BITS}
			${search_dir}/out/Release-x${BITS}
			${search_dir}/out/${ARCH}/Release_${ARCH}
			${search_dir}/out/${ARCH}/Release-${ARCH}
			${search_dir}/out/${ARCH}/Release_x${ARCH}
			${search_dir}/out/${ARCH}/Release-x${ARCH}
			${search_dir}/out/${BITS}/Release_${BITS}
			${search_dir}/out/${BITS}/Release-${BITS}
			${search_dir}/out/${BITS}/Release_x${BITS}
			${search_dir}/out/${BITS}/Release-x${BITS}
			${search_dir}/out/x${ARCH}/Release_${ARCH}
			${search_dir}/out/x${ARCH}/Release-${ARCH}
			${search_dir}/out/x${ARCH}/Release_x${ARCH}
			${search_dir}/out/x${ARCH}/Release-x${ARCH}
			${search_dir}/out/x${BITS}/Release_${BITS}
			${search_dir}/out/x${BITS}/Release-${BITS}
			${search_dir}/out/x${BITS}/Release_x${BITS}
			${search_dir}/out/x${BITS}/Release-x${BITS}
			${search_dir}/out/${ARCH}/Release
			${search_dir}/out/x${ARCH}/Release
			${search_dir}/out/${BITS}/Release
			${search_dir}/out/x${BITS}/Release
			${search_dir}/out/Release
			# Mac
			${search_dir}/out/macOS/Release
			# Fall back to debug
			${WEBRTC_PATH_DEBUG}
		PATHS
			/usr/lib /usr/local/lib /opt/local/lib
		PATH_SUFFIXES
			obj${BITS} obj_${BITS} obj-${BITS}
			obj${ARCH} obj_${ARCH} obj-${ARCH}
			objx${BITS} obj_x${BITS} obj-x${BITS}
			objx${ARCH} obj_x${ARCH} obj-x${ARCH}
			obj
			lib${BITS} lib_${BITS} lib-${BITS}
			lib${ARCH} lib_${ARCH} lib-${ARCH}
			libx${BITS} lib_x${BITS} lib-x${BITS}
			libx${ARCH} lib_x${ARCH} lib-x${ARCH}
			lib
	)

	find_library(WEBRTC_LIBRARY_DEBUG
		NAMES
			webrtc.${lib_suffix} libwebrtc.${lib_suffix}
		PATHS
			${WEBRTC_PATH_DEBUG}
	)

	find_library(WEBRTC_LIBRARY_RELEASE
		NAMES
			webrtc.${lib_suffix} libwebrtc.${lib_suffix}
		PATHS
			${WEBRTC_PATH_RELEASE}
	)

	# Find all debug libraries.
	if (EXISTS ${WEBRTC_PATH_DEBUG})
		file(GLOB_RECURSE _dlibs "${WEBRTC_PATH_DEBUG}/*.${lib_suffix}")

		set(_libs_debug ${WEBRTC_LIBRARY_DEBUG})
		foreach(lib ${_dlibs})
			if(${lib} MATCHES ${_WEBRTC_DEPENDENCY_INCLUDES})
				list(APPEND _libs_debug ${lib})
			endif()
		endforeach()

		foreach(lib ${_libs_debug})
			list(APPEND _libs "debug" ${lib})
		endforeach()
	endif()

	# Find all release libraries.
	if (EXISTS ${WEBRTC_PATH_RELEASE})
		file(GLOB_RECURSE _rlibs "${WEBRTC_PATH_RELEASE}/*.${lib_suffix}")

		set(_libs_release ${WEBRTC_LIBRARY_RELEASE})
		foreach(lib ${_rlibs})
			if(${lib} MATCHES ${_WEBRTC_DEPENDENCY_INCLUDES})
				list(APPEND _libs_release ${lib})
			endif()
		endforeach()

		foreach(lib ${_libs_release})
			list(APPEND _libs "optimized" ${lib})
		endforeach()
	endif()

	set(WEBRTC_PATH_DEBUG ${WEBRTC_PATH_DEBUG} PARENT_SCOPE)
	set(WEBRTC_LIBRARY_DEBUG ${WEBRTC_LIBRARY_DEBUG} PARENT_SCOPE)
	set(WEBRTC_LIBRARIES_DEBUG ${_libs_debug} PARENT_SCOPE)
	set(WEBRTC_PATH_RELEASE ${WEBRTC_PATH_RELEASE} PARENT_SCOPE)
	set(WEBRTC_LIBRARY_RELEASE ${WEBRTC_LIBRARY_RELEASE} PARENT_SCOPE)
	set(WEBRTC_LIBRARIES_RELEASE ${_libs_release} PARENT_SCOPE)
	set(WEBRTC_LIBRARIES ${_libs} PARENT_SCOPE)
endfunction()

function(find_webrtc_dependencies)
	unset(_libs)

	# Add required system libraries
	if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
		add_definitions(-DWEBRTC_WIN)
		set(_libs
			Secur32.lib
			Winmm.lib
			msdmo.lib
			dmoguids.lib
			wmcodecdspuuid.lib
#			strmbase.lib
		)
	elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
		add_definitions(-DWEBRTC_POSIX)

		# For ABI compatability between precompiled WebRTC libraries using clang and new GCC versions
		add_definitions(-D_GLIBCXX_USE_CXX11_ABI=0)
		set(_libs
			-lrt
			-lX11
			-lGLU
#			-lGL
		)
	elseif(APPLE)
		add_definitions(-DWEBRTC_MAC -DWEBRTC_POSIX)

		find_library(AUDIOTOOLBOX_LIBRARY AudioToolbox)
  		find_library(COREAUDIO_LIBRARY CoreAudio)
  		find_library(COREFOUNDATION_LIBRARY CoreFoundation)
  		find_library(COREGRAPHICS_LIBRARY CoreGraphics)
		find_library(FOUNDATION_LIBRARY Foundation)

		set(_libs ${AUDIOTOOLBOX_LIBRARY} ${COREAUDIO_LIBRARY}
			${COREFOUNDATION_LIBRARY} ${COREGRAPHICS_LIBRARY} ${FOUNDATION_LIBRARY})
	endif()

	set(WEBRTC_DEPENDENCIES ${_libs} PARENT_SCOPE)
endfunction()

find_webrtc_includes("${WEBRTC_ROOT_DIR}")
find_webrtc_libraries("${WEBRTC_ROOT_DIR}")
find_webrtc_dependencies()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
	WEBRTC
	FOUND_VAR WEBRTC_FOUND
	REQUIRED_VARS
		WEBRTC_LIBRARIES_DEBUG
		WEBRTC_LIBRARIES_RELEASE
		WEBRTC_LIBRARIES
		WEBRTC_INCLUDE_DIRS
)

if(NOT WEBRTC_FOUND)
	message(FATAL_ERROR)
endif()

set(WEBRTC_INCLUDE_DIRS "${WEBRTC_INCLUDE_DIRS}" CACHE INTERNAL "WebRTC Include Directories")
set(WEBRTC_LIBRARIES_DEBUG "${WEBRTC_LIBRARIES_DEBUG}" CACHE INTERNAL "WebRTC Libraries (Debug)")
set(WEBRTC_LIBRARIES_RELEASE "${WEBRTC_LIBRARIES_RELEASE}" CACHE INTERNAL "WebRTC Libraries (Release)")
set(WEBRTC_LIBRARIES "${WEBRTC_LIBRARIES}" CACHE INTERNAL "WebRTC Libraries")
set(WEBRTC_FOUND "${WEBRTC_FOUND}" CACHE INTERNAL "WebRTC Found")
