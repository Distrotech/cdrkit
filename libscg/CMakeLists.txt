PROJECT (LIBSCG)
INCLUDE_DIRECTORIES(../include .  ${CMAKE_BINARY_DIR}  ../cdrecord)

#SET(LIBSCG_SRCS rdummy.c scgsettarget.c scgtimes.c scsi-linux-ata.c scsi-linux-pg.c scsi-linux-sg.c scsierrs.c scsihack.c scsihelp.c scsiopen.c scsitransp.c)
SET(LIBSCG_SRCS rdummy.c scgsettarget.c scgtimes.c scsierrs.c scsihack.c scsihelp.c scsiopen.c scsitransp.c)
SET_SOURCE_FILES_PROPERTIES(${LIBSCG_SRCS} PROPERTIES COMPILE_FLAGS -O2)
LINK_DIRECTORIES(../libschily)
ADD_LIBRARY (scg STATIC ${LIBSCG_SRCS})
TARGET_LINK_LIBRARIES(scg schily)
