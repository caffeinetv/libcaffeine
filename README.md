# libcaffeine

This is a library for setting up broadcasts and streaming on [Caffeine.tv](https://www.caffeine.tv).

TODO: Documentation

## Build instructions

### Windows

Prereqs:

* Visual Studio 2017
* LLVM toolchain
* Google depot tools

Steps:

* Follow [Google's instructions](https://webrtc.org/native-code/development/) for checking out webrtc with their tools until you get to the `gn gen ...` command. Instead, do the following:
    * `gn gen outDebug --args="rtc_include_tests=false is_debug=true rtc_use_h264=true" --ide=vs2017`
    * `gn gen outRelease --args="rtc_include_tests=false is_debug=true rtc_use_h264=true" --ide=vs2017`
    * `ninja -C out\Debug webrtc`
    * `ninja -C out\Release webrtc`
* Set `WEBRTC_ROOT_DIR` environment variable to the webrtc src/ directory
* In the libcaffeine root directory:
    * `mkdir build`
    * `cd build`
    * `cmake .. -G "Visual Studio 15 2017 Win64" -T LLVM`
    * `start libcaffeine.sln`
* Build the solution in Visual Studio

**TODO:** Mac

**TODO:** Linux

## Code style

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
