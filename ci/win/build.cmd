@ECHO OFF
CALL "%~dp0\bootstrap"

IF NOT EXIST "%ROOT%\build" (
	MKDIR "%ROOT%\build"
)

REM Configure project
ECHO Configuring...
CALL cmake -H. -Bbuild -G"%CMAKE_GENERATOR%" -T"%CMAKE_TOOLSET%" -DWEBRTC_ROOT_DIR=".cache\webrtc%WEBRTC_SRC_PATH_SUFFIX%" -DDepsPath=".cache\obs_deps\%OBS_DEPENDENCIES_ARCH%" -DCMAKE_INSTALL_PREFIX=".install"

REM Build project
ECHO Building...
CALL cmake --build "%ROOT%\build" --target libcaffeine --config RelWithDebInfo -- /logger:"C:\Program Files\AppVeyor\BuildAgent\Appveyor.MSBuildLogger.dll"
