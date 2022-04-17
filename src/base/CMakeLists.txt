# Base library: for thread, socket, file, time, etc.

set(HEADER_LIST "" 
#	${CMAKE_CURRENT_BINARY_DIR}/config.h
)

list(APPEND MISC_SRC_LIST
	baseConfig.c
	baseLog.c
	baseCtype.c
	baseErrno.c 
	baseExcept.c 
	baseHash.c
	baseLogWriterStdout.c 
	baseRand.c 
	baseTimer.c 
)

list(APPEND DATA_SET_SRC_LIST
	baseArray.c
	baseFifoBuf.c 
	baseRbTree.c 
	baseList.c 
	baseString.c 
	baseGuid.c 
	pool/basePool.c 
	pool/basePoolBuf.c
	pool/basePoolCaching.c
#	pool/basePoolDbg.c
	pool/basePoolPolicyMalloc.c

)	

list(APPEND NET_SRC_LIST
	baseActiveSock.c
	baseAddrResolvSock.c
	net/baseIoQueueSelect.c 
	net/baseSockCommon.c 
	net/baseSockBsd.c 
	net/baseSockSelect.c 
	net/baseSockQosBsd.c 
	net/baseSockQosCommon.c 
)

list(APPEND OS_SRC_LIST
	os/baseOsTimeCommon.c
	os/baseOsTimestampCommon.c 
	os/baseOsInfo.c 
	baseLock.c
#	baseSymbols.c
)

if(MSVC)
	list(APPEND OS_SRC_LIST
		os/baseGuidWin32.c
		os/baseUnicodeWin32.c
		os/baseOsTimeWin32.c
		os/baseOsTimestampWin32.c
		os/baseOsErrorWin32.c
		os/baseOsCoreWin32.c
	)

	list(APPEND NET_SRC_LIST
		net/baseIpHelperWin32.c
	)

	list(APPEND IO_SRC_LIST
		os/baseFileAccessWin32.c
		os/baseFileIoWin32.c
	)
endif(MSVC)

if(UNIX OR MINGW OR MSYS)
	list(APPEND OS_SRC_LIST
		os/baseGuidSimple.c
		os/baseOsTimeUnix.c
		os/baseOsTimestampPosix.c
		os/baseOsErrorUnix.c
		os/baseOsCoreUnix.c
	)

	list(APPEND NET_SRC_LIST
		net/baseIpHelperGeneric.c
	)
	
	list(APPEND IO_SRC_LIST
		os/baseFileAccessUnistd.c
		os/baseFileIoAnsi.c
	)
endif(UNIX OR MINGW OR MSYS)

set(SRC_LIST 
	${DATA_SET_SRC_LIST}
	${OS_SRC_LIST}
	${IO_SRC_LIST}
	${NET_SRC_LIST}
	${MISC_SRC_LIST}
)

set(LIBNAME libBase)
message( "source files ${SRC_LIST}")
#  cmn_version.c
# Make an automatic library - will be static or dynamic based on user setting
# add_library(${LIBNAME} SHARED ${SRC_LIST} ${HEADER_LIST})
# static libary for easy debug. Mar.27, 2020
add_library(${LIBNAME} ${SRC_LIST} ${HEADER_LIST})

# only when explicitly export symbols
# include(GenerateExportHeader)
# generate_export_header(${LIBNAME})

include(defineFileName)
define_relative_file_paths("${SRC_LIST}") # quote "" must be added for variable
# add_definitions(-DTHIS_FILE=__FILE__)

if(MSVC)
#	target_compile_definitions(${LIBNAME} PRIVATE "BASE_DLL=_declspec(dllexport)" INTERFACE "BASE_DLL=_declspec(dllimport)")
	add_definitions(-DBASE_DLL)
endif (MSVC)

# We need this directory, and users of our library will need it too
target_include_directories(${LIBNAME} PUBLIC ${PROJECT_SOURCE_DIR}/include )
target_include_directories(${LIBNAME} PUBLIC ${PROJECT_SOURCE_DIR}/include/base )
target_include_directories(${LIBNAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} )
target_include_directories(${LIBNAME} PUBLIC "${PROJECT_BINARY_DIR}" )

# Add parts of libcompat as required
# target_sources(${LIBNAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/deps/fpclassify.c)


# This depends on (header only) boost
# both 32/64-bit with same name; now move to source code with '#pragma comment(lib, "Ws2_32.lib")'
# target_link_libraries(${LIBNAME} ws2_32 ) 

# All users of this library will need at least C++11
# target_compile_features(FtdiCtrl PUBLIC cxx_std_11)

# IDEs should put the headers in a nice place
# source_group(TREE "${PROJECT_SOURCE_DIR}/include" PREFIX "Header Files" FILES ${HEADER_LIST})

install(TARGETS ${LIBNAME} DESTINATION .)
#install(FILES ../include/ienso.h DESTINATION include)
