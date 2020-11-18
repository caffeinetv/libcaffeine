# libcaffeine

This is a library for setting up broadcasts and streaming on [Caffeine.tv](https://www.caffeine.tv).

Libcaffeine is licensed under the GNU GPL version 2. See LICENSE.txt for details.

For information on contributing to this project, see [CONTRIBUTING.md](CONTRIBUTING.md).

TODO: More Documentation

## Using the library

Minimal Example:

```c
// Callbacks
void started(void *myContext) {
    // Start capturing audio & video
}

void failed(void *myContext, caff_Result result) {
    // Handle error
    // Stop capturing if necessary
}


// Once on startup
caff_initialize("example", "1.0", caff_SeverityDebug, NULL);


// Starting a broadcast
caff_InstanceHandle instance = caff_createInstance();
caff_Result result = caff_signIn(instance, "Myuser", "|\\/|Yp455WYrd", NULL);
if (result != caff_ResultSuccess) return -1;
result = caff_startBroadcast(instance, &myContext, "My title!", caff_RatingNone, NULL, started, failed);


// On your video/audio capture thread(s)
caff_sendVideo(
    instance, caff_VideoFormatNv12, pixels, pixelBytes, width, height, caff_TimestampGenerate);
caff_sendAudio(instance, samples, 2);


// When it's over:
caff_endBroadcast(instance);
caff_freeInstance(&instance);
```

## Build instructions

### Windows

Prereqs:

* Visual Studio 2017 (and [LLVM Compiler Toolchain Extension](https://marketplace.visualstudio.com/items?itemName=LLVMExtensions.llvm-toolchain))
* LLVM toolchain

Steps:

* Download the latest version of webrtc-prebuilt-windows.7z from https://github.com/caffeinetv/webrtc/releases
* Extract the file somewhere convenient
* Set `WEBRTC_ROOT_DIR` environment variable to the directory you extracted the files to
* In the libcaffeine root directory:
    * `mkdir build`
    * `cd build`
    * `cmake .. -G "Visual Studio 15 2017 Win64" -T LLVM`
    * `start libcaffeine.sln`
* Build the solution in Visual Studio

### Mac
Prereqs:

* Xcode
* CMAKE

Steps:

* Download the latest version of webrtc-prebuilt-macos.7z from https://github.com/caffeinetv/webrtc/releases
* Extract the archive somewhere convenient
* Set `WEBRTC_ROOT_DIR` environment variable to the directory you extracted the files to
* In the libcaffeine root directory:
    * `mkdir build`
    * `cd build`
    * `cmake .. -G Xcode`
    * Build the project in Xcode

Note: if the cmake command above fails, it may be due to the XCode commandline tools being selected. To resolve this, reset your XCode selection and retry: `sudo xcode-select -r`

**TODO:** Linux

## Packaging Instructions    



### Prerequisites  
To run the build 
### Windows
* Download [7z](https://www.7-zip.org/download.html) 
* Add 7z.exe to system Path env variable.
* Add msbuild binary to system PATH env var
* Add cmake binary to system PATH env var.
* Add LLVM bin to system PATH env var.

### Mac
* Install using homebrew 
  ```
  brew install p7zip 
  ```
### Automated steps:

Change directory to be the libcaffeine directory.
### Windows:
* Run the automated script in command prompt     
    ``` 
    build.bat [OPTION] 
    ```
### Mac
* Run the automated script in Terminal     
    ``` 
    sh build.sh [OPTION] 
    ```
Supported options 

    Options         | Usage
    -------------   | -------------
    -check          | To check project prerequisites
    -build          | Builds the project
    -package        | Packages libcaffein with 7z
    -buildAndPackage| Builds and packages libcaffeine.

The packaged output will be in `build` directory in Mac and dist directory in Windows.

### Manual steps: 

### Windows:
* Build libcaffeine for both 32- and 64-bit windows
* Create a folder named libcaffeine-vX.Y-windows
* Inside that create these folders:
    * bin32
    * bin64
    * include
    * lib32
    * lib64
* Copy caffeine.h into include
* Copy the .dll and .pdb files into the binXX folders
* Copy the .lib files into the libXX folders
* From `CMakeFiles/Export/libXX/cmake` folder copy `libcaffeineTarget.cmake`, `libcaffeineTarget-debug.cmake` , `libcaffeineTarget-debug.cmake`  into libXX/cmake
* Copy libcaffeineConfig.cmake into libXX/cmake
* Copy libcaffeineConfigVersion.cmake into libXX/cmake
* 7z a libcaffeine-vX.Y-windows libcaffeine-vX.Y-windows.7z

### Mac:
* Build for macOS
* Create a folder named libcaffeine-vX.Y-macos
* Inside that create these folders:
    * binXX
    * include
    * libXX
* Copy `caffeine.h` into include
* Copy `libcaffeine.dylib` into binXX
* Copy `libcaffeined.a` (static lib) and `libcaffeined.dylib` into libXX
* Copy `libcaffeine.a` (static lib) and `libcaffeine.dylib` into libXX
* From `CMakeFiles/Export/libXX/cmake` folder copy `libcaffeineTarget.cmake`, `libcaffeineTarget-debug.cmake` , `libcaffeineTarget-relwithdebinfo.cmake`  into libXX/cmake
* Copy libcaffeineConfig.cmake into libXX/cmake
* Copy libcaffeineConfigVersion.cmake into libXX/cmake
* 7z a libcaffeine-vX.Y-macos.7z libcaffeine-vX.Y-macos

## Metadata

- Team: Frontend / Desktop
- Product Owner: Viral Desai
- Engineer: Prateeksha Das
- Engineer: David Chen


