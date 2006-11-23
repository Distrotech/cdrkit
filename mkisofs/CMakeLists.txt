PROJECT (MKISOFS)

INCLUDE(../include/AddScgBits.cmake)
INCLUDE(../include/AddSchilyBits.cmake)

INCLUDE_DIRECTORIES(../include ../libhfs_iso ../wodim ${CMAKE_BINARY_DIR})

FIND_FILE (USE_MAGIC magic.h)
IF(USE_MAGIC)
	ADD_DEFINITIONS(-DUSE_MAGIC)
	SET(MAGICLIBS magic)
ENDIF(USE_MAGIC)

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

#SET_SOURCE_FILES_PROPERTIES(${MKISOFS_MOST_SRCS} PROPERTIES )
LINK_DIRECTORIES(../libhfs_iso ../libschily ../libusal ../libunls ../libdeflt)

ADD_EXECUTABLE (mkisofs mkisofs.c ${MKISOFS_MOST_SRCS})
TARGET_LINK_LIBRARIES(mkisofs hfs_iso deflt ${MAGICLIBS} c unls z ${EXTRA_LIBICONV} ${EXTRA_LIBS})

# common lib set and mkisofs source parts for the rest
LINK_LIBRARIES(${EXTRA_LIBS} deflt unls ${EXTRA_LIBICONV} )
SET(MI_COMMON scsi.c scsi_cdr.c cd_misc.c modes.c defaults.c getnum.c)
ADD_EXECUTABLE(devdump diag/dump.c  ${MI_COMMON})
ADD_EXECUTABLE(isodebug	diag/isodebug.c   ${MI_COMMON})
ADD_EXECUTABLE(isodump	diag/isodump.c    ${MI_COMMON})
ADD_EXECUTABLE(isoinfo diag/isoinfo.c	 ${MI_COMMON})
ADD_EXECUTABLE(isovfy	diag/isovfy.c	 ${MI_COMMON})


SET_TARGET_PROPERTIES(mkisofs devdump isodebug isodump isoinfo isovfy PROPERTIES  SKIP_BUILD_RPATH TRUE)

INSTALL(TARGETS mkisofs devdump isodebug isodump isoinfo isovfy DESTINATION bin)
INSTALL(FILES 
mkisofs.8
diag/devdump.8
diag/isodebug.8
diag/isodump.8
diag/isoinfo.8
diag/isovfy.8
DESTINATION share/man/man8)
