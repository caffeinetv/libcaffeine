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

* [LLVM 8.0.1 or later](http://releases.llvm.org/download.html)
* [Visual Studio 2019](https://visualstudio.microsoft.com/downloads/)
* [LLVM Compiler Toolchain Extension](https://marketplace.visualstudio.com/items?itemName=LLVMExtensions.llvm-toolchain) (can also be installed from within Visual Studio)

Steps:

* Download the latest version of webrtc-prebuilt-windows.7z from https://github.com/caffeinetv/webrtc/releases
* Extract the file somewhere convenient
* Set `WEBRTC_ROOT_DIR` environment variable to the directory you extracted the files to
* In the libcaffeine root directory:
    * `mkdir build`
    * `cd build`
    * `cmake .. -G "Visual Studio 16 2019" -T LLVM`
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

**TODO:** Linux
