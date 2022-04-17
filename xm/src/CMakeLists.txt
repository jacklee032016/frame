# Base library: for thread, socket, file, time, etc.

set(HEADER_LIST "" 
#	${CMAKE_CURRENT_BINARY_DIR}/config.h
	)


set(SRC_LIST 
	iesDeviceMgr.cpp
	iesDevice.cpp
	iesSession.cpp
	iesCommand.cpp
	iesResponse.cpp
	iesIoCtx.cpp
	iesIoSocket.cpp
	iesUtils.cpp
	json/jsonReader.cpp
	json/jsonValue.cpp
	json/jsonWriter.cpp
	iesJsonFactory.cpp
#	sdkInit.cpp
#	sdkClientSocket.cpp
)

set(LIBNAME libIensoSdk)

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
define_relative_file_paths("${SRC_LIST}") 
# add_definitions(-DTHIS_FILE=__FILE__)

if(MSVC)
#	target_compile_definitions(${LIBNAME} PRIVATE "BASE_DLL=_declspec(dllexport)" INTERFACE "BASE_DLL=_declspec(dllimport)")
	add_definitions(-DBASE_DLL)
	#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++latest")
endif (MSVC)

# We need this directory, and users of our library will need it too
target_include_directories(${LIBNAME} PUBLIC ${PROJECT_SOURCE_DIR}/include )
target_include_directories(${LIBNAME} PUBLIC ${PROJECT_SOURCE_DIR}/include/base )
target_include_directories(${LIBNAME} PUBLIC ${PROJECT_SOURCE_DIR}/include/util )
target_include_directories(${LIBNAME} PUBLIC ${PROJECT_SOURCE_DIR}/include/cmn )
target_include_directories(${LIBNAME} PUBLIC ${PROJECT_SOURCE_DIR}/src/cmn )
target_include_directories(${LIBNAME} PUBLIC ${PROJECT_SOURCE_DIR}/xm/include )
target_include_directories(${LIBNAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} )
target_include_directories(${LIBNAME} PUBLIC "${PROJECT_BINARY_DIR}" )

# Add parts of libcompat as required
# target_sources(${LIBNAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/deps/fpclassify.c)


# This depends on (header only) boost
# both 32/64-bit with same name; now move to source code with '#pragma comment(lib, "Ws2_32.lib")'
target_link_libraries(${LIBNAME} libBase ) 
target_link_libraries(${LIBNAME} libUtil ) 
target_link_libraries(${LIBNAME} libCmn ) 


# All users of this library will need at least C++11
# target_compile_features(${LIBNAME} PUBLIC cxx_std_11)
# target_compile_features(${LIBNAME} PUBLIC cxx_std_17)
target_compile_features(${LIBNAME} PUBLIC cxx_std_17)

# IDEs should put the headers in a nice place
# source_group(TREE "${PROJECT_SOURCE_DIR}/include" PREFIX "Header Files" FILES ${HEADER_LIST})

install(TARGETS ${LIBNAME} DESTINATION .)
#install(FILES ../include/ienso.h DESTINATION include)
