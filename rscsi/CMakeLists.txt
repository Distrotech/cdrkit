PROJECT (RSCSI)
INCLUDE_DIRECTORIES(../include ../wodim ${CMAKE_BINARY_DIR} )
INCLUDE(../include/AddScgBits.cmake)
INCLUDE(../include/AddSchilyBits.cmake)

LINK_DIRECTORIES(../libschily ../libscg ../libdeflt)
ADD_EXECUTABLE (rscsi rscsi.c)
#SET_SOURCE_FILES_PROPERTIES(rscsi.c )
TARGET_LINK_LIBRARIES(rscsi ${EXTRA_LIBS} deflt)
#SET_TARGET_PROPERTIES(rscsi PROPERTIES  SKIP_BUILD_RPATH TRUE)
INSTALL(TARGETS rscsi DESTINATION sbin)

