/*
 * This file has been modified for the cdrkit suite.
 *
 * The behaviour and appearence of the program code below can differ to a major
 * extent from the version distributed by the original author(s).
 *
 * For details, see Changelog file distributed with the cdrkit package. If you
 * received this file from another source then ask the distributing person for
 * a log of modifications.
 *
 */

/* @(#)toc.c	1.57 06/02/19 Copyright 1998-2003 Heiko Eissfeldt */
/*
 * Copyright: GNU Public License 2 applies
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * CDDA2WAV (C) Heiko Eissfeldt heiko@hexco.de
 * CDDB routines (C) Ti Kan and Steve Scherf
 */
#include "config.h"
#include <stdio.h>
#include <standard.h>
#include <stdxlib.h>
#include <strdefs.h>
#include <utypes.h>
#include <intcvt.h>
#include <unixstd.h>		/* sleep */
#include <ctype.h>
#include <errno.h>
#include <fctldefs.h>
#include <vadefs.h>
#include <schily.h>
#include <libport.h>
#include <sys/ioctl.h>

#define CD_TEXT
#define CD_EXTRA
#undef DEBUG_XTRA
#undef DEBUG_CDTEXT
#undef DEBUG_CDDBP


#include <usal/scsitransp.h>

#include "mytype.h"
#include "byteorder.h"
#include "interface.h"
#include "icedax.h"
#include "global.h"
#include "sha.h"
#include "base64.h"
#include "toc.h"
#include "exitcodes.h"
#include "ringbuff.h"

int Get_Mins(unsigned long p_track);
int Get_Secs(unsigned long p_track);
int Get_Frames(unsigned long p_track);
int Get_Flags(unsigned long p_track);
int Get_SCMS(unsigned long p_track);


#if	defined	USE_REMOTE
/* tcp stuff */
/* fix OS/2 compilation */
#ifdef	__EMX__
#define	gethostid	nogethostid
#endif
#include <sys/socket.h>
#undef gethostid
#include <netinet/in.h>
#if	defined(HAVE_NETDB_H) && !defined(HOST_NOT_FOUND) && \
				!defined(_INCL_NETDB_H)
#include <netdb.h>
#define	_INCL_NETDB_H
#endif
#endif

int have_CD_text;
int have_multisession;
int have_CD_extra;
int have_CDDB;

struct iterator;

static void 			UpdateTrackData(int p_num);
static void 			UpdateIndexData(int p_num);
static void 			UpdateTimeData(int p_min, int p_sec, int p_frm);
static unsigned int 	is_multisession(void);
static unsigned int 	get_end_of_last_audio_track(unsigned mult_off);
static int 				cddb_sum(int n);
static void 			dump_extra_info(unsigned from);
static int 				GetIndexOfSector(unsigned sec, unsigned track);
static int 				patch_cd_extra(unsigned track, unsigned long sector);
static void 			patch_to_audio(unsigned long p_track);
static int 				restrict_tracks_illleadout(void);
static void 			Set_MCN(unsigned char *MCN_arg);
static void 			Set_ISRC(int track, const unsigned char *ISRC_arg);
static void 			InitIterator(struct iterator *iter, unsigned long p_track);

static unsigned char g_track=0xff, g_index=0xff;	/* current track, index */

/* Conversion function: from logical block adresses  to minute,second,frame
 */
int lba_2_msf(long lba, int *m, int *s, int *f)
{
#ifdef  __follow_redbook__
	if (lba >= -150 && lba < 405000) {      /* lba <= 404849 */
#else
	if (lba >= -150) {
#endif
		lba += 150;
	} else if (lba >= -45150 && lba <= -151) {
		lba += 450150;
	} else
		return 1;

	*m = lba / 60 / 75;
	lba -= (*m)*60*75;
	*s = lba / 75;
	lba -= (*s)*75;
	*f = lba;

	return 0;
}

/* print the track currently read */
static void UpdateTrackData(int p_num)
{
  if (global.quiet == 0) { 
    fprintf (stderr, "\ntrack: %.2d, ", p_num); fflush(stderr);
  }
  g_track = (unsigned char) p_num;
}


/* print the index currently read */
static void UpdateIndexData(int p_num)
{
  if (global.quiet == 0) { 
    fprintf (stderr, "index: %.2d\n", p_num); fflush(stderr);
  }
  g_index = (unsigned char) p_num;
}


/* print the time of track currently read */
static void UpdateTimeData(int p_min, int p_sec, int p_frm)
{
  if (global.quiet == 0) {
    fprintf (stderr, "time: %.2d:%.2d.%.2d\r", p_min, p_sec, p_frm); 
    fflush(stderr);
  }
}

void AnalyzeQchannel(unsigned frame)
{
    subq_chnl *sub_ch;

    if (trackindex_disp != 0) {
	sub_ch = ReadSubQ(get_scsi_p(), GET_POSITIONDATA,0);
	/* analyze sub Q-channel data */
	if (sub_ch->track != g_track ||
	    sub_ch->index != g_index) {
	    UpdateTrackData (sub_ch->track);
	    UpdateIndexData (sub_ch->index);
	}
    }
    frame += 150;
    UpdateTimeData ((unsigned char) (frame / (60*75)), 
		    (unsigned char) ((frame % (60*75)) / 75), 
		    (unsigned char) (frame % 75));
}

unsigned cdtracks = 0;

int no_disguised_audiotracks(void)
{
	/* we can assume no audio tracks according to toc here. */
	/* read a data sector from the first data track */
	unsigned char p[3000];
	int retval;
	get_scsi_p()->silent++;
	retval = 1 == ReadCdRomData(get_scsi_p(), p, Get_StartSector(1), 1);
	get_scsi_p()->silent--;
	if (retval == 0) {
		int i;
fprintf(stderr, "Warning: wrong track types found: patching to audio...\n");
		for (i = 0; i < cdtracks; i++)
			patch_to_audio(i);
	}
	return retval;
}


#undef SIM_ILLLEADOUT
int ReadToc(void)
{
    int retval = (*doReadToc)( get_scsi_p() );
#if	defined SIM_ILLLEADOUT
    g_toc[cdtracks+1] = 20*75;
#endif
    return retval;
}

static int can_read_illleadout(void);

static int can_read_illleadout(void)
{
	SCSI *usalp = get_scsi_p();

	UINT4 buffer [CD_FRAMESIZE_RAW/4];
	if (global.illleadout_cd == 0) return 0;

	usalp->silent++;
	global.reads_illleadout = 
	    ReadCdRom(usalp, buffer, Get_AudioStartSector(CDROM_LEADOUT), 1);
	usalp->silent--;
	return global.reads_illleadout;
}


unsigned find_an_off_sector(unsigned lSector, unsigned SectorBurstVal);

unsigned find_an_off_sector(unsigned lSector, unsigned SectorBurstVal)
{
	long track_of_start = Get_Track(lSector);
	long track_of_end = Get_Track(lSector + SectorBurstVal -1);
	long start = Get_AudioStartSector(track_of_start);
	long end = Get_EndSector(track_of_end);

	if (lSector - start > end - lSector + SectorBurstVal -1)
		return start;
	else
		return end;
}

#ifdef CD_TEXT
#include "scsi_cmds.h"
#endif


int handle_cdtext(void)
{
#ifdef CD_TEXT
	if (bufferTOC[0] == 0 && bufferTOC[1] == 0) {
		have_CD_text = 0;
		return have_CD_text;
	}

	/* do a quick scan over all pack type indicators */
	{
		int i;
		int count_fails = 0;
		int len = (bufferTOC[0] << 8) | bufferTOC[1];

		len = min(len, 2048);
		for (i = 0; i < len-4; i += 18) {
			if (bufferTOC[4+i] < 0x80 || bufferTOC[4+i] > 0x8f) {
				count_fails++;
			}
		}
		have_CD_text = len > 4 && count_fails < 3;
	}

#else
	have_CD_text = 0;
#endif
	return have_CD_text;
}


#ifdef CD_TEXT
#include "cd_text.c"
#endif


#if defined CDROMMULTISESSION
static int tmp_fd;
#endif

#ifdef CD_EXTRA
#include "cd_extra.c"
#endif

static unsigned session_start;
/*
   A Cd-Extra is detected, if it is a multisession CD with
   only audio tracks in the first session and a data track
   in the last session.
 */
static unsigned is_multisession(void)
{
  unsigned mult_off;
#if defined CDROMMULTISESSION
  /*
   * FIXME: we would have to do a ioctl (CDROMMULTISESSION)
   *        for the cdrom device associated with the generic device
   *	    not just AUX_DEVICE
   */
  struct cdrom_multisession ms_str;

  if (interface == GENERIC_SCSI)
    tmp_fd = open (global.aux_name, O_RDONLY);
  else
    tmp_fd = global.cooked_fd;

  if (tmp_fd != -1) {
    int result;

    ms_str.addr_format = CDROM_LBA;
    result = ioctl(tmp_fd, CDROMMULTISESSION, &ms_str);
    if (result == -1) {
      if (global.verbose != 0)
        perror("multi session ioctl not supported: ");
    } else {
#ifdef DEBUG_XTRA
  fprintf(stderr, "current ioctl multisession_offset = %u\n", ms_str.addr.lba);
#endif
	if (interface == GENERIC_SCSI)
		close (tmp_fd);
	if (ms_str.addr.lba > 0)
	  return ms_str.addr.lba;
    }
  }
#endif
  mult_off = 0;
  if (LastAudioTrack() + 1 == FirstDataTrack()) {
	  mult_off = Get_StartSector(FirstDataTrack());
  }

#ifdef DEBUG_XTRA
  fprintf(stderr, "current guessed multisession_offset = %u\n", mult_off);
#endif
  return mult_off;
}

#define SESSIONSECTORS (152*75)
/*
   The solution is to read the Table of Contents of the first
   session only (if the drive permits that) and directly use
   the start of the leadout. If this is not supported, we subtract
   a constant of SESSIONSECTORS sectors (found heuristically).
 */
static unsigned get_end_of_last_audio_track(unsigned mult_off)
{
   unsigned retval;

   /* Try to read the first session table of contents.
      This works for Sony and mmc type drives. */
   if (ReadLastAudio && (retval = ReadLastAudio(get_scsi_p())) != 0) {
     return retval;
   } else {
     return mult_off - SESSIONSECTORS;
   }
}

static void dump_cdtext_info(void);

#if defined CDDB_SUPPORT
static void emit_cddb_form(char *fname_baseval);
#endif

#if defined CDINDEX_SUPPORT
static void emit_cdindex_form(char *fname_baseval);
#endif


typedef struct TOC {	/* structure of table of contents (cdrom) */
	unsigned char reserved1;
	unsigned char bFlags;
	unsigned char bTrack;
	unsigned char reserved2;
	unsigned int dwStartSector;
	int mins;
	int secs;
	int frms;
	unsigned char ISRC[16];
	int	SCMS;
} TOC;


/* Flags contains two fields:
    bits 7-4 (ADR)
 	: 0 no sub-q-channel information
	: 1 sub-q-channel contains current position
	: 2 sub-q-channel contains media catalog number
	: 3 sub-q-channel contains International Standard
				   Recording Code ISRC
	: other values reserved
    bits 3-0 (Control) :
    bit 3 : when set indicates there are 4 audio channels else 2 channels
    bit 2 : when set indicates this is a data track else an audio track
    bit 1 : when set indicates digital copy is permitted else prohibited
    bit 0 : when set indicates pre-emphasis is present else not present
 */

#define GETFLAGS(x) ((x)->bFlags)
#define GETTRACK(x) ((x)->bTrack)
#define GETSTART(x) ((x)->dwStartSector)
#define GETMINS(x)  ((x)->mins)
#define GETSECS(x)  ((x)->secs)
#define GETFRAMES(x) ((x)->frms)
#define GETISRC(x)  ((x)->ISRC)

#define IS__PREEMPHASIZED(p) ( (GETFLAGS(p) & 0x10) != 0)
#define IS__INCREMENTAL(p) ( (GETFLAGS(p) & 0x10) != 0)
#define IS__COPYRESTRICTED(p) (!(GETFLAGS(p) & 0x20) != 0)
#define IS__COPYRIGHTED(p) (!(GETFLAGS(p) & 0x20) != 0)
#define IS__DATA(p)        ( (GETFLAGS(p) & 0x40) != 0)
#define IS__AUDIO(p)       (!(GETFLAGS(p) & 0x40) != 0)
#define IS__QUADRO(p)      ( (GETFLAGS(p) & 0x80) != 0)

/*
 * Iterator interface inspired from Java
 */
struct iterator {
	int index;
	int startindex;
	void        (*reset)(struct iterator *this);
	struct TOC *(*getNextTrack)(struct iterator *this);
	int         (*hasNextTrack)(struct iterator *this);
};




/* The Table of Contents needs to be corrected if we
   have a CD-Extra. In this case all audio tracks are
   followed by a data track (in the second session).
   Unlike for single session CDs the end of the last audio
   track cannot be set to the start of the following
   track, since the lead-out and lead-in would then
   errenously be part of the audio track. This would
   lead to read errors when trying to read into the
   lead-out area.
   So the length of the last track in case of Cd-Extra
   has to be fixed.
 */
unsigned FixupTOC(unsigned no_tracks)
{
    unsigned mult_off;
    unsigned offset = 0;
    int j = -1;
    unsigned real_end = 2000000;

    /* get the multisession offset in sectors */
    mult_off = is_multisession();

    /* if the first track address had been the victim of an underflow,
     * set it to zero.
     */
    if (Get_StartSector(1) > Get_StartSector(LastTrack())) {
	fprintf(stderr, "Warning: first track has negative start sector! Setting to zero.\n");
	toc_entry( 1, Get_Flags(1), Get_Tracknumber(1), Get_ISRC(1), 0, 0, 2, 0 );
    }

#ifdef DEBUG_XTRA
    fprintf(stderr, "current multisession_offset = %u\n", mult_off);
#endif
    dump_cdtext_info();

    if (mult_off > 100) { /* the offset has to have a minimum size */

      /* believe the multisession offset :-) */
      /* adjust end of last audio track to be in the first session */
      real_end = get_end_of_last_audio_track(mult_off);
#ifdef DEBUG_XTRA
      fprintf(stderr, "current end = %u\n", real_end);
#endif

      j = FirstDataTrack();
      if (LastAudioTrack() + 1 == j) {
	  long sj = Get_StartSector(j);
	  if (sj > (long)real_end) {
	    session_start = mult_off;
		have_multisession = sj;

#ifdef CD_EXTRA
	    offset = Read_CD_Extra_Info(sj);

	    if (offset != 0) {
		have_CD_extra = sj;
		dump_extra_info(offset);
	    }
#endif
	  }
      }
    }
    if (global.cddbp) {
#if	defined USE_REMOTE
        if (global.disctitle == NULL) {
	    have_CDDB = !request_titles();
        }
#else
        fprintf(stderr, "Cannot lookup titles: no cddbp support included!\n");
#endif
    }
#if defined CDINDEX_SUPPORT || defined CDDB_SUPPORT
    if (have_CD_text || have_CD_extra || have_CDDB) {
	    unsigned long	count_audio_tracks = 0;
	    static struct iterator i;
	    if (i.reset == NULL)
		    InitIterator(&i, 1);

	    while (i.hasNextTrack(&i)) {
		    struct TOC *p = i.getNextTrack(&i);
		    if (IS__AUDIO(p)) count_audio_tracks++;
	    }

	    if (count_audio_tracks > 0 && global.no_cddbfile == 0) {
#if defined CDINDEX_SUPPORT
		    emit_cdindex_form(global.fname_base);
#endif
#if defined CDDB_SUPPORT
		    emit_cddb_form(global.fname_base);
#endif
	    }
    }
#endif
    if (have_multisession) {
	/* set start of track to beginning of lead-out */
	patch_cd_extra(j, real_end);
#if	defined CD_EXTRA && defined DEBUG_XTRA
	fprintf(stderr, "setting end of session (track %d) to %u\n", j, real_end);
#endif
    }
    return offset;
}

static int cddb_sum(int n)
{
  int ret;

  for (ret = 0; n > 0; n /= 10) {
    ret += (n % 10);
  }

  return ret;
}

void calc_cddb_id(void)
{
  UINT4 i;
  UINT4 t = 0;
  UINT4 n = 0;

  for (i = 1; i <= cdtracks; i++) {
    n += cddb_sum(Get_StartSector(i)/75 + 2);
  }

  t = Get_StartSector(i)/75 - Get_StartSector(1)/75;

  global.cddb_id = (n % 0xff) << 24 | (t << 8) | cdtracks;
}


#undef TESTCDINDEX
#ifdef	TESTCDINDEX
void TestGenerateId(void)
{
   SHA_INFO       sha;
   unsigned char  digest[20], *base64;
   unsigned long  size;

   sha_init(&sha);
   sha_update(&sha, (unsigned char *)"0123456789", 10);
   sha_final(digest, &sha);

   base64 = rfc822_binary((char *)digest, 20, &size);
   if (strncmp((char*) base64, "h6zsF82dzSCnFsws9nQXtxyKcBY-", size))
   {
       free(base64);

       fprintf(stderr, "The SHA-1 hash function failed to properly generate the\n");
       fprintf(stderr, "test key.\n");
       exit(INTERNAL_ERROR);
   }
   free(base64);
}
#endif

void calc_cdindex_id()
{
	SHA_INFO 	sha;
	unsigned char	digest[20], *base64;
	unsigned long	size;
	unsigned	i;
	char		temp[9];

#ifdef	TESTCDINDEX
	TestGenerateId();
	g_toc[1].bTrack = 1;
	cdtracks = 15;
	g_toc[cdtracks].bTrack = 15;
	i = 1;
	g_toc[i++].dwStartSector = 0U;
	g_toc[i++].dwStartSector = 18641U;
	g_toc[i++].dwStartSector = 34667U;
	g_toc[i++].dwStartSector = 56350U;
	g_toc[i++].dwStartSector = 77006U;
	g_toc[i++].dwStartSector = 106094U;
	g_toc[i++].dwStartSector = 125729U;
	g_toc[i++].dwStartSector = 149785U;
	g_toc[i++].dwStartSector = 168885U;
	g_toc[i++].dwStartSector = 185910U;
	g_toc[i++].dwStartSector = 205829U;
	g_toc[i++].dwStartSector = 230142U;
	g_toc[i++].dwStartSector = 246659U;
	g_toc[i++].dwStartSector = 265614U;
	g_toc[i++].dwStartSector = 289479U;
	g_toc[i++].dwStartSector = 325732U;
#endif
	sha_init(&sha);
	sprintf(temp, "%02X", Get_Tracknumber(1));
	sha_update(&sha, (unsigned char *)temp, 2);
	sprintf(temp, "%02X", Get_Tracknumber(cdtracks));
	sha_update(&sha, (unsigned char *)temp, 2);

	/* the position of the leadout comes first. */
	sprintf(temp, "%08lX", 150 + Get_StartSector(CDROM_LEADOUT));
	sha_update(&sha, (unsigned char *)temp, 8);

	/* now 99 tracks follow with their positions. */
	for (i = 1; i <= cdtracks; i++) {
		sprintf(temp, "%08lX", 150+Get_StartSector(i));
		sha_update(&sha, (unsigned char *)temp, 8);
	}
	for (i++  ; i <= 100; i++) {
		sha_update(&sha, (unsigned char *)"00000000", 8);
	}
	sha_final(digest, &sha);

	base64 = rfc822_binary((char *)digest, 20, &size);
	global.cdindex_id = base64;
}


#if defined CDDB_SUPPORT

#ifdef	PROTOTYPES
static void escape_and_split(FILE *channel, const char *args, ...)
#else
/*VARARGS3*/
static void escape_and_split(FILE *channel, const char *args, va_dcl va_alist)
#endif
{
	va_list	marker;

	int prefixlen;
	int len;
	char	*q;

#ifdef	PROTOTYPES
	va_start(marker, args);
#else
	va_start(marker);
#endif

	prefixlen = strlen(args);
	len = prefixlen;
	fputs(args, channel);

	q = va_arg(marker, char *);
	while (*q != '\0') {
		while (*q != '\0') {
			len += 2;
			if (*q == '\\')
				fputs("\\\\", channel);
			else if (*q == '\t')
				fputs("\\t", channel);
			else if (*q == '\n')
				fputs("\\n", channel);
			else {
				fputc(*q, channel);
				len--;
			}
			if (len > 78) {
				fputc('\n', channel);
				fputs(args, channel);
				len = prefixlen;
			}
			q++;
		}
		q = va_arg(marker, char *);
	}
	fputc('\n', channel);

	va_end(marker);
}

static void emit_cddb_form(char *fname_baseval)
{
  static struct iterator i;
  unsigned first_audio;
  FILE *cddb_form;
  char fname[200];
  char *pp;

  if (fname_baseval == NULL || fname_baseval[0] == 0)
	return;

  if (!strcmp(fname_baseval,"standard_output")) return;
  InitIterator(&i, 1);

  strncpy(fname, fname_baseval, sizeof(fname) -1);
  fname[sizeof(fname) -1] = 0;
  pp = strrchr(fname, '.');
  if (pp == NULL) {
    pp = fname + strlen(fname);
  }
  strncpy(pp, ".cddb", sizeof(fname) - 1 - (pp - fname));

  cddb_form = fopen(fname, "w");
  if (cddb_form == NULL) return;

  first_audio = FirstAudioTrack();
  fprintf( cddb_form, "# xmcd\n#\n");
  fprintf( cddb_form, "# Track frame offsets:\n#\n");

  while (i.hasNextTrack(&i)) {
	  struct TOC *p = i.getNextTrack(&i);
	  if (GETTRACK(p) == CDROM_LEADOUT) break;
	  fprintf( cddb_form,
		   "# %lu\n", 150 + Get_AudioStartSector(GETTRACK(p)));
  }

  fprintf( cddb_form, "#\n# Disc length: %lu seconds\n#\n", 
           (150 + Get_StartSector(CDROM_LEADOUT)) / 75);
  fprintf( cddb_form, "# Revision: %u\n", global.cddb_revision );
  fprintf( cddb_form, "# Submitted via: icedax " VERSION "\n" );

  fprintf( cddb_form, "DISCID=%08lx\n", (unsigned long)global.cddb_id);

  if (global.disctitle == NULL && global.creator == NULL) {
	fprintf( cddb_form, "DTITLE=\n");
  } else {
	if (global.creator == NULL) {
		escape_and_split( cddb_form, "DTITLE=", global.disctitle, "");
	} else if (global.disctitle == NULL) {
		escape_and_split( cddb_form, "DTITLE=", global.creator, "");
	} else {
		escape_and_split( cddb_form, "DTITLE=", global.creator, " / ", global.disctitle, "");
	}
  }
  if (global.cddb_year != 0)
	fprintf( cddb_form, "DYEAR=%4u\n", global.cddb_year);
  else
	fprintf( cddb_form, "DYEAR=\n");
  fprintf( cddb_form, "DGENRE=%s\n", global.cddb_genre);

  i.reset(&i);
  while (i.hasNextTrack(&i)) {
	  struct TOC *p = i.getNextTrack(&i);
	  int ii;

	  ii = GETTRACK(p);
	  if (ii == CDROM_LEADOUT) break;

	  if (global.tracktitle[ii] != NULL) {
		char prefix[10];
		sprintf(prefix, "TTITLE%d=", ii-1);
		  escape_and_split( cddb_form, prefix, global.tracktitle[ii], "");
	  } else {
		  fprintf( cddb_form, "TTITLE%d=\n", ii-1);
	  }
  }

  if (global.copyright_message == NULL) {
	fprintf( cddb_form, "EXTD=\n");
  } else {
	escape_and_split( cddb_form, "EXTD=", "Copyright ", global.copyright_message, "");
  }

  i.reset(&i);
  while (i.hasNextTrack(&i)) {
	  struct TOC *p = i.getNextTrack(&i);
	  int ii;

	  ii = GETTRACK(p);

	  if (ii == CDROM_LEADOUT) break;

	  fprintf( cddb_form, "EXTT%d=\n", ii-1);
  }
  fprintf( cddb_form, "PLAYORDER=\n");
  fclose( cddb_form );
}

#if	defined	USE_REMOTE
#include <pwd.h>

static int readn(register int fd, register char *ptr, register int nbytes)
{
	int	nread;

	nread = read(fd, ptr, nbytes);
#ifdef	DEBUG_CDDBP
	if (nread > 0) {
		fprintf(stderr, "READ :(%d)", nread);
		write(2, ptr, nread);
	}
#endif
	if (nread < 0) {
	   perror("socket read error: ");
	   fprintf(stderr, "fd=%d, ptr=%p, nbytes=%d\n", fd, ptr, nbytes);
	}

	return nread;
}

static ssize_t writez(int fd, const char *ptr)
{
	size_t nleft, nbytes;

	nleft = nbytes = strlen(ptr);

	while (nleft > 0) {
		ssize_t nwritten = write(fd, ptr, nleft);
		if (nwritten <= 0) {
			return nwritten;	/* return error */
		}
#ifdef	DEBUG_CDDBP
		fprintf(stderr, "WRITE:%s\n", ptr);
#endif

		nleft -= nwritten;
		ptr   += nwritten;
	}

	return nbytes - nleft;
}

#define	SOCKBUFF	2048
  
static void filter_nonprintable(char *c, size_t l)
{
	size_t i;
	for(i = 0; i < l; ++i) {
		if(!isprint(c[i]) && !isspace(c[i])) {
			c[i] = '_';
		}
	}
}


int process_cddb_titles(int sock_fd, char *inbuff, int readbytes);
int process_cddb_titles(int sock_fd, char *inbuff, int readbytes)
{
	int	finished = 0;
	char	*p = inbuff;
	int	ind = 0;
	unsigned char **	target = &global.creator;

	do {
		while (readbytes > 0) {
			/* do we have a complete line in the buffer? */
			p = (char *)memchr(inbuff+ind, '\n', readbytes);
			if (p == NULL) break;

			/* look for the terminator first */
			if (!strncmp(".\r\n", inbuff+ind, 3)) {
				finished = 1;
				break;
			}
			/* kill carriage return */
			if (p > inbuff+ind && *(p-1) == '\r') {
				*(p-1) = '\0';
			}
			/* kill line feed */
			*p = '\0';

			/* handle escaped characters */

			{
				char *q = inbuff+ind;
				while (*q) {
					if (*q++ == '\\' && *q != '\0') {
						if (*q == '\\') {
							readbytes--;
							p--;
							memmove(q, q+1, readbytes - (q-inbuff-ind));
						} else if (*q == 'n') {
							*(q-1) = '\n';
							readbytes--;
							p--;
							memmove(q, q+1, readbytes - (q-inbuff-ind));
						} else if (*q == 't') {
							*(q-1) = '\t';
							readbytes--;
							p--;
							memmove(q, q+1, readbytes - (q-inbuff-ind));
						}
					}
				}
						
			}

			/* handle multi line entries concatenate fields */

/* TODO if the delimiter is split into two lines, it is not recognized. */
			if (!strncmp(inbuff+ind, "DTITLE=", 7)) {
				char *res = strstr(inbuff+ind+7, " / ");
				int clen;
				char *q;

				if (res == NULL) {
					/* no limiter found yet */
					/* copy until the end */
					q = p;
				} else {
					/* limiter found */
					/* copy until the limiter */
					q = res;
					*q = '\0';
				}

				clen = q - (inbuff+ind+7);
				if (*target == NULL) {
					*target = malloc(clen+1);
					if (*target != NULL)
						**target = '\0';
				} else {
				        *target = realloc(*target, strlen((char *)*target) + clen - 1);
				}
				if (*target != NULL) {
					strcat((char *)*target, inbuff+ind+7);
				}

				/* handle part after the delimiter, if present */
				if (res != NULL) {
					target = (unsigned char **)&global.disctitle;
					/* skip the delimiter */
					q += 3;
					clen = p - q;
					if (*target == NULL) {
						*target = malloc(clen+1);
						if (*target != NULL)
							**target = '\0';
					}
					if (*target != NULL) {
						strcat((char *)*target, q);
					}
				}
			} else if (!strncmp(inbuff+ind, "TTITLE", 6)) {
				char	*q = (char *)memchr(inbuff+ind, '=', readbytes);
				unsigned tno;

				if (q != NULL) {
					*q = '\0';
					tno = (unsigned)atoi(inbuff+ind+6);
					tno++;
					if (tno < 100) {
						if (global.tracktitle[tno] == NULL) {
							global.tracktitle[tno] = malloc( p - q + 1 );
							if (global.tracktitle[tno] != NULL)
								*(global.tracktitle[tno]) = '\0';
						} else {
							global.tracktitle[tno] = realloc(global.tracktitle[tno], strlen((char *)global.tracktitle[tno]) + p - q + 1 );
						}
						if (global.tracktitle[tno] != NULL) {
							strcat((char *)global.tracktitle[tno], q+1);
						}
					}
				}
			} else if (!strncmp(inbuff+ind, "DYEAR", 5)) {
				char	*q = (char *)memchr(inbuff+ind, '=', readbytes);
				if (q++ != NULL) {
					sscanf(q, "%d", &global.cddb_year);
				}
			} else if (!strncmp(inbuff+ind, "DGENRE", 6)) {
				char	*q = (char *)memchr(inbuff+ind, '=', readbytes);
				if (q++ != NULL) {
					/* patch from Joe Nuzman, thanks */
					/* might have significant whitespace */
					strncpy(global.cddb_genre, q, sizeof(global.cddb_genre)-1);
					/* always have a terminator */
					global.cddb_genre[sizeof(global.cddb_genre)-1] = '\0';
				}
			} else if (!strncmp(inbuff+ind, "# Revision: ", 12)) {
				char	*q = inbuff+ind+11;
				sscanf(q, "%d", &global.cddb_revision);
				global.cddb_revision++;
			}
			readbytes -= (p - inbuff -ind) + 1;
			ind = (p - inbuff) + 1;
		}
		if (!finished) {
			int	newbytes;
			memmove(inbuff, inbuff+ind, readbytes);
			newbytes = readn(sock_fd, inbuff+readbytes, SOCKBUFF-readbytes);
			if (newbytes < 0) {
				fprintf(stderr, "Could not read from socket.\n");
				return 0; /* Caller checks for != 1 */
			}
			filter_nonprintable(inbuff+readbytes, newbytes);
			if (newbytes <= 0)
				break;
			readbytes += newbytes;
			ind = 0;
		}
	} while (!(finished || readbytes == 0));
	return finished;
}

static int handle_userchoice(char *p, unsigned size);

static int handle_userchoice(char *p, unsigned size)
{
	unsigned	nr = 0;
	unsigned	user_choice;
	int		i;
	char		*q;
	char		*o;

	/* count lines. */
	q = p;
	while ((q = (char *)memchr(q, '\n', size - (q-p))) != NULL) {
		nr++;
		q++;
	}
	if (nr > 1) nr--;

	/* handle escaped characters */

	{
		char *r = p;
		while (*r) {
			if (*r++ == '\\' && *r != '\0') {
				if (*r == '\\') {
					size--;
					memmove(r, r+1, size - (r-p));
				} else if (*r == 'n') {
					*(r-1) = '\n';
					size--;
					memmove(r, r+1, size - (r-p));
				} else if (*r == 't') {
					*(r-1) = '\t';
					size--;
					memmove(r, r+1, size - (r-p));
				}
			}
		}
	}

	/* list entries. */
	q = p;
	fprintf(stderr, "%u entries found:\n", nr);
	for (q = (char *)memchr(q, '\n', size - (q-p)), o = p, i = 0; i < nr; i++) {
		*q = '\0';
		fprintf(stderr, "%02u: %s\n", i, o);
		o = q+1;
		q = (char *)memchr(q, '\n', size - (q-p));
	}
	fprintf(stderr, "%02u: ignore\n", i);

	/* get user response. */
	do {
		fprintf(stderr, "please choose one (0-%u): ", nr);
		scanf("%u", &user_choice); /* FIXME: check return value */
	} while (user_choice > nr);

	if (user_choice == nr)
		return -1;

	/* skip to choice. */
	q = p;
	for (i = 0; i <= (int)user_choice - 1; i++) {
		q = (char *)memchr(q, '\0', size - (q-p)) + 1;
	}
	return	q-p;
}

/* request disc and track titles from a cddbp server.
 *
 * return values:
 *	0	titles have been found exactly (success)
 *	-1	some communication error happened.
 *	1	titles have not been found.
 *	2	multiple fuzzy matches have been found.
 */
int
request_titles(void)
{
	int		retval = 0;
	int		sock_fd;
	struct sockaddr_in sa;
	struct hostent *he;
	struct servent *se;
	struct passwd *pw = getpwuid(getuid());
	char		hostname[HOST_NAME_MAX];
	char		inbuff[SOCKBUFF];
	char		outbuff[SOCKBUFF];
	int		i;
	char		category[64];
	unsigned	cat_offset;
	unsigned	disc_id;
	ssize_t		readbytes;

	sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (sock_fd < 0) {
		perror("cddb socket failed: ");
		retval = -1;
		goto errout;
	}

	/* TODO fallbacks
	 * freedb.freedb.org
	 * de.freedb.org
	 * at.freedb.org
	 */
	if (global.cddbp_server != NULL)
		he = gethostbyname(global.cddbp_server);
	else
		he = gethostbyname(CDDBHOST /*"freedb.freedb.org"*/);

	if (he == NULL) {
		perror("cddb cannot resolve freedb host: ");
		he = malloc(sizeof(struct hostent));
		memset(he, 0 , sizeof(struct hostent));
		he->h_length = 4;
		he->h_addrtype = AF_INET;
		he->h_addr_list = malloc(4);
		he->h_addr_list[0] = malloc(4);
		((struct in_addr *)(he->h_addr_list[0]))->s_addr =
			/* kingfisher.berlios.de          freedb.freedb.de */
			 htonl(UINT_C(0xc3254d85));	/*0xc2610412*/
		he->h_name = "freedb.freedb.org";
#if	0
		retval = -1;
		goto errout;
#endif
	}

	/* save result data IMMEDIATELY!! */
	memset(&sa, 0 , sizeof(struct sockaddr_in));
	sa.sin_family 	   = he->h_addrtype;	/* AF_INET; */
	sa.sin_addr.s_addr = ((struct in_addr *)((he->h_addr_list)[0]))->s_addr;

	se = NULL;
	if (global.cddbp_port == NULL)
		se = getservbyname("cddbp-alt", "tcp");

	if (se == NULL) {
		if (global.cddbp_port == NULL) {
			se = getservbyname("cddbp", "tcp");
		}
		if (se == NULL) {
			se = malloc(sizeof(struct servent));
			memset(se, 0 , sizeof(struct servent));
			se->s_port = htons(CDDBPORT /*8880*/);
#if	0	
			perror("cddb cannot resolve cddbp or cddbp-alt port:\n "); 
			retval = -1;
			goto errout;
#endif
		}
	}
	if (global.cddbp_port != NULL) {
		se->s_port = htons(atoi(global.cddbp_port));
	}

	sa.sin_port        = se->s_port;

/* TODO timeout */
	if (0 > connect(sock_fd, (struct sockaddr *)&sa, 
			sizeof(struct sockaddr_in))) {
		perror("cddb connect failed: ");
		retval = -1;
		goto errout;
	}

	/* read banner */
	readbytes = readn(sock_fd, inbuff, sizeof(inbuff));
	if (readbytes < 0) {
		fprintf(stderr, "Could not read from socket\n");
		retval = -1;
		goto errout;
	}

	if (strncmp(inbuff, "200 ", 4) && strncmp(inbuff, "201 ", 4)) {
		if(readbytes == sizeof(inbuff))
			--readbytes;
		inbuff[readbytes] = '\0';
		filter_nonprintable(inbuff, readbytes);
		fprintf(stderr, "bad status from freedb server during sign-on banner: %s\n", inbuff);
		retval = -1;
		goto errout;
	}

	/* say hello */
	hostname[0] = '\0'; 
	if (0 > gethostname(hostname, sizeof(hostname)))
		strcpy(hostname, "unknown_host"); 
	hostname[sizeof(hostname)-1] = '\0';
	writez(sock_fd, "cddb hello ");
	if (pw != NULL) {
		BOOL	space_err = FALSE;
		BOOL	ascii_err = FALSE;
		/* change spaces to underscores */
		char *q = pw->pw_name;
		while (*q != '\0') {
			if (*q == ' ') {
				if (!space_err) {
					space_err = TRUE;
					errmsgno(EX_BAD,
					"Warning: Space in user name '%s'.\n",
					pw->pw_name);
				}
				*q = '_';
			}
			if (*q < ' ' || *q > '~') {
				if (!ascii_err) {
					ascii_err = TRUE;
					errmsgno(EX_BAD,
					"Warning: Nonascii character in user name '%s'.\n",
					pw->pw_name);
				}
				*q = '_';
			}
			q++;
		}
		writez(sock_fd, pw->pw_name);
		writez(sock_fd, " ");
	} else {
		writez(sock_fd, "unknown ");
	}

	/* change spaces to underscores */
	{
		char *q = hostname;
		BOOL	space_err = FALSE;
		BOOL	ascii_err = FALSE;

		while (*q != '\0') {
			if (*q == ' ') {
				if (!space_err) {
					space_err = TRUE;
					errmsgno(EX_BAD,
					"Warning: Space in hostname '%s'.\n",
					hostname);
				}
				*q = '_';
			}
			if (*q < ' ' || *q > '~') {
				if (!ascii_err) {
					ascii_err = TRUE;
					errmsgno(EX_BAD,
					"Warning: Nonascii character in hostname '%s'.\n",
					hostname);
				}
				*q = '_';
			}
			q++;
		}
	}

	writez(sock_fd, hostname);
	writez(sock_fd, " icedax " VERSION "\n");

	readbytes = readn(sock_fd, inbuff, sizeof(inbuff));
	if (readbytes < 0) {
		fprintf(stderr, "Could not read from socket\n");
		retval = -1;
		goto errout;
	}
	if (strncmp(inbuff, "200 ", 4)) {
		if(readbytes == sizeof(inbuff))
			--readbytes;
		inbuff[readbytes] = '\0';
		filter_nonprintable(inbuff, readbytes);
		fprintf(stderr, "bad status from freedb server during hello: %s\n", inbuff);
		retval = -1;
		goto signoff;
	}

	/* enable new protocol variant. Weird command here, no cddb prefix ?!?! */
	writez(sock_fd, "proto\n");
	readbytes = readn(sock_fd, inbuff, sizeof(inbuff));
	if (readbytes < 0) {
		fprintf(stderr, "Could not read from socket\n");
		retval = -1;
		goto errout;
	}
	/* check for errors and maximum supported protocol level */
	if (strncmp(inbuff, "201 ", 4) > 0) {
		if(readbytes == sizeof(inbuff))
			--readbytes;
		inbuff[readbytes] = '\0';
		filter_nonprintable(inbuff, readbytes);
		fprintf(stderr, "bad status from freedb server during proto command: %s\n", inbuff);
		retval = -1;
		goto signoff;
	}
	
	/* check the supported protocol level */
	if (!memcmp(inbuff, "200 CDDB protocol level: current 1, supported ", 46)) {
		char *q = strstr(inbuff, " supported ");
		unsigned	pr_level;

		if (q != NULL) {
			q += 11;
			sscanf(q, "%u\n", &pr_level);
			if (pr_level > 1) {
				if (pr_level > 5)
					pr_level = 5;
				sprintf(inbuff, "proto %1u\n", pr_level);
				writez(sock_fd, inbuff);
				readbytes = readn(sock_fd, inbuff, sizeof(inbuff));
				if (readbytes < 0) {
					fprintf(stderr, "Could not read from socket\n");
					retval = -1;
					goto errout;
				}
				/* check for errors and maximum supported protocol level */
				if (strncmp(inbuff, "201 ", 4) > 0) {
					if(readbytes == sizeof(inbuff))
						--readbytes;
					inbuff[readbytes] = '\0';
					filter_nonprintable(inbuff,
					  readbytes);
					fprintf(stderr, "bad status from freedb server during proto x: %s\n", inbuff);
					retval = -1;
					goto signoff;
				}
			}
		}
	}

	/* format query string */
	/* query */
#define	CDDPB_INCLUDING_DATATRACKS
#ifdef	CDDPB_INCLUDING_DATATRACKS
	sprintf(outbuff, "cddb query %08lx %ld ", (unsigned long)global.cddb_id, LastTrack() - FirstTrack() + 1);
	/* first all leading datatracks */
  	{
		int j = FirstAudioTrack();
		if (j < 0)
			j = LastTrack() +1;
  		for (i = FirstTrack(); i < j; i++) {
			sprintf(outbuff + strlen(outbuff), "%ld ", 150 + Get_StartSector(i));
		}
	}
#else
	sprintf(outbuff, "cddb query %08lx %ld ", global.cddb_id, LastAudioTrack() - FirstAudioTrack() + 1);
#endif
	/* all audio tracks */
  	for (i = FirstAudioTrack(); i != -1 && i <= LastAudioTrack(); i++) {
		sprintf(outbuff + strlen(outbuff), "%ld ", 150 + Get_AudioStartSector(i));
	}
#ifdef	CDDPB_INCLUDING_DATATRACKS
	/* now all trailing datatracks */
  	for (; i != -1 && i <= LastTrack(); i++) {
		sprintf(outbuff + strlen(outbuff), "%ld ", 150 + Get_StartSector(i));
	}
	sprintf(outbuff + strlen(outbuff), "%lu\n",
           (150 + Get_StartSector(CDROM_LEADOUT)) / 75);
#else
	sprintf(outbuff + strlen(outbuff), "%lu\n",
           (150 + Get_LastSectorOnCd(FirstAudioTrack())) / 75);
#endif
/*	strcpy(outbuff, "cddb query 9709210c 12 150 12010 33557 50765 65380 81467 93235 109115 124135 137732 152575 166742 2339\n"); */
/*	strcpy(outbuff, "cddb query 03015501 1 296 344\n"); */
	writez(sock_fd, outbuff);

	readbytes = readn(sock_fd, inbuff, sizeof(inbuff) - 1);
	if (readbytes < 0) {
		fprintf(stderr, "Could not read from socket\n");
		retval = -1;
		goto errout;
	}
	inbuff[readbytes] = '\0';
	filter_nonprintable(inbuff, readbytes);
	cat_offset = 4;
	if (!strncmp(inbuff, "210 ", 4)
	   || !strncmp(inbuff, "211 ", 4)) {
		/* Check if there are really multiple entries. */
		char *p = (char *)memchr(inbuff, '\n', readbytes-1);

		if (p != NULL) cat_offset = p+1 - inbuff;
		/* first entry */
		if (p) p = (char *)memchr(p+1, '\n', inbuff+readbytes - p);
		/* second entry */
		if (p) p = (char *)memchr(p+1, '\n', inbuff+readbytes - p);
		/* . */
		if (p) p = (char *)memchr(p+1, '\n', inbuff+readbytes - p);
		if (p) {
			/* multiple entries */
			switch (global.cddbp) {
				case	2:	/* take the first entry */
				break;
				case	1:	/* ask user */
					if (!global.gui) {
						int userret = handle_userchoice(inbuff+cat_offset, readbytes - cat_offset);
						if (userret == -1) {
							/* ignore any selection */
							retval = -1;
							goto signoff;
						}
						cat_offset += userret;
					}
				break;
				default:
					fprintf(stderr, "multiple entries found: %s\n", inbuff);
					retval = 2;
					goto signoff;
			}
		}

	} else if (strncmp(inbuff, "200 ", 4)) {
		if (!strncmp(inbuff, "202 ", 4)) {
			fprintf(stderr, "no cddb entry found: %s\n", inbuff);
			retval = 1;
		} else {
			fprintf(stderr, "bad status from freedb server during query: %s\n%s", inbuff, outbuff);
			retval = -1;
		}
		goto signoff;
	}
	sscanf(inbuff + cat_offset, "%s %x", category, &disc_id );


	/* read */
	sprintf(inbuff, "cddb read %s %08x\n", category, disc_id);
	writez(sock_fd, inbuff);

	/* read status and first buffer size. */
	readbytes = readn(sock_fd, inbuff, sizeof(inbuff));
	if (readbytes < 0) {
		fprintf(stderr, "Could not read from socket\n");
		retval = -1;
		goto errout;
	}
	filter_nonprintable(inbuff, readbytes);
	if (strncmp(inbuff, "210 ", 4)) {
		if(readbytes == sizeof(inbuff))
			--readbytes;
		inbuff[readbytes] = '\0';
		fprintf(stderr, "bad status from freedb server during read: %s\n", inbuff);
		retval = -1;
		goto signoff;
	}

	if (1 != process_cddb_titles(sock_fd, inbuff, readbytes)) {
		fprintf(stderr, "cddb read finished not correctly!\n");
	}

signoff:
	/* sign-off */
	writez(sock_fd, "quit\n");
	readbytes = readn(sock_fd, inbuff, sizeof(inbuff));
	if (readbytes < 0) {
		fprintf(stderr, "Could not read from socket\n");
		retval = -1;
		goto errout;
	}
	if (strncmp(inbuff, "230 ", 4)) {
		if(readbytes == sizeof(inbuff))
			--readbytes;
		inbuff[readbytes] = '\0';
		filter_nonprintable(inbuff, readbytes);
		fprintf(stderr, "bad status from freedb server during quit: %s\n", inbuff);
		goto errout;
	}

errout:
	close(sock_fd);
	return retval;
}
#endif
#endif

#if	defined CDINDEX_SUPPORT

static int IsSingleArtist(void);

/* check, if there are more than one track creators */
static int IsSingleArtist(void)
{
	static struct iterator i;
	InitIterator(&i, 1);

	while (i.hasNextTrack(&i)) {
		struct TOC *p = i.getNextTrack(&i);
		int ii;

		if (IS__DATA(p) || GETTRACK(p) == CDROM_LEADOUT) continue;

		ii = GETTRACK(p);
		if (global.creator && global.trackcreator[ii]
			&& strcmp((char *) global.creator,
				  (char *) global.trackcreator[ii]) != 0)
			return 0;
	}
	return 1;
}

static const char *a2h[255-191] = {
"&Agrave;",
"&Aacute;",
"&Acirc;",
"&Atilde;",
"&Auml;",
"&Aring;",
"&AElig;",
"&Ccedil;",
"&Egrave;",
"&Eacute;",
"&Ecirc;",
"&Euml;",
"&Igrave;",
"&Iacute;",
"&Icirc;",
"&Iuml;",
"&ETH;",
"&Ntilde;",
"&Ograve;",
"&Oacute;",
"&Ocirc;",
"&Otilde;",
"&Ouml;",
"&times;",
"&Oslash;",
"&Ugrave;",
"&Uacute;",
"&Ucirc;",
"&Uuml;",
"&Yacute;",
"&THORN;",
"&szlig;",
"&agrave;",
"&aacute;",
"&acirc;",
"&atilde;",
"&auml;",
"&aring;",
"&aelig;",
"&ccedil;",
"&egrave;",
"&eacute;",
"&ecirc;",
"&euml;",
"&igrave;",
"&iacute;",
"&icirc;",
"&iuml;",
"&eth;",
"&ntilde;",
"&ograve;",
"&oacute;",
"&ocirc;",
"&otilde;",
"&ouml;",
"&divide;",
"&oslash;",
"&ugrave;",
"&uacute;",
"&ucirc;",
"&uuml;",
"&yacute;",
"&thorn;",
"&yuml;",
};

static char *ascii2html(unsigned char *inp)
{
	static size_t buflen = 256;
	static char *outline = 0;
	size_t pos = 0;
    size_t l=0;

	/* init */
	if(!outline) {
		outline = malloc(buflen);
		if(outline == 0) {
			fprintf(stderr, "error: memory exhausted\n");
			_exit(EXIT_FAILURE);
		}
	}

	outline[pos] = '\0';

	while (*inp != '\0') {

		/* Pick the sequence to insert */
		const char *insert;
		char b[2];
		const int c = (unsigned char)*inp;
		switch(c) {
			case '"':	insert = "&quot;";	break;
			case '&':	insert = "&amp;";	break;
			case '<':	insert = "&lt;";	break;
			case '>':	insert = "&gt;";	break;
			case 160:	insert = "&nbsp;";	break;
			default: {
					 if(c < 192) {
						 b[0] = c;
						 b[1] = '\0';
						 insert = b;
					 } else {
						 insert = a2h[c - 192];
					 }
				 }
		};

		/* Resize buffer */
		l = strlen(insert);
		while(pos + l + 1 >= buflen) {
			outline = realloc(outline, buflen *= 2);
			if(outline == 0) {
				fprintf(stderr, "error: memory exhausted\n");
				_exit(EXIT_FAILURE);
			}
		}

		/* Copy in */
		strcpy(&outline[pos], insert);
		pos += l;

		++inp;
	}

	return outline;
}

static void emit_cdindex_form(char *fname_baseval)
{
  FILE *cdindex_form;
  char fname[200];
  char *pp;

  if (fname_baseval == NULL || fname_baseval[0] == 0)
	return;

  strncpy(fname, fname_baseval, sizeof(fname) -1);
  fname[sizeof(fname) -1] = 0;
  pp = strrchr(fname, '.');
  if (pp == NULL) {
    pp = fname + strlen(fname);
  }
  strncpy(pp, ".cdindex", sizeof(fname) - 1 - (pp - fname));

  cdindex_form = fopen(fname, "w");
  if (cdindex_form == NULL) return;

#define	CDINDEX_URL	"http://www.musicbrainz.org/dtd/CDInfo.dtd"

  /* format XML page according to cdindex DTD (see www.musicbrainz.org) */
  fprintf( cdindex_form, "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n<!DOCTYPE CDInfo SYSTEM \"%s\">\n\n<CDInfo>\n",
		  CDINDEX_URL);

  fprintf( cdindex_form, "   <Title>%s</Title>\n",
	 global.disctitle ? ascii2html(global.disctitle) : "");
  /* 
   * In case of mixed mode and Extra-CD, nonaudio tracks are included!
   */
  fprintf( cdindex_form, "   <NumTracks>%d</NumTracks>\n\n", cdtracks);
  fprintf( cdindex_form, "   <IdInfo>\n      <DiskId>\n         <Id>%s</Id>\n      </DiskId>\n", global.cdindex_id); 

  fprintf( cdindex_form, "   </IdInfo>\n\n");

  if (IsSingleArtist()) {
    static struct iterator i;
    InitIterator(&i, 1);

    fprintf( cdindex_form, "   <SingleArtistCD>\n      <Artist>%s</Artist>\n",
	 global.creator ? ascii2html(global.creator) : "");

    while (i.hasNextTrack(&i)) {
	    struct TOC *p = i.getNextTrack(&i);
	    int ii = GETTRACK(p);

	    if (ii == CDROM_LEADOUT) break;
	    if (IS__AUDIO(p)) {
		    fprintf( cdindex_form,
			     "      <Track Num=\"%d\">\n         <Name>%s</Name>\n      </Track>\n",
			     ii, global.tracktitle[ii] ? ascii2html(global.tracktitle[ii]) : "");
	    } else {
		    fprintf( cdindex_form,
			     "      <Track Num=\"%d\">\n         <Name>data track</Name>\n      </Track>\n",
			     ii );
	    }
    }
    fprintf( cdindex_form, "   </SingleArtistCD>\n");
  } else {
    static struct iterator i;
    InitIterator(&i, 1);

    fprintf( cdindex_form, "   <MultipleArtistCD>\n");

    while (i.hasNextTrack(&i)) {
	    struct TOC *p = i.getNextTrack(&i);
	    int ii = GETTRACK(p);

	    if (ii == CDROM_LEADOUT) break;
	    if (IS__AUDIO(p)) {
		    fprintf( cdindex_form, "         <Artist>%s</Artist>\n",
			     global.trackcreator[ii] ? ascii2html(global.trackcreator[ii]) : "");
		    fprintf( cdindex_form, "         <Name>%s</Name>\n      </Track>\n",
		  global.tracktitle[ii] ? ascii2html(global.tracktitle[ii]) : "");
	    } else {
		    fprintf( cdindex_form,
			     "         <Artist>data track</Artist>\n         <Name>data track</Name>\n      </Track>\n");
	    }
    }
    fprintf( cdindex_form, "   </MultipleArtistCD>\n");
  }
  fprintf( cdindex_form, "</CDInfo>\n");

  fclose( cdindex_form );
}
#endif

static void dump_cdtext_info(void)
{
#ifdef CD_TEXT
  /* interpret the contents of CD Text information based on an early draft
     of SCSI-3 mmc version 2 from jan 2, 1998
     CD Text information consists of a text group containing up to
     8 language blocks containing up to
     255 Pack data chunks of
     18 bytes each.
     So we have at most 36720 bytes to cope with.
   */
  {
    short int datalength;
    unsigned char *p = bufferTOC;
    unsigned char lastline[255*12];
    int		lastitem = -1;
    int		itemcount = 0;
    int		inlinecount = 0;
    int		outlinecount = 0;

    lastline[0] = '\0';
    datalength = ((p[0] << 8) + p[1]) - 2;
    datalength = min(datalength, 2048-4);
    p += 4;
    for (;datalength > 0;
	datalength -= sizeof (cdtextpackdata),
	p += sizeof (cdtextpackdata)) {
      unsigned char *zeroposition;

      /* handle one packet of CD Text Information Descriptor Pack Data */
      /* this is raw R-W subchannel data converted to 8 bit values. */
      cdtextpackdata *c = (cdtextpackdata *)p;
      int dbcc;
      int crc_error;
      unsigned tracknr;

#ifdef DEBUG_CDTEXT
      fprintf(stderr, "datalength =%d\n", datalength);
#endif
      crc_error = !cdtext_crc_ok(c);

      if (lastitem != c->headerfield[0]) {
	itemcount = 0;
	lastitem = c->headerfield[0];
      }

      tracknr = c->headerfield[1] & 0x7f;
      dbcc = ((unsigned)(c->headerfield[3] & 0x80)) >> 7; /* double byte character code */

#if defined DEBUG_CDTEXT
	{
      int extension_flag;
      int sequence_number;
      int block_number;
      int character_position;

      extension_flag = ((unsigned)(c->headerfield[1] & 0x80)) >> 7;
      sequence_number = c->headerfield[2];
      block_number = ((unsigned)(c->headerfield[3] & 0x30)) >> 4; /* language */
      character_position = c->headerfield[3] & 0x0f;

      fprintf(stderr, "CDText: ext_fl=%d, trnr=%u, seq_nr=%d, dbcc=%d, block_nr=%d, char_pos=%d\n",
             extension_flag, tracknr, sequence_number, dbcc, block_number, character_position);
	}
#endif

      /* print ASCII information */
      memcpy(lastline+inlinecount, c->textdatafield, 12);
      inlinecount += 12;
      zeroposition = (unsigned char *)memchr(lastline+outlinecount, '\0', inlinecount-outlinecount);
      while (zeroposition != NULL) {
	  process_header(c, tracknr, dbcc, lastline+outlinecount);
	  outlinecount += zeroposition - (lastline+outlinecount) + 1;

#if defined DEBUG_CDTEXT
	  fprintf(stderr, "\tin=%d, out=%d, items=%d, trcknum=%u\n", inlinecount, outlinecount, itemcount, tracknr);
{ int q; for (q= outlinecount; q < inlinecount; q++) fprintf (stderr, "%c", lastline[q] ? lastline[q] : '�'); fputs("\n", stderr); }
#else
	  if (DETAILED) {
	    if (crc_error) fputs(" ! uncorr. CRC-Error", stderr);
	    fputs("\n", stderr);
	  }
#endif

	  itemcount++;
	  if (itemcount > (int)cdtracks
	      	|| (c->headerfield[0] == 0x8f
		|| (c->headerfield[0] <= 0x8d && c->headerfield[0] >= 0x86))) {
	    outlinecount = inlinecount;
	    break;
	  }
	  tracknr++;
	  zeroposition = (unsigned char *)memchr(lastline+outlinecount, '\0', inlinecount-outlinecount);
      }
    }
  }
#endif
}



static void dump_extra_info(unsigned int from)
{
#ifdef CD_EXTRA
  unsigned char *p;
  unsigned pos, length;

  if (from == 0) return;

  p = Extra_buffer + 48;
  while (*p != '\0') {
    pos    = GET_BE_UINT_FROM_CHARP(p+2);
    length = GET_BE_UINT_FROM_CHARP(p+6);
    if (pos == (unsigned)-1) {
      pos = from+1;
    } else {
      pos += session_start;
    }

#ifdef DEBUG_XTRA
    if (global.gui == 0 && global.verbose != 0) {
	fprintf(stderr, "Language: %c%c (as defined by ISO 639)", *p, *(p+1));
	fprintf(stderr, " at sector %u, len=%u (sessionstart=%u)", pos, length, session_start);
	fputs("\n", stderr);
    }
#endif
    /* dump this entry */
    Read_Subinfo(pos, length);
    p += 10;

    if (p + 9 > (Extra_buffer + CD_FRAMESIZE))
      break;
  }
#endif
}

static char *quote(unsigned char *string);

static char *quote(unsigned char *string)
{
  static char result[200];
  unsigned char *p = (unsigned char *)result;

  while (*string) {
    if (*string == '\'' || *string == '\\') {
	*p++ = '\\';
    }
    *p++ = *string++;
  }
  *p = '\0';

  return result;
}



static void DisplayToc_with_gui(unsigned long dw);

static void DisplayToc_with_gui(unsigned long dw)
{
	unsigned mins;
	unsigned secnds;
	unsigned frames;
	int count_audio_trks;
	static struct iterator i;
	if (i.reset == NULL) InitIterator(&i, 1);
	else i.reset(&i);

	mins	=   dw / ( 60*75 );
	secnds  = ( dw % ( 60*75 ) ) / 75;
	frames  = ( dw %      75   );

	/* summary */
	count_audio_trks = 0;

	if ((global.verbose & SHOW_STARTPOSITIONS) != 0) {
		if (global.illleadout_cd != 0 && have_CD_extra == 0) {
			fprintf( stderr, "Tracks:%u > %u:%02u.%02u\n", cdtracks, mins, secnds, frames );
		} else {
			fprintf( stderr, "Tracks:%u %u:%02u.%02u\n", cdtracks, mins, secnds, frames );
		}
	}

	if (global.quiet == 0) {
		fprintf( stderr, "CDINDEX discid: %s\n", global.cdindex_id);
		fprintf( stderr, "CDDB discid: 0x%08lx", (unsigned long) global.cddb_id);

		if (have_CDDB != 0) {
			fprintf(stderr, " CDDBP titles: resolved\n");
		} else {
			fprintf(stderr, "\n");
		}
		if (have_CD_text != 0) {
			fprintf(stderr, "CD-Text: detected\n");
			dump_cdtext_info();
		} else {
			fprintf(stderr, "CD-Text: not detected\n");
		}
		if (have_CD_extra != 0) {
			fprintf(stderr, "CD-Extra: detected\n");
			dump_extra_info(have_CD_extra);
		} else {
			fprintf(stderr, "CD-Extra: not detected\n");
		}
 
		fprintf( stderr, 
			 "Album title: '%s'", (void *)global.disctitle != NULL
			 ? quote(global.disctitle) : "");

		fprintf( stderr, " from '%s'\n", (void *)global.creator != NULL 
			 ? quote(global.creator) : "");
	}
	count_audio_trks = 0;


	if ((global.verbose & (SHOW_TOC | SHOW_STARTPOSITIONS | SHOW_SUMMARY | SHOW_TITLES)) != 0
	    && i.hasNextTrack(&i)) {
		TOC *o = i.getNextTrack(&i);
		while (i.hasNextTrack(&i)) {
			TOC *p = i.getNextTrack(&i);
			int from;
			from = GETTRACK(o);

			fprintf(stderr,	"T%02d:", from);

			if (IS__DATA(o)) {
				/*
				 * Special case of cd extra
				 */
				unsigned int real_start = have_CD_extra
					? have_CD_extra	: GETSTART(o);


				dw = (unsigned long) (GETSTART(p) - real_start);

				mins   =   dw / ( 60*75 );
				secnds = ( dw % ( 60*75 )) / 75;
				frames = ( dw %      75   );

				if ( global.verbose & SHOW_STARTPOSITIONS )
					fprintf(stderr,
						" %7u",
						real_start
					);

				if ( global.verbose & SHOW_TOC )
					fprintf(stderr,
						" %2u:%02u.%02u",
						mins, secnds, frames
					);

				if ( global.verbose & SHOW_SUMMARY )
					fprintf(stderr,
						" data %s %s N/A",
						
						/* how recorded */
						IS__INCREMENTAL(o)
						? "incremental" : "uninterrupted",

						/* copy-permission */
						IS__COPYRIGHTED(o)
						? "copydenied" : "copyallowed"
					);
				fputs("\n", stderr);
			} else {
				dw = (unsigned long) (GETSTART(p) - GETSTART(o));
				mins   =   dw / ( 60*75 );
				secnds = ( dw % ( 60*75 )) / 75;
				frames = ( dw %      75   );
					
				if ( global.verbose & SHOW_STARTPOSITIONS )
					fprintf(stderr,
						" %7u",
						GETSTART(o)
					);

				if ( global.verbose & SHOW_TOC )
					fprintf(stderr,
						" %2u:%02u.%02u",
						mins, secnds, frames
					);

				if ( global.verbose & SHOW_SUMMARY )
					fprintf(stderr,
						" audio %s %s %s",

					/* how recorded */
					IS__PREEMPHASIZED(o)
					? "pre-emphasized" : "linear",
						
					/* copy-permission */
					IS__COPYRIGHTED(o)
					? "copydenied" : "copyallowed",

					/* channels */
					IS__QUADRO(o)
						? "quadro" : "stereo");

				/* Title */
				if ( global.verbose & SHOW_TITLES ) {
					fprintf(stderr,
						" title '%s' from ",

						(void *) global.tracktitle[GETTRACK(o)] != NULL
						? quote(global.tracktitle[GETTRACK(o)]) : ""
					);
					
					fprintf(stderr,
						"'%s'",

						(void *) global.trackcreator[GETTRACK(o)] != NULL
						? quote(global.trackcreator[GETTRACK(o)]) : ""
					);
				}
				fputs("\n", stderr);
				count_audio_trks++;
			}
			o = p;
		} /* while */
		if ( global.verbose & SHOW_STARTPOSITIONS )
			if (GETTRACK(o) == CDROM_LEADOUT) {
				fprintf(stderr, "Leadout: %7u\n", GETSTART(o));
			}
	} /* if */
}

static void DisplayToc_no_gui(unsigned long dw);

static void DisplayToc_no_gui(unsigned long dw)
{
	unsigned mins;
	unsigned secnds;
	unsigned frames;
	int count_audio_trks;
	unsigned ii = 0;
	static struct iterator i;
	if (i.reset == NULL) InitIterator(&i, 1);
	else i.reset(&i);

	mins	=   dw / ( 60*75 );
	secnds  = ( dw % ( 60*75 ) ) / 75;
	frames  = ( dw %      75   );

	/* summary */
	count_audio_trks = 0;

	if (i.hasNextTrack(&i)) {
		TOC *o = i.getNextTrack(&i);
		while (i.hasNextTrack(&i)) {
			TOC *p = i.getNextTrack(&i);
			int from;
			from = GETTRACK(o);


			while ( p != NULL && GETTRACK(p) != CDROM_LEADOUT
				&& GETFLAGS(o) == GETFLAGS(p) ) {
				o = p;
				p = i.getNextTrack(&i);
			}
			if ((global.verbose & SHOW_SUMMARY) == 0) continue;

			if (IS__DATA(o)) {
				fputs( " DATAtrack recorded      copy-permitted tracktype\n" , stderr);
				fprintf(stderr,
					"     %2d-%2d %13.13s %14.14s      data\n",
					from,
					GETTRACK(o),
					/* how recorded */
					IS__INCREMENTAL(o) 
					 ? "incremental" : "uninterrupted",

					/* copy-perm */
					IS__COPYRIGHTED(o) ? "no" : "yes"
					);
			} else { 
				fputs( "AUDIOtrack pre-emphasis  copy-permitted tracktype channels\n" , stderr);
				fprintf(stderr,
					"     %2d-%2d %12.12s  %14.14s     audio    %1c\n",
					from,
					GETTRACK(o),
					IS__PREEMPHASIZED(o) 
					 ? "yes" : "no",
					IS__COPYRIGHTED(o) ? "no" : "yes",
					IS__QUADRO(o) ? '4' : '2'
					);
				count_audio_trks++;
			}
			o = p;
		}
	}
	if ((global.verbose & SHOW_STARTPOSITIONS) != 0) {
		if (global.illleadout_cd != 0 && have_multisession == 0) {

			fprintf ( stderr, 
				  "Table of Contents: total tracks:%u, (total time more than %u:%02u.%02u)\n",
				  cdtracks, mins, secnds, frames );
		} else {
			fprintf ( stderr, 
				  "Table of Contents: total tracks:%u, (total time %u:%02u.%02u)\n",
				  cdtracks, mins, secnds, frames );
		}
	}

	i.reset(&i);
	if ((global.verbose & SHOW_TOC) != 0 &&
		i.hasNextTrack(&i)) {
		TOC *o = i.getNextTrack(&i);

		for (; i.hasNextTrack(&i);) {
			TOC *p = i.getNextTrack(&i);

			if ( GETTRACK(o) <= MAXTRK ) {
				unsigned char brace1, brace2;
				unsigned trackbeg;
				trackbeg = have_multisession && IS__DATA(o) ? have_multisession : GETSTART(o);
			
				dw = (unsigned long) (GETSTART(p) - trackbeg);
				mins   =   dw / ( 60*75 );
				secnds = ( dw % ( 60*75 )) / 75;
				frames = ( dw %      75   );

				if ( IS__DATA(o) ) {
					/* data track display */
					brace1 = '[';
					brace2 = ']';
				} else if (have_multisession
					   && GETTRACK(o) == LastAudioTrack()) {
					/* corrected length of
					 * last audio track in cd extra
					 */
					brace1 = '|';
					brace2 = '|';
				} else {
					/* audio track display */
					brace1 = '(';
					brace2 = ')';
				}
				fprintf ( stderr,
					  " %2u.%c%2u:%02u.%02u%c",
					  GETTRACK(o),
					  brace1,
					  mins, secnds, frames,
					  brace2
					);
				ii++;
			
				if ( ii % 5 == 0 )
					fputs( ",\n", stderr );
				else if (ii != cdtracks)
					fputc ( ',', stderr );
			}
			o = p;
		} /* for */
		if ( (ii % 5) != 0 )
			fputs( "\n", stderr );
	
	} /* if */

	if ((global.verbose & SHOW_STARTPOSITIONS) != 0) {
		fputs ("\nTable of Contents: starting sectors\n", stderr);

		ii = 0;
		i.reset(&i);
		if (i.hasNextTrack(&i)) {
			TOC *o = i.getNextTrack(&i);
			for ( ; i.hasNextTrack(&i);) {
				TOC *p = i.getNextTrack(&i);
				fprintf ( stderr,
					  " %2u.(%8u)",
					  GETTRACK(o),
					  have_multisession
					   && GETTRACK(o) == FirstDataTrack()
					    ? have_multisession
					    : GETSTART(o)
#ifdef DEBUG_CDDB
					  +150
#endif
					);

				ii++;
				if ( (ii) % 5 == 0 )
					fputs( ",\n", stderr );
				else
					fputc ( ',', stderr );
				o = p;
			}
			fprintf ( stderr, " lead-out(%8u)", GETSTART(o));
			fputs ("\n", stderr);
		}
	}
	if (global.quiet == 0) {
		fprintf(stderr, "CDINDEX discid: %s\n", global.cdindex_id);
		fprintf( stderr, "CDDB discid: 0x%08lx", (unsigned long) global.cddb_id);

		if (have_CDDB != 0) {
			fprintf(stderr, " CDDBP titles: resolved\n");
		} else {
			fprintf(stderr, "\n");
		}
		if (have_CD_text != 0) {
			fprintf(stderr, "CD-Text: detected\n");
		} else {
			fprintf(stderr, "CD-Text: not detected\n");
		}
		if (have_CD_extra != 0) {
			fprintf(stderr, "CD-Extra: detected\n");
		} else {
			fprintf(stderr, "CD-Extra: not detected\n");
		}
	}
	if ((global.verbose & SHOW_TITLES) != 0) {
		int maxlen = 0;
		
		if ( global.disctitle != NULL ) {
			fprintf( stderr, "Album title: '%s'", global.disctitle);
			if ( global.creator != NULL ) {
				fprintf( stderr, "\t[from %s]", global.creator);
			}
			fputs("\n", stderr);
		}

		i.reset(&i);
		for ( ; i.hasNextTrack(&i);) {
			TOC *p = i.getNextTrack(&i);
			int jj = GETTRACK(p);

			if ( global.tracktitle[jj] != NULL ) {
				int len = strlen((char *)global.tracktitle[jj]);
				maxlen = max(maxlen, len);
			}
		}
		maxlen = (maxlen + 12 + 8 + 7)/8;
		
		i.reset(&i);
		for ( ; i.hasNextTrack(&i); ) {
			TOC *p = i.getNextTrack(&i);
			int jj;

			if (IS__DATA(p))
				continue;

			jj = GETTRACK(p);

			if (jj == CDROM_LEADOUT)
				break;
			
			if ( maxlen != 3 ) {
				if ( global.tracktitle[jj] != NULL ) {
					fprintf( stderr, "Track %2u: '%s'", jj, global.tracktitle[jj]);
				} else {
					fprintf( stderr, "Track %2u: '%s'", jj, "");
				}
				if ( global.trackcreator[jj] != NULL
				     && global.trackcreator[jj][0] != '\0'
#if 1
				     && (global.creator == NULL
					 || 0 != strcmp((char *)global.creator,(char *)global.trackcreator[jj]))
#endif
					) {
					int j;
					char *o = global.tracktitle[jj] != NULL
						? (char *)global.tracktitle[jj]
						: "";
					for ( j = 0;
					      j < (maxlen - ((int)strlen(o) + 12)/8);
					      j++)
						fprintf(stderr, "\t");
					fprintf( stderr, "[from %s]", global.trackcreator[jj]);
				}
				fputs("\n", stderr);
			}
		}
	}
}

void DisplayToc(void)
{
	unsigned long dw;

	/* special handling of pseudo-red-book-audio cds */
	if (cdtracks > 1
	    && Get_StartSector(CDROM_LEADOUT) < Get_StartSector(cdtracks)) {
		global.illleadout_cd = 1;
		can_read_illleadout();
	}


	/* get total time */
	if (global.illleadout_cd == 0)
		dw = (unsigned long) Get_StartSector(CDROM_LEADOUT) - Get_StartSector(1);
	else
		dw = (unsigned long) Get_StartSector(cdtracks     ) - Get_StartSector(1);

	if ( global.gui == 0 ) {
		/* table formatting when in cmdline mode */
		DisplayToc_no_gui( dw );
	} else if (global.gui == 1) {
		/* line formatting when in gui mode */
		DisplayToc_with_gui( dw );
	}

	if (global.illleadout_cd != 0) {
		if (global.quiet == 0) {
			fprintf(stderr, "CD with illegal leadout position detected!\n");
		}

		if (global.reads_illleadout == 0) {
			/* limit accessible tracks 
			 * to lowered leadout position
			 */
			restrict_tracks_illleadout();

			if (global.quiet == 0) {
				fprintf(stderr,
					"The cdrom drive firmware does not permit access beyond the leadout position!\n");
			}
			if (global.verbose & (SHOW_ISRC | SHOW_INDICES)) {
				global.verbose &= ~(SHOW_ISRC | SHOW_INDICES);
				fprintf(stderr, "Switching index scan and ISRC scan off!\n");
			}

			if (global.quiet == 0) {
				fprintf(stderr,
					"Audio extraction will be limited to track %ld with maximal %ld sectors...\n",
					LastTrack(),
					Get_EndSector(LastTrack())+1
					);
			}
		} else {
			/* The cdrom drive can read beyond the
			 * indicated leadout. We patch a new leadout
			 * position to the maximum:
			 *   99 minutes, 59 seconds, 74 frames
			 */
			patch_real_end(150 + (99*60+59)*75 + 74);
			if (global.quiet == 0) {
				fprintf(stderr,
					"Restrictions apply, since the size of the last track is unknown!\n");
			}
		}
	}
}

static void Read_MCN_toshiba(subq_chnl **sub_ch);

static void Read_MCN_toshiba(subq_chnl **sub_ch)
{
	if (Toshiba3401() != 0 && global.quiet == 0
	    && ((*sub_ch) != 0
		|| (((subq_catalog *)(*sub_ch)->data)->mc_valid & 0x80))) {
		/* no valid MCN yet. do more searching */
		long h = Get_AudioStartSector(1);
		
		while (h <= Get_AudioStartSector(1) + 100) {
			if (Toshiba3401())
				ReadCdRom(get_scsi_p(), RB_BASE->data, h, global.nsectors);
			(*sub_ch) = ReadSubQ(get_scsi_p(), GET_CATALOGNUMBER,0);
			if ((*sub_ch) != NULL) {
				subq_catalog *subq_cat;

				subq_cat = (subq_catalog *) (*sub_ch)->data;
				if ((subq_cat->mc_valid & 0x80) != 0) {
					break;
				}
			}
			h += global.nsectors;
		}
	}
}

static void Get_Set_MCN(void);

static void Get_Set_MCN(void)
{
	subq_chnl *sub_ch;
	subq_catalog *subq_cat = NULL;
	fprintf(stderr, "scanning for MCN...");
    
	sub_ch = ReadSubQ(get_scsi_p(), GET_CATALOGNUMBER,0);

#define EXPLICIT_READ_MCN_ISRC 1
#if EXPLICIT_READ_MCN_ISRC == 1 /* TOSHIBA HACK */
	Read_MCN_toshiba( &sub_ch );
#endif

	if (sub_ch != NULL)
		subq_cat = (subq_catalog *)sub_ch->data;
			
	if (sub_ch != NULL
	    && (subq_cat->mc_valid & 0x80) != 0
	    && global.quiet == 0) {

		/* unified format guesser:
		 * format MCN all digits in bcd
		 *     1                                  13
		 * A: ab cd ef gh ij kl m0  0  0  0  0  0  0  Plextor 6x Rel. 1.02
		 * B: 0a 0b 0c 0d 0e 0f 0g 0h 0i 0j 0k 0l 0m  Toshiba 3401
		 * C: AS AS AS AS AS AS AS AS AS AS AS AS AS  ASCII SCSI-2 Plextor 4.5x and 6x Rel. 1.06
		 */
		unsigned char *cp = subq_cat->media_catalog_number;
		if (!(cp[8] | cp[9] | cp[10] | cp[11] | cp[12])
		    && ((cp[0] & 0xf0) | (cp[1] & 0xf0)
			| (cp[2] & 0xf0) | (cp[3] & 0xf0)
			| (cp[4] & 0xf0) | (cp[5] & 0xf0)
			| (cp[6] & 0xf0))) {
			/* reformat A: to B: */
			cp[12] = cp[6] >> 4; cp[11] = cp[5] & 0xf;
			cp[10] = cp[5] >> 4; cp[ 9] = cp[4] & 0xf;
			cp[ 8] = cp[4] >> 4; cp[ 7] = cp[3] & 0xf;
			cp[ 6] = cp[3] >> 4; cp[ 5] = cp[2] & 0xf;
			cp[ 4] = cp[2] >> 4; cp[ 3] = cp[1] & 0xf;
			cp[ 2] = cp[1] >> 4; cp[ 1] = cp[0] & 0xf;
			cp[ 0] = cp[0] >> 4;
		}

		if (!isdigit(cp[0])
		    && (memcmp(subq_cat->media_catalog_number,
			       "\0\0\0\0\0\0\0\0\0\0\0\0\0", 13) != 0)) {
			sprintf((char *)
				subq_cat->media_catalog_number, 
				"%1.1X%1.1X%1.1X%1.1X%1.1X%1.1X%1.1X%1.1X%1.1X%1.1X%1.1X%1.1X%1.1X", 
				subq_cat->media_catalog_number [0],
				subq_cat->media_catalog_number [1],
				subq_cat->media_catalog_number [2],
				subq_cat->media_catalog_number [3],
				subq_cat->media_catalog_number [4],
				subq_cat->media_catalog_number [5],
				subq_cat->media_catalog_number [6],
				subq_cat->media_catalog_number [7],
				subq_cat->media_catalog_number [8],
				subq_cat->media_catalog_number [9],
				subq_cat->media_catalog_number [10],
				subq_cat->media_catalog_number [11],
				subq_cat->media_catalog_number [12]
				);
		}

		if (memcmp(subq_cat->media_catalog_number,"0000000000000",13)
		    != 0) {
			Set_MCN(subq_cat->media_catalog_number);
		}
	}
}


static void Read_ISRC_toshiba(subq_chnl **sub_ch, unsigned tr);

static void Read_ISRC_toshiba(subq_chnl **sub_ch, unsigned tr)
{
	if (Toshiba3401() != 0) {
		int j;
		j = (Get_AudioStartSector(tr)/100 + 1) * 100;
		do {
			ReadCdRom(get_scsi_p(), RB_BASE->data, j, global.nsectors);
			*sub_ch = ReadSubQ(get_scsi_p(), GET_TRACK_ISRC, Get_Tracknumber(tr));
			if (*sub_ch != NULL) {
				subq_track_isrc * subq_tr;

				subq_tr = (subq_track_isrc *) (*sub_ch)->data;
				if (subq_tr != NULL && (subq_tr->tc_valid & 0x80) != 0)
					break;
			}
			j += global.nsectors;
		} while (j < (Get_AudioStartSector(tr)/100 + 1) * 100 + 100);
	}
}


static void Get_Set_ISRC(unsigned tr);

static void Get_Set_ISRC(unsigned tr)
{
	subq_chnl *sub_ch;
	subq_track_isrc * subq_tr;

	fprintf(stderr, "\rscanning for ISRCs: %d ...", tr);

	subq_tr = NULL;
	sub_ch = ReadSubQ(get_scsi_p(), GET_TRACK_ISRC, tr);

#if EXPLICIT_READ_MCN_ISRC == 1 /* TOSHIBA HACK */
	Read_ISRC_toshiba( &sub_ch, tr );
#endif
    
	if (sub_ch != NULL)
		subq_tr = (subq_track_isrc *)sub_ch->data;
	
	if (sub_ch != NULL && (subq_tr->tc_valid & 0x80)
	    && global.quiet == 0) {
		unsigned char p_start[16];
		unsigned char *p = p_start;
		unsigned char *cp = subq_tr->track_isrc;

		/* unified format guesser:
		 * there are 60 bits and 15 bytes available.
		 * 5 * 6bit-items + two zero fill bits + 7 * 4bit-items
		 *
		 * A: ab cd ef gh ij kl mn o0 0  0  0  0  0  0  0  Plextor 6x Rel. 1.02
		 * B: 0a 0b 0c 0d 0e 0f 0g 0h 0i 0j 0k 0l 0m 0n 0o Toshiba 3401
		 * C: AS AS AS AS AS AS AS AS AS AS AS AS AS AS AS ASCII SCSI-2
		 * eg 'G''B''-''A''0''7''-''6''8''-''0''0''2''7''0' makes most sense
		 * D: 'G''B''A''0''7''6''8''0''0''2''7''0'0  0  0  Plextor 6x Rel. 1.06 and 4.5x R. 1.01 and 1.04
		 */

		/* Check for format A: */
		if (!(cp[8] | cp[9] | cp[10] | cp[11] | cp[12] | cp[13] | cp[14]) &&
		    ((cp[0] & 0xf0) | (cp[1] & 0xf0) | (cp[2] & 0xf0) | 
		     (cp[3] & 0xf0) | (cp[4] & 0xf0) | (cp[5] & 0xf0) | 
		     (cp[6] & 0xf0) | (cp[7] & 0xf0))) {
#if DEBUG_ISRC
	fprintf(stderr, "a!\t");
#endif
			/* reformat A: to B: */
			cp[14] = cp[7] >> 4; cp[13] = cp[6] & 0xf;
			cp[12] = cp[6] >> 4; cp[11] = cp[5] & 0xf;
			cp[10] = cp[5] >> 4; cp[ 9] = cp[4] & 0xf;
			cp[ 8] = cp[4] >> 4; cp[ 7] = cp[3] & 0xf;
			cp[ 6] = cp[3] >> 4; cp[ 5] = cp[2] & 0xf;
			cp[ 4] = cp[2] >> 4; cp[ 3] = cp[1] & 0xf;
			cp[ 2] = cp[1] >> 4; cp[ 1] = cp[0] & 0xf;
			cp[ 0] = cp[0] >> 4;
#if DEBUG_ISRC
	fprintf(stderr, "a->b: %15.15s\n", cp);
#endif
		}
      
		/* Check for format B:
		 * If not yet in ASCII format, do the conversion
		 */
		if (cp[0] < '0' && cp[1] < '0') {
			/* coding table for International Standard Recording Code */
			static char bin2ISRC[] = {
			 '0','1','2','3','4','5','6','7','8','9',      /* 10 */
			 ':',';','<','=','>','?','@',		       /* 17 */
			 'A','B','C','D','E','F','G','H','I','J','K',  /* 28 */
			 'L','M','N','O','P','Q','R','S','T','U','V',  /* 39 */
			 'W','X','Y','Z',			       /* 43 */
#if 1
			 '[','\\',']','^','_','`',		       /* 49 */
			 'a','b','c','d','e','f','g','h','i','j','k',  /* 60 */
			 'l','m','n','o'			       /* 64 */
#endif
			};
	
			/* build 6-bit vector of coded values */
			unsigned ind;
			int bits;
	
#if DEBUG_ISRC
	fprintf(stderr, "b!\n");
#endif
			ind =   (cp[0] << 26) +
				(cp[1] << 22) +
				(cp[2] << 18) + 
				(cp[3] << 14) +
				(cp[4] << 10) +
				(cp[5] << 6) +
				(cp[6] << 2) +
				(cp[7] >> 2);

			if ((cp[7] & 3) == 3) {
				if (global.verbose) {
					fprintf(stderr,
						"Recorder-ID encountered: ");
					for (bits = 0; bits < 30; bits +=6) {
						unsigned binval = (ind & (ULONG_C(0x3f) << (24-bits)))
											>> (24-bits);
						if ((binval < sizeof(bin2ISRC)) &&
						    (binval <= 9 || binval >= 16)) {
							fprintf(stderr, "%X", bin2ISRC[binval]);
						}
					}

					fprintf(stderr, "%.1X%.1X%.1X%.1X%.1X%.1X%.1X",
					    subq_tr->track_isrc [8] & 0x0f,
					    subq_tr->track_isrc [9] & 0x0f,
					    subq_tr->track_isrc [10] & 0x0f,
					    subq_tr->track_isrc [11] & 0x0f,
					    subq_tr->track_isrc [12] & 0x0f,
					    subq_tr->track_isrc [13] & 0x0f,
					    subq_tr->track_isrc [14] & 0x0f
					    );
					fprintf(stderr, "\n");
				}
				return;
			}
			if ((cp[7] & 3) != 0) {
				fprintf(stderr, "unknown mode 3 entry C1=0x%02x, C2=0x%02x\n",
					(cp[7] >> 1) & 1, cp[7] & 1);
				return;
			}
	  
			/* decode ISRC due to IEC 908 */
			for (bits = 0; bits < 30; bits +=6) {
				unsigned binval = (ind & ((unsigned long) 0x3fL << (24L-bits))) >> (24L-bits);
				if ((binval >= sizeof(bin2ISRC)) ||
				    (binval > 9 && binval < 16)) {
					/* Illegal ISRC, dump and skip */
					int y;
						    
					Get_ISRC(tr)[0] = '\0';
					fprintf(stderr, "\nIllegal ISRC for track %d, skipped: ", tr);
					for (y = 0; y < 15; y++) {
						fprintf(stderr, "%02x ", cp[y]);
					}
					fputs("\n", stderr);
					return;
				}
				*p++ = bin2ISRC[binval];
				
				/* insert a dash after two country characters for legibility */
				if (bits == 6)
					*p++ = '-';
			}
			
			/* format year and serial number */
			sprintf ((char *)p, "-%.1X%.1X-%.1X%.1X%.1X%.1X%.1X",
				 subq_tr->track_isrc [8] & 0x0f,
				 subq_tr->track_isrc [9] & 0x0f,
				 subq_tr->track_isrc [10] & 0x0f,
				 subq_tr->track_isrc [11] & 0x0f,
				 subq_tr->track_isrc [12] & 0x0f,
				 subq_tr->track_isrc [13] & 0x0f,
				 subq_tr->track_isrc [14] & 0x0f
				); 
#if DEBUG_ISRC
	fprintf(stderr, "b: %15.15s!\n", p_start);
#endif
		} else {
			/* It might be in ASCII, surprise */
			int ii;
			for (ii = 0; ii < 12; ii++) {
				if (cp[ii] < '0' || cp[ii] > 'Z') {
					break;
				}
			}
			if (ii != 12) {
				int y;
				
				Get_ISRC(ii)[0] = '\0';
				fprintf(stderr, "\nIllegal ISRC for track %d, skipped: ", ii+1);
				for (y = 0; y < 15; y++) {
					fprintf(stderr, "%02x ", cp[y]);
				}
				fputs("\n", stderr);
				return;
			}
			
#if DEBUG_ISRC
	fprintf(stderr, "ascii: %15.15s!\n", cp);
#endif
			for (ii = 0; ii < 12; ii++) {
#if 1
				if ((ii == 2 || ii == 5 || ii == 7) && cp[ii] != ' ')
					*p++ = '-';
#endif
				*p++ = cp[ii];
			}
			if (p - p_start >= 16)
				*(p_start + 15) = '\0';
			else
				*p = '\0';
		}

		if (memcmp(p_start,"00-000-00-00000",15) != 0) {
			Set_ISRC(tr, p_start);
		}
	}
}

/* get and display Media Catalog Number ( one per disc )
 *  and Track International Standard Recording Codes (for each track)
 */
void Read_MCN_ISRC(void)
{
	if ((global.verbose & SHOW_MCN) != 0) {

		if (Get_MCN()[0] == '\0') {
			Get_Set_MCN();
		}

		if (Get_MCN()[0] != '\0')
			fprintf(stderr, "\rMedia catalog number: %13.13s\n", Get_MCN());
		else
			fprintf(stderr, "\rNo media catalog number present.\n");
	}



	if ((global.verbose & SHOW_ISRC) != 0) {
		static struct iterator i;

		InitIterator(&i, 1);

		while (i.hasNextTrack(&i)) {
			struct TOC *p = i.getNextTrack(&i);
			unsigned ii = GETTRACK(p);
			
			if (ii == CDROM_LEADOUT) break;
			
			if (!IS__AUDIO(p))
				continue;

			if (GETISRC(p)[0] == '\0') {
				Get_Set_ISRC(ii);
			}

			if (GETISRC(p)[0] != '\0') {
				fprintf (stderr, "\rT: %2d ISRC: %15.15s\n", ii, GETISRC(p));
				fflush(stderr); 
			}
		} /* for all tracks */

		fputs("\n", stderr);
	} /* if SHOW_ISRC */
}

static int playing = 0;

static subq_chnl *ReadSubChannel(unsigned sec);

static subq_chnl *ReadSubChannel(unsigned sec)
{
	subq_chnl *sub_ch;

	/*
	 * For modern drives implement a direct method. If the drive supports
	 * reading of subchannel data, do direct reads.
	 */
	if (ReadSubChannels != NULL) {
		get_scsi_p()->silent++;
		sub_ch = ReadSubChannels(get_scsi_p(), sec);
		get_scsi_p()->silent--;
		if (sub_ch == NULL /*&& (usal_sense_key(get_scsi_p()) == 5)*/) {
			/* command is not implemented */
			ReadSubChannels = NULL;
#if	defined DEBUG_SUB
fprintf(stderr, "\nCommand not implemented: switching ReadSubChannels off !\n");
#endif
			goto fallback;
		}

		/* check the adress mode field */
		if ((sub_ch->control_adr & 0x0f) == 0) {
			/* no Q mode information present at all, weird */
			sub_ch->control_adr = 0xAA;
		}

		if ((int)(sub_ch->control_adr & 0x0f) > 0x01) {
			/* this sector just has no position information.
			 * we try the one before and then the one after.
			 */
			if (sec > 1) {
				sec -= 1;
				sub_ch = ReadSubChannels(get_scsi_p(), sec);
				if (sub_ch == NULL) return NULL;
				sec += 1;
			}
			if ((sub_ch->control_adr & 0x0f) != 0x01) {
				sec += 2;
				sub_ch = ReadSubChannels(get_scsi_p(), sec);
				if (sub_ch == NULL) return NULL;
				sec -= 2;
			}
		}

		/* check adress mode field for position information */
		if ((sub_ch->control_adr & 0x0f) == 0x01) {
			return sub_ch;
		}
		ReadSubChannels = NULL;
fprintf(stderr, "\nCould not get position information (%02x) for sectors %d, %d, %d: switching ReadSubChannels off !\n", sub_ch->control_adr &0x0f, sec-1, sec, sec+2);
	}

	/*
	 * We rely on audio sectors here!!!
	 * The only method that worked even with my antique Toshiba 3401,
	 * is playing the sector and then request the subchannel afterwards.
	 */
fallback:
	/* We need a conformed audio track here! */

	/* Fallback to ancient method */
	if (-1 == Play_at(get_scsi_p(), sec, 1)) {
		return NULL;
	}
	playing = 1;
	sub_ch = ReadSubQ(get_scsi_p(), GET_POSITIONDATA,0);
	return sub_ch;
}

static int ReadSubControl(unsigned sec);
static int ReadSubControl(unsigned sec)
{
	subq_chnl *sub_ch = ReadSubChannel(sec);
	if (sub_ch == NULL) return -1;

	return	sub_ch->control_adr & 0xf0;
}

static int HaveSCMS(unsigned StartSector);
static int HaveSCMS(unsigned StartSector)
{
	int i;
	int	cr;
	int	copy_bits_set = 0;

	for (i = 0; i < 8; i++) {
		cr = ReadSubControl(StartSector + i);
		if (cr == -1) continue;
		(cr & 0x20) ? copy_bits_set++ : 0;
	}
	return (copy_bits_set >= 1 && copy_bits_set < 8);
}

void Check_Toc(void)
{
	/* detect layout */
	
	/* detect tracks */
}

static int GetIndexOfSector(unsigned sec, unsigned track)
{
	subq_chnl *sub_ch = ReadSubChannel(sec);
	if (sub_ch == NULL) {
		if ((long)sec == Get_EndSector(track)) {
			fprintf(stderr, "Driver and/or firmware bug detected! Drive cannot play the very last sector (%u)!\n", sec);
		}
		return -1;
	}

	/* can we trust that these values are hex and NOT bcd? */
	if ((sub_ch->track >= 0x10) && (sub_ch->track - track > 5)) {
		/* change all values from bcd to hex */
		sub_ch->track = (sub_ch->track >> 4)*10 + (sub_ch->track & 0x0f);
		sub_ch->index = (sub_ch->index >> 4)*10 + (sub_ch->index & 0x0f);
	}

#if 1
    /* compare tracks */
    if (sub_ch->index != 0 && track != sub_ch->track) {
	if (global.verbose) fprintf(stderr, "\ntrack mismatch: %1d, in-track subchannel: %1d (index %1d, sector %1d)\n",
		track, sub_ch->track, sub_ch->index, sec);
    }
#endif

    /* compare control field with the one from the TOC */
    if ((Get_Flags(track) & 0xf0) != (sub_ch->control_adr & 0xf0)) {
	int	diffbits = (Get_Flags(track) & 0xf0) ^ (sub_ch->control_adr & 0xf0);
	if ((diffbits & 0x80) == 0x80) {
		/* broadcast difference */
		if (global.verbose) fprintf(stderr, "broadcast type conflict detected -> TOC:%s, subchannel:%s\n",
		(sub_ch->control_adr & 0x80) == 0 ? "broadcast" : "nonbroadcast"
		,(sub_ch->control_adr & 0x80) != 0 ? "broadcast" : "nonbroadcast"
		);
	}
	if ((diffbits & 0x40) == 0x40) {
		/* track type difference */
		if (global.verbose) fprintf(stderr, "track type conflict detected -> TOC:%s, subchannel:%s\n",
		(sub_ch->control_adr & 0x40) == 0 ? "data" : "audio"
		,(sub_ch->control_adr & 0x40) != 0 ? "data" : "audio"
		);
	}
	if ((diffbits & 0x20) == 0x20 && !Get_SCMS(track)) {
		/* copy permission difference is a sign for SCMS
		 * and is treated elsewhere. */
		if (global.verbose) fprintf(stderr, "difference: TOC:%s, subchannel:%s\ncorrecting TOC...\n",
		(sub_ch->control_adr & 0x20) == 0 ? "unprotected" : "copyright protected",
		(sub_ch->control_adr & 0x20) != 0 ? "unprotected" : "copyright protected"
		);
		toc_entry(track, 
		  (Get_Flags(track) & 0xDF) | (sub_ch->control_adr & 0x20),
		  Get_Tracknumber(track),
		  Get_ISRC(track),
		  Get_AudioStartSector(track),
		  Get_Mins(track),
		  Get_Secs(track),
		  Get_Frames(track)
		  );
	}
	if ((diffbits & 0x10) == 0x10) {
		/* preemphasis difference */
		if (global.verbose) fprintf(stderr, "difference: TOC:%s, subchannel:%s preemphasis\ncorrecting TOC...\n",
		(sub_ch->control_adr & 0x10) == 0 ? "with" : "without",
		(sub_ch->control_adr & 0x10) != 0 ? "with" : "without"
		);
		toc_entry(track, 
		  (Get_Flags(track) & 0xEF) | (sub_ch->control_adr & 0x10),
		  Get_Tracknumber(track),
		  Get_ISRC(track),
		  Get_AudioStartSector(track),
		  Get_Mins(track),
		  Get_Secs(track),
		  Get_Frames(track)
		  );
	}

    }

    return sub_ch ? sub_ch->index == 244 ? 1 : sub_ch->index : -1;
}

static int ScanBackwardFrom(unsigned sec, unsigned limit, int *where, 
									 unsigned track);

static int ScanBackwardFrom(unsigned sec, unsigned limit, int *where, 
									 unsigned track)
{
	unsigned lastindex = 0;
	unsigned mysec = sec;

	/* try to find the transition of index n to index 0,
	 * if the track ends with an index 0.
	 */
	while ((lastindex = GetIndexOfSector(mysec, track)) == 0) {
		if (mysec < limit+75) {
			break;
		}
		mysec -= 75;
	}
	if (mysec == sec) {
		/* there is no pre-gap in this track */
		if (where != NULL) *where = -1;
	} else {
		/* we have a pre-gap in this track */

		if (lastindex == 0) {
			/* we did not cross the transition yet -> search backward */
			do {
				if (mysec < limit+1) {
					break;
				}
				mysec --;
			} while ((lastindex = GetIndexOfSector(mysec,track)) == 0);
			if (lastindex != 0) {
				/* successful */
				mysec ++;
				/* register mysec as transition */
				if (where != NULL) *where = (int) mysec;
			} else {
				/* could not find transition */
				if (!global.quiet)
					fprintf(stderr,
					 "Could not find index transition for pre-gap.\n");
				if (where != NULL) *where = -1;
			}
		} else {
			int myindex = -1;
			/* we have crossed the transition -> search forward */
			do {
				if (mysec >= sec) {
					break;
				}
				mysec ++;
			} while ((myindex = GetIndexOfSector(mysec,track)) != 0);
			if (myindex == 0) {
				/* successful */
				/* register mysec as transition */
				if (where != NULL) *where = (int) mysec;
			} else {
				/* could not find transition */
				if (!global.quiet)
					fprintf(stderr,
					 "Could not find index transition for pre-gap.\n");
				if (where != NULL) *where = -1;
			}
		}
	}
	return lastindex;
}

#ifdef	USE_LINEAR_SEARCH
static int linear_search(int searchInd, unsigned int Start, unsigned int End, 
								 unsigned track);

static int linear_search(int searchInd, unsigned int Start, unsigned int End, 
								 unsigned track)
{
      int l = Start;
      int r = End;

      for (; l <= r; l++ ) {
          int ind;

	  ind = GetIndexOfSector(l, track);
	  if ( searchInd == ind ) {
	      break;
	  }
      }
      if ( l <= r ) {
        /* Index found. */
	return l;
      }

      return -1;
}
#endif

#ifndef	USE_LINEAR_SEARCH
#undef DEBUG_BINSEARCH
static int binary_search(int searchInd, unsigned int Start, unsigned int End, 
								 unsigned track);

static int binary_search(int searchInd, unsigned Start, unsigned End, 
								 unsigned track)
{
      int l = Start;
      int r = End;
      int x = 0;
      int ind;

      while ( l <= r ) {
	  x = ( l + r ) / 2;
	  /* try to avoid seeking */
	  ind = GetIndexOfSector(x, track);
	  if ( searchInd == ind ) {
	      break;
	  } else {
	      if ( searchInd < ind ) r = x - 1;
	      else	     	     l = x + 1;
	  }
      }
#ifdef DEBUG_BINSEARCH
fprintf(stderr, "(%d,%d,%d > ",l,x,r);
#endif
      if ( l <= r ) {
        /* Index found. Now find the first position of this index */
	/* l=LastPos	x=found		r=NextPos */
        r = x;
	while ( l < r-1 ) {
	  x = ( l + r ) / 2;
	  /* try to avoid seeking */
	  ind = GetIndexOfSector(x, track);
	  if ( searchInd == ind ) {
	      r = x;
	  } else {
	      l = x;
	  }
#ifdef DEBUG_BINSEARCH
fprintf(stderr, "%d -> ",x);
#endif
        }
#ifdef DEBUG_BINSEARCH
fprintf(stderr, "%d,%d)\n",l,r);
#endif
	if (searchInd == GetIndexOfSector(l, track))
	  return l;
	else
	  return r;
      }

      return -1;
}
#endif


static void register_index_position(int IndexOffset, 
												index_list **last_index_entry);

static void register_index_position(int IndexOffset, 
												index_list **last_index_entry)
{
      index_list *indexentry;

      /* register higher index entries */
      if (*last_index_entry != NULL) {
        indexentry = (index_list *) malloc( sizeof(index_list) );
      } else {
        indexentry = NULL;
      }
      if (indexentry != NULL) {
        indexentry->next = NULL;
        (*last_index_entry)->next = indexentry;
        *last_index_entry = indexentry;
        indexentry->frameoffset = IndexOffset;
#if defined INFOFILES
      } else {
        fprintf( stderr, "No memory for index lists. Index positions\nwill not be written in info file!\n");
#endif
      }
}

static void Set_SCMS(unsigned long p_track);

#undef DEBUG_INDLIST
/* experimental code */
/* search for indices (audio mode required) */
unsigned ScanIndices(unsigned track, unsigned cd_index, int bulk)
{
  /* scan for indices. */
  /* look at last sector of track. */
  /* when the index is not equal 1 scan by bipartition 
   * for offsets of all indices */

  unsigned starttrack, endtrack;
  unsigned startindex, endindex;

  unsigned j;
  int LastIndex=0;
  int n_0_transition;
  unsigned StartSector;
  unsigned retval = 0;

  index_list *baseindex_pool;
  index_list *last_index_entry;

  SCSI *usalp = get_scsi_p();

  static struct iterator i;
  InitIterator(&i, 1);
  
  EnableCdda(usalp, 0, 0);
  EnableCdda(usalp, 1, CD_FRAMESIZE_RAW + 16);

  if (!global.quiet && !(global.verbose & SHOW_INDICES))
    fprintf(stderr, "seeking index start ...");

  if (bulk != 1) {
    starttrack = track; endtrack = track;
  } else {
    starttrack = 1; endtrack = cdtracks;
  }
  baseindex_pool = (index_list *) malloc( sizeof(index_list) * (endtrack - starttrack + 1));
#ifdef DEBUG_INDLIST
  fprintf(stderr, "index0-mem-pool %p\n", baseindex_pool);
#endif


  while (i.hasNextTrack(&i)) {
	  struct TOC *p = i.getNextTrack(&i);
	  unsigned ii = GETTRACK(p);

	  if ( ii < starttrack || IS__DATA(p) )
		  continue;	/* skip nonaudio tracks */

	  if ( ii > endtrack )
		  break;

	  if ( global.verbose & SHOW_INDICES ) { 
		  if (global.illleadout_cd && global.reads_illleadout && ii == endtrack) {
			  fprintf(stderr, "Analysis of track %d skipped due to unknown length\n", ii);
		  }
	  }
	  if (global.illleadout_cd && global.reads_illleadout 
	      && ii == endtrack) continue;

	  StartSector = Get_AudioStartSector(ii);
	  if (HaveSCMS(StartSector)) {
		Set_SCMS(ii);
	  }
	  if ( global.verbose & SHOW_INDICES ) { 
		  fprintf( stderr, "\rindex scan: %d...", ii ); 
		  fflush (stderr);
	  }
	  LastIndex = ScanBackwardFrom(Get_EndSector(ii), StartSector, &n_0_transition, ii);
	  if (LastIndex > 99) continue;

	  if (baseindex_pool != NULL) {
#ifdef DEBUG_INDLIST
#endif
		  /* register first index entry for this track */
		  baseindex_pool[ii - starttrack].next = NULL;
		  baseindex_pool[ii - starttrack].frameoffset = StartSector;
		  global.trackindexlist[ii] = &baseindex_pool[ii - starttrack];
#ifdef DEBUG_INDLIST
#endif
	  } else {
		  global.trackindexlist[ii] = NULL;
	  }
	  last_index_entry = global.trackindexlist[ii];

	  if (LastIndex < 2) {
		  register_index_position(n_0_transition, &last_index_entry);
		  continue;
	  }

	  if ((global.verbose & SHOW_INDICES) && LastIndex > 1)
		  fprintf(stderr, "\rtrack %2d has %d indices, index table (pairs of 'index: frame offset')\n", ii, LastIndex);
	  
	  startindex = 0;
	  endindex = LastIndex;

	  for (j = startindex; j <= endindex; j++) {
		  int IndexOffset;
		  
		  /* this track has indices */

#ifdef	USE_LINEAR_SEARCH
		  /* do a linear search */
		  IndexOffset = linear_search(j, StartSector, Get_EndSector(ii), ii);
#else
		  /* do a binary search */
		  IndexOffset = binary_search(j, StartSector, Get_EndSector(ii), ii);
#endif

		  if (IndexOffset != -1) {
			  StartSector = IndexOffset;
		  }

		  if (j == 1)
			  last_index_entry->frameoffset = IndexOffset;
		  else if (j > 1)
			  register_index_position(IndexOffset, &last_index_entry);

		  if ( IndexOffset == -1 ) {
			  if (global.verbose & SHOW_INDICES) {
				  if (global.gui == 0) {
					  fprintf(stderr, "%2u: N/A   ",j);
					  if (((j + 1) % 8) == 0) fputs("\n", stderr);
				  } else {
					  fprintf(stderr, "\rT%02d I%02u N/A\n",ii,j);
				  }
			  }
		  } else {
			  if (global.verbose & SHOW_INDICES) {
				  if (global.gui == 0) {
					  fprintf(stderr, 
						"%2u:%6lu ",
						j,
						IndexOffset-Get_AudioStartSector(ii)
						 );
					  if (((j + 1) % 8) == 0) fputs("\n", stderr);
				  } else {
					  fprintf(stderr,
						"\rT%02d I%02u %06lu\n",
						ii,
						j,
						IndexOffset-Get_AudioStartSector(ii)
						 );
				  }
			  }

			  if (track == ii && cd_index == j) {
				  retval = IndexOffset-Get_AudioStartSector(ii);
			  }
		  } /* if IndexOffset */
	  } /* for index */
	  register_index_position(n_0_transition, &last_index_entry);

	  /* sanity check. clear all consecutive nonindex entries (frameoffset -1) from the end. */
	  {
	  	index_list *ip = global.trackindexlist[ii];
		index_list *iq = NULL;
		index_list *lastgood = iq;

		while (ip != NULL)
		{
			if (ip->frameoffset == -1)
			{
				/* no index available */
				if (lastgood == NULL)
				{
					/* if this is the first one in a sequence, store predecessor */
					lastgood = iq;
				}
			} else {
				/* this is a valid index, reset marker */
				lastgood = NULL;
			}

			iq = ip;
			ip = ip->next;
		}
		/* terminate chain at the last well defined entry. */
		if (lastgood != NULL)
			lastgood->next = NULL;
	  }

	  if (global.gui == 0 && (global.verbose & SHOW_INDICES)
	      && ii != endtrack)
		  fputs("\n", stderr);
  } /* for tracks */
  if (global.gui == 0 && (global.verbose & SHOW_INDICES))
	  fputs("\n", stderr);
  if (playing != 0) StopPlay(get_scsi_p());

  EnableCdda(usalp, 0, 0);
  EnableCdda(usalp, 1, CD_FRAMESIZE_RAW);

  return retval;
}

static unsigned char MCN[14];

static void Set_MCN(unsigned char *MCN_arg)
{
	memcpy(MCN, MCN_arg, 14);
	MCN[13] = '\0';
}

unsigned char *Get_MCN(void)
{
	return MCN;
}


static TOC g_toc [MAXTRK+1]; /* hidden track + 100 regular tracks */

/*#define IS_AUDIO(i) (!(g_toc[i].bFlags & 0x40))*/

int 
TOC_entries(unsigned tracks, unsigned char *a, unsigned char *b, int binvalid)
{
	int i;
	for (i = 1; i <= (int)tracks; i++) {
		unsigned char *p;
		unsigned long dwStartSector;

		if (binvalid) {
			p = a + 8*(i-1);

			g_toc[i].bFlags = p[1];
			g_toc[i].bTrack = p[2];
			g_toc[i].ISRC[0] = 0;
			dwStartSector = a_to_u_4_byte(p+4);
			g_toc[i].dwStartSector = dwStartSector;
			lba_2_msf((long)dwStartSector,
				  &g_toc[i].mins,
				  &g_toc[i].secs,
				  &g_toc[i].frms);
		} else {
			p = b + 8*(i-1);
			g_toc[i].bFlags = p[1];
			g_toc[i].bTrack = p[2];
			g_toc[i].ISRC[0] = 0;
			if ((int)((p[5]*60 + p[6])*75 + p[7]) >= 150) {
				g_toc[i].dwStartSector = (p[5]*60 + p[6])*75 + p[7] -150;
			} else {
				g_toc[i].dwStartSector = 0;
			}
			g_toc[i].mins = p[5];
			g_toc[i].secs = p[6];
			g_toc[i].frms = p[7];
		}
	}
	return 0;
}

void toc_entry(unsigned nr, unsigned flag, unsigned tr, unsigned char *ISRC, 
               unsigned long lba, int m, int s, int f)
{
	if (nr > MAXTRK) return;

	g_toc[nr].bFlags = flag;
	g_toc[nr].bTrack = tr;
	if (ISRC) {
		strncpy((char *)g_toc[nr].ISRC, (char *)ISRC,
			sizeof(g_toc[nr].ISRC) -1);
		g_toc[nr].ISRC[sizeof(g_toc[nr].ISRC) -1] = '\0';
	}
	g_toc[nr].dwStartSector = lba;
	g_toc[nr].mins = m;
	g_toc[nr].secs = s;
	g_toc[nr].frms = f;
}

int patch_real_end(unsigned long sector)
{
	g_toc[cdtracks+1].dwStartSector = sector;
	return 0;
}

static int patch_cd_extra(unsigned track, unsigned long sector)
{
	if (track <= cdtracks)
		g_toc[track].dwStartSector = sector;
	return 0;
}

static int restrict_tracks_illleadout(void)
{
	struct TOC *o = &g_toc[cdtracks+1];
	int i;
	for (i = cdtracks; i >= 0; i--) {
		struct TOC *p = &g_toc[i];
		if (GETSTART(o) > GETSTART(p)) break;
	}
	patch_cd_extra(i+1, GETSTART(o));
	cdtracks = i;

	return 0;
}

static void Set_ISRC(int track, const unsigned char *ISRC_arg)
{
	if (track <= (int)cdtracks) {
		memcpy(Get_ISRC(track), ISRC_arg, 16);
	}
}


unsigned char *Get_ISRC(unsigned long p_track)
{
	if (p_track <= cdtracks)
		return g_toc[p_track].ISRC;
	return NULL;
}

static void patch_to_audio(unsigned long p_track)
{
	if (p_track <= cdtracks)
		g_toc[p_track].bFlags &= ~0x40;
}

int Get_Flags(unsigned long p_track)
{
	if (p_track <= cdtracks)
		return g_toc[p_track].bFlags;
	return -1;
}

int Get_Mins(unsigned long p_track)
{
	if (p_track <= cdtracks)
		return g_toc[p_track].mins;
	return -1;
}

int Get_Secs(unsigned long p_track)
{
	if (p_track <= cdtracks)
		return g_toc[p_track].secs;
	return -1;
}

int Get_Frames(unsigned long p_track)
{
	if (p_track <= cdtracks)
		return g_toc[p_track].frms;
	return -1;
}

int Get_Preemphasis(unsigned long p_track)
{
	if (p_track <= cdtracks)
		return g_toc[p_track].bFlags & 0x10;
	return -1;
}

static void Set_SCMS(unsigned long p_track)
{
	g_toc[p_track].SCMS = 1;
}

int Get_SCMS(unsigned long p_track)
{
	if (p_track <= cdtracks)
		return g_toc[p_track].SCMS;
	return -1;
}

int Get_Copyright(unsigned long p_track)
{
	if (p_track <= cdtracks) {
		if (g_toc[p_track].SCMS) return 1;
		return ((int)g_toc[p_track].bFlags & 0x20) >> 4;
	}
	return -1;
}

int Get_Datatrack(unsigned long p_track)
{
	if (p_track <= cdtracks)
		return g_toc[p_track].bFlags & 0x40;
	return -1;
}

int Get_Channels(unsigned long p_track)
{
	if (p_track <= cdtracks)
		return g_toc[p_track].bFlags & 0x80;
	return -1;
}

int Get_Tracknumber(unsigned long p_track)
{
	if (p_track <= cdtracks)
		return g_toc[p_track].bTrack;
	return -1;
}

static int useHiddenTrack(void)
{
	return 0;
}



static void it_reset(struct iterator *this);

static void it_reset(struct iterator *this)
{
	this->index = this->startindex;
}


static int it_hasNextTrack(struct iterator *this);
static struct TOC *it_getNextTrack(struct iterator *this);

static int it_hasNextTrack(struct iterator *this)
{
	return this->index <= (int)cdtracks+1;
}



static struct TOC *it_getNextTrack(struct iterator *this)
{
	/* if ( (*this->hasNextTrack)(this) == 0 ) return NULL; */
	if ( this->index > (int)cdtracks+1 ) return NULL;

	return &g_toc[ this->index++ ];
}


static void InitIterator(struct iterator *iter, unsigned long p_track)
{
	if (iter == NULL) return;

	iter->index = iter->startindex = useHiddenTrack() ? 0 : p_track;
	iter->reset = it_reset;
	iter->getNextTrack = it_getNextTrack;
	iter->hasNextTrack = it_hasNextTrack;
}

#if	0
static struct iterator *NewIterator(void);

static struct iterator *NewIterator ()
{
	struct iterator *retval;

	retval = malloc (sizeof(struct iterator));
	if (retval != NULL) {
		InitIterator(retval, 1);
	}
	return retval;
}
#endif

long Get_AudioStartSector(unsigned long p_track)
{
#if	1
	if (p_track == CDROM_LEADOUT)
		p_track = cdtracks + 1;

	if (p_track <= cdtracks +1
		&& IS__AUDIO(&g_toc[p_track]))
		return GETSTART(&g_toc[p_track]);
#else
	static struct iterator i;
	if (i.reset == NULL) InitIterator(&i, p_track);
	else i.reset(&i);

	if (p_track == cdtracks + 1) p_track = CDROM_LEADOUT;

	while (i.hasNextTrack(&i)) {
		TOC *p = i.getNextTrack(&i);

		if (GETTRACK(p) == p_track) {
			if (IS__DATA(p)) {
				return -1;
			}
			return GETSTART(p);
		}
	}
#endif
	return -1;
}


long Get_StartSector(unsigned long p_track)
{
#if	1
	if (p_track == CDROM_LEADOUT)
		p_track = cdtracks + 1;

	if (p_track <= cdtracks +1)
		return GETSTART(&g_toc[p_track]);
#else
	static struct iterator i;
	if (i.reset == NULL) InitIterator(&i, p_track);
	else i.reset(&i);

	if (p_track == cdtracks + 1) p_track = CDROM_LEADOUT;

	while (i.hasNextTrack(&i)) {
		TOC *p = i.getNextTrack(&i);

		if (GETTRACK(p) == p_track) {
			return GETSTART(p);
		}
	}
#endif
	return -1;
}


long Get_EndSector(unsigned long p_track)
{
#if	1
	if (p_track <= cdtracks)
		return GETSTART(&g_toc[p_track+1])-1;
#else
	static struct iterator i;
	if (i.reset == NULL) InitIterator(&i, p_track);
	else i.reset(&i);

	if (p_track == cdtracks + 1) p_track = CDROM_LEADOUT;

	while (i.hasNextTrack(&i)) {
		TOC *p = i.getNextTrack(&i);
		if (GETTRACK(p) == p_track) {
			p = i.getNextTrack(&i);
			if (p == NULL) {
				return -1;
			}
			return GETSTART(p)-1;
		}
	}
#endif
	return -1;
}

long FirstTrack(void)
{
	static struct iterator i;
	if (i.reset == NULL) InitIterator(&i, 1);
	else i.reset(&i);

	if (i.hasNextTrack(&i)) {
		return GETTRACK(i.getNextTrack(&i));
	}
	return -1;
}

long FirstAudioTrack(void)
{
	static struct iterator i;
	if (i.reset == NULL) InitIterator(&i, 1);
	else i.reset(&i);

	while (i.hasNextTrack(&i)) {
		TOC *p = i.getNextTrack(&i);
		unsigned ii = GETTRACK(p);

		if (ii == CDROM_LEADOUT) break;
		if (IS__AUDIO(p)) {
			return ii;
		}
	}
	return -1;
}

long FirstDataTrack(void)
{
	static struct iterator i;
	if (i.reset == NULL) InitIterator(&i, 1);
	else i.reset(&i);

	while (i.hasNextTrack(&i)) {
		TOC *p = i.getNextTrack(&i);
		if (IS__DATA(p)) {
			return GETTRACK(p);
		}
	}
	return -1;
}

long LastTrack(void)
{
	return g_toc[cdtracks].bTrack;
}

long LastAudioTrack(void)
{
	long j = -1;
	static struct iterator i;
	if (i.reset == NULL) InitIterator(&i, 1);
	else i.reset(&i);

	while (i.hasNextTrack(&i)) {
		TOC *p = i.getNextTrack(&i);
		if (IS__AUDIO(p) && (GETTRACK(p) != CDROM_LEADOUT)) {
			j = GETTRACK(p);
		}
	}
	return j;
}

long Get_LastSectorOnCd(unsigned long p_track)
{
	long LastSec = 0;
	static struct iterator i;

	if (global.illleadout_cd && global.reads_illleadout)
		return 150+(99*60+59)*75+74;

	if (i.reset == NULL) InitIterator(&i, p_track);
	else i.reset(&i);

	if (p_track == cdtracks + 1) p_track = CDROM_LEADOUT;

	while (i.hasNextTrack(&i)) {
		TOC *p = i.getNextTrack(&i);

		if (GETTRACK(p) < p_track)
			continue;

		LastSec = GETSTART(p);

		if (IS__DATA(p)) break;
	}
	return LastSec;
}

int Get_Track(unsigned long sector)
{
	static struct iterator i;
	if (i.reset == NULL) InitIterator(&i, 1);
	else i.reset(&i);

	if (i.hasNextTrack(&i)) {
		TOC *o = i.getNextTrack(&i);
		while (i.hasNextTrack(&i)) {
			TOC *p = i.getNextTrack(&i);
			if ((GETSTART(o) <= sector) && (GETSTART(p) > sector)) {
				if (IS__DATA(o)) {
					return -1;
				} else {
					return GETTRACK(o);
				}
			}
			o = p;
		}
	}
	return -1;
}

int CheckTrackrange(unsigned long from, unsigned long upto)
{
	static struct iterator i;
	if (i.reset == NULL) InitIterator(&i, from);
	else i.reset(&i);

	while (i.hasNextTrack(&i)) {
		TOC *p = i.getNextTrack(&i);

		if (GETTRACK(p) < from)
			continue;

		if (GETTRACK(p) == upto)
			return 1;

		/* data tracks terminate the search */
		if (IS__DATA(p))
			return 0;
	}
	/* track not found */
	return 0;
}

#ifdef	USE_PARANOIA
long cdda_disc_firstsector(void *d);

long cdda_disc_firstsector(void *d)
{
	return Get_StartSector(FirstAudioTrack());
}

int cdda_tracks(void *d);

int cdda_tracks(void *d)
{
	return LastAudioTrack() - FirstAudioTrack() +1;
}

int cdda_track_audiop(void *d, int track);

int cdda_track_audiop(void *d, int track)
{
	return Get_Datatrack(track) == 0;
}

long cdda_track_firstsector(void *d, int track);

long cdda_track_firstsector(void *d, int track)
{
	return Get_AudioStartSector(track);
}

long cdda_track_lastsector(void *d, int track);

long cdda_track_lastsector(void *d, int track)
{
	return Get_EndSector(track);
}

long cdda_disc_lastsector(void *d);

long cdda_disc_lastsector(void *d)
{
	return Get_LastSectorOnCd(cdtracks) - 1;
}

int cdda_sector_gettrack(void *d,long sector);

int cdda_sector_gettrack(void *d, long sector)
{
	return Get_Track(sector);
}

#endif
