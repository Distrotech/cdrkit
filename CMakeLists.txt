PROJECT (TALIBUS)
SUBDIRS( cdda2wav cdrecord doc libdeflt libedc libfile libhfs_iso libparanoia librscg libscg libschily libunls mkisofs readcd rscsi scgcheck)

INSTALL(PROGRAMS mkisofs/mkisofs cdrecord/wodim rscsi/rscsi readcd/readcd mkisofs/isodebug mkisofs/isodump mkisofs/isoinfo mkisofs/isovfy DESTINATION bin)
INSTALL(FILES ./cdda2wav/cdda2ogg.1 ./cdda2wav/cdda2ogg.mk1 ./cdda2wav/cdda2wav.1 ./cdrecord/wodim.1 ./readcd/readcd.1 DESTINATION share/man/man1)
INSTALL(FILES ./mkisofs/diag/devdump.8 ./mkisofs/diag/isodebug.8 ./mkisofs/diag/isodump.8 ./mkisofs/diag/isoinfo.8 ./mkisofs/diag/isovfy.8 ./mkisofs/mkhybrid.8 ./mkisofs/mkisofs.8 DESTINATION share/man/man8)


