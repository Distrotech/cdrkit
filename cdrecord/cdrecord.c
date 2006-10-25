/* %Z%%M%	%I% %E% Copyright 1995 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"%Z%%M%	%I% %E% Copyright 1995 J. Schilling";
#endif
/*
 *	Record data on a CD/CVD-Recorder
 *
 *	Copyright (c) 1995 J. Schilling
 */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <mconfig.h>
#include <stdio.h>
#include <standard.h>
#include <stdxlib.h>
#include <fctldefs.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#ifdef	HAVE_SYS_RESOURCE_H
#include <sys/resource.h>	/* for rlimit */
#endif
#include <sys/stat.h>
#include <statdefs.h>
#include <unixstd.h>
#ifdef	HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#include <strdefs.h>
#include <waitdefs.h>
#include <utypes.h>
#include <signal.h>

#include <scg/scsireg.h>	/* XXX wegen SC_NOT_READY */
#include <scg/scsitransp.h>
#include <scg/scgcmd.h>		/* XXX fuer read_buffer */
#include "scsi_scan.h"

#include "auheader.h"
#include "cdrecord.h"

char	cdr_version[] = "1.8a28+cdrkit.1.0pre5+experimental";

/*
 * Map toc/track types into names.
 */
char	*toc2name[] = {
		"CD-DA",
		"CD-ROM",
		"CD-ROM XA mode 1",
		"CD-ROM XA mode 2",
		"CD-I",
		"Illegal toc type 5",
		"Illegal toc type 6",
		"Illegal toc type 7",
};

/*
 * Map sector types into names.
 */
char	*st2name[] = {
		"Illegal sector type 0",
		"CD-ROM mode 1",
		"CD-ROM mode 2",
		"Illegal sector type 3",
		"CD-DA without preemphasis",
		"CD-DA with preemphasis",
		"Illegal sector type 6",
		"Illegal sector type 7",
};

/*
 * Map data block types into names.
 */
char	*db2name[] = {
		"Raw (audio)",
		"Raw (audio) with P/Q sub channel",
		"Raw (audio) with P/W sub channel",
		"Raw (audio) with P/W raw sub channel",
		"Reserved mode 4",
		"Reserved mode 5",
		"Reserved mode 6",
		"Vendor unique mode 7",
		"CD-ROM mode 1",
		"CD-ROM mode 2",
		"CD-ROM XA mode 1",
		"CD-ROM XA mode 2 form 1",
		"CD-ROM XA mode 2 form 2",
		"CD-ROM XA mode 2 form 1/2/mix",
		"Reserved mode 14",
		"Vendor unique mode 15",
};

int		debug;		/* print debug messages */
static	int	scsi_verbose;	/* SCSI verbose flag */
int		lverbose;	/* local verbose flag */

/*
 * NOTICE:	You should not make BUF_SIZE more than
 *		the buffer size of the CD-Recorder.
 *
 * Do not set BUF_SIZE to be more than 126 KBytes
 * if you are running cdrecord on a sun4c machine.
 *
 * WARNING:	Philips CDD 521 dies if BUF_SIZE is to big.
 */
/*#define	BUF_SIZE	(126*1024)*/
/*#define	BUF_SIZE	(100*1024)*/
#define	BUF_SIZE	(63*1024)
/*#define	BUF_SIZE	(56*1024)*/

char	*buf;			/* The transfer buffer */
long	bufsize;		/* The size of the transfer buffer */
int	data_secs_per_tr;	/* # of data secs per transfer */
int	audio_secs_per_tr;	/* # of audio secs per transfer */

int	didintr;

struct timeval	starttime;
struct timeval	stoptime;
struct timeval	fixtime;

static	long	fs = -1L;	/* fifo (ring buffer) size */

extern	int 	main		(int ac, char **av);
static	void	usage		(int);
static	void	blusage		(int);
static	void	intr		(int sig);
extern	int	read_buf	(int f, char *bp, int size);
extern	int	get_buf		(int f, char **bpp, int size);
static	int	write_track_data (SCSI *scgp, cdr_t *, int , track_t *);
extern	int	pad_track	(SCSI *scgp, cdr_t *dp, int track, track_t *trackp,
				     long startsec, long amt,
				     BOOL dolast, long *bytesp);
extern	int	write_buf	(SCSI *scgp, cdr_t *dp, int track, track_t *trackp,
				     char *bp, long startsec, long amt, int secsize,
				     BOOL dolast, long *bytesp);
static	void	printdata	(int, track_t *);
static	void	printaudio	(int, track_t *);
static	void	checkfile	(int, track_t *);
static	int	checkfiles	(int, track_t *);
static	void	setpregaps	(int, track_t *);
static	long	checktsize	(int, track_t *);
static	void	checksize	(track_t *);
static	BOOL	checkdsize	(SCSI *scgp, cdr_t *dp, dstat_t *dsp, long tsize);
static	void	raise_fdlim	(void);
static	void	gargs		(int, char **, int *, track_t *, char **,
					int *, cdr_t **,
					int *, long *, int *, int *);
static	void	set_trsizes	(cdr_t *, int, track_t *);
extern	void	load_media	(SCSI *scgp, cdr_t *);
extern	void	unload_media	(SCSI *scgp, cdr_t *, int);
extern	void	set_secsize	(SCSI *scgp, int secsize);
static	void	check_recovery	(SCSI *scgp, cdr_t *, int);
	void	audioread	(SCSI *scgp, cdr_t *, int);
static	void	print_msinfo	(SCSI *scgp, cdr_t *);
static	void	print_toc	(SCSI *scgp, cdr_t *);
static	void	print_track	(int, long, struct msf *, int, int, int);
static	void	prtimediff	(const char *fmt,
					struct timeval *start,
					struct timeval *stop);
#if !defined(HAVE_SYS_PRIOCNTL_H);
static	int	rt_raisepri	(int);
#endif
extern	void	raisepri	(int);
static	char *	astoll		(const char *s, Llong *ll);
static	Llong	number		(char* arg, int* retp);
extern	int	getnum		(char* arg, long* valp);
extern	int	getllnum	(char *arg, Llong* lvalp);
static	int	getbltype	(char* optstr, long *typep);

/* provided by libschily and others nowadays */

static char * astoll(register const char *s, Llong *ll);

extern int 
main(ac, av)
	int	ac;
	char	*av[];
{
	char	*dev = NULL;
	int	timeout = 40;	/* Set default timeout to 40s CW-7502 is slow*/
	int	speed = 1;
	long	flags = 0L;
	int	toctype = -1;
	int	blanktype = 0;
	int	i;
	int	tracks = 0;
	int	trackno;
	long	tsize;
	track_t	track[MAX_TRACK+2];	/* Max tracks + track 0 + track AA */
	cdr_t	*dp = (cdr_t *)0;
	dstat_t	ds;
	long	startsec = 0L;
	int	old_secsize = -1;
	int	errs = 0;
	SCSI	*scgp;
	char	errstr[80];

#ifdef __EMX__ 
	/* This gives wildcard expansion with Non-Posix shells with EMX */ 
	_wildcard(&ac, &av); 
#endif 
	save_args(ac, av);

	fillbytes(track, sizeof(track), '\0');
	raise_fdlim();
	gargs(ac, av, &tracks, track, &dev, &timeout, &dp, &speed, &flags,
							&toctype, &blanktype);
	if (toctype < 0)
		comerrno(EX_BAD, "Internal error: Bad TOC type.\n");
	if ((flags & F_MSINFO) == 0 || lverbose || flags & F_VERSION)
		printf("Cdrecord release %s Copyright (C) 1995-1999 Jörg Schilling\n",
								cdr_version);
	if (flags & F_VERSION)
		exit(0);


	if (debug || lverbose) {
		printf("TOC Type: %d = %s\n",
			toctype, toc2name[toctype & TOC_MASK]);
	}

	if ((flags & (F_MSINFO|F_TOC|F_PRATIP|F_FIX|F_VERSION|F_CHECKDRIVE|F_INQUIRY|F_SCANBUS|F_RESET)) == 0) {
		/*
		 * Try to lock us im memory (will only work for root)
		 * but you need access to root anyway to use /dev/scg?
		 */
#if defined(HAVE_MLOCKALL) || defined(_POSIX_MEMLOCK)
		if (mlockall(MCL_CURRENT|MCL_FUTURE) < 0)
			comerr("Cannot do mlockall(2).\n");
#endif

		raisepri(0); /* max priority */
		init_fifo(fs);
	}

	if ((scgp = open_scsi(dev, errstr, sizeof(errstr),
				debug, (flags & F_MSINFO) == 0 || lverbose)) == (SCSI *)0) {
			comerr("%s%sCannot open SCSI driver.\n", errstr, errstr[0]?". ":"");
	}
	scsi_settimeout(scgp, timeout);
	scgp->verbose = scsi_verbose;
	scgp->debug = debug;
	scgp->cap->c_bsize = 2048;

	bufsize = scsi_bufsize(scgp, BUF_SIZE);
	if ((buf = scsi_getbuf(scgp, bufsize)) == NULL)
		comerr("Cannot get SCSI I/O buffer.\n");

	if ((flags & F_SCANBUS) != 0) {
		select_target(scgp);
		exit(0);
	}
	if ((flags & F_RESET) != 0) {
		scsireset(scgp);
		exit(0);
	}
	/*
	 * First try to check which type of SCSI device we
	 * have.
	 */
	if (debug || lverbose)
		printf("atapi: %d\n", scsi_isatapi(scgp));
	scgp->silent++;
	test_unit_ready(scgp);	/* eat up unit attention */
	scgp->silent--;
	if (!do_inquiry(scgp, (flags & F_MSINFO) == 0 || lverbose)) {
		errmsgno(EX_BAD, "Cannot do inquiry for CD/DVD-Recorder.\n");
		if (unit_ready(scgp))
			errmsgno(EX_BAD, "The unit seems to be hung and needs power cycling.\n");
		exit(EX_BAD);
	}

	if ((flags & F_PRCAP) != 0) {
		print_capabilities(scgp);
		exit(0);
	}
	if ((flags & F_INQUIRY) != 0)
		exit(0);

	if (dp == (cdr_t *)NULL) {	/* No driver= option specified */
		dp = get_cdrcmds(scgp);
	} else if (!is_unknown_dev(scgp) && dp != get_cdrcmds(scgp)) {
		errmsgno(EX_BAD, "WARNING: Trying to use other driver on known device.\n");
	}

	if (!is_cddrive(scgp))
		comerrno(EX_BAD, "Sorry, no CD/DVD-Drive found on this target.\n");
	if (dp == (cdr_t *)0)
		comerrno(EX_BAD, "Sorry, no supported CD/DVD-Recorder found on this target.\n");

	if (((flags & (F_MSINFO|F_TOC|F_LOAD|F_EJECT)) == 0 || tracks > 0) &&
					(dp->cdr_flags & CDR_ISREADER) != 0) {
		comerrno(EX_BAD,
		"Sorry, no CD/DVD-Recorder or unsupported CD/DVD-Recorder found on this target.\n");
	}

	if ((*dp->cdr_attach)(scgp, dp) != 0)
		comerrno(EX_BAD, "Cannot attach driver for CD/DVD-Recorder.\n");

	if ((flags & F_MSINFO) == 0 || lverbose) {
		printf("Using %s (%s).\n", dp->cdr_drtext, dp->cdr_drname);
		printf("Driver flags   : ");
		if ((dp->cdr_flags & CDR_SWABAUDIO) != 0)
			printf("SWABAUDIO");
		printf("\n");
	}
	scgp->silent++;
	if ((debug || lverbose)) {
		tsize = -1;
		if ((*dp->cdr_buffer_cap)(scgp, &tsize, (long *)0) < 0 || tsize <= 0) {
			if (read_buffer(scgp, buf, 4, 0) >= 0)
				tsize = a_to_u_long(buf);
		}
		if (tsize > 0) {
			printf("Drive buf size : %lu = %lu KB\n",
						tsize, tsize >> 10);
		}
	}
	scgp->silent--;

	if (tracks > 0 && (debug || lverbose))
		printf("FIFO size      : %lu = %lu KB\n", fs, fs >> 10);

	if ((flags & F_CHECKDRIVE) != 0)
		exit(0);

	if (tracks == 0 && (flags & (F_FIX|F_BLANK)) == 0 && (flags & F_EJECT) != 0) {
		/*
		 * Do not check if the unit is ready here to allow to open
		 * an empty unit too.
		 */
		unload_media(scgp, dp, flags);
		exit(0);
	}

	flush();
	if (tracks > 1)
		sleep(2);	/* Let the user watch the inquiry messages */

	set_trsizes(dp, tracks, track);
	setpregaps(tracks, track);
	checkfiles(tracks, track);
	tsize = checktsize(tracks, track);
do_cue(tracks, track, 0);

	/*
	 * Is this the right place to do this ?
	 */
	check_recovery(scgp, dp, flags);

	if ((flags & F_FORCE) == 0)
		load_media(scgp, dp);

	if ((flags & F_LOAD) != 0) {
		scsi_prevent_removal(scgp, 0);	/* allow manual open */
		exit(0);
	}
	old_secsize = sense_secsize(scgp, 1);
	if (old_secsize < 0)
		old_secsize = sense_secsize(scgp, 0);
	if (lverbose)
		printf("Current Secsize: %d\n", old_secsize);

/*audioread(dp, flags);*/
/*unload_media(scgp, dp, flags);*/
/*return 0;*/
	fillbytes(&ds, sizeof(ds), '\0');
	if (flags & F_WRITE)
		ds.ds_cdrflags = RF_WRITE;
	if (flags & F_PRATIP) {
		lverbose++;			/* XXX Hack */
	}
	if ((*dp->cdr_getdisktype)(scgp, dp, &ds) < 0) {
		errmsgno(EX_BAD, "Cannot get disk type.\n");
		if ((flags & F_FORCE) == 0)
			exit(EX_BAD);
	}
	if (flags & F_PRATIP) {
		lverbose--;			/* XXX Hack */
		scsi_prevent_removal(scgp, 0);
		exit(0);
	}
	/*
	 * The next actions should depend on the disk type.
	 */

	if (flags & F_MSINFO) {
		/*
		 * Dirty hack!
		 * At least MMC drives will not return the next writable
		 * address we expect when the drive's write mode is set
		 * tp DAO/SAO. We need this address for mkisofs and thus
		 * it must be the first user accessible sector and not the
		 * first sector of the pregap. Set_speed_dummy() witha a
		 * 'speedp' f 0 sets the write mode to TAO on MMC drives.
		 */
		scgp->silent++;
		(*dp->cdr_set_speed_dummy)(scgp, 0, TRUE);
		scgp->silent--;
		print_msinfo(scgp, dp);
		scsi_prevent_removal(scgp, 0);
		exit(0);
	}
	if (flags & F_TOC) {
		print_toc(scgp, dp);
		scsi_prevent_removal(scgp, 0);
		exit(0);
	}

#ifdef	XXX
	if ((*dp->cdr_check_session)() < 0) {
		set_secsize(scgp, old_secsize);
		unload_media(scgp, dp, flags);
		exit(EX_BAD);
	}
#endif
	if (tsize == 0) {
		if (tracks > 0) {
			errmsgno(EX_BAD,
			"WARNING: Track size unknown. Data may not fit on disk.\n");
		}
	} else if (!checkdsize(scgp, dp, &ds, tsize) && (flags & (F_IGNSIZE|F_FORCE)) == 0) {
		set_secsize(scgp, old_secsize);
		unload_media(scgp, dp, flags);
		exit(EX_BAD);
	}
	if (tracks > 0 && fs > 0l) {
		/*
		 * Start the extra process needed for improved buffering.
		 */
		if (!init_faio(tracks, track, bufsize))
			fs = 0L;
	}
	if (old_secsize > 0 && old_secsize != 2048) {
		/*
		 * Some drives (e.g. Plextor) don't like to write correctly
		 * in DAO mode if the sector size is set to 512 bytes.
		 */
		set_secsize(scgp, 2048);
	}
	if ((*dp->cdr_set_speed_dummy)(scgp, &speed, flags & F_DUMMY) < 0) {
		errmsgno(EX_BAD, "Cannot set speed/dummy.\n");
		set_secsize(scgp, old_secsize);
		unload_media(scgp, dp, flags);
		exit(EX_BAD);
	}
	if ((flags & (F_BLANK|F_FORCE)) == (F_BLANK|F_FORCE)) {
		wait_unit_ready(scgp, 120);
		scsi_blank(scgp, 0L, blanktype);
		exit(0);
	}
	/*
	 * Last chance to quit!
	 */
	printf("Starting to write CD/DVD at speed %d in %s mode for %s session.\n",
		speed,
		(flags & F_DUMMY) ? "dummy" : "write",
		(flags & F_MULTI) ? "multi" : "single");
	printf("Last chance to quit, starting %s write in 9 seconds.",
		(flags & F_DUMMY)?"dummy":"real");
	flush();
	signal(SIGINT, intr);
	for (i=9; --i > 0;) {
		sleep(1);
		if (didintr) {
			printf("\n");
			goto restore_it;
		}
		printf("\b\b\b\b\b\b\b\b\b\b%d seconds.", i);
		flush();
	}
	signal(SIGINT, SIG_DFL);
	printf("\n");
	if (tracks > 0 && fs > 0l) {
		/*
		 * Wait for the read-buffer to become full.
		 * This should be take no extra time if the input is a file.
		 * If the input is a pipe (e.g. mkisofs) this can take a
		 * while. If mkisofs dumps core before it starts writing,
		 * we abort before the writing process started.
		 */
		if (!await_faio()) {
			errmsgno(EX_BAD, "Input buffer error, aborting.\n");
			set_secsize(scgp, old_secsize);
			unload_media(scgp, dp, flags);
			exit(EX_BAD);
		}
	}
	if (gettimeofday(&starttime, (struct timezone *)0) < 0)
		errmsg("Cannot get start time\n");

	/*
	 * Blank the media if we were requested to do so
	 */
	if (flags & F_BLANK) {
		if ((*dp->cdr_blank)(scgp, 0L, blanktype) < 0) {
			errmsgno(EX_BAD, "Cannot blank disk, aborting.\n");
			set_secsize(scgp, old_secsize);
			unload_media(scgp, dp, flags);
			exit(EX_BAD);
		}
		if (gettimeofday(&fixtime, (struct timezone *)0) < 0)
			errmsg("Cannot get blank time\n");
		if (lverbose)
			prtimediff("Blanking time: ", &starttime, &fixtime);

		if (!wait_unit_ready(scgp, 60) || tracks == 0) {
			set_secsize(scgp, old_secsize);
			unload_media(scgp, dp, flags);
			exit(0);
		}
		/*
		 * Reset start time so we will not see blanking time and
		 * writing time counted together.
		 */
		if (gettimeofday(&starttime, (struct timezone *)0) < 0)
			errmsg("Cannot get start time\n");
	}
	/*
	 * Get the number of the next recordable track.
	 */
	scgp->silent++;
	if (read_tochdr(scgp, dp, NULL, &trackno) < 0) {
		trackno = 0;
	}
	scgp->silent--;
	for (i = 1; i <= tracks; i++) {
		track[i].trackno = i + trackno;
	}
	trackno++;
	track[0].trackno = trackno;	/* XXX Hack for TEAC fixate */

	/*
	 * Now we actually start writing to the CD/DVD.
	 * XXX Check total size of the tracks and remaining size of disk.
	 */
	if ((*dp->cdr_open_session)(scgp, tracks, track, toctype, flags & F_MULTI) < 0) {
		errmsgno(EX_BAD, "Cannot open new session.\n");
		set_secsize(scgp, old_secsize);
		unload_media(scgp, dp, flags);
		exit(EX_BAD);
	}

	/*
	 * As long as open_session() will do nothing but
	 * set up parameters, we may leave fix_it here.
	 * I case we have to add an open_session() for a drive
	 * that wants to do something that modifies the disk
	 * We have to think about a new solution.
	 */
	if (flags & F_FIX)
		goto fix_it;

	if (flags & F_SAO) {
		if (debug || lverbose) {
			printf("Sending CUE sheet...\n");
			flush();
		}
		if ((*dp->cdr_send_cue)(scgp, tracks, track) < 0) {
			errmsgno(EX_BAD, "Cannot send CUE sheet.\n");
			set_secsize(scgp, old_secsize);
			unload_media(scgp, dp, flags);
			exit(EX_BAD);
		}
	}
	if ((flags & F_SAO)) {
		(*dp->cdr_next_wr_address)(scgp, 0, &track[0], &startsec);
		if (startsec <= 0 && startsec != -150) {
			errmsgno(EX_BAD, "WARNING: Drive returns wrong startsec (%d) using -150\n",
					startsec);
			startsec = -150;
		}

		if (debug)
			printf("SAO startsec: %ld\n", startsec);

		for (i = 1; i <= tracks; i++) {
			track[i].trackstart += startsec +150;
		}
#ifdef	XXX
		if (debug || lverbose)
			printf("Writing lead-in...\n");

		pad_track(scgp, dp, 1, &track[1], -150, 0,
					FALSE, 0);
#endif
	}
	/*
	 * Need to set trackno to the real value from
	 * the current disk status.
	 */
	for (i = 1; i <= tracks; i++, trackno++) {
		startsec = 0L;

		/*
		 * trackno is the "real" track number while 'i' is a counter
		 * going from 1 to tracks.
		 */
		if ((*dp->cdr_open_track)(scgp, dp, trackno, &track[i]) < 0) {
			errs++;
			break;
		}

		if ((flags & F_SAO) == 0) {
			if ((*dp->cdr_next_wr_address)(scgp, trackno, &track[i], &startsec) < 0) {
				errs++;
				break;
			}
			track[i].trackstart = startsec;
		}
		if (debug || lverbose) {
			printf("Starting new track at sector: %ld\n",
						track[i].trackstart);
			flush();
		}
		if (write_track_data(scgp, dp, trackno, &track[i]) < 0) {
			errs++;
			sleep(5);
			request_sense(scgp);
			(*dp->cdr_close_track)(scgp, trackno, &track[i]);
			break;
		}
		if ((*dp->cdr_close_track)(scgp, trackno, &track[i]) < 0) {
			/*
			 * Check for "Dummy blocks added" message first.
			 */
			if (scsi_sense_key(scgp) != SC_ILLEGAL_REQUEST ||
					scsi_sense_code(scgp) != 0xB5) {
				errs++;
				break;
			}
		}
	}
fix_it:
	if (gettimeofday(&stoptime, (struct timezone *)0) < 0)
		errmsg("Cannot get stop time\n");
	if (lverbose)
		prtimediff("Writing  time: ", &starttime, &stoptime);

	if ((flags & F_NOFIX) == 0) {
		if (lverbose) {
			printf("Fixating...\n");
			flush();
		}
		if ((*dp->cdr_fixate)(scgp, flags & F_MULTI, flags & F_DUMMY,
				toctype, tracks, track) < 0)
			errs++;

		if (gettimeofday(&fixtime, (struct timezone *)0) < 0)
			errmsg("Cannot get fix time\n");
		if (lverbose)
			prtimediff("Fixating time: ", &stoptime, &fixtime);
	}

restore_it:
	/*
	 * Try to restore the old sector size.
	 */
	set_secsize(scgp, old_secsize);
	unload_media(scgp, dp, flags);

#ifdef	FIFO
	if (tracks > 0 && fs > 0L) {
		kill_faio();
		wait(0);
		if (debug || lverbose)
			fifo_stats();
	}
#endif

	exit(errs?-2:0);
	return (0);
}

static void 
usage(int excode)
{
	errmsgno(EX_BAD, "Usage: %s [options] track1...trackn\n",
		get_progname());

	schily_error("Options:\n");
	schily_error("\t-version	print version information and exit\n");
	schily_error("\t-v		increment general verbose level by one\n");
	schily_error("\t-V		increment SCSI command transport verbose level by one\n");
	schily_error("\t-debug		print additional debug messages\n");
	schily_error("\tdev=target	SCSI target to use as CD/DVD-Recorder\n");
	schily_error("\ttimeout=#	set the default SCSI command timeout to #.\n");
	schily_error("\tdriver=name	user supplied driver name, use with extreme care\n");
	schily_error("\t-checkdrive	check if a driver for the drive is present\n");
	schily_error("\t-prcap		print drive capabilities for MMC compliant drives\n");
	schily_error("\t-inq		do an inquiry for the drive end exit\n");
	schily_error("\t-scanbus	scan the SCSI bus end exit\n");
	schily_error("\t-reset		reset the SCSI bus with the cdrecorder (if possible)\n");
	schily_error("\t-ignsize	ignore the known size of a medium (may cause problems)\n");
	schily_error("\t-useinfo	use *.inf files to overwrite audio options.\n");
	schily_error("\tspeed=#		set speed of drive\n");
	schily_error("\tblank=type	blank a CD-RW disc (see blank=help)\n");
#ifdef	FIFO
	schily_error("\tfs=#		Set fifo size to # (0 to disable, default is %ld MB)\n",
							DEFAULT_FIFOSIZE/(1024*1024));
#endif
	schily_error("\t-load		load the disk and exit (works only with tray loader)\n");
	schily_error("\t-eject		eject the disk after doing the work\n");
	schily_error("\t-dummy		do everything with laser turned off\n");
	schily_error("\t-msinfo		retrieve multi-session info for mkisofs >= 1.10\n");
	schily_error("\t-toc		retrieve and print TOC/PMA data\n");
	schily_error("\t-atip		retrieve and print ATIP data\n");
	schily_error("\t-multi		generate a TOC that allows multi session\n");
	schily_error("\t		In this case default track type is CD-ROM XA2\n");
	schily_error("\t-fix		fixate a corrupt or unfixated disk (generate a TOC)\n");
	schily_error("\t-nofix		do not fixate disk after writing tracks\n");
	schily_error("\t-force		force to continue on some errors to allow blanking bad disks\n");
	schily_error("\t-dao		Write disk in DAO mode. This option will go away in the future.\n");
	schily_error("\ttsize=#		Length of valid data in next track\n");
	schily_error("\tpadsize=#	Amount of padding for next track\n");
	schily_error("\tpregap=#	Amount of pre-gap sectors before next track\n");
	schily_error("\tdefpregap=#	Amount of pre-gap sectors for all but track #1\n");
	schily_error("\tmcn=text	Set the media catalog number for this CD to 'text'\n");
	schily_error("\tisrc=text	Set the ISRC number for the next track to 'text'\n");
	schily_error("\tindex=list	Set the index list for the next track to 'list'\n");

	schily_error("\t-audio		Subsequent tracks are CD-DA audio tracks\n");
	schily_error("\t-data		Subsequent tracks are CD-ROM data mode 1 (default)\n");
	schily_error("\t-mode2		Subsequent tracks are CD-ROM data mode 2\n");
	schily_error("\t-xa1		Subsequent tracks are CD-ROM XA mode 1\n");
	schily_error("\t-xa2		Subsequent tracks are CD-ROM XA mode 2\n");
	schily_error("\t-cdi		Subsequent tracks are CDI tracks\n");
	schily_error("\t-isosize	Use iso9660 file system size for next data track\n");
	schily_error("\t-preemp		Audio tracks are mastered with 50/15 µs preemphasis\n");
	schily_error("\t-nopreemp	Audio tracks are mastered with no preemphasis (default)\n");
	schily_error("\t-pad		Pad data tracks with %d zeroed sectors\n", PAD_SECS);
	schily_error("\t		Pad audio tracks to a multiple of %d bytes\n", AUDIO_SEC_SIZE);
	schily_error("\t-nopad		Do not pad data tracks (default)\n");
	schily_error("\t-swab		Audio data source is byte-swapped (little-endian/Intel)\n");
	schily_error("The type of the first track is used for the toc type.\n");
	schily_error("Currently only form 1 tracks are supported.\n");
	exit(excode);
}

static void blusage(int	ret) 
{
	schily_error("Blanking options:\n");
	schily_error("\tall\t\tblank the entire disk\n");
	schily_error("\tdisc\t\tblank the entire disk\n");
	schily_error("\tdisk\t\tblank the entire disk\n");
	schily_error("\tfast\t\tminimally blank the entire disk (PMA, TOC, pregap)\n");
	schily_error("\tminimal\t\tminimally blank the entire disk (PMA, TOC, pregap)\n");
	schily_error("\ttrack\t\tblank a track\n");
	schily_error("\tunreserve\tunreserve a track\n");
	schily_error("\ttrtail\t\tblank a track tail\n");
	schily_error("\tunclose\t\tunclose last session\n");
	schily_error("\tsession\t\tblank last session\n");

	exit(ret);
	/* NOTREACHED */
}

static void intr(int sig) {
	sig = 0;	/* Fake usage */

	signal(SIGINT, intr);

	didintr++;
}

extern int
read_buf(f, bp, size)
	int	f;
	char	*bp;
	int	size;
{
	char	*p = bp;
	int	amount = 0;
	int	n;

	do {
		do {
			n = read(f, p, size-amount);
		} while (n < 0 && (errno == EAGAIN || errno == EINTR));
		if (n < 0)
			return (n);
		amount += n;
		p += n;

	} while (amount < size && n > 0);
	return (amount);
}

extern	int	get_buf		(int f, char **bpp, int size) {
	if (fs > 0) {
		/*		return (faio_read_buf(f, *bpp, size));*/
		return (faio_get_buf(f, bpp, size));
	} else {
		return (read_buf(f, *bpp, size));
	}
}

static int write_track_data(SCSI	*scgp, cdr_t	*dp, int	track, track_t	*trackp) {
	int	f;
	int	isaudio;
	long	startsec;
	long	bytes_read = 0;
	long	bytes	= 0;
	long	savbytes = 0;
	int	count;
	long	tracksize;
	int	secsize;
	int	secspt;
	int	bytespt;
	int	bytes_to_read;
	long	amount;
	int	pad;
	int	bswab;
	BOOL	neednl	= FALSE;
	BOOL	islast	= FALSE;
	char	*bp	= buf;
long bsize;
long bfree;
#ifdef	BCAP
int per;
int oper = -1;
#endif

	scgp->silent++;
	if (read_buff_cap(scgp, &bsize, &bfree) < 0)
		bsize = -1;
	scgp->silent--;


	if (is_packet(trackp))	/* XXX Ugly hack for now */
		return (write_packet_data(scgp, dp, track, trackp));

	f = trackp->f;
	isaudio = is_audio(trackp);
	tracksize = trackp->tracksize;
	startsec = trackp->trackstart;

	secsize = trackp->secsize;
	secspt = trackp->secspt;
	bytespt = secsize * secspt;
	
	pad = !isaudio && is_pad(trackp);	/* Pad only data tracks */
	bswab = isaudio && is_swab(trackp);	/* Swab only audio tracks */

	if (debug) {
		printf("secsize:%d secspt:%d bytespt:%d audio:%d pad:%d\n",
			secsize, secspt, bytespt, isaudio, pad);
	}

	if (lverbose) {
		if (tracksize > 0)
			printf("\rTrack %02d:   0 of %3ld MB written.",
			       track, tracksize >> 20);
		else
			printf("\rTrack %02d:   0 MB written.", track);
		flush();
		neednl = TRUE;
	}

	do {
		bytes_to_read = bytespt;
		if (tracksize > 0) {
			bytes_to_read = tracksize - bytes_read;
			if (bytes_to_read > bytespt)
				bytes_to_read = bytespt;
		}
		count = get_buf(f, &bp, bytes_to_read);

		if (count < 0)
			comerr("read error on input file\n");
		if (count == 0)
			break;
		bytes_read += count;
		if (tracksize >= 0 && bytes_read >= tracksize) {
			count -= bytes_read - tracksize;
			if (trackp->padsize == 0 && (bytes_read/secsize) >= 300)
				islast = TRUE;
		}

		if (bswab)
			swabbytes(bp, count);

		if (count < bytespt) {
			if (debug) {
				printf("\nNOTICE: reducing block size for last record.\n");
				neednl = FALSE;
			}

			if ((amount = count % secsize) != 0) {
				amount = secsize - amount;
				fillbytes(&bp[count], amount, '\0');
				count += amount;
				printf("\nWARNING: padding up to secsize.\n");
				neednl = FALSE;
			}
			bytespt = count;
			secspt = count / secsize;
			if (trackp->padsize == 0 && (bytes_read/secsize) >= 300)
				islast = TRUE;
		}

		amount = (*dp->cdr_write_trackdata)(scgp, bp, startsec, bytespt, secspt, islast);
		if (amount < 0) {
			printf("%swrite track data: error after %ld bytes\n",
							neednl?"\n":"", bytes);
			return (-1);
		}
		bytes += amount;
		startsec += amount / secsize;

		if (lverbose && (bytes >= (savbytes + 0x100000))) {
			int	fper;

			printf("\rTrack %02d: %3ld", track, bytes >> 20);
			if (tracksize > 0)
				printf(" of %3ld MB", tracksize >> 20);
			else
				printf(" MB");
			printf(" written");
			fper = fifo_percent(TRUE);
			if (fper >= 0)
				printf(" (fifo %3d%%)", fper);
			printf(".");
			savbytes = (bytes >> 20) << 20;
			flush();
			neednl = TRUE;
		}
#ifdef	BCAP
		if (bsize >= 0) {
			read_buff_cap(scgp, 0, &bfree);
			per = 100*(bsize - bfree) / bsize;
			if (per != oper)
				printf("[%3d] %3d %3d\b\b\b\b\b\b\b\b\b\b\b\b\b\b",
					per, bsize >> 10, bfree >> 10);
			oper = per;
			flush();
		}
#endif
	} while (tracksize < 0 || bytes_read < tracksize);

	if ((bytes / secsize) < 300) {
		amount = roundup(trackp->padsize, secsize);
		if (((bytes+amount) / secsize) < 300)
			trackp->padsize = 300 * secsize - bytes;
	}
	if (trackp->padsize) {
		if (neednl) {
			printf("\n");
			neednl = FALSE;
		}
		if ((trackp->padsize >> 20) > 0) {
			neednl = TRUE;
		} else if (lverbose) {
			printf("Track %02d: writing %3ld KB of pad data.\n",
						track, trackp->padsize >> 10);
			neednl = FALSE;
		}
		pad_track(scgp, dp, track, trackp, startsec, trackp->padsize,
					TRUE, &amount);
		bytes += amount;
		startsec += amount / secsize;
	}
	printf("%sTrack %02d: Total bytes read/written: %ld/%ld (%ld sectors).\n",
	       neednl?"\n":"", track, bytes_read, bytes, bytes/secsize);
	flush();
	return 0;
}

extern int
pad_track(scgp, dp, track, trackp, startsec, amt, dolast, bytesp)
	SCSI	*scgp;
	cdr_t	*dp;
	int	track;
	track_t	*trackp;
	long	startsec;
	long	amt;
	BOOL	dolast;
	long	*bytesp;
{
	long	bytes	= 0;
	long	savbytes = 0;
	int	secsize;
	int	secspt;
	int	bytespt;
	int	amount;
	BOOL	neednl	= FALSE;
	BOOL	islast	= FALSE;

	secsize = trackp->secsize;
	secspt = trackp->secspt;
	bytespt = secsize * secspt;
	
	fillbytes(buf, bytespt, '\0');

	if ((amt >> 20) > 0) {
		printf("\rTrack %02d:   0 of %3ld MB pad written.",
						track, amt >> 20);
		flush();
	}
	do {
		if (amt < bytespt) {
			bytespt = roundup(amt, secsize);
			secspt = bytespt / secsize;	
		}
		if (dolast && (amt - bytespt) <= 0)
			islast = TRUE;

		amount = (*dp->cdr_write_trackdata)(scgp, buf, startsec, bytespt, secspt, islast);
		if (amount < 0) {
			printf("%swrite track data: error after %ld bytes\n",
							neednl?"\n":"", bytes);
			if (bytesp)
				*bytesp = bytes;
read_buff_cap(scgp, 0, 0);
			return (-1);
		}
		amt -= amount;
		bytes += amount;
		startsec += amount / secsize;

		if (lverbose && (bytes >= (savbytes + 0x100000))) {
			printf("\rTrack %02d: %3ld", track, bytes >> 20);
			savbytes = (bytes >> 20) << 20;
			flush();
			neednl = TRUE;
		}
	} while (amt > 0);

	if (bytesp)
		*bytesp = bytes;
	return (bytes);
}

#ifdef	USE_WRITE_BUF
extern int
write_buf(scgp, dp, track, trackp, bp, startsec, amt, secsize, dolast, bytesp)
	SCSI	*scgp;
	cdr_t	*dp;
	int	track;
	track_t	*trackp;
	char	*bp;
	long	startsec;
	long	amt;
	int	secsize;
	BOOL	dolast;
	long	*bytesp;
{
	long	bytes	= 0;
	long	savbytes = 0;
/*	int	secsize;*/
	int	secspt;
	int	bytespt;
	int	amount;
	BOOL	neednl	= FALSE;
	BOOL	islast	= FALSE;

/*	secsize = trackp->secsize;*/
/*	secspt = trackp->secspt;*/

	secspt = bufsize/secsize;
	secspt = min(255, secspt);
	bytespt = secsize * secspt;
	
/*	fillbytes(buf, bytespt, '\0');*/

	if ((amt >> 20) > 0) {
		printf("\rTrack %02d:   0 of %3ld MB pad written.",
						track, amt >> 20);
		flush();
	}
	do {
		if (amt < bytespt) {
			bytespt = roundup(amt, secsize);
			secspt = bytespt / secsize;	
		}
		if (dolast && (amt - bytespt) <= 0)
			islast = TRUE;

		amount = (*dp->cdr_write_trackdata)(scgp, bp, startsec, bytespt, secspt, islast);
		if (amount < 0) {
			printf("%swrite track data: error after %ld bytes\n",
							neednl?"\n":"", bytes);
			if (bytesp)
				*bytesp = bytes;
read_buff_cap(scgp, 0, 0);
			return (-1);
		}
		amt -= amount;
		bytes += amount;
		startsec += amount / secsize;

		if (lverbose && (bytes >= (savbytes + 0x100000))) {
			printf("\rTrack %02d: %3ld", track, bytes >> 20);
			savbytes = (bytes >> 20) << 20;
			flush();
			neednl = TRUE;
		}
	} while (amt > 0);

	if (bytesp)
		*bytesp = bytes;
	return (bytes);
}
#endif	/* USE_WRITE_BUF */

static void
printdata(track, trackp)
	int	track;
	track_t	*trackp;
{
	if (trackp->tracksize >= 0) {
		printf("Track %02d: data  %3ld MB        ",
					track, trackp->tracksize >> 20);
	} else {
		printf("Track %02d: data  unknown length",
					track);
	}
	if (trackp->padsize > 0) {
		if ((trackp->padsize >> 20) > 0)
			printf(" padsize: %3ld MB", trackp->padsize >> 20);
		else
			printf(" padsize: %3ld KB", trackp->padsize >> 10);
	}
	if (trackp->pregapsize != 150) {
		printf(" pregapsize: %3ld", trackp->pregapsize);
	}
	printf("\n");
}

static void
printaudio(track, trackp)
	int	track;
	track_t	*trackp;
{
	if (trackp->tracksize >= 0) {
		printf("Track %02d: audio %3ld MB (%02d:%02d.%02d) %spreemp%s%s",
			track, trackp->tracksize >> 20,
			minutes(trackp->tracksize),
			seconds(trackp->tracksize),
			hseconds(trackp->tracksize),
			is_preemp(trackp) ? "" : "no ",
			is_swab(trackp) ? " swab":"",
			((trackp->tracksize < 300L*trackp->secsize) ||
			(trackp->tracksize % trackp->secsize)) &&
			is_pad(trackp) ? " pad" : "");
	} else {
		printf("Track %02d: audio unknown length    %spreemp%s%s",
			track, is_preemp(trackp) ? "" : "no ",
			is_swab(trackp) ? " swab":"",
			(trackp->tracksize % trackp->secsize) && is_pad(trackp) ? " pad" : "");
	}
	if (trackp->padsize > 0) {
		if ((trackp->padsize >> 20) > 0)
			printf(" padsize: %3ld MB", trackp->padsize >> 20);
		else
			printf(" padsize: %3ld KB", trackp->padsize >> 10);
		printf(" (%02d:%02d.%02d)",
			minutes(trackp->padsize),
			seconds(trackp->padsize),
			hseconds(trackp->padsize));
	}
	if (trackp->pregapsize != 150) {
		printf(" pregapsize: %3ld", trackp->pregapsize);
	}
	printf("\n");
}

static void
checkfile(track, trackp)
	int	track;
	track_t	*trackp;
{
	if (trackp->tracksize > 0 &&
			is_audio(trackp) &&
			((trackp->tracksize < 300L*trackp->secsize) ||
			(trackp->tracksize % trackp->secsize)) &&
						!is_pad(trackp)) {
		errmsgno(EX_BAD, "Bad audio track size %ld for track %02d.\n",
				trackp->tracksize, track);
		comerrno(EX_BAD, "Audio tracks must be at least %ld bytes and a multiple of %d.\n",
				300L*trackp->secsize, trackp->secsize);
	}
	
	if (!lverbose)
		return;

	if (is_audio(trackp))
		printaudio(track, trackp);
	else
		printdata(track, trackp);
}

static int
checkfiles(tracks, trackp)
	int	tracks;
	track_t	*trackp;
{
	int	i;
	int	isaudio = 1;

	for (i = 1; i <= tracks; i++) {
		if (!is_audio(&trackp[i]))
			isaudio = 0;
		checkfile(i, &trackp[i]);
	}
	return (isaudio);
}

static void
setpregaps(tracks, trackp)
	int	tracks;
	track_t	*trackp;
{
	int	i;
	int	sectype;
	track_t	*tp;

	/*
	 * Set some values for track 0 (the lead-in)
	 * XXX There should be a better place to do this.
	 */
	sectype = trackp[1].sectype;
	trackp[0].sectype = sectype;
	trackp[0].dbtype = trackp[1].dbtype;

	for (i = 1; i <= tracks; i++) {
		tp = &trackp[i];
		if (tp->pregapsize == -1L) {
			tp->pregapsize = 150;		/* Default Pre GAP */
			if (sectype != tp->sectype) {
				tp->pregapsize = 255;	/* Pre GAP is 255 */
				tp->flags &= ~TI_PREGAP;
			}
		}
		sectype = tp->sectype;			/* Save old sectype */
	}
	/*
	 * Set some values for track 0xAA (the lead-out)
	 * XXX There should be a better place to do this.
	 */
	trackp[tracks+1].sectype = sectype;
	trackp[tracks+1].dbtype = trackp[tracks].dbtype;
}

static long
checktsize(tracks, trackp)
	int	tracks;
	track_t	*trackp;
{
	int	i;
	long	curr;
	long	total = -150;
	Ullong	btotal;
	track_t	*tp;

	for (i = 1; i <= tracks; i++) {
		tp = &trackp[i];
		if (!is_pregap(tp))
			total += tp->pregapsize;

		if (lverbose > 1) {
			printf("track: %d start: %ld pregap: %ld\n",
					i, total, tp->pregapsize);
		}
		tp->trackstart = total;
		if (tp->tracksize >= 0) {
			curr = (tp->tracksize + (tp->secsize-1)) / tp->secsize;
			curr += (tp->padsize + (tp->secsize-1)) / tp->secsize;

			if (curr < 300)		/* Minimum track size is 4s */
				curr = 300;
			if (is_tao(tp) && !is_audio(tp)) {
				curr += 2;
			}
			total += curr;
		}
	}
	tp = &trackp[i];
	tp->trackstart = total;
	if (!lverbose)
		return (total);

	btotal = (Ullong)total * 2352;
/* XXX Sector Size ??? */
	if (tracks > 0) {
		printf("Total size:     %3lu MB (%02d:%02d.%02d) = %ld sectors\n",
			(Ulong)(btotal >> 20),
			minutes(btotal),
			seconds(btotal),
			hseconds(btotal), total);
		btotal += 150 * 2352;
		printf("Lout start:     %3lu MB (%02d:%02d/%02d) = %ld sectors\n",
			(Ulong)(btotal >> 20),
			minutes(btotal),
			seconds(btotal),
			frames(btotal), total);
	}
	return (total);
}

static void
checksize(trackp)
	track_t	*trackp;
{
	struct stat	st;

	/*
	 * If the current input file is a regular file and
	 * 'padsize=' has not been specified,
	 * use fstat() or file parser to get the size of the file.
	 */
	if (trackp->tracksize < 0 && (trackp->flags & TI_ISOSIZE) != 0) {
		trackp->tracksize = isosize(trackp->f);
	}
	if (trackp->tracksize < 0 && (trackp->flags & TI_NOAUHDR) == 0) {
		trackp->tracksize = ausize(trackp->f);
	}
	if (trackp->tracksize < 0 && (trackp->flags & TI_NOAUHDR) == 0) {
		trackp->tracksize = wavsize(trackp->f);
		if (trackp->tracksize > 0)	/* Force little endian input */
			trackp->flags |= TI_SWAB;
	}
	if (trackp->tracksize == AU_BAD_CODING) {
		comerrno(EX_BAD, "Inappropriate audio coding in '%s'.\n",
							trackp->filename);
	}
	if (trackp->tracksize < 0 &&
			fstat(trackp->f, &st) >= 0 && S_ISREG(st.st_mode)) {
		trackp->tracksize = st.st_size;
	}
}

static BOOL
checkdsize(scgp, dp, dsp, tsize)
	SCSI	*scgp;
	cdr_t	*dp;
	dstat_t	*dsp;
	long	tsize;
{
	long	startsec = 0L;
	long	endsec = 0L;

	scgp->silent++;
	(*dp->cdr_next_wr_address)(scgp, /*i*/ 0, (track_t *)0, &startsec);
	scgp->silent--;

	endsec = startsec + tsize;

	if (dsp->ds_maxblocks > 0) {
		if (lverbose)
			printf("Blocks total: %ld Blocks current: %ld Blocks remaining: %ld\n",
					dsp->ds_maxblocks,
					dsp->ds_maxblocks - startsec,
					dsp->ds_maxblocks - endsec);

		if (endsec > dsp->ds_maxblocks) {
			errmsgno(EX_BAD,
			"WARNING: Data may not fit on current disk.\n");

			/* XXX Check for flags & CDR_NO_LOLIMIT */
/*			return (FALSE);*/
		}
		if (lverbose && dsp->ds_maxrblocks > 0)
			printf("RBlocks total: %ld RBlocks current: %ld RBlocks remaining: %ld\n",
					dsp->ds_maxrblocks,
					dsp->ds_maxrblocks - startsec,
					dsp->ds_maxrblocks - endsec);
		if (dsp->ds_maxrblocks > 0 && endsec > dsp->ds_maxrblocks) {
			errmsgno(EX_BAD,
			"Data does not fit on current disk.\n");
			return (FALSE);
		}
	} else {
		if (endsec >= (405000-301)) {			/*<90 min disk*/
			errmsgno(EX_BAD,
				"Data will not fit on any disk.\n");
			return (FALSE);
		} else if (endsec >= (333000-150)) {		/* 74 min disk*/
			errmsgno(EX_BAD,
			"WARNING: Data may not fit on standard 74min disk.\n");
		}
	}
	return (TRUE);
}

static void
raise_fdlim()
{
#ifdef	RLIMIT_NOFILE

	struct rlimit	rlim;

	/*
	 * Set max # of file descriptors to be able to hold all files open
	 */
	getrlimit(RLIMIT_NOFILE, &rlim);
	rlim.rlim_cur = MAX_TRACK + 10;
	if (rlim.rlim_cur > rlim.rlim_max)
		errmsgno(EX_BAD,
			"warning: low file descriptor limit (%ld)\n",
							rlim.rlim_max);
	setrlimit(RLIMIT_NOFILE, &rlim);

#endif	/* RLIMIT_NOFILE */
}

char	*opts =
"help,version,checkdrive,prcap,inq,scanbus,reset,ignsize,useinfo,dev*,timeout#,driver*,tsize&,padsize&,pregap&,defpregap&,speed#,load,eject,dummy,msinfo,toc,atip,multi,fix,nofix,debug,v+,V+,audio,data,mode2,xa1,xa2,cdi,isosize,nopreemp,preemp,nopad,pad,swab,fs&,blank&,pktsize#,packet,noclose,force,dao,scms,isrc*,mcn*,index*";

static void
gargs(ac, av, tracksp, trackp, devp, timeoutp, dpp, speedp, flagsp, toctypep, blankp)
	int	ac;
	char	**av;
	int	*tracksp;
	track_t	*trackp;
	cdr_t	**dpp;
	char	**devp;
	int	*timeoutp;
	int	*speedp;
	long	*flagsp;
	int	*toctypep;
	int	*blankp;
{
	int	cac;
	char	* const*cav;
	char	*driver = NULL;
	char	*dev = NULL;
	char	*isrc = NULL;
	char	*mcn = NULL;
	char	*tindex = NULL;
	long	bltype = -1;
	Llong	tracksize;
	Llong	padsize;
	long	pregapsize;
	long	defpregap = -1L;
	long	secsize;
	int	pktsize;
	int	speed = -1;
	int	help = 0;
	int	version = 0;
	int	checkdrive = 0;
	int	prcap = 0;
	int	inq = 0;
	int	scanbus = 0;
	int	reset = 0;
	int	ignsize = 0;
	int	useinfo = 0;
	int	load = 0;
	int	eject = 0;
	int	dummy = 0;
	int	msinfo = 0;
	int	toc = 0;
	int	atip = 0;
	int	multi = 0;
	int	fix = 0;
	int	nofix = 0;
	int	audio;
	int	autoaudio = 0;
	int	data;
	int	mode2;
	int	xa1;
	int	xa2;
	int	cdi;
	int	isize;
	int	ispacket = 0;
	int	noclose = 0;
	int	force = 0;
	int	dao = 0;
	int	scms = 0;
	int	preemp = 0;
	int	nopreemp;
	int	pad = 0;
	int	bswab = 0;
	int	nopad;
	int	flags;
	int	tracks = *tracksp;
	int	tracktype = TOC_ROM;
	int	sectype = ST_ROM_MODE1;
	int	dbtype = DB_ROM_MODE1;
	int	got_track;

	trackp[0].flags |= TI_TAO;
	trackp[1].pregapsize = -1;
	*flagsp |= F_WRITE;

	cac = --ac;
	cav = ++av;
	for (;; cac--, cav++) {
		tracksize = (Llong)-1L;
		padsize = (Llong)0L;
		pregapsize = defpregap;
		audio = data = mode2 = xa1 = xa2 = cdi = 0;
		isize = nopreemp = nopad = 0;
		pktsize = 0;
		isrc = NULL;
		tindex = NULL;
		if (getargs(&cac, &cav, opts,
				&help, &version, &checkdrive, &prcap,
				&inq, &scanbus, &reset, &ignsize,
				&useinfo,
				devp, timeoutp, &driver,
				getllnum, &tracksize,
				getllnum, &padsize,
				getnum, &pregapsize,
				getnum, &defpregap,
				&speed,
				&load, &eject, &dummy, &msinfo, &toc, &atip,
				&multi, &fix, &nofix,
				&debug, &lverbose, &scsi_verbose,
				&audio, &data, &mode2,
				&xa1, &xa2, &cdi,
				&isize,
				&nopreemp, &preemp,
				&nopad, &pad, &bswab, getnum, &fs,
				getbltype, &bltype, &pktsize,
				&ispacket, &noclose, &force,
				&dao, &scms,
				&isrc, &mcn, &tindex) < 0) {
			errmsgno(EX_BAD, "Bad Option: %s.\n", cav[0]);
			usage(EX_BAD);
		}
		if (help)
			usage(0);
		if (tracks == 0) {
			if (driver)
				set_cdrcmds(driver, dpp);
			if (version)
				*flagsp |= F_VERSION;
			if (checkdrive)
				*flagsp |= F_CHECKDRIVE;
			if (prcap)
				*flagsp |= F_PRCAP;
			if (inq)
				*flagsp |= F_INQUIRY;
			if (scanbus)
				*flagsp |= F_SCANBUS;
			if (reset)
				*flagsp |= F_RESET;
			if (ignsize)
				*flagsp |= F_IGNSIZE;
			if (load)
				*flagsp |= F_LOAD;
			if (eject)
				*flagsp |= F_EJECT;
			if (dummy)
				*flagsp |= F_DUMMY;
			if (msinfo)
				*flagsp |= F_MSINFO;
			if (toc) {
				*flagsp |= F_TOC;
				*flagsp &= ~F_WRITE;
			}
			if (atip) {
				*flagsp |= F_PRATIP;
				*flagsp &= ~F_WRITE;
			}
			if (multi) {
				*flagsp |= F_MULTI;
				tracktype = TOC_XA2;
				sectype = ST_ROM_MODE2;
				dbtype = DB_XA_MODE2;
			}
			if (fix)
				*flagsp |= F_FIX;
			if (nofix)
				*flagsp |= F_NOFIX;
			if (force) 
				*flagsp |= F_FORCE;

			if (bltype >= 0) {
				*flagsp |= F_BLANK;
				*blankp = bltype;
			}
			if (dao) {
				*flagsp |= F_SAO;
				trackp[0].flags &= ~TI_TAO;
			}
			if (mcn) {
#ifdef	AUINFO
				setmcn(mcn, &trackp[0]);
#else
				trackp[0].isrc = malloc(16);
				fillbytes(trackp[0].isrc, 16, '\0');
				strncpy(trackp[0].isrc, mcn, 13);
#endif
				mcn = NULL;
			}
			version = checkdrive = prcap = inq = scanbus = reset = ignsize =
			load = eject = dummy = msinfo = toc = atip = multi = fix = nofix = force = dao = 0;
		} else if ((version + checkdrive + prcap + inq + scanbus + reset + ignsize +
			    load + eject + dummy + msinfo + toc + atip + multi + fix + nofix + force + dao) > 0 ||
				mcn != NULL)
			comerrno(EX_BAD, "Badly placed option. Global options must be before any track.\n");

		if (nopreemp)
			preemp = 0;
		if (nopad)
			pad = 0;

		if ((audio + data + mode2 + xa1 + xa2 + cdi) > 1) {
			errmsgno(EX_BAD, "Too many types for track %d.\n", tracks+1);
			comerrno(EX_BAD, "Only one of -audio, -data, -mode2, -xa1, -xa2, -cdi allowed.\n");
		}
		if (ispacket && audio) {
			comerrno(EX_BAD, "Audio data cannot be written in packet mode.\n");
		}
		got_track = getfiles(&cac, &cav, opts);
		if (autoaudio) {
			autoaudio = 0;
			tracktype = TOC_ROM;
			sectype = ST_ROM_MODE1;
			dbtype = DB_ROM_MODE1;
		}
		if (got_track != 0 && (is_auname(cav[0]) || is_wavname(cav[0]))) {
			autoaudio++;
			audio++;
		}
		if (data) {
			tracktype = TOC_ROM;
			sectype = ST_ROM_MODE1;
			dbtype = DB_ROM_MODE1;
		}
		if (mode2) {
			tracktype = TOC_ROM;
			sectype = ST_ROM_MODE2;
			dbtype = DB_ROM_MODE2;
		}
		if (audio) {
			tracktype = TOC_DA;
			sectype = preemp ? ST_AUDIO_PRE : ST_AUDIO_NOPRE;
			dbtype = DB_RAW;
		}
		if (xa1) {
			tracktype = TOC_XA1;
			sectype = ST_ROM_MODE1;
			dbtype = DB_XA_MODE1;
		}
		if (xa2) {
			tracktype = TOC_XA2;
			sectype = ST_ROM_MODE2;
			dbtype = DB_XA_MODE2_F1;
		}
		if (cdi) {
			tracktype = TOC_CDI;
			sectype = ST_ROM_MODE2;
			dbtype = DB_XA_MODE2_F1;
		}
		if (tracks == 0)
			*toctypep = tracktype;

		flags = 0;
		if ((sectype & ST_AUDIOMASK) != 0)
			flags |= TI_AUDIO;
		if (isize) {
			flags |= TI_ISOSIZE;
			if ((*flagsp & F_MULTI) != 0)
				comerrno(EX_BAD, "Cannot get isosize for multi session disks.\n");
		}
		if (preemp)
			flags |= TI_PREEMP;
		if (scms)
			flags |= TI_SCMS;

		if ((flags & TI_AUDIO) == 0 && padsize > (Llong)0L)
			pad = TRUE;
		if (pad) {
			flags |= TI_PAD;
			if ((flags & TI_AUDIO) == 0 && padsize == (Llong)0L)
				padsize = (Llong)PAD_SIZE;
		}
		if (bswab)
			flags |= TI_SWAB;
		if (ispacket) 
			flags |= TI_PACKET;
		if (noclose) 
			flags |= TI_NOCLOSE;

		if ((*flagsp & F_SAO) == 0)
			flags |= TI_TAO;

		if (got_track == 0)
			break;
		tracks++;

		if (tracks > MAX_TRACK)
			comerrno(EX_BAD, "Track limit (%d) exceeded\n",
								MAX_TRACK);

		if (strcmp("-", cav[0]) == 0) {
			trackp[tracks].f = STDIN_FILENO;

#if	defined(__CYGWIN32__) || defined(__EMX__)
			setmode(STDIN_FILENO, O_BINARY);
#endif
		} else {
			if (access(cav[0], R_OK) < 0)
				comerr("No read access for '%s'.\n", cav[0]);

			if ((trackp[tracks].f = open(cav[0], O_RDONLY|O_BINARY)) < 0)
				comerr("Cannot open '%s'.\n", cav[0]);
		}
		if (!is_auname(cav[0]) && !is_wavname(cav[0]))
			flags |= TI_NOAUHDR;

		if ((*flagsp & F_SAO) != 0 && (flags & TI_AUDIO) != 0)
			flags |= TI_PREGAP;	/* Hack for now */
		if (tracks == 1)
			flags &= ~TI_PREGAP;

		if (tracks == 1 && (pregapsize != -1L && pregapsize != 150))
			pregapsize = -1L;
		secsize = tracktype == TOC_DA ? AUDIO_SEC_SIZE : DATA_SEC_SIZE;
		trackp[tracks].filename = cav[0];;
		trackp[tracks].trackstart = 0L;
		trackp[tracks].tracksize = tracksize;
		if (trackp[tracks].pregapsize < 0)
			trackp[tracks].pregapsize = pregapsize;
		trackp[tracks+1].pregapsize = -1;
		trackp[tracks].padsize = padsize;
		trackp[tracks].secsize = secsize;
		trackp[tracks].secspt = 0;	/* transfer size is set up in set_trsizes() */
		trackp[tracks].pktsize = pktsize;
		trackp[tracks].trackno = tracks;
		trackp[tracks].sectype = sectype;
		trackp[tracks].tracktype = tracktype;
		trackp[tracks].dbtype = dbtype;
		trackp[tracks].flags = flags;
		trackp[tracks].nindex = 1;
		trackp[tracks].tindex = 0;
		checksize(&trackp[tracks]);
		tracksize = trackp[tracks].tracksize;
		if (tracksize > 0 && (tracksize / secsize) < 300) {
			tracksize = roundup(tracksize, secsize);
			padsize = tracksize + roundup(padsize, secsize);
			if ((padsize / secsize) < 300) {
				trackp[tracks].padsize =
					300 * secsize - tracksize;
			}
		}
#ifdef	AUINFO
		if (useinfo) {
			auinfo(cav[0], tracks, trackp);
			if (tracks == 1)
				printf("pregap1: %ld\n", trackp[1].pregapsize);
		}
#endif
		if (isrc) {
#ifdef	AUINFO
			setisrc(isrc, &trackp[tracks]);
#else
			trackp[tracks].isrc = malloc(16);
			fillbytes(trackp[tracks].isrc, 16, '\0');
			strncpy(trackp[tracks].isrc, isrc, 12);
#endif
		}
		if (tindex) {
#ifdef	AUINFO
			setindex(tindex, &trackp[tracks]);
#endif
		}

		if (debug) {
			printf("File: '%s' tracksize: %ld secsize: %d tracktype: %d = %s sectype: %X = %s dbtype: %s flags %X\n",
				cav[0], trackp[tracks].tracksize, 
				trackp[tracks].secsize, 
				tracktype, toc2name[tracktype & TOC_MASK],
				sectype, st2name[sectype & ST_MASK], db2name[dbtype], flags);
		}
	}

	if (speed < 0 && speed != -1)
		comerrno(EX_BAD, "Bad speed option.\n");

	if (fs < 0L && fs != -1L)
		comerrno(EX_BAD, "Bad fifo size option.\n");

	dev = *devp;
	cdr_defaults(&dev, &speed, &fs);
	if (debug)
		printf("dev: %s speed: %d fs: %ld\n", dev, speed, fs);

	if (speed >= 0)
		*speedp = speed;

	if (fs < 0L)
		fs = DEFAULT_FIFOSIZE;

	if (dev != *devp && (*flagsp & F_SCANBUS) == 0)
		*devp = dev;

	if (!*devp && (*flagsp & (F_VERSION|F_SCANBUS)) == 0) {
		errmsgno(EX_BAD, "No CD/DVD-Recorder device specified.\n");
		usage(EX_BAD);
	}
	if (*flagsp & (F_LOAD|F_MSINFO|F_TOC|F_PRATIP|F_FIX|F_VERSION|F_CHECKDRIVE|F_PRCAP|F_INQUIRY|F_SCANBUS|F_RESET)) {
		if (tracks != 0) {
			errmsgno(EX_BAD, "No tracks allowed with this option\n");
			usage(EX_BAD);
		}
		return;
	}
	if (tracks == 0 && (*flagsp & (F_LOAD|F_EJECT|F_BLANK)) == 0) {
		errmsgno(EX_BAD, "No tracks specified. Need at least one.\n");
		usage(EX_BAD);
	}
	*tracksp = tracks;
	if (*flagsp & F_SAO) {
		/*
		 * Make sure that you change WRITER_MAXWAIT & READER_MAXWAIT
		 * too if you change this timeout.
		 */
		if (*timeoutp < 200)		/* Lead in size is 2:30 */
			*timeoutp = 200;	/* 200s is 150s *1.33	*/
	}
}

static void
set_trsizes(dp, tracks, trackp)
	cdr_t	*dp;
	int	tracks;
	track_t	*trackp;
{
	int	i;

	/*
	 * We are using SCSI Group 0 write
	 * and cannot write more than 255 secs at once.
	 */
	data_secs_per_tr = bufsize/DATA_SEC_SIZE;
	audio_secs_per_tr = bufsize/AUDIO_SEC_SIZE;
	data_secs_per_tr = min(255, data_secs_per_tr);
	audio_secs_per_tr = min(255, audio_secs_per_tr);

	trackp[1].flags		|= TI_FIRST;
	trackp[tracks].flags	|= TI_LAST;
	
	for (i = 1; i <= tracks; i++) {
		trackp[i].secspt =
			is_audio(&trackp[i]) ?
				audio_secs_per_tr :
				data_secs_per_tr;
		if (is_packet(&trackp[i]) && trackp[i].pktsize > 0) {
			if (trackp[i].secspt >= trackp[i].pktsize) {
				trackp[i].secspt = trackp[i].pktsize;
			} else {
				comerrno(EX_BAD,
					"Track %d packet size %d exceeds buffer limit of %d sectors",
					i, trackp[i].pktsize, trackp[i].secspt);
			}
		}
		if ((dp->cdr_flags & CDR_SWABAUDIO) != 0 &&
					is_audio(&trackp[i])) {
			trackp[i].flags ^= TI_SWAB;
		}
	}
}

extern void
load_media(scgp, dp)
	SCSI	*scgp;
	cdr_t	*dp;
{
	int	code;
	int	key;

	/*
	 * Do some preparation before...
	 */
	scgp->silent++;			/* Be quiet if this fails	*/
	test_unit_ready(scgp);		/* First eat up unit attention	*/
	(*dp->cdr_load)(scgp);		/* now try to load media and	*/
	scsi_start_stop_unit(scgp, 1, 0);/* start unit in silent mode	*/
	scgp->silent--;

	if (!wait_unit_ready(scgp, 60)) {
		code = scsi_sense_code(scgp);
		key = scsi_sense_key(scgp);
		scsi_prevent_removal(scgp, 0);/* In case someone locked it */

		if (key == SC_NOT_READY && (code == 0x3A || code == 0x30))
			comerrno(EX_BAD, "No disk / Wrong disk!\n");
		comerrno(EX_BAD, "CD/DVD-Recorder not ready.\n");
	}

	scsi_prevent_removal(scgp, 1);
	scsi_start_stop_unit(scgp, 1, 0);
	wait_unit_ready(scgp, 120);
	scgp->silent++;
	rezero_unit(scgp);	/* Is this needed? Not supported by some drvives */
	scgp->silent--;
	test_unit_ready(scgp);
	scsi_start_stop_unit(scgp, 1, 0);
	wait_unit_ready(scgp, 120);
}

extern void
unload_media(scgp, dp, flags)
	SCSI	*scgp;
	cdr_t	*dp;
	int	flags;
{
	scsi_prevent_removal(scgp, 0);
	if ((flags & F_EJECT) != 0)
		(*dp->cdr_unload)(scgp);
}

extern void
set_secsize(scgp, secsize)
	SCSI	*scgp;
	int	secsize;
{
	if (secsize > 0) {
		/*
		 * Try to restore the old sector size.
		 */
		scgp->silent++;
		select_secsize(scgp, secsize);
		scgp->silent--;
	}
}

static void
check_recovery(scgp, dp, flags)
	SCSI	*scgp;
	cdr_t	*dp;
	int	flags;
{
	if ((*dp->cdr_check_recovery)(scgp)) {
		errmsgno(EX_BAD, "Recovery needed.\n");
		unload_media(scgp, dp, flags);
		exit(EX_BAD);
	}
}

#define	DEBUG
void audioread(scgp, dp, flags)
	SCSI	*scgp;
	cdr_t	*dp;
	int	flags;
{
#ifdef	DEBUG
	int speed = 1;
	int dummy = 0;

	if ((*dp->cdr_set_speed_dummy)(scgp, &speed, dummy) < 0)
		exit(-1);
	if ((*dp->cdr_set_secsize)(scgp, 2352) < 0)
		exit(-1);
	scgp->cap->c_bsize = 2352;

	read_scsi(scgp, buf, 1000, 1);
	printf("XXX:\n");
	write(1, buf, 512);
	unload_media(scgp, dp, flags);
	exit(0);
#endif
}

static void
print_msinfo(scgp, dp)
	SCSI	*scgp;
	cdr_t	*dp;
{
	long	off;
	long	fa;

	if ((*dp->cdr_session_offset)(scgp, &off) < 0) {
		errmsgno(EX_BAD, "Cannot read session offset\n");
		return;
	}
	if (lverbose)
		printf("session offset: %ld\n", off);

	if (dp->cdr_next_wr_address(scgp, 0, (track_t *)0, &fa) < 0) {
		errmsgno(EX_BAD, "Cannot read first writable address\n");
		return;
	}
	printf("%ld,%ld\n", off, fa);
}

static void
print_toc(scgp, dp)
	SCSI	*scgp;
	cdr_t	*dp;
{
	int	first;
	int	last;
	long	lba;
	long	xlba;
	struct msf msf;
	int	adr;
	int	control;
	int	mode;
	int	i;

	scgp->silent++;
	if (read_capacity(scgp) < 0) {
		scgp->silent--;
		errmsgno(EX_BAD, "Cannot read capacity\n");
		return;
	}
	scgp->silent--;
	if (read_tochdr(scgp, dp, &first, &last) < 0) {
		errmsgno(EX_BAD, "Cannot read TOC/PMA\n");
		return;
	}
	printf("first: %d last %d\n", first, last);
	for (i = first; i <= last; i++) {
		read_trackinfo(scgp, i, &lba, &msf, &adr, &control, &mode);
		xlba = -150 +
			msf.msf_frame + (75*msf.msf_sec) + (75*60*msf.msf_min);
		if (xlba == lba/4)
			lba = xlba;
		print_track(i, lba, &msf, adr, control, mode);
	}
	i = 0xAA;
	read_trackinfo(scgp, i, &lba, &msf, &adr, &control, &mode);
	xlba = -150 +
		msf.msf_frame + (75*msf.msf_sec) + (75*60*msf.msf_min);
	if (xlba == lba/4)
		lba = xlba;
	print_track(i, lba, &msf, adr, control, mode);
	if (lverbose > 1) {
		scgp->silent++;
		if (read_cdtext(scgp) < 0)
			errmsgno(EX_BAD, "No CD-Text or CD-Text unaware drive.\n");
		scgp->silent++;
	}
}

static void
print_track(track, lba, msp, adr, control, mode)
	int	track;
	long	lba;
	struct msf *msp;
	int	adr;
	int	control;
	int	mode;
{
	long	lba_512 = lba*4;

	if (track == 0xAA)
		printf("track:lout ");
	else
		printf("track: %3d ", track);

	printf("lba: %9ld (%9ld) %02d:%02d:%02d adr: %X control: %X mode: %d\n",
			lba, lba_512,
			msp->msf_min,
			msp->msf_sec,
			msp->msf_frame,
			adr, control, mode);
}

static	void	prtimediff	(const char *fmt,
					struct timeval *start,
					struct timeval *stop)
{
	struct timeval tv;

	tv.tv_sec = stop->tv_sec - start->tv_sec;
	tv.tv_usec = stop->tv_usec - start->tv_usec;
	while (tv.tv_usec > 1000000) {
		tv.tv_usec -= 1000000;
		tv.tv_sec += 1;
	}
	while (tv.tv_usec < 0) {
		tv.tv_usec += 1000000;
		tv.tv_sec -= 1;
	}
	/*
	 * We need to cast timeval->* to long because
	 * of the broken sys/time.h in Linux.
	 */
	printf("%s%4ld.%03lds\n", fmt, (long)tv.tv_sec, (long)tv.tv_usec/1000);
	flush();
}

#ifdef	HAVE_SYS_PRIOCNTL_H

#include <sys/priocntl.h>
#include <sys/rtpriocntl.h>

extern	void
raisepri(pri)
	int pri;
{
	int		pid;
	int		classes;
	int		ret;
	pcinfo_t	info;
	pcparms_t	param;
	rtinfo_t	rtinfo;
	rtparms_t	rtparam;

	pid = getpid();

	/* get info */
	strcpy(info.pc_clname, "RT");
	classes = priocntl(P_PID, pid, PC_GETCID, (void *)&info);
	if (classes == -1)
		comerr("Cannot get priority class id priocntl(PC_GETCID)\n");

	movebytes(info.pc_clinfo, &rtinfo, sizeof(rtinfo_t));
 
	/* set priority to max */
	rtparam.rt_pri = rtinfo.rt_maxpri - pri;
	rtparam.rt_tqsecs = 0;
	rtparam.rt_tqnsecs = RT_TQDEF;
	param.pc_cid = info.pc_cid;
	movebytes(&rtparam, param.pc_clparms, sizeof(rtparms_t));
	ret = priocntl(P_PID, pid, PC_SETPARMS, (void *)&param);
	if (ret == -1)
		comerr("Cannot set priority class parameters priocntl(PC_SETPARMS)\n");
}

#else	/* HAVE_SYS_PRIOCNTL_H */

#if defined(_POSIX_PRIORITY_SCHEDULING)
/*
 * XXX Ugly but needed because of a typo in /usr/iclude/sched.h on Linux.
 * XXX This should be removed as soon as we are sure that Linux-2.0.29 is gone.
 */
#ifdef	__linux
#define	_P	__P
#endif

#include <sched.h>

#ifdef	__linux
#undef	_P
#endif

static	int
rt_raisepri(pri)
	int pri;
{
	struct sched_param scp;

	/*
	 * Verify that scheduling is available
	 */
#ifdef	_SC_PRIORITY_SCHEDULING
	if (sysconf(_SC_PRIORITY_SCHEDULING) == -1) {
		errmsg("WARNING: RR-scheduler not available, disabling.\n");
		return(-1);
	}
#endif
	fillbytes(&scp, sizeof(scp), '\0');
	scp.sched_priority = sched_get_priority_max(SCHED_RR) - pri;
	if (sched_setscheduler(0, SCHED_RR, &scp) < 0) {
		errmsg("WARNING: Cannot set RR-scheduler\n");
		return (-1);
	}
	return (0);
}

#else	/* _POSIX_PRIORITY_SCHEDULING */

#ifdef	__CYGWIN32__

/*
 * NOTE: Base.h has a second typedef for BOOL.
 *	 We define BOOL to make all local code use BOOL
 *	 from Windows.h and use the hidden __SBOOL for
 *	 our global interfaces.
 */
#define	BOOL	WBOOL		/* This is the Win BOOL		*/
#define	format	__format
#include <vadefs.h>
#include <Windows32/Base.h>
#include <Windows32/Defines.h>
#include <Windows32/Structures.h>
#include <Windows32/Functions.h>
#undef format

static	int
rt_raisepri(pri)
	int pri;
{
	/* set priority class */
	if (SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS) == FALSE) {
		errmsgno(EX_BAD, "No realtime priority class possible.\n");
		return (-1);
	}

	/* set thread priority */
	if (SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL-pri) == FALSE) {
		errmsgno(EX_BAD, "Could not set realtime priority.\n");
		return (-1);
	}
	return (0);
}

#else

static	int
rt_raisepri(pri)
	int pri;
{
	return (-1);
}

#endif	/* __CYGWIN32__ */

#endif	/* _POSIX_PRIORITY_SCHEDULING */

extern	void
raisepri(pri)
	int pri;
{
	if (rt_raisepri(pri) >= 0)
		return;
#if	defined(HAVE_SETPRIORITY) && defined(PRIO_PROCESS)

	if (setpriority(PRIO_PROCESS, getpid(), -20 + pri) < 0)
		comerr("Cannot set priority using setpriority().\n");
#else
#ifdef	HAVE_DOSSETPRIORITY	/* RT priority on OS/2 */
	/*
	 * Set priority to timecritical 31 - pri (arg)
	 */
	DosSetPriority(0, 3, 31, 0);
	DosSetPriority(0, 3, -pri, 0);
#else
#ifdef	HAVE_NICE
	if (nice(-20 + pri) == -1)
		comerr("Cannot set priority using nice().\n");
#else
	errmsgno(EX_BAD, "Cannot set priority on this OS.\n");
#endif
#endif
#endif
}

#endif	/* HAVE_SYS_PRIOCNTL_H */


static Llong
number(arg, retp)
	register char	*arg;
		int	*retp;
{
	Llong	val	= 0;

	if (*retp != 1)
		return (val);
	if (*arg == '\0') {
		*retp = -1;
	} else if (*(arg = astoll(arg, &val))) {
		if (*arg == 'p' || *arg == 'P') {
			val *= (1024*1024);
			val *= (1024*1024*1024);
			arg++;
		}
		if (*arg == 't' || *arg == 'T') {
			val *= (1024*1024);
			val *= (1024*1024);
			arg++;
		}
		if (*arg == 'g' || *arg == 'G') {
			val *= (1024*1024*1024);
			arg++;
		}
		if (*arg == 'm' || *arg == 'M') {
			val *= (1024*1024);
			arg++;
		}
		else if (*arg == 'f' || *arg == 'F') {
			val *= 2352;
			arg++;
		}
		else if (*arg == 's' || *arg == 'S') {
			val *= 2048;
			arg++;
		}
		else if (*arg == 'k' || *arg == 'K') {
			val *= 1024;
			arg++;
		}
		else if (*arg == 'b' || *arg == 'B') {
			val *= 512;
			arg++;
		}
		else if (*arg == 'w' || *arg == 'W') {
			val *= 2;
			arg++;
		}
		if (*arg == '*' || *arg == 'x')
			val *= number(++arg, retp);
		else if (*arg != '\0')
			*retp = -1;
	}
	return (val);
}

extern int
getnum(arg, valp)
	char	*arg;
	long	*valp;
{
	int	ret = 1;

	*valp = (long)number(arg, &ret);
	return (ret);
}

extern int
getllnum(arg, lvalp)
	char	*arg;
	Llong	*lvalp;
{
	int	ret = 1;

	*lvalp = number(arg, &ret);
	return (ret);
}

static int
getbltype(optstr, typep)
	char	*optstr;
	long	*typep;
{
	if (streql(optstr, "all")) {
		*typep = BLANK_DISC;
	} else if (streql(optstr, "disc")) {
		*typep = BLANK_DISC;
	} else if (streql(optstr, "disk")) {
		*typep = BLANK_DISC;
	} else if (streql(optstr, "fast")) {
		*typep = BLANK_MINIMAL;
	} else if (streql(optstr, "minimal")) {
		*typep = BLANK_MINIMAL;
	} else if (streql(optstr, "track")) {
		*typep = BLANK_TRACK;
	} else if (streql(optstr, "unreserve")) {
		*typep = BLANK_UNRESERVE;
	} else if (streql(optstr, "trtail")) {
		*typep = BLANK_TAIL;
	} else if (streql(optstr, "unclose")) {
		*typep = BLANK_UNCLOSE;
	} else if (streql(optstr, "session")) {
		*typep = BLANK_SESSION;
	} else if (streql(optstr, "help")) {
		blusage(0);
	} else {
		schily_error("Illegal blanking type '%s'.\n", optstr);
		blusage(EX_BAD);
		return (-1);
	}
	return (TRUE);
}
