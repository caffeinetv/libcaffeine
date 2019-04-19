@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION
REM Script to install git hooks

FOR /R .hooks %%i IN (*) DO (
	SET "T_PATH=%%i"
	FOR %%j IN ("!T_PATH!") DO (
		SET "T_DIR=%%~dj"
		SET "T_NAME=%%~nj"
		SET "T_EXT=%%~xj"
		SET "T_FULLNAME=%%~nxj"
	)
	IF NOT EXIST "%~dp0/.git/hooks/!T_NAME!" (
		MKLINK /H "%~dp0/.git/hooks/!T_NAME!" "!T_PATH!"
	)
)
