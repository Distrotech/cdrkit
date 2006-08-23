PROJECT (MKISOFS)
INCLUDE_DIRECTORIES(../include ../libhfs_iso . ../cdrecord)

FIND_FILE (USE_ICONV iconv.h)
IF(USE_ICONV)
	ADD_DEFINITIONS(-DUSE_ICONV)
ENDIF(USE_ICONV)
# iconv may be provided by the 3rd party library
FIND_LIBRARY (USE_LIBICONV iconv)
IF(USE_LIBICONV)
	SET(EXTRA_LIBICONV iconv)
endif(USE_LIBICONV)
	

ADD_DEFINITIONS(-DUSE_LARGEFILES -DABORT_DEEP_ISO_ONLY -DAPPLE_HYB -DUDF -DDVD_VIDEO -DSORTING -DHAVE_CONFIG_H -DUSE_LIBSCHILY -DUSE_SCG -DJIGDO_TEMPLATE)

SET(MKISOFS_MOST_SRCS apple.c boot.c cd_misc.c defaults.c desktop.c dvd_file.c dvd_reader.c eltorito.c exclude.c files.c fnmatch.c getnum.c getopt.c getopt1.c hash.c ifo_read.c joliet.c mac_label.c match.c modes.c multi.c name.c rock.c scsi.c scsi_cdr.c stream.c tree.c udf.c vms.c volume.c write.c boot-alpha.c boot-hppa.c boot-mips.c md5.c jte.c rsync.c
	boot-mipsel.c endian.c  )

LINK_DIRECTORIES(../libhfs_iso ../libschily ../libscg ../libunls ../libdeflt ../libfile)

LINK_LIBRARIES(schily hfs_iso deflt file scg unls z ${EXTRA_LIBICONV})
ADD_EXECUTABLE (mkisofs mkisofs.c ${MKISOFS_MOST_SRCS})

#ADD_EXECUTABLE (isoinfo diag/isoinfo.c diag/isoinfo.c diag/scsi.c diag/scsi_cdr.c diag/cd_misc.c diag/modes.c  diag/defaults.c diag/getnum.c)
#TARGET_LINK_LIBRARIES(isoinfo schily hfs_iso deflt file scg unls )

#ADD_EXECUTABLE (mkisofs mkisofs.c ${MKISOFS_MOST_SRCS})
#
#ADD_EXECUTABLE (isoinfo diag/isoinfo.c diag/isoinfo.c diag/scsi.c diag/scsi_cdr.c diag/cd_misc.c diag/modes.c  diag/defaults.c diag/getnum.c)
##TARGET_LINK_LIBRARIES(isoinfo schily hfs_iso deflt file scg unls )


ADD_EXECUTABLE(devdump diag/dump.c diag/scsi.c diag/scsi_cdr.c diag/cd_misc.c diag/modes.c diag/defaults.c diag/getnum.c)
ADD_EXECUTABLE(isodebug	diag/isodebug.c diag/scsi.c diag/scsi_cdr.c diag/cd_misc.c diag/modes.c diag/defaults.c diag/getnum.c)
ADD_EXECUTABLE(isodump	diag/isodump.c diag/scsi.c diag/scsi_cdr.c diag/cd_misc.c diag/modes.c diag/defaults.c diag/getnum.c )
ADD_EXECUTABLE(isoinfo diag/isoinfo.c diag/scsi.c diag/scsi_cdr.c diag/cd_misc.c diag/modes.c diag/defaults.c diag/getnum.c)
ADD_EXECUTABLE(isovfy	diag/isovfy.c diag/scsi.c diag/scsi_cdr.c diag/cd_misc.c diag/modes.c diag/defaults.c diag/getnum.c)


SET_TARGET_PROPERTIES(mkisofs devdump isodebug isodump isoinfo isovfy PROPERTIES COMPILE_FLAGS -O2 COMPILE_FLAGS -O2 SKIP_BUILD_RPATH TRUE)

