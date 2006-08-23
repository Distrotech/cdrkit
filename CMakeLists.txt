PROJECT (TALIBUS)
SUBDIRS(cdda2wav cdrecord libdeflt libedc libfile libhfs_iso libparanoia librscg libscg libschily libunls mkisofs readcd rscsi 3rd-party/zisofs_tools)

INSTALL(PROGRAMS mkisofs/mkisofs cdrecord/wodim rscsi/rscsi readcd/readcd mkisofs/devdump mkisofs/isodebug mkisofs/isodump mkisofs/isoinfo mkisofs/isovfy 3rd-party/zisofs_tools/mkzftree cdda2wav/cdda2wav cdda2wav/pitchplay cdda2wav/readmult cdda2wav/cdda2mp3 cdda2wav/cdda2ogg  DESTINATION bin)
INSTALL(PROGRAMS rscsi/rscsi DESTINATION sbin)
INSTALL(FILES ./cdda2wav/cdda2ogg.1 ./cdda2wav/cdda2ogg.mk1 ./cdda2wav/cdda2wav.1 ./cdrecord/wodim.1 ./readcd/readcd.1 cdda2wav/pitchplay.1 cdda2wav/readmult.1 cdda2wav/list_audio_tracks.1 DESTINATION share/man/man1)
INSTALL(FILES 3rd-party/zisofs_tools/mkzftree.8 ./mkisofs/diag/devdump.8 ./mkisofs/diag/isodebug.8 ./mkisofs/diag/isodump.8 ./mkisofs/diag/isoinfo.8 ./mkisofs/diag/isovfy.8 ./mkisofs/mkhybrid.8 ./mkisofs/mkisofs.8 DESTINATION share/man/man8)


