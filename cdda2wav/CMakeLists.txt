PROJECT (CDDA2WAV)
INCLUDE_DIRECTORIES(../include ../cdrecord . )

LINK_DIRECTORIES(../libschily ../libscg ../libdeflt ../libfile ../libparanoia)
ADD_EXECUTABLE (cdda2wav aifc.c aiff.c base64.c cd_misc.c cdda2wav.c defaults.c getnum.c interface.c ioctl.c md5c.c modes.c raw.c resample.c ringbuff.c scsi_cdr.c scsi_cmds.c scsi_scan.c semshm.c setuid.c sha_func.c sndconfig.c sun.c toc.c wav.c)
TARGET_LINK_LIBRARIES(cdda2wav scg schily deflt paranoia)
