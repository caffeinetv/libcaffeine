@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION

PUSHD ..
cmake -H. -Bbuild32 -G "Visual Studio 16 2019" -A "Win32" -T "ClangCL" -D WEBRTC_ROOT_DIR="third_party/webrtc" -D DepsPath="third_party/obsdeps/win32" -D BUILD_TESTING=OFF -Dlibcaffeine_BuildTests=OFF
cmake -H. -Bbuild64 -G "Visual Studio 16 2019" -A "x64" -T "ClangCL" -D WEBRTC_ROOT_DIR="third_party/webrtc" -D DepsPath="third_party/obsdeps/win64" -D BUILD_TESTING=OFF -Dlibcaffeine_BuildTests=OFF
POPD

PUSHD ..
START "Build 32-Bit" /LOW cmake --build build32 --config RelWithDebInfo
START "Build 64-Bit" /LOW cmake --build build64 --config RelWithDebInfo
POPD
