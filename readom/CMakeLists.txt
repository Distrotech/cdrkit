PROJECT (READECD C)
INCLUDE_DIRECTORIES(../include ../wodim ${CMAKE_BINARY_DIR} ${CMAKE_BINARY_DIR}/include)
INCLUDE(../include/AddScgBits.cmake)
INCLUDE(../include/AddSchilyBits.cmake)

AUX_SOURCE_DIRECTORY(. readom_SRCS)

LINK_DIRECTORIES(../librols ../libusal  ../wodim)

ADD_DEFINITIONS(-DCLONE_WRITE)

ADD_EXECUTABLE (readom ${readom_SRCS})
TARGET_LINK_LIBRARIES(readom wodimstuff ${EXTRA_LIBS})
SET_TARGET_PROPERTIES(readom PROPERTIES  SKIP_BUILD_RPATH TRUE)

INSTALL(TARGETS readom DESTINATION bin)
INSTALL(FILES 
readom.1
DESTINATION ${MANSUBDIR}/man1)
