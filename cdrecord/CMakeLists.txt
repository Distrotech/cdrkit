PROJECT (CDRECORD)
INCLUDE_DIRECTORIES(../include . )
#ADD_DEFINITIONS(-DUSE_LARGEFILES -DABORT_DEEP_ISO_ONLY -DAPPLE_HYB -DUDF -DDVD_VIDEO -DSORTING -DHAVE_CONFIG_H -DUSE_LIBSCHILY -DUSE_SCG)

SET(CDRECORD_SRCS audiosize.c auinfo.c cd_misc.c cdr_drv.c cdrecord.c cdtext.c clone.c crc16.c cue.c defaults.c diskid.c drv_7501.c drv_jvc.c drv_mmc.c drv_philips.c drv_simul.c drv_sony.c fifo.c getnum.c isosize.c misc.c modes.c movesect.c scsi_cdr.c scsi_cdr_mmc4.c scsi_mmc.c scsi_mmc4.c scsi_scan.c sector.c subchan.c wm_packet.c wm_session.c wm_track.c xio.c )

LINK_DIRECTORIES(../libschily ../libscg ../libdeflt ../libfile)

ADD_EXECUTABLE (wodim ${CDRECORD_SRCS})
TARGET_LINK_LIBRARIES(wodim scg schily deflt file)
