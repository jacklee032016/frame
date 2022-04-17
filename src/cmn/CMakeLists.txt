
# Note that headers are optional, and do not affect add_library, but they will not
# show up in IDEs unless they are listed in add_library.

set(HEADER_LIST "" 
#	${CMAKE_CURRENT_BINARY_DIR}/config.h
	)

set(UTILS_SRC_LIST
	utils/cmnUtils.c
	utils/cmnMem.c
#	utils/cmnRbTree.c 
	)

set(MD5_SRC_LIST
	md5/base64.c 
	md5/cmnChecksum.c 
	md5/md5.c 
	md5/md5hl.c 
)	

set(SOURCE_LIST
	portable/cmnMutex.c
	portable/cmnFifo.c
	portable/cmnFsm.c
	portable/cmnList.c
	portable/cmnFtpLib.c 
	portable/cmnThread.c 
	portable/cmnLog.c
	cjson/cJSON.c 
	)

if(MSVC)
	set(SOURCE_LIST ${SOURCE_LIST}
		sys/winPthread.c
	)
else(MSVC)
endif(MSVC)

set (SRC_LIST 
	${MD5_SRC_LIST}
	${SOURCE_LIST}
	${UTILS_SRC_LIST}
)

message( "source files ${SRC_LIST}")
#  cmn_version.c
# Make an automatic library - will be static or dynamic based on user setting SHARED
add_library(libCmn  ${SRC_LIST} ${HEADER_LIST})

# only when explicitly export symbols
# include(GenerateExportHeader)
# generate_export_header(libCmn)


# We need this directory, and users of our library will need it too
target_include_directories(libCmn PUBLIC ${PROJECT_SOURCE_DIR}/include )
target_include_directories(libCmn PUBLIC ${PROJECT_SOURCE_DIR}/include/cmn )
target_include_directories(libCmn PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} )
target_include_directories(libCmn PUBLIC "${PROJECT_BINARY_DIR}" )

# Add parts of libcompat as required
target_sources(libCmn PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/deps/fpclassify.c)

if (NOT HAVE_LIBRT)
	target_sources(libCmn PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/deps/clock_gettime.c)
	target_sources(libCmn PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/deps/timer_create.c)
	target_sources(libCmn PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/deps/timer_delete.c)
	target_sources(libCmn PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/deps/timer_settime.c)
endif(NOT HAVE_LIBRT)

if(NOT HAVE_GETLINE)
	target_sources(libCmn PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/deps/getline.c)
endif(NOT HAVE_GETLINE)

if(NOT HAVE_GETTIMEOFDAY)
	target_sources(libCmn PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/deps/gettimeofday.c)
endif(NOT HAVE_GETTIMEOFDAY)

if(NOT HAVE_DECL_LOCALTIME_R)
	target_sources(libCmn PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/deps/localtime_r.c)
endif(NOT HAVE_DECL_LOCALTIME_R)

if(NOT HAVE_MALLOC)
	target_sources(libCmn PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/deps/malloc.c)
endif(NOT HAVE_MALLOC)

if(NOT HAVE_REALLOC)
	target_sources(libCmn PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/deps/realloc.c)
endif(NOT HAVE_REALLOC)

if(NOT HAVE_SNPRINTF)
	target_sources(libCmn PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/deps/snprintf.c)
endif(NOT HAVE_SNPRINTF)

if(NOT HAVE_DECL_STRDUP AND NOT HAVE__STRDUP)
	target_sources(libCmn PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/deps/strdup.c)
endif(NOT HAVE_DECL_STRDUP AND NOT HAVE__STRDUP)

if(NOT HAVE_DECL_STRSIGNAL)
	target_sources(libCmn PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/deps/strsignal.c)
endif(NOT HAVE_DECL_STRSIGNAL)

if(NOT HAVE_DECL_ALARM)
	target_sources(libCmn PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/deps/alarm.c)
endif(NOT HAVE_DECL_ALARM)

# Include libraries if available
if (HAVE_LIBM)
	target_link_libraries(libCmn PUBLIC m)
endif (HAVE_LIBM)

if (HAVE_LIBRT)
	target_link_libraries(libCmn PUBLIC rt)
endif (HAVE_LIBRT)
if (HAVE_SUBUNIT)
	target_link_libraries(libCmn PUBLIC subunit)
endif (HAVE_SUBUNIT)

if(MSVC)
	target_compile_definitions(libCmn PRIVATE "CK_DLL_EXP=_declspec(dllexport)" INTERFACE "CK_DLL_EXP=_declspec(dllimport)")
endif (MSVC)


# This depends on (header only) boost
# both 32/64-bit with same name; now move to source code with '#pragma comment(lib, "Ws2_32.lib")'
# target_link_libraries(libCmn ws2_32 ) 

# All users of this library will need at least C++11
# target_compile_features(FtdiCtrl PUBLIC cxx_std_11)

# IDEs should put the headers in a nice place
# source_group(TREE "${PROJECT_SOURCE_DIR}/include" PREFIX "Header Files" FILES ${HEADER_LIST})

install(TARGETS libCmn DESTINATION .)
#install(FILES ../include/ienso.h DESTINATION include)
