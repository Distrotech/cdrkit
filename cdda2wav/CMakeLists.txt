PROJECT (CDDA2WAV)
INCLUDE_DIRECTORIES(../include ../cdrecord . ${CMAKE_BINARY_DIR})

FIND_FILE (HAVE_SYS_SOUNDCARD_H sys/soundcard.h)
IF(HAVE_SYS_SOUNDCARD_H)
   	ADD_DEFINITIONS(-DHAVE_SYS_SOUNDCARD_H)
ENDIF(HAVE_SYS_SOUNDCARD_H)
FIND_FILE (HAVE_LINUX_SOUNDCARD_H linux/soundcard.h)
IF(HAVE_LINUX_SOUNDCARD_H)
   	ADD_DEFINITIONS(-DHAVE_LINUX_SOUNDCARD_H)
ENDIF(HAVE_LINUX_SOUNDCARD_H)

LINK_DIRECTORIES(../libschily ../libscg ../libdeflt ../libparanoia)
ADD_EXECUTABLE (cdda2wav aifc.c aiff.c base64.c cd_misc.c cdda2wav.c defaults.c getnum.c interface.c ioctl.c md5c.c modes.c raw.c resample.c ringbuff.c scsi_cdr.c scsi_cmds.c scsi_scan.c semshm.c setuid.c sha_func.c sndconfig.c sun.c toc.c wav.c)
TARGET_LINK_LIBRARIES(cdda2wav scg schily deflt paranoia)
SET_TARGET_PROPERTIES(cdda2wav PROPERTIES  SKIP_BUILD_RPATH TRUE)


INSTALL(TARGETS cdda2wav DESTINATION bin)
INSTALL(PROGRAMS pitchplay readmult cdda2mp3 cdda2ogg DESTINATION bin)
INSTALL(FILES cdda2ogg.1 cdda2wav.1 pitchplay.1 readmult.1 list_audio_tracks.1 DESTINATION share/man/man1)

