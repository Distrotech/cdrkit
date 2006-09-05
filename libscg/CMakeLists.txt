PROJECT (LIBSCG)
INCLUDE_DIRECTORIES(../include .  ${CMAKE_BINARY_DIR}  ../cdrecord)
include(../include/AddScgBits.cmake)

#SET(LIBSCG_SRCS rdummy.c scgsettarget.c scgtimes.c scsi-linux-ata.c scsi-linux-pg.c scsi-linux-sg.c scsierrs.c scsihack.c scsihelp.c scsiopen.c scsitransp.c)
SET(LIBSCG_SRCS rdummy.c scgsettarget.c scgtimes.c scsierrs.c scsihack.c scsihelp.c scsiopen.c scsitransp.c)
LINK_DIRECTORIES(../libschily)
ADD_LIBRARY (scg STATIC ${LIBSCG_SRCS})
TARGET_LINK_LIBRARIES(scg schily ${SCG_SELF_LIBS})
