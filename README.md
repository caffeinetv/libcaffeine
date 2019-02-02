# libcaffeine

This is a library for setting up broadcasts and streaming on Caffeine.tv

TODO: Documentation

## Build instructions

### Windows

Prereqs:

* Visual Studio 2017
* LLVM toolchain
* Google depot tools

Steps:

* Build webrtc
* Set WEBRTC\_ROOT\_DIR to the webrtc src/ directory
* `mkdir build`
* `cd build`
* `cmake .. -G "Visual Studio 15 2017 Win64" -T LLVM`
* `start libcaffeine.sln`
* Build in visual studio
