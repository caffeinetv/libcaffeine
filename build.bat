@echo off

setlocal
set BUILD_DIR64="build64"
set BUILD_DIR32="build32"
set DEST_DIR="dist"
set PACKAGE_NAME="libcaffeine-v0.7-windows"
set BIN_DIR64="%PACKAGE_NAME%\bin64"
set LIB_DIR64="%PACKAGE_NAME%\lib64"
set BIN_DIR32="%PACKAGE_NAME%\bin32"
set LIB_DIR32="%PACKAGE_NAME%\lib32"

goto :MAIN

:: @function to check project prerequisites
:-check 
    echo "Checking prerequisites...."
    if not defined WEBRTC_ROOT_DIR (
        echo "Error: Missing required dependencies. Check if WEBRTC_ROOT_DIR env var is set."
        EXIT /b /1
    )
    if not defined DepsPath (
        echo "Error: Missing required dependencies. Check if DepsPath env var is set."
        EXIT /b /1
    )
    echo "Found all prerequisites."
    goto:EOF

:: @function to delete file/directories 
:-clean
    IF EXIST %~1 (
        del /q /f %~1\* && for /d %%x in (%~1\*) do @rd /s /q "%%x"
    ) ELSE (
        mkdir %~1
    )
    goto:EOF
    goto:EOF

:: @function to find vs studio 
:-checkVS
    for /f "tokens=* USEBACKQ" %%f in (`call "%PROGRAMFILES(x86)%\Microsoft Visual Studio\Installer\vswhere" -version "[16.0,17.0)" -property installationPath`) DO (
        set VSINSTALLATION=%%f
    )
    if "%VSINSTALLATION%" == "" (
    echo Unable to find Visual Studio installation.
    exit /b 1
    )
    echo "%VSINSTALLATION%" 
    goto:EOF

:: @function to build libcaffeine
:-build
    call :-check
    echo "Building libcaffeine 64 bit"
    call :-clean %BUILD_DIR64%
    echo "Looking for Visual Studio path"
    for /f "tokens=* USEBACKQ" %%f in (`call "%PROGRAMFILES(x86)%\Microsoft Visual Studio\Installer\vswhere" -version "[16.0,17.0)" -property installationPath`) DO (
        set VSINSTALLATION=%%f
    )
    if "%VSINSTALLATION%" == "" (
        echo Unable to find Visual Studio installation.
        exit /b 1
    )
    echo "%VSINSTALLATION%"
    echo "%VSINSTALLATION%\VC\Tools\Llvm\bin\clang.exe"
    echo "changing slases"
    set "VSINSTALLATION=%VSINSTALLATION:\=/%"
    echo "%VSINSTALLATION%"
   
    cmake -H. -Bbuild64 -G "Visual Studio 16 2019" -A "x64" -T "ClangCL" -D BUILD_TESTS=OFF -DWEBRTC_ROOT_DIR="%WEBRTC_ROOT_DIR%" -DDepsPath="%DepsPath%\win64\include" -DCMAKE_INSTALL_PREFIX=dist  -DCMAKE_C_COMPILER="%VSINSTALLATION%/VC/Tools/Llvm/bin/clang.exe" -DCMAKE_CXX_COMPILER="%VSINSTALLATION%/VC/Tools/Llvm/bin/clang++.exe"
    msbuild build64/libcaffeine.sln /p:Configuration=Debug /p:Platform=x64 /t:Build /m
    if %errorlevel% neq 0 (
        echo x64 Debug build failed.
        exit /b 1
    )
    msbuild build64/libcaffeine.sln /p:Configuration=RelWithDebInfo /p:Platform=x64 /t:Build /m
    if %errorlevel% neq 0 (
        echo x64 RelWithDebInfo build failed.
        exit /b 1
    )
    echo "Successfully built libcaffeine 64 bit."
    echo "Now building libcaffeine 32 bit ...."
    call :-clean %BUILD_DIR32%
    cmake -H. -Bbuild32 -G "Visual Studio 16 2019" -A "Win32" -T "ClangCL" -D BUILD_TESTS=OFF -D WEBRTC_ROOT_DIR="%WEBRTC_ROOT_DIR%" -D DepsPath="%DepsPath%\win32\include" -D CMAKE_INSTALL_PREFIX=dist -DCMAKE_C_COMPILER="%VSINSTALLATION%/VC/Tools/Llvm/bin/clang.exe" -DCMAKE_CXX_COMPILER="%VSINSTALLATION%/VC/Tools/Llvm/bin/clang++.exe"
    msbuild build32/libcaffeine.sln /p:Configuration=Debug /p:PlatformTarget=x86 /t:Build /m
    if %errorlevel% neq 0 (
        echo x86 Debug build failed.
        exit /b 1
    )
    msbuild build32/libcaffeine.sln /p:Configuration=RelWithDebInfo /p:PlatformTarget=x86 /t:Build /m
    if %errorlevel% neq 0 (
        echo x86 Debug build failed.
        exit /b 1
    )
    echo "Successfully built libcaffeine 32 bit."
    goto:EOF

:: @function to package libcaffeine
:-package
    echo "Packaging ..."
    IF EXIST %DEST_DIR% (
        echo "File already exists"
        del /s /q  %DEST_DIR%\*
        rmdir /Q /S %DEST_DIR%\
    ) else (
        mkdir %DEST_DIR%
    )
    
    cd %DEST_DIR%
    mkdir %PACKAGE_NAME%
    cd %PACKAGE_NAME%
    mkdir "bin64"
    mkdir "bin32"
    cd ..
    echo "Packaging 64 bit..."
    xcopy  ..\include %PACKAGE_NAME%\include /E /I
    copy ..\%BUILD_DIR64%\RelWithDebInfo\libcaffeine.dll %BIN_DIR64%\
    copy ..\%BUILD_DIR64%\RelWithDebInfo\libcaffeine.pdb %BIN_DIR64%\
    copy ..\%BUILD_DIR64%\Debug\libcaffeined.dll %BIN_DIR64%\
    copy ..\%BUILD_DIR64%\Debug\libcaffeined.pdb %BIN_DIR64%\
    xcopy ..\%BUILD_DIR64%\CMakeFiles\Export\lib64\cmake\libcaffeineTarget.cmake %PACKAGE_NAME%\lib64\cmake\ /E /I
    xcopy ..\%BUILD_DIR64%\CMakeFiles\Export\lib64\cmake\libcaffeineTarget-relwithdebinfo.cmake %PACKAGE_NAME%\lib64\cmake\ /E /I
    xcopy ..\%BUILD_DIR64%\CMakeFiles\Export\lib64\cmake\libcaffeineTarget-debug.cmake %PACKAGE_NAME%\lib64\cmake\ /E /I
    copy ..\%BUILD_DIR64%\libcaffeineConfigVersion.cmake %LIB_DIR64%\cmake\
    copy ..\%BUILD_DIR64%\libcaffeineConfig.cmake %LIB_DIR64%\cmake\
    copy ..\%BUILD_DIR64%\Debug\libcaffeined.lib %LIB_DIR64%\
    copy ..\%BUILD_DIR64%\RelWithDebInfo\libcaffeine.lib %LIB_DIR64%\

    echo "Packaging 32 bit..."
    copy ..\%BUILD_DIR32%\RelWithDebInfo\libcaffeine.dll %BIN_DIR32%\
    copy ..\%BUILD_DIR32%\RelWithDebInfo\libcaffeine.pdb %BIN_DIR32%\
    copy ..\%BUILD_DIR32%\Debug\libcaffeined.dll %BIN_DIR32%\
    copy ..\%BUILD_DIR32%\Debug\libcaffeined.pdb %BIN_DIR32%\
    xcopy ..\%BUILD_DIR32%\CMakeFiles\Export\lib32\cmake\libcaffeineTarget.cmake %PACKAGE_NAME%\lib32\cmake\ /E /I
    xcopy ..\%BUILD_DIR32%\CMakeFiles\Export\lib32\cmake\libcaffeineTarget-relwithdebinfo.cmake %PACKAGE_NAME%\lib32\cmake\ /E /I
    xcopy ..\%BUILD_DIR32%\CMakeFiles\Export\lib32\cmake\libcaffeineTarget-debug.cmake %PACKAGE_NAME%\lib32\cmake\ /E /I
    copy ..\%BUILD_DIR32%\libcaffeineConfigVersion.cmake %LIB_DIR32%\cmake\
    copy ..\%BUILD_DIR32%\libcaffeineConfig.cmake %LIB_DIR32%\cmake\
    copy ..\%BUILD_DIR32%\Debug\libcaffeined.lib %LIB_DIR32%\
    copy ..\%BUILD_DIR32%\RelWithDebInfo\libcaffeine.lib %LIB_DIR32%\
    7z a %PACKAGE_NAME%.7z %PACKAGE_NAME%
    cd ..
    goto:EOF

:: @function to build and package 
:-buildAndPackage
    call:-build
    call:-package
    goto:EOF

:: @function to print help output 
:-help
    echo "Supported options: "
    echo -help : Prints this output.
    echo -check : Checks prerequisites.
    echo -checkVS : Visual studio exists.
    echo -build : Builds project.
    echo -package : Packages libcaffeine with 7z.
    echo -buildAndPackage : Builds and packages libcaffeine.
    goto:EOF

:: @function entry point for the script
:MAIN
    if [%1]==[] (
        echo "Please run the script build.bat [OPTION]"
        call:-help
        goto:EOF
    )
    call :%~1
    goto:EOF