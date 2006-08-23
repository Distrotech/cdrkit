PROJECT (READECD)
INCLUDE_DIRECTORIES(../include ../cdrecord . )

LINK_DIRECTORIES(../libschily ../libscg ../libdeflt ../libfile)
ADD_EXECUTABLE (readcd cd_misc.c defaults.c getnum.c io.c misc.c modes.c movesect.c readcd.c scsi_cdr.c scsi_mmc.c scsi_scan.c)
TARGET_LINK_LIBRARIES(readcd scg schily deflt file)
SET_TARGET_PROPERTIES(readcd PROPERTIES SKIP_BUILD_RPATH TRUE)

