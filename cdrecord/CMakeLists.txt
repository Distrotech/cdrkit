PROJECT (CDRECORD)
INCLUDE_DIRECTORIES(../include . ${CMAKE_BINARY_DIR})
INCLUDE(../include/AddScgBits.cmake)

AUX_SOURCE_DIRECTORY(. CDRECORD_SRCS)

FIND_LIBRARY(CAPTEST cap)
if(CAPTEST) 
   set(CAPLINK cap)
endif(CAPTEST)

#INCLUDE(CheckIncludeFiles)
#CHECK_INCLUDE_FILES("stdio.h;linux/syscalls.h" IS_LINUX)
#if(IS_LINUX AND NOT CAPTEST)
#   MESSAGE(FATAL_ERROR "Error: found a Linux system but no libcap header. Install libcap-dev.")
#endif(IS_LINUX AND NOT CAPTEST)

LINK_DIRECTORIES(../libschily ../libscg ../libdeflt)
ADD_EXECUTABLE (wodim ${CDRECORD_SRCS})
TARGET_LINK_LIBRARIES(wodim ${SCG_LIBS} schily deflt ${CAPLINK})
SET_TARGET_PROPERTIES(wodim PROPERTIES SKIP_BUILD_RPATH TRUE)

INSTALL(TARGETS wodim DESTINATION bin)
INSTALL(FILES 
   wodim.1
DESTINATION share/man/man1)
