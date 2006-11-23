PROJECT (LIBSCG)
INCLUDE_DIRECTORIES(../include ${CMAKE_BINARY_DIR}  ../wodim)
include(../include/AddScgBits.cmake)

#SET(LIBSCG_SRCS rdummy.c usalsettarget.c usaltimes.c scsi-linux-ata.c scsi-linux-pg.c scsi-linux-sg.c scsierrs.c scsihack.c scsihelp.c scsiopen.c scsitransp.c)
SET(LIBSCG_SRCS usalsettarget.c usaltimes.c scsierrs.c scsihack.c scsihelp.c scsiopen.c scsitransp.c scsi-remote.c)
LINK_DIRECTORIES(../libschily)
ADD_LIBRARY (usal STATIC ${LIBSCG_SRCS})
TARGET_LINK_LIBRARIES(usal schily ${SCG_SELF_LIBS})
