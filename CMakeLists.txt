# Works with 3.11 and tested through 3.15
cmake_minimum_required(VERSION 3.16)

###### global variables
#set(_REL_VERSION Release)
set(_REL_VERSION Debug)


# Project name and a few useful settings. Other commands can pick up the results
project( frame 
    VERSION 0.1
    DESCRIPTION "Command Framework across platform"
    LANGUAGES C CXX)

# path to local modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# Set build features
include(BuildType)


# Configure a project for testing with CTest/CDash
# Only do these if this is the main project, and not if it is included through add_subdirectory
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
endif()

# extract version info
macro(extract_version file setting_name)
	# open file and read to local VERSION_NUMBER
	file(STRINGS ${file} VERSION_NUMBER REGEX "^${setting_name}")
	# replace and put into VERSION_NUMBER_LIST
	string(REPLACE "=" ";" VERSION_NUMBER_LIST ${VERSION_NUMBER})
	# get from list, and set to variable
	list(GET VERSION_NUMBER_LIST 1 ${setting_name})
endmacro(extract_version)

extract_version(version.ac CMN_MAJOR_VERSION)
extract_version(version.ac CMN_MINOR_VERSION)
extract_version(version.ac CMN_MICRO_VERSION)


set(PROJECT_VERSION_MAJOR ${CMN_MAJOR_VERSION})
set(PROJECT_VERSION_MINOR ${CMN_MINOR_VERSION})
set(PROJECT_VERSION_PATCH ${CMN_MICRO_VERSION})
set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

message("MAJOR:${CMN_MAJOR_VERSION}; MINOR:${CMN_MINOR_VERSION}; VERSION:${PROJECT_VERSION}")

# Provides install directory variables as defined by the GNU Coding Standards.
include(GNUInstallDirs)


# Follow ISO C99 standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)          # Use GNU extensions and POSIX standard


option(CMN_ENABLE_GCOV "Turn on test coverage" OFF)
if (CMN_ENABLE_GCOV AND NOT ${CMAKE_C_COMPILER_ID} MATCHES "GNU")
  message(FATAL_ERROR "Code Coverage (gcov) only works if GNU compiler is used!")
endif (CMN_ENABLE_GCOV AND NOT ${CMAKE_C_COMPILER_ID} MATCHES "GNU")


option(ENABLE_MEMORY_LEAKING_TESTS "Enable certain memory leaking tests" ON)

# export all symbols in Windows without explicitly declared
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

option(__EXT_RELEASE__ "Disable most debug output" ON)

if(MSVC)
  add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  add_definitions(-D_CRT_NONSTDC_NO_WARNINGS)
endif(MSVC)



include(CheckCSourceCompiles)
include(CheckCSourceRuns)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckLibraryExists)
include(CheckStructMember)	# local module
include(CheckSymbolExists)
include(CheckTypeExists)	# local module, cmake/CheckTypeExists.cmake
include(CheckTypeSize)


#### Check header file, and assign to a variable
set(INCLUDES "")
macro(ck_check_include_file header var)
  check_include_files("${INCLUDES};${header}" ${var})
  if(${var})
    set(INCLUDES ${INCLUDES} ${header})
  endif(${var})
endmacro(ck_check_include_file)

# Some FreeBSD headers assume sys/types.h was already included.
ck_check_include_file("sys/types.h" HAVE_SYS_TYPES_H)

# Alphabetize the rest unless there's a compelling reason
ck_check_include_file("errno.h" HAVE_ERRNO_H)
ck_check_include_file("inttypes.h" HAVE_INTTYPES_H)
ck_check_include_file("limits.h" HAVE_LIMITS_H)
ck_check_include_file("regex.h" HAVE_REGEX_H)
ck_check_include_file("signal.h" HAVE_SIGNAL_H)
ck_check_include_file("stdarg.h" HAVE_STDARG_H)
ck_check_include_file("stdint.h" HAVE_STDINT_H)
ck_check_include_file("stdlib.h" HAVE_STDLIB_H)
ck_check_include_file("string.h" HAVE_STRING_H)
ck_check_include_file("strings.h" HAVE_STRINGS_H)
ck_check_include_file("sys/time.h" HAVE_SYS_TIME_H)
ck_check_include_file("time.h" HAVE_TIME_H)
ck_check_include_file("unistd.h" HAVE_UNISTD_H)


#### Check functions
check_function_exists(fork HAVE_FORK)
check_function_exists(getline HAVE_GETLINE)
check_function_exists(getpid HAVE_GETPID)
check_function_exists(gettimeofday HAVE_GETTIMEOFDAY)
check_function_exists(localtime_r HAVE_DECL_LOCALTIME_R)
check_function_exists(malloc HAVE_MALLOC)
check_function_exists(mkstemp HAVE_MKSTEMP)
check_function_exists(realloc HAVE_REALLOC)
check_function_exists(setenv HAVE_DECL_SETENV)
check_function_exists(sigaction HAVE_SIGACTION)
check_function_exists(strdup HAVE_DECL_STRDUP)
check_function_exists(strsignal HAVE_DECL_STRSIGNAL)
check_function_exists(_getpid HAVE__GETPID)
check_function_exists(_strdup HAVE__STRDUP)
check_function_exists(alarm HAVE_DECL_ALARM)
if (HAVE_REGEX_H)
  check_function_exists(regcomp HAVE_REGCOMP)
  check_function_exists(regexec HAVE_REGEXEC)
endif()

##### printf related checks
check_function_exists(snprintf HAVE_SNPRINTF_FUNCTION)
check_function_exists(vsnprintf HAVE_VSNPRINTF_FUNCTION)
check_symbol_exists(snprintf stdio.h HAVE_SNPRINTF_SYMBOL)
check_symbol_exists(vsnprintf stdio.h HAVE_VSNPRINTF_SYMBOL)

if(NOT HAVE_SNPRINTF_FUNCTION AND NOT HAVE_SNPRINTF_SYMBOL)
  add_definitions(-Dsnprintf=rpl_snprintf)
  set(snprintf rpl_snprintf)
  add_definitions(-Dvsnprintf=rpl_vsnprintf)
  set(vsnprintf rpl_vsnprintf)
else(NOT HAVE_SNPRINTF_FUNCTION AND NOT HAVE_SNPRINTF_SYMBOL)
  set(HAVE_SNPRINTF 1)
  add_definitions(-DHAVE_SNPRINTF=1)
  set(HAVE_VSNPRINTF 1)
  add_definitions(-DHAVE_VSNPRINTF=1)
endif(NOT HAVE_SNPRINTF_FUNCTION AND NOT HAVE_SNPRINTF_SYMBOL)

if(HAVE_FORK)
  add_definitions(-DHAVE_FORK=1)
  set(HAVE_FORK 1)
else(HAVE_FORK)
  add_definitions(-DHAVE_FORK=0)
  set(HAVE_FORK 0)
endif(HAVE_FORK)

if(HAVE_MKSTEMP)
  add_definitions(-DHAVE_MKSTEMP=1)
  set(HAVE_MKSTEMP 1)
else(HAVE_MKSTEMP)
  add_definitions(-DHAVE_MKSTEMP=0)
  set(HAVE_MKSTEMP 0)
endif(HAVE_MKSTEMP)

if(HAVE_DECL_ALARM)
  add_definitions(-DHAVE_DECL_ALARM=1)
  set(HAVE_DECL_ALARM 1)
else(HAVE_DECL_ALARM)
  add_definitions(-DHAVE_DECL_ALARM=0)
  set(HAVE_DECL_ALARM 0)
endif(HAVE_DECL_ALARM)

if(HAVE_REGEX_H AND HAVE_REGCOMP AND HAVE_REGEXEC)
  add_definitions(-DHAVE_REGEX=1)
  set(HAVE_REGEX 1)
  add_definitions(-DENABLE_REGEX=1)
  set(ENABLE_REGEX 1)
endif()


###############################################################################
# Check defines
set(headers "limits.h")

if(HAVE_STDINT_H)
  list(APPEND headers "stdint.h")
endif(HAVE_STDINT_H)

if(HAVE_INTTYPES_H)
  list(APPEND headers "inttypes.h")
endif(HAVE_INTTYPES_H)

check_symbol_exists(INT64_MAX "${headers}" HAVE_INT64_MAX)
check_symbol_exists(INT64_MIN "${headers}" HAVE_INT64_MIN)
check_symbol_exists(UINT32_MAX "${headers}" HAVE_UINT32_MAX)
check_symbol_exists(UINT64_MAX "${headers}" HAVE_UINT64_MAX)
check_symbol_exists(SIZE_MAX "${headers}" HAVE_SIZE_MAX)
check_symbol_exists(SSIZE_MAX "limits.h" HAVE_SSIZE_MAX)

###############################################################################
# Check struct members

# Check for  tv_sec in struct timeval
if(NOT HAVE_SYS_TIME_H)
  if(MSVC)
    check_struct_member("struct timeval" tv_sec "Winsock2.h" HAVE_STRUCT_TIMEVAL_TV_SEC)
    check_struct_member("struct timeval" tv_usec "Winsock2.h" HAVE_STRUCT_TIMEVAL_TV_USEC)
    check_struct_member("struct timespec" tv_sec "Winsock2.h" HAVE_WINSOCK2_H_STRUCT_TIMESPEC_TV_SEC)
    check_struct_member("struct timespec" tv_sec "time.h" HAVE_TIME_H_STRUCT_TIMESPEC_TV_SEC)
    check_struct_member("struct itimerspec" it_value "Winsock2.h" HAVE_STRUCT_ITIMERSPEC_IT_VALUE)

    if(NOT HAVE_WINSOCK2_H_STRUCT_TIMESPEC_TV_SEC AND NOT HAVE_TIME_H_STRUCT_TIMESPEC_TV_SEC)
      add_definitions(-DSTRUCT_TIMESPEC_DEFINITION_MISSING=1)
      set(STRUCT_TIMESPEC_DEFINITION_MISSING 1)
    endif(NOT HAVE_WINSOCK2_H_STRUCT_TIMESPEC_TV_SEC AND NOT HAVE_TIME_H_STRUCT_TIMESPEC_TV_SEC)

    if(NOT HAVE_STRUCT_ITIMERSPEC_IT_VALUE)
      add_definitions(-DSTRUCT_ITIMERSPEC_DEFINITION_MISSING=1)
      set(STRUCT_ITIMERSPEC_DEFINITION_MISSING 1)
    endif(NOT HAVE_STRUCT_ITIMERSPEC_IT_VALUE)
  endif(MSVC)
endif(NOT HAVE_SYS_TIME_H)

# OSX has sys/time.h, but it still lacks itimerspec
if(HAVE_SYS_TIME_H)
  check_struct_member("struct itimerspec" it_value "sys/time.h" HAVE_STRUCT_ITIMERSPEC_IT_VALUE)
  if(NOT HAVE_STRUCT_ITIMERSPEC_IT_VALUE)
    add_definitions(-DSTRUCT_ITIMERSPEC_DEFINITION_MISSING=1)
    set(STRUCT_ITIMERSPEC_DEFINITION_MISSING 1)
  endif(NOT HAVE_STRUCT_ITIMERSPEC_IT_VALUE)
endif(HAVE_SYS_TIME_H)

###############################################################################
# Check for integer types
check_type_size("short" SIZE_OF_SHORT)
check_type_size("int" SIZE_OF_INT)
check_type_size("long" SIZE_OF_LONG)
check_type_size("long long" SIZE_OF_LONG_LONG)

check_type_size("unsigned short" SIZE_OF_UNSIGNED_SHORT)
check_type_size("unsigned" SIZE_OF_UNSIGNED)
check_type_size("unsigned long" SIZE_OF_UNSIGNED_LONG)
check_type_size("unsigned long long" SIZE_OF_UNSIGNED_LONG_LONG)

check_type_size("__int64" __INT64)
check_type_size("unsigned __int64" UNSIGNED___INT64)

check_type_size(int16_t INT16_T)
check_type_size(int32_t INT32_T)
check_type_size(int64_t INT64_T)
check_type_size(intmax_t INTMAX_T)
check_type_size(uint8_t UINT8_T)
check_type_size(uint16_t UINT16_T)
check_type_size(uint32_t UINT32_T)
check_type_size(uint64_t UINT64_T)
check_type_size(uintmax_t UINTMAX_T)

#
set(CMAKE_EXTRA_INCLUDE_FILES time.h)
check_type_size(clock_t CLOCK_T)
if(NOT HAVE_CLOCK_T)
  set(clock_t int)
endif(NOT HAVE_CLOCK_T)
unset(CMAKE_EXTRA_INCLUDE_FILES)
#
set(CMAKE_EXTRA_INCLUDE_FILES time.h)
check_type_size(clockid_t CLOCKID_T)
if(NOT HAVE_CLOCKID_T)
  set(clockid_t int)
endif(NOT HAVE_CLOCKID_T)
unset(CMAKE_EXTRA_INCLUDE_FILES)
#
check_type_size(size_t SIZE_T)
if(NOT HAVE_SIZE_T)
  if("${CMAKE_SIZEOF_VOID_P}" EQUAL 8)
    set(size_t "uint64_t")
  else("${CMAKE_SIZEOF_VOID_P}" EQUAL 8)
    set(size_t   "uint32_t")
  endif("${CMAKE_SIZEOF_VOID_P}" EQUAL 8)
endif(NOT HAVE_SIZE_T)
#
check_type_size(ssize_t SSIZE_T)
if(NOT HAVE_SSIZE_T)
  if("${CMAKE_SIZEOF_VOID_P}" EQUAL 8)
    set(ssize_t "int64_t")
  else("${CMAKE_SIZEOF_VOID_P}" EQUAL 8)
    set(ssize_t "long")
  endif("${CMAKE_SIZEOF_VOID_P}" EQUAL 8)
endif(NOT HAVE_SSIZE_T)
#
check_type_size(pid_t PID_T)
if(NOT HAVE_PID_T)
  if(WIN32)
    set(pid_t "int")
  else(WIN32)
    MESSAGE(FATAL_ERROR "pid_t doesn't exist on this platform?")
  endif(WIN32)
endif(NOT HAVE_PID_T)
#
set(CMAKE_EXTRA_INCLUDE_FILES time.h)
check_type_size(timer_t TIMER_T)
if(NOT HAVE_TIMER_T)
  set(timer_t int)
endif(NOT HAVE_TIMER_T)
unset(CMAKE_EXTRA_INCLUDE_FILES)

###############################################################################
# Check libraries

check_library_exists(m floor "" HAVE_LIBM)
if (HAVE_LIBM)
  set (LIBM "m")
endif (HAVE_LIBM)

check_library_exists(rt clock_gettime "" HAVE_LIBRT)
if (HAVE_LIBRT)
  set(LIBRT "rt")
  ADD_DEFINITIONS(-DHAVE_LIBRT=1)
endif (HAVE_LIBRT)

check_library_exists(subunit subunit_test_start "" HAVE_SUBUNIT)
if (HAVE_SUBUNIT)
  set(SUBUNIT "subunit")
  set(ENABLE_SUBUNIT 1)
  add_definitions(-DENABLE_SUBUNIT=1)
else(HAVE_SUBUNIT)
  set(ENABLE_SUBUNIT 0)
  add_definitions(-DENABLE_SUBUNIT=0)
endif (HAVE_SUBUNIT)


# Generate "config.h" from "config.h.in"
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h )
# Param @ONLY not used on purpose!
include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR})
# add macro for compiler
add_definitions(-DHAVE_CONFIG_H)
set(CONFIG_HEADER ${CMAKE_CURRENT_BINARY_DIR}/config.h)



if (HAVE_SUBUNIT)
	set(LIBSUBUNIT_PC "libsubunit")
else (HAVE_SUBINIT)
	set(LIBSUBUNIT_PC "")
endif (HAVE_SUBUNIT)

if (CHECK_ENABLE_GCOV)
	set(GCOV_LIBS "-lgcov")
else (CHECK_ENABLE_GCOV)
	set(GCOV_LIBS "")
endif (CHECK_ENABLE_GCOV)

set(PTHREAD_LIBS "")
set(LIBS "")

if (HAVE_LIBM)
	set(LIBS "${LIBS} -lm")
endif (HAVE_LIBM)

if (HAVE_LIBRT)
	set(LIBS "${LIBS} -lrt")
endif (HAVE_LIBRT)

set(PTHREAD_CFLAGS "-pthread")

# configure_file(check.pc.in check.pc @ONLY)

unset(PTHREAD_CFLAGS)
unset(LIBS)
unset(PTHREAD_LIBS)
unset(GCOV_LIBS)
unset(LIBSUBUNIT_PC)
unset(VERSION)
unset(includedir)
unset(libdir)
unset(exec_prefix)


# library code
add_subdirectory(src/cmn)

add_subdirectory(src/base)
add_subdirectory(src/util)
add_subdirectory(xm/src)

# executable code
#add_subdirectory(tests)
add_subdirectory(tests/base)
add_subdirectory(tests/util)

add_subdirectory(xm/programs)
add_subdirectory(xm/tests)

set(CMAKE_BUILD_TYPE ${REL_VERSION})
#set(CMAKE_VERBOSE_MAKEFILE	ON)
set(CMAKE_INSTALL_PREFIX "./_install")

set(CPACK_PACKAGE_VENDOR "Corp.")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Common Framework Library")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

# TIMESTAMP is keyword of string; TODAY is output variable;
string(TIMESTAMP TODAY "%Y%m%d")

set(CPACK_SOURCE_IGNORE_FILES
	"xrootd-dsi/;/build/;/.deps/;/.bzr/;~$;'.'o$;\
	/CMakeFiles/;/_CPack_Packages/;.tar.gz$;.tar.bz2;CMakeCache.txt; \
	ApMon;build;install_manifest.txt;/lib/;/.git/;${CPACK_SOURCE_IGNORE_FILES};")

# SET(CPACK_SOURCE_GENERATOR "TGZ;TZ")
# after change source file name, 'cmake --build .' must be executed before package_source
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${PROJECT_VERSION}-source-${TODAY}")

include(CPack)
