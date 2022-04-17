# README of FTDI MPSSE control library


# Build

## requirements

Download and install software:

* download and install cmake(https://cmake.org/download/);
* download ***Visual Studio Installer***(https://visualstudio.microsoft.com/downloads/) and install 'Visual Studio Build Tools';


## Build, package

From "Start Menu/Visual Studio/Visual Studio Tools/VC/", select item of 'XXXX Tools Command Prompt for VS XXXX' to enter the command line window;

And then input following command to build and package binary and header files;

```
	
	mkdir build

	cd build

	cmake ../heat-chamber-tester -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="./_install"   -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON

	cmake --build . --target install

	cpack -G ZIP -C Release
		then 'ftdiCtrl-0.1-win64|32.zip' is created in current directory

	cmake --build . --target package_source
		package source file
```

