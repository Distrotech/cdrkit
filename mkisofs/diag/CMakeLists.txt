PROJECT (MKISOFS)
INCLUDE_DIRECTORIES(../../include ../../libhfs_iso .. . ../../cdrecord)
ADD_DEFINITIONS(-DUSE_LARGEFILES -DABORT_DEEP_ISO_ONLY -DAPPLE_HYB -DUDF -DDVD_VIDEO -DSORTING -DHAVE_CONFIG_H -DUSE_LIBSCHILY -DUSE_SCG)

LINK_DIRECTORIES(../../libhfs_iso ../../libschily ../../libscg ../../libunls ../../libdeflt ../../libfile)
LINK_LIBRARIES(schily hfs_iso deflt file scg unls )

#ADD_EXECUTABLE (mkisofs mkisofs.c ${MKISOFS_MOST_SRCS})
#
#ADD_EXECUTABLE (isoinfo diag/isoinfo.c diag/isoinfo.c diag/scsi.c diag/scsi_cdr.c diag/cd_misc.c diag/modes.c  diag/defaults.c diag/getnum.c)
##TARGET_LINK_LIBRARIES(isoinfo schily hfs_iso deflt file scg unls )


ADD_EXECUTABLE(devdump dump.c scsi.c scsi_cdr.c cd_misc.c modes.c defaults.c getnum.c)
ADD_EXECUTABLE(isodebug	isodebug.c scsi.c scsi_cdr.c cd_misc.c modes.c defaults.c getnum.c)
ADD_EXECUTABLE(isodump	isodump.c scsi.c scsi_cdr.c cd_misc.c modes.c defaults.c getnum.c )
ADD_EXECUTABLE(isoinfo isoinfo.c scsi.c scsi_cdr.c cd_misc.c modes.c defaults.c getnum.c)
ADD_EXECUTABLE(isovfy	isovfy.c scsi.c scsi_cdr.c cd_misc.c modes.c defaults.c getnum.c )
