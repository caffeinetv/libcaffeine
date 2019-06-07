# libcaffeine

This is a library for setting up broadcasts and streaming on [Caffeine.tv](https://www.caffeine.tv).

Libcaffeine is licensed under the GNU GPL version 2. See LICENSE.txt for details.

TODO: More Documentation

## Using the library

Minimal Example:

```c
void started(void *myContext) {
    // Start capturing audio & video
}

void failed(void *myContext, caff_Result result) {
    // Handle error
    // Stop capturing if necessary
}

// Starting a broadcast:
caff_initialize(caff_SeverityDebug, NULL);
caff_InstanceHandle instance = caff_createInstance();
caff_Result result = caff_signIn(instance, "Myuser", "|\\/|Yp455WYrd", NULL);
if (result != caff_ResultSuccess) return -1;
result = caff_startBroadcast(
    instance, &myContext, "My title!", caff_RatingNone, NULL, started, failed);


// In your video/audio capture thread:
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

**TODO:** Linux

## Code style

`.clang-format` is the source of truth. Ignore anything here that conflicts.

Rule of thumb: mimic the style of whatever is around what you're changing. Here are some decided-on details:

### Text formatting

* 120-column text width
* 4-space indentation
* Always use braces for compound statements (`if`, `for`, `switch`, etc)
* Spaces on both sides of `&` and `*` in declarations. `char * foo;`
* East const (`char const * const` instead of `const char * const`)

### Naming

* Types, enum values, and file names: `PascalCase`
* Macros (to be avoided): `SCREAMING_SNAKE_CASE`
* Other names: `camelCase`

Names should be unadorned (no hungarian notation, no distincion between global, local, and member variables, etc.).

Acronyms, abbreviations, initialisms, etc should be treated like words, e.g. `IceUrl iceUrl;`

All names should be declared in the `caff` namespace, except when extending 3rd-party namespaces.

In C, all names should be prefixed with `caff_`, and otherwise follow the same conventions.

In C++, prefer `enum class`es. In C, include the enum type in the name of the enum value, e.g. `caff_ErrorNotSignedIn`

C++ filenames use `.hpp` and `.cpp`. C filenames use `.h` and `.c`.
