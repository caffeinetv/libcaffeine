#!/bin/bash
#!/usr/local/bin/bash

# Script to build and package libcaffeine

# define constants 
BUILD_DIR="build"
PACKAGE_NAME="libcaffeine-v0.6.7-macos"
BIN_DIR="$PACKAGE_NAME/bin$(getconf LONG_BIT)"
LIB_DIR="$PACKAGE_NAME/lib$(getconf LONG_BIT)"

check () {
    if [ -z "$WEBRTC_ROOT_DIR" ]; then
        echo "WEBRITC_ROOT_DIR environment var is not set"
        exit 1
    else
        echo "Found all prerequisites."
    fi
}

build () {
    clean
    if [ ! -d $BUILD_DIR ]; then
        echo "Creating build directory"
        mkdir build
    else 
        echo "Found the build"
    fi
    cd $BUILD_DIR 
    cmake .. -G Xcode 
    xcodebuild  -project libcaffeine.xcodeproj -target libcaffeine -configuration RelWithDebInfo
    xcodebuild  -project libcaffeine.xcodeproj -target libcaffeine -configuration Release
    xcodebuild  -project libcaffeine.xcodeproj -target libcaffeine -configuration Debug
    xcodebuild  -project libcaffeine.xcodeproj -target libcaffeine -configuration MinSizeRel
    xcodebuild  -project libcaffeine.xcodeproj -target libcaffeine-static -configuration RelWithDebInfo
    xcodebuild  -project libcaffeine.xcodeproj -target libcaffeine-static -configuration Debug
    cd ..
}

package () {
    if [ ! -d build ]; then 
        echo "No build directory found." 
        exit 1  
    fi
    cd $BUILD_DIR
    if [ -d $PACKAGE_NAME ]; then 
        rm -R $PACKAGE_NAME 
    fi 
    mkdir $PACKAGE_NAME
    cd $PACKAGE_NAME
    mkdir "bin$(getconf LONG_BIT)"
    cd ..
    cp -R ../include $PACKAGE_NAME
    cp -R RelWithDebInfo/libcaffeine.dylib  $BIN_DIR
    cp -R Release/libcaffeiner.dylib  $BIN_DIR
    cp -R Debug/libcaffeined.dylib  $BIN_DIR
    cp -R MinSizeRel/libcaffeines.dylib  $BIN_DIR
    cp -R CMakeFiles/Export/lib64 $PACKAGE_NAME
    cp -R libcaffeineConfigVersion.cmake $LIB_DIR/cmake
    cp -R libcaffeineConfig.cmake $LIB_DIR/cmake
    cp -rf $BIN_DIR/ $LIB_DIR
    cp -rf Debug/libcaffeine-staticd.a $LIB_DIR/libcaffeined.a 
    cp -rf RelWithDebInfo/libcaffeine-static.a $LIB_DIR/libcaffeine.a 
    7z a $PACKAGE_NAME.7z $PACKAGE_NAME
    cd ..
}

clean () {
    if [ -d $BUILD_DIR ]; then 
        rm -R $BUILD_DIR
    fi
}

help () {
    echo "Supported options: "
    echo -help : Prints this output.
    echo -check : Checks prerequisites.
    echo -build : Builds project.
    echo -package : Packages libcaffeine with 7z.
    echo -buildAndPackage : Builds and packages libcaffeine.
}

[ $# -eq 0 ] && { echo "Please run the script build.sh [OPTION]"; help; exit 1; }
while [ ! $# -eq 0 ]
do
	case "$1" in
		-check)
			check
			exit
			;;
		-build)
			build
			exit
			;;
        -package)
			package
			exit
			;;
        -buildAndPackage)
            build
            package
            exit
            ;;
        -help)
			help
			exit
			;;
	esac
	shift
done