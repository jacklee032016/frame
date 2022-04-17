set(TEST_NAME utilTest)

list(APPEND TEST_SRC_LIST
	testUtilEncryption.c
	testUtilHttpClient.c
	testUtilJsonTest.c
	testUtilResolverTest.c
	testUtilStun.c
	testUtilTest.c
	testUtilMain.c
#	testUtilMainRtems.c
#	testUtilMainWin32.c
	testUtilXml.c
)

	
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
target_include_directories(${TEST_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include/util )
target_include_directories(${TEST_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} )

if(MSVC)
#	target_compile_definitions(${LIBNAME} PRIVATE "BASE_DLL=_declspec(dllexport)" INTERFACE "BASE_DLL=_declspec(dllimport)") 
	add_definitions(-DBASE_DLL )
endif (MSVC)

target_link_libraries(${TEST_NAME} PUBLIC libBase libUtil)
if(MSVC)
else(MSVC)
target_link_libraries(${TEST_NAME} PUBLIC "-pthread" m )
endif(MSVC)

install(TARGETS ${TEST_NAME} DESTINATION .)
