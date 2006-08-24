/* lconfig.h.  Generated automatically by configure.  */
#if 0
/* @(#)lconfig.h.in	1.5 03/09/04 Copyright 1998-2003 Heiko Eissfeldt */
#endif
/* #undef HAVE_SYS_CDIO_H */		/* if we should use sys/cdio.h */

/* #undef HAVE_SUNDEV_SRREG_H */	/* if we should use sundev/srreg.h */

/* #undef HAVE_SYS_AUDIOIO_H */	/* if we should use sys/audioio.h */

/* #undef HAVE_SUN_AUDIOIO_H */	/* if we should use sun/audioio.h */

/* #undef HAVE_SOUNDCARD_H */		/* if we should use soundcard.h */

/* TESTED BY CMAKE, (eduard) */
//#define HAVE_SYS_SOUNDCARD_H 1	/* if we should use sys/soundcard.h */
//#define HAVE_LINUX_SOUNDCARD_H 1	/* if we should use linux/soundcard.h */

/* #undef HAVE_MACHINE_SOUNDCARD_H */	/* if we should use machine/soundcard.h */

/* #undef HAVE_SYS_ASOUNDLIB_H */	/* if we should use sys/asoundlib.h */

/* #undef HAVE_WINDOWS_H */		/* if we should use windows.h */

/* #undef HAVE_MMSYSTEM_H */		/* if we should use mmsystem.h */

/* #undef HAVE_OS2_H */		/* if we should use os2.h */

/* #undef HAVE_OS2ME_H */		/* if we should use os2me.h */

#if	defined HAVE_SOUNDCARD_H || defined HAVE_SYS_SOUNDCARD_H  || defined HAVE_LINUX_SOUNDCARD_H || defined HAVE_MACHINE_SOUNDCARD_H
#define HAVE_OSS	1
#endif

/*
#if	defined HAVE_WINDOWS_H && defined HAVE_MMSYSTEM_H
#define HAVE_WINSOUND	1
#endif

#if	defined HAVE_OS2_H && defined HAVE_OS2ME_H
#define HAVE_OS2SOUND	1
#endif
*/

#define HAVE_STRTOUL 1


/* EB, some defaults, fixme
 */

#define CD_DEVICE "/dev/cdrom"
#define FILENAME "audio"
#define UNDERSAMPLING 1
#define VERSION "2.01.01a08+debburn"
#define BITS_P_S 16
#define CHANNELS 2
#define AUDIOTYPE "wav"
#define DURATION 0
#define DEF_INTERFACE "generic_scsi"
#define USE_PARANOIA 1
#define DEFAULT_SPEED 0
#define CDINDEX_SUPPORT
#define CDDB_SUPPORT
#define CDDBHOST "freedb.freedb.org"
#define CDDBPORT 8880
#define HAVE_IOCTL_INTERFACE
#define ECHO_TO_SOUNDCARD
#define SOUND_DEV "/dev/dsp"
#define NSECTORS 75
#define INFOFILES
#define MD5_SIGNATURES
#define AUX_DEVICE "/dev/cdrom"

