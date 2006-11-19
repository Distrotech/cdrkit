PROJECT (cdrkit)
SUBDIRS(libdeflt libedc libhfs_iso libschily libunls mkisofs 3rd-party/dirsplit include)
#SUBDIRS(cdda2wav cdrecord libdeflt libedc libhfs_iso libparanoia libscg libschily libunls mkisofs readcd rscsi 3rd-party/dirsplit include)

#ADD_DEFINITIONS(-DSHUT_UP)

SET(BINS_SET cdrecord/wodim readcd/readcd)
FOREACH(app ${BINS_SET})
   install(PROGRAMS ${CMAKE_BINARY_DIR}/${app} DESTINATION bin)
ENDFOREACH(app ${BINS_SET})
FOREACH(app ${SCRIPTS_SET})
   install(PROGRAMS ${CMAKE_SOURCE_DIR}/${app} DESTINATION bin)
ENDFOREACH(app ${BINS_SET})
