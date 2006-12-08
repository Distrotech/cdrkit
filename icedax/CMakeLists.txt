PROJECT (icedax)
INCLUDE_DIRECTORIES(../include ../wodim ${CMAKE_BINARY_DIR})
include(../include/AddScgBits.cmake)
include(../include/AddSchilyBits.cmake)
include(../include/AddNetworkBits.cmake)

FIND_FILE (HAVE_SYS_SOUNDCARD_H sys/soundcard.h)
IF(HAVE_SYS_SOUNDCARD_H)
   	ADD_DEFINITIONS(-DHAVE_SYS_SOUNDCARD_H)
ENDIF(HAVE_SYS_SOUNDCARD_H)
FIND_FILE (HAVE_LINUX_SOUNDCARD_H linux/soundcard.h)
IF(HAVE_LINUX_SOUNDCARD_H)
   	ADD_DEFINITIONS(-DHAVE_LINUX_SOUNDCARD_H)
ENDIF(HAVE_LINUX_SOUNDCARD_H)

LIST(APPEND EXTRA_LIBS deflt paranoia)
IF (WIN32)
	LIST(APPEND EXTRA_LIBS winmm)
ENDIF (WIN32)

LINK_DIRECTORIES(../librols ../libusal ../libdeflt ../libparanoia)
ADD_EXECUTABLE (icedax aifc.c aiff.c base64.c icedax.c interface.c ioctl.c md5c.c raw.c resample.c ringbuff.c scsi_cmds.c semshm.c setuid.c sha_func.c sndconfig.c sun.c toc.c wav.c)
TARGET_LINK_LIBRARIES(icedax wodimstuff ${EXTRA_LIBS})
SET_TARGET_PROPERTIES(icedax PROPERTIES  SKIP_BUILD_RPATH TRUE)

INSTALL(TARGETS icedax DESTINATION bin)
INSTALL(PROGRAMS pitchplay readmult cdda2mp3 cdda2ogg DESTINATION bin)
INSTALL(FILES cdda2ogg.1 icedax.1 pitchplay.1 readmult.1 list_audio_tracks.1 DESTINATION share/man/man1)

