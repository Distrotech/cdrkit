PROJECT (TALIBUS)
SUBDIRS( cdda2wav cdrecord doc libdeflt libedc libfile libhfs_iso libparanoia librscg libscg libschily libunls mkisofs readcd rscsi scgcheck)

INSTALL(PROGRAMS mkisofs/mkisofs cdrecord/cdrecord rscsi/rscsi readcd/readcd mkisofs/isodebug mkisofs/isodump mkisofs/isoinfo mkisofs/isovfy DESTINATION bin)

