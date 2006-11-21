PROJECT (SCGCHECK)
INCLUDE_DIRECTORIES(../include ../wodim )
INCLUDE(../include/AddScgBits.cmake)


LINK_DIRECTORIES(../libschily ../libscg ../libdeflt)
ADD_EXECUTABLE (scgcheck cd_misc.c dmaresid.c modes.c scgcheck.c scsi_cdr.c scsi_scan.c sense.c)
TARGET_LINK_LIBRARIES(scgcheck ${EXTRA_LIBS} schily)
