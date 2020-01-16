# Adapted from obs-studio's version

# Once done these will be defined:
#
#  LIBCURL_FOUND
#  LIBCURL_INCLUDE_DIRS
#  LIBCURL_LIBRARIES
#
# For use in OBS:
#
#  CURL_INCLUDE_DIRS
# TODO is ^^^^ needed?

# Variables
set(CURL_DIR "" CACHE PATH "Path to NVIDIA Audio Effects SDK")

include(FindPackageHandleStandardArgs)
find_package(PkgConfig QUIET)
if (PKG_CONFIG_FOUND)
	pkg_check_modules(_CURL QUIET curl libcurl)
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_lib_suffix 64)
else()
	set(_lib_suffix 32)
endif()

find_path(CURL_INCLUDE_DIRS
	NAMES curl/curl.h
	HINTS
		ENV curlPath${_lib_suffix}
		ENV curlPath
		ENV DepsPath${_lib_suffix}
		ENV DepsPath
		${curlPath${_lib_suffix}}
		${curlPath}
		${DepsPath${_lib_suffix}}
		${DepsPath}
		${_CURL_INCLUDE_DIRS}
		${CURL_DIR}
	PATHS
		/usr/include /usr/local/include /opt/local/include /sw/include
	PATH_SUFFIXES
		include)

find_library(CURL_LIBRARIES
	NAMES ${_CURL_LIBRARIES} curl libcurl
	HINTS
		ENV curlPath${_lib_suffix}
		ENV curlPath
		ENV DepsPath${_lib_suffix}
		ENV DepsPath
		${curlPath${_lib_suffix}}
		${curlPath}
		${DepsPath${_lib_suffix}}
		${DepsPath}
		${_CURL_LIBRARY_DIRS}
		${CURL_DIR}
	PATHS
		/usr/lib /usr/local/lib /opt/local/lib /sw/lib
	PATH_SUFFIXES
		lib${_lib_suffix} lib
		libs${_lib_suffix} libs
		bin${_lib_suffix} bin
		../lib${_lib_suffix} ../lib
		../libs${_lib_suffix} ../libs
		../bin${_lib_suffix} ../bin
		"build/Win${_lib_suffix}/VC12/DLL Release - DLL Windows SSPI"
		"../build/Win${_lib_suffix}/VC12/DLL Release - DLL Windows SSPI")

find_package_handle_standard_args(libCURL
	FOUND_VAR LIBCURL_FOUND
	REQUIRED_VARS CURL_INCLUDE_DIRS CURL_LIBRARIES
)
mark_as_advanced(CURL_INCLUDE_DIRS CURL_LIBRARIES)

if(LIBCURL_FOUND)
	set(LIBCURL_INCLUDE_DIRS ${CURL_INCLUDE_DIRS})
	set(LIBCURL_LIBRARIES ${CURL_LIBRARIES})
endif()
