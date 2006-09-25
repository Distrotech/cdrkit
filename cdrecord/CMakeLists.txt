PROJECT (CDRECORD)
INCLUDE_DIRECTORIES(../include ../libedc ${CMAKE_BINARY_DIR})
INCLUDE(../include/AddScgBits.cmake)
include(../include/AddSchilyBits.cmake)

AUX_SOURCE_DIRECTORY(. CDRECORD_SRCS)

FIND_FILE(CAPTEST "sys/capability.h")
if(CAPTEST)
	LIST(APPEND EXTRA_LIBS cap)
endif(CAPTEST)

if(CMAKE_SYSTEM_NAME MATCHES "Linux" AND NOT CAPTEST)
   MESSAGE(FATAL_ERROR "Error: found a Linux system but no libcap header. Install libcap-dev.")
endif(CMAKE_SYSTEM_NAME MATCHES "Linux" AND NOT CAPTEST)

LINK_DIRECTORIES(../libschily ../libscg ../libdeflt ../libedc)

ADD_DEFINITIONS(-DHAVE_LIB_EDC_ECC)

ADD_EXECUTABLE (wodim ${CDRECORD_SRCS})
TARGET_LINK_LIBRARIES(wodim ${EXTRA_LIBS} deflt libedc)
SET_TARGET_PROPERTIES(wodim PROPERTIES SKIP_BUILD_RPATH TRUE)

INSTALL(TARGETS wodim DESTINATION bin)
INSTALL(FILES 
   wodim.1
DESTINATION share/man/man1)
