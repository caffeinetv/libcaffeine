@ECHO OFF
CALL "%~dp0\bootstrap"

REM Update Submodules (if there are any)...
ECHO Updating submodules...
CALL git submodule update --init --force --recursive 2>NUL 1>NUL

REM Download dependencies
IF NOT EXIST "%ROOT%\.cache" (
	MKDIR "%ROOT%\.cache"
)

ECHO Updating libcurl...
IF EXIST %ROOT%\.cache\obs_deps.zip (
	curl -kL "%OBS_DEPENDENCIES_URL%" -f --retry 5 -o "%ROOT%\.cache\obs_deps.zip" -z "%ROOT%\.cache\obs_deps.zip"
) ELSE (
	curl -kL "%OBS_DEPENDENCIES_URL%" -f --retry 5 -o "%ROOT%\.cache\obs_deps.zip"
)
7z x -y -o"%ROOT%\.cache\obs_deps" "%ROOT%\.cache\obs_deps.zip"

ECHO Updating WebRTC Source...
IF EXIST "%ROOT%\.cache\webrtc_src.zip" (
	curl -kL "%WEBRTC_SRC_URL%" -f --retry 5 -o "%ROOT%\.cache\webrtc_src.zip" -z "%ROOT%\.cache\webrtc_src.zip"
) ELSE (
	curl -kL "%WEBRTC_SRC_URL%" -f --retry 5 -o "%ROOT%\.cache\webrtc_src.zip"
)
7z x -y -o"%ROOT%\.cache\webrtc" "%ROOT%\.cache\webrtc_src.zip"

ECHO Updating WebRTC Binaries...
IF EXIST "%ROOT%\.cache\webrtc_bin.7z" (
	curl -kL "%WEBRTC_BIN_URL%" -f --retry 5 -o "%ROOT%\.cache\webrtc_bin.7z" -z "%ROOT%\.cache\webrtc_bin.7z"
) ELSE (
	curl -kL "%WEBRTC_BIN_URL%" -f --retry 5 -o "%ROOT%\.cache\webrtc_bin.7z"
)
7z x -y -o"%ROOT%\.cache\webrtc%WEBRTC_SRC_PATH_SUFFIX%\out" "%ROOT%\.cache\webrtc_bin.7z"

ECHO Done.