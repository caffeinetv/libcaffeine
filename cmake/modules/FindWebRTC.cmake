########################################################################
# CMake module for finding WEBRTC
#
# The following variables will be defined:
#
#  WEBRTC_FOUND
#  WEBRTC_INCLUDE_DIR
#  WEBRTC_LIBRARIES
#  WEBRTC_DEPENDENCIES
#

# Set required variables
if (NOT "$ENV{WEBRTC_ROOT_DIR}" STREQUAL "")
    set(WEBRTC_ROOT_DIR "$ENV{WEBRTC_ROOT_DIR}" CACHE INTERNAL "Copied from environment")
else()
    set(WEBRTC_ROOT_DIR "" CACHE STRING "Where is the WebRTC root directory located?")
endif()

# set(WEBRTC_BUILD_DIR_SUFFIX_DEBUG "out/Debug" CACHE STRING "What is the WebRTC debug build directory suffix?")
# set(WEBRTC_BUILD_DIR_SUFFIX_RELEASE "out/Release" CACHE STRING "What is the WebRTC release build directory suffix?")

# ----------------------------------------------------------------------
# Find WEBRTC include path
# ----------------------------------------------------------------------
find_path(WEBRTC_INCLUDE_DIR
  NAMES
    api/peerconnectioninterface.h
  PATHS
    ${WEBRTC_ROOT_DIR}
)

# ----------------------------------------------------------------------
# Find WEBRTC libraries
# ----------------------------------------------------------------------
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_lib_suffix 64)
else()
	set(_lib_suffix 86)
endif()

if(WEBRTC_INCLUDE_DIR)
  find_existing_directory(debug_dir
      ${WEBRTC_ROOT_DIR}/out/Default
      ${WEBRTC_ROOT_DIR}/out/x${_lib_suffix}/Debug
      ${WEBRTC_ROOT_DIR}/out/Debug_x${_lib_suffix}
      ${WEBRTC_ROOT_DIR}/out/Debug-x${_lib_suffix}
      ${WEBRTC_ROOT_DIR}/out/Debug)

  find_existing_directory(release_dir
      ${WEBRTC_ROOT_DIR}/out/Default
      ${WEBRTC_ROOT_DIR}/out/x${_lib_suffix}/Release
      ${WEBRTC_ROOT_DIR}/out/Release_x${_lib_suffix}
      ${WEBRTC_ROOT_DIR}/out/Release-x${_lib_suffix}
      ${WEBRTC_ROOT_DIR}/out/Release)

  message("\n\ndebug_dir: ${debug_dir}\nrelease_dir: ${release_dir}")

  # Attempt to find the monolithic library built with `webrtcbuilds`
  #find_library_extended(WEBRTC
  #  NAMES webrtc webrtc_full libwebrtc_full
  #  PATHS_DEBUG ${debug_dir}
  #  PATHS_RELEASE ${release_dir}
  #)

  #find_library(WEBRTC_LIBRARIES_DEBUG
  #    NAMES webrtc libwebrtc
  #    PATHS ${debug_dir}
  #    PATH_SUFFIXES obj)

  #find_library(WEBRTC_LIBRARIES_RELEASE
  #    NAMES webrtc libwebrtc
  #    PATHS ${release_dir}
  #    PATH_SUFFIXES obj)

  #if(WEBRTC_RELEASE_LIBRARY AND WEBRTC_DEBUG_LIBRARY)
  #    set (WEBRTC_LIBRARIES
  #        ${WEBRTC_DEBUG_LIBRARY} "debug"
  #        ${WEBRTC_RELEASE_LIBRARY} "optimized")
  #elseif(WEBRTC_RELEASE_LIBRARY)
  #    set (WEBRTC_LIBRARIES ${WEBRTC_RELEASE_LIBRARY})
  #elseif(WEBRTC_DEBUG_LIBRARY)
  #    set (WEBRTC_LIBRARIES ${WEBRTC_DEBUG_LIBRARY})
  #endif()

  #message("FOUND ${WEBRTC_LIBRARIES}\n\n")

  # Otherwise fallback to library objects (slower and unreliable)
  if(NOT WEBRTC_LIBRARIES)

    if(MSVC)
      set(lib_suffix "lib")
    else()
      set(lib_suffix "a")
    endif()

    set(_WEBRTC_DEPENDENCY_INCLUDES ".*") #|common|video|media

    # Debug
    if (EXISTS ${debug_dir})
      file(GLOB_RECURSE debug_libs "${debug_dir}/*.${lib_suffix}")
      foreach(lib ${debug_libs})
        # if(${lib} NOT MATCHES ${_WEBRTC_DEPENDENCY_EXCLUDES})
        if(${lib} MATCHES ${_WEBRTC_DEPENDENCY_INCLUDES})
            message("match: ${lib}")
          list(APPEND WEBRTC_LIBRARIES_DEBUG ${lib})
      else()
            message("nomatch: ${lib}")
        endif()
        # endif()
      endforeach()
      foreach(lib ${WEBRTC_LIBRARIES_DEBUG})
        if(WIN32 AND (CMAKE_CONFIGURATION_TYPES OR CMAKE_BUILD_TYPE))
          list(APPEND WEBRTC_LIBRARIES "debug" ${lib})
        else()
          list(APPEND WEBRTC_LIBRARIES ${lib})
        endif()
      endforeach()
    endif()

    # Release
    if (EXISTS ${release_dir})
      file(GLOB_RECURSE release_libs "${release_dir}/*.${lib_suffix}")
      foreach(lib ${release_libs})
        # if(${lib} NOT MATCHES ${_WEBRTC_DEPENDENCY_EXCLUDES})
        if(${lib} MATCHES ${_WEBRTC_DEPENDENCY_INCLUDES})
          list(APPEND WEBRTC_LIBRARIES_RELEASE ${lib})
        endif()
      endforeach()
      foreach(lib ${WEBRTC_LIBRARIES_RELEASE})
        if(WIN32 AND (CMAKE_CONFIGURATION_TYPES OR CMAKE_BUILD_TYPE))
          list(APPEND WEBRTC_LIBRARIES "optimized" ${lib})
        else()
          list(APPEND WEBRTC_LIBRARIES ${lib})
        endif()
      endforeach()
    endif()
  endif()

  # Add required system libraries
  if(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND MSVC)
    add_definitions(-DWEBRTC_WIN)
    set(WEBRTC_DEPENDENCIES Secur32.lib Winmm.lib msdmo.lib dmoguids.lib wmcodecdspuuid.lib) # strmbase.lib
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_definitions(-DWEBRTC_POSIX)

    # For ABI compatability between precompiled WebRTC libraries using clang and new GCC versions
    add_definitions(-D_GLIBCXX_USE_CXX11_ABI=0)
    set(WEBRTC_DEPENDENCIES -lrt -lX11 -lGLU) # -lGL

    # Enable libstdc++ debugging if you build WebRTC with `enable_iterator_debugging=true`
    # set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_GLIBCXX_DEBUG=1")
  endif()

  # Add vendor include directories
  if(WEBRTC_INCLUDE_DIR AND NOT WEBRTC_INCLUDE_DIRS)
    list(APPEND WEBRTC_INCLUDE_DIRS ${WEBRTC_INCLUDE_DIR} ${WEBRTC_INCLUDE_DIR}/third_party/boringssl/src/include)
  endif()

  ## Workaround for fixing error WEBRTC_LIBRARY-NOTFOUND
  #set(WEBRTC_LIBRARY_RELEASE ${WEBRTC_LIBRARIES_RELEASE})
  #set(WEBRTC_LIBRARY_DEBUG ${WEBRTC_LIBRARIES_DEBUG})
  #include(${CMAKE_ROOT}/Modules/SelectLibraryConfigurations.cmake)
  #select_library_configurations(WEBRTC)

  # message("WEBRTC_LIBRARIES_DEBUG: ${WEBRTC_LIBRARIES_DEBUG}")
  # message("WEBRTC_LIBRARIES_RELEASE: ${WEBRTC_LIBRARIES_RELEASE}")
  # message("WEBRTC_LIBRARIES: ${WEBRTC_LIBRARIES}")
endif()

# ----------------------------------------------------------------------
# Display status
# ----------------------------------------------------------------------
include(FindPackageHandleStandardArgs)
message("WEBRTC_INCLUDE_DIR: ${WEBRTC_INCLUDE_DIR}")
message("WEBRTC_INCLUDE_DIRS: ${WEBRTC_INCLUDE_DIRS}")
message("WEBRTC_LIBRARIES_RELEASE: ${WEBRTC_LIBRARIES_RELEASE}")
message("WEBRTC_LIBRARIES_RELEASE: ${WEBRTC_LIBRARIES_DEBUG}")

set(WEBRTC_FOUND TRUE)
find_package_handle_standard_args(WEBRTC DEFAULT_MSG WEBRTC_LIBRARIES_RELEASE WEBRTC_LIBRARIES_DEBUG WEBRTC_INCLUDE_DIRS)

print_module_variables(WEBRTC)

# HACK: WEBRTC_LIBRARIES and WEBRTC_DEPENDENCIES not propagating to parent scope
# while the WEBRTC_DEBUG_LIBRARY and WEBRTC_RELEASE_LIBRARY vars are.
# Setting PARENT_SCOPE fixes this solves theis issue for now.
set(WEBRTC_LIBRARIES ${WEBRTC_LIBRARIES} CACHE INTERNAL "")
set(WEBRTC_DEPENDENCIES ${WEBRTC_DEPENDENCIES} CACHE INTERNAL "")
set(WEBRTC_INCLUDE_DIRS ${WEBRTC_INCLUDE_DIRS} CACHE INTERNAL "")
set(WEBRTC_FOUND ${WEBRTC_FOUND} CACHE INTERNAL "")

mark_as_advanced(WEBRTC_LIBRARIES WEBRTC_INCLUDE_DIR
                 WEBRTC_LIBRARIES_DEBUG WEBRTC_LIBRARIES_RELEASE
                 WEBRTC_BUILD_DIR_SUFFIX_DEBUG WEBRTC_BUILD_DIR_SUFFIX_RELEASE
                 WEBRTC_DEPENDENCIES)
