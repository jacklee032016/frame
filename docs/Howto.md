# About build, package and install
03.27, 2020

## About Cross-Compile
* cmake don't support


## Modify file name of source package

```
	# TIMESTAMP is keyword of string; TODAY is output variable;
	string(TIMESTAMP TODAY "%Y%m%d")

	set(CPACK_SOURCE_IGNORE_FILES
		"xrootd-dsi/;/build/;/.deps/;/.bzr/;~$;'.'o$;\
		/CMakeFiles/;/_CPack_Packages/;.tar.gz$;.tar.bz2;CMakeCache.txt; \
		ApMon;build;install_manifest.txt;/lib/;/.git/;${CPACK_SOURCE_IGNORE_FILES};")

	# SET(CPACK_SOURCE_GENERATOR "TGZ;TZ")
	# after change source file name, 'cmake --build .' must be executed before package_source
	set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${PROJECT_VERSION}-source-${TODAY}")
```

## Link to Linux libraries

* When link the generated libraries, using:

```
	target_link_libraries(${TEST_NAME} PUBLIC libBase libUtil libCmn libIensoSdk)
```

* When link Linux libraries in Linux platform:

```
	target_link_libraries(${TEST_NAME} PUBLIC "-pthread" m )
```

## about **install** target

* define "install" statement in every CMakeLists.txt when some files are needed to be installed;
* for example:

```
	install(TARGETS ${TEST_NAME} DESTINATION .)
	install(FILES ${PROJECT_SOURCE_DIR}/tests/open.bat DESTINATION .)
	
```


## basic build process for Windows/Linux PC

```
	cmake ..\frame -G "NMake Makefiles"  # Windows
	cmake ../frame  # Linux, generate Makefiles automatically
	
	cmake --build .

```

### generate source and binary packages

```	
	cmake --build . --target package_source # create package of source files
	
	
	cmake --build . --target install   # install all binaries into `_install`
	cpack -G ZIP -C Release # 

```

Never use "cmake --build . --target package ", because no "generator NSIS" installed;
