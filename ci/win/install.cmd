@ECHO OFF
CALL "%~dp0\bootstrap"

REM Update Submodules (if there are any)...
ECHO Updating submodules...
CALL git submodule update --init --force --recursive 2>NUL 1>NUL

REM Download dependencies
IF NOT EXIST "%ROOT%\.cache" (
    MKDIR "%ROOT%\.cache"
)

SET VSIXPATH=%ROOT%\.cache\llvm.vsix

ECHO Installing LLVM toolchain
IF EXIST %ROOT%\.cache\obs_deps.zip (
    curl -kL "%LLVM_EXTENSION%" -f --retry 5 -o "%VSIXPATH%" -z "%VSIXPATH%"
) ELSE (
    curl -kL "%LLVM_EXTENSION%" -f --retry 5 -o "%VSIXPATH%"
)
"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\IDE\VSIXInstaller.exe" /q /a "%VSIXPATH%"

ECHO Updating libcurl...
IF EXIST %ROOT%\.cache\obs_deps.zip (
    curl -kL "%OBS_DEPENDENCIES_URL%" -f --retry 5 -o "%ROOT%\.cache\obs_deps.zip" -z "%ROOT%\.cache\obs_deps.zip"
) ELSE (
    curl -kL "%OBS_DEPENDENCIES_URL%" -f --retry 5 -o "%ROOT%\.cache\obs_deps.zip"
)
7z x -y -o"%ROOT%\%OBS_DEPENDENCIES_PATH%" "%ROOT%\.cache\obs_deps.zip"

ECHO Updating WebRTC...
IF EXIST "%ROOT%\.cache\webrtc.7z" (
    curl -kL "%WEBRTC_URL%" -f --retry 5 -o "%ROOT%\.cache\webrtc.7z" -z "%ROOT%\.cache\webrtc.7z"
) ELSE (
    curl -kL "%WEBRTC_URL%" -f --retry 5 -o "%ROOT%\.cache\webrtc.7z"
)
7z x -y -o"%ROOT%\%WEBRTC_PATH%" "%ROOT%\.cache\webrtc.7z"

ECHO Done.
