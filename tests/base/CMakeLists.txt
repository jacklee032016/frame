set(TEST_NAME baseTest)

list(APPEND TEST_SRC_LIST
	testBaseActiveSock.c
	testBaseAtomic.c 
	testBaseEchoClt.c 
	testBaseErrno.c 
	testBaseException.c 
	testBaseFifoBuf.c 
	testBaseFile.c 
	testBaseHash.c 
	testBaseIoqPerf.c 
	testBaseIoqUdp.c 
	testBaseIoqUnreg.c 
	testBaseIoqTcp.c 
	testBaseList.c 
	testBaseMutex.c 
	testBaseOs.c 
	testBasePool.c 
	testBasePoolPerf.c 
	testBaseRand.c 
	testBaseRbTree.c 
	testBaseSelect.c 
	testBaseSleep.c 
	testBaseSock.c 
	testBaseSockPerf.c 
	testBaseSslSock.c 
	testBaseString.c 
	testBaseThread.c 
	testBaseTimer.c 
	testBaseTimeStamp.c 
	testBaseUdpEchoSrvSync.c 
	testBaseUdpEchoSrvIoqueue.c 
	testBaseUtil.c 

	testBaseTest.c
)

if(MSVC)
	list(APPEND TEST_SRC_LIST
		testBaseMain.c
#		testBaseMainWin32.c
	)

endif(MSVC)

if(UNIX OR MINGW OR MSYS)
	list(APPEND TEST_SRC_LIST
		testBaseMain.c
	)
	
endif(UNIX OR MINGW OR MSYS)

	
if(MSVC)
	add_executable(${TEST_NAME} ${TEST_SRC_LIST}) # WIN32 
else(MSVC)
	add_executable(${TEST_NAME} ${TEST_SRC_LIST})
endif(MSVC)

include(defineFileName)
define_relative_file_paths("${TEST_SRC_LIST}") # quote "" must be added for variable

# add_definitions(-DTHIS_FILE=__FILE__)


# target_compile_features(app PRIVATE cxx_std_17)

# target_link_options(${TEST_NAME} PRIVATE -mwindows)

target_include_directories(${TEST_NAME} PUBLIC ${PROJECT_BINARY_DIR} )
target_include_directories(${TEST_NAME} PUBLIC ${PROJECT_SOURCE_DIR} )
target_include_directories(${TEST_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include )
target_include_directories(${TEST_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include/base )
target_include_directories(${TEST_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} )

if(MSVC)
#	target_compile_definitions(${LIBNAME} PRIVATE "BASE_DLL=_declspec(dllexport)" INTERFACE "BASE_DLL=_declspec(dllimport)") 
	add_definitions(-DBASE_DLL )
endif (MSVC)

target_link_libraries(${TEST_NAME} PUBLIC libBase)

if(MSVC)
else(MSVC)
target_link_libraries(${TEST_NAME} PUBLIC "-pthread" m )
endif(MSVC)

install(TARGETS ${TEST_NAME} DESTINATION .)
#install(FILES ${PROJECT_SOURCE_DIR}/tests/open.bat DESTINATION .)
#install(FILES ../docs/usages.md DESTINATION .)
