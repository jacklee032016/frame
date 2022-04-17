@ECHO OFF

SET "PWD_DIR=%~dp0"
echo %PWD_DIR%

set "NET_SDK_HOME=%PWD_DIR%"

@ECHO ON
echo %NET_SDK_HOME%

set "PATH=%NET_SDK_HOME%\bin;%PATH%"

rem put this in path "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build"

rem check vc version in C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Tools\MSVC
rem vcvarsall.bat x64 -vcvars_ver=14.24
vcvarsall.bat x64 

