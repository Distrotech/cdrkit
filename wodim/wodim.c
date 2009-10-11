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

/*
 *
 * Modified by Eduard Bloch in 08/2006 and later
 */

/* @(#)cdrecord.c	1.310 06/02/09 Copyright 1995-2006 J. Schilling */
/*
 *	Record data on a CD/CVD-Recorder
 *
 *	Copyright (c) 1995-2006 J. Schilling
 */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; see the file COPYING.  If not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <mconfig.h>
#include <stdio.h>
#include <standard.h>
#include <stdxlib.h>
#include <fctldefs.h>
#include <errno.h>
#include <timedefs.h>
#ifdef	HAVE_SYS_RESOURCE_H
#include <sys/resource.h>	/* for rlimit */
#endif
#include <statdefs.h>
#include <unixstd.h>
#ifdef	HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#include <strdefs.h>
#include <utypes.h>
#include <intcvt.h>
#include <signal.h>
#include <schily.h>
#include <string.h>
#include <getargs.h>
#ifdef	HAVE_PRIV_H
#include <priv.h>
#endif

#include "xio.h"

#include <usal/scsireg.h>	/* XXX wegen SC_NOT_READY */
#include <usal/scsitransp.h>
#include <usal/usalcmd.h>		/* XXX fuer read_buffer */
#include "scsi_scan.h"

#include "auheader.h"
#include "wodim.h"
#include "defaults.h"
#include "movesect.h"

#ifdef __linux__
#include <sys/capability.h> 	/* for rawio capability */
#endif

#if defined(_POSIX_PRIORITY_SCHEDULING) && _POSIX_PRIORITY_SCHEDULING -0 >= 0
#ifdef  HAVE_SYS_PRIOCNTL_H	/* The preferred SYSvR4 schduler */
#else
#define	USE_POSIX_PRIORITY_SCHEDULING
#endif
#endif

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
		"Raw (audio) with P/W packed sub channel",
		"Raw (audio) with P/W raw sub channel",
		"Reserved mode 4",
		"Reserved mode 5",
		"Reserved mode 6",
		"Vendor unique mode 7",
		"CD-ROM mode 1",
		"CD-ROM mode 2",
		"CD-ROM XA mode 2 form 1",
		"CD-ROM XA mode 2 form 1 (with subheader)",
		"CD-ROM XA mode 2 form 2",
		"CD-ROM XA mode 2 form 1/2/mix",
		"Reserved mode 14",
		"Vendor unique mode 15",
};

/*
 * Map write modes into names.
 */
static	char	wm_none[] = "unknown";
static	char	wm_ill[]  = "illegal";

char	*wm2name[] = {
		wm_none,
		"BLANK",
		"FORMAT",
		wm_ill,
		"PACKET",
		wm_ill,
		wm_ill,
		wm_ill,
		"TAO",
		wm_ill,
		wm_ill,
		wm_ill,
		"SAO",
		"SAO/RAW16",	/* Most liklely not needed */
		"SAO/RAW96P",
		"SAO/RAW96R",
		"RAW",
		"RAW/RAW16",
		"RAW/RAW96P",
		"RAW/RAW96R",
};

int		debug;		/* print debug messages */
static	int	kdebug;		/* print kernel debug messages */
static	int	scsi_verbose;	/* SCSI verbose flag */
static	int	silent;		/* SCSI silent flag */
int		lverbose;	/* static verbose flag */
int		xdebug;		/* extended debug flag */

char	*buf;			/* The transfer buffer */
long	bufsize = -1;		/* The size of the transfer buffer */

static	int	gracetime = GRACE_TIME;
static	int	raw_speed = -1;
static	int	dma_speed = -1;
static	int	dminbuf = -1;	/* XXX Hack for now drive min buf fill */
BOOL	isgui;
static	int	didintr;
char	*driveropts;
static	char	*cuefilename = NULL;
static	uid_t	oeuid = (uid_t)-1;

struct timeval	starttime;
struct timeval	wstarttime;
struct timeval	stoptime;
struct timeval	fixtime;

static	long	fs = -1;	/* fifo (ring buffer) size */
static Llong warn_minisize = -1L;

static	int	gracewait(cdr_t *dp, BOOL *didgracep);
static	void	cdrstats(cdr_t *dp);
static	void	susage(int);
static	void	usage(int);
static	void	blusage(int);
static	void	formattypeusage(int);
static	void	intr(int sig);
static	void	catchsig(int sig);
static	int	scsi_cb(void *arg);
static	void	intfifo(int sig);
static	void	exscsi(int excode, void *arg);
static	void	excdr(int excode, void *arg);
int	read_buf(int f, char *bp, int size);
int	fill_buf(int f, track_t *trackp, long secno, char *bp, int size);
int	get_buf(int f, track_t *trackp, long secno, char **bpp, int size);
int	write_secs(SCSI *usalp, cdr_t *dp, char *bp, long startsec, int bytespt,
					  int secspt, BOOL islast);
static	int	write_track_data(SCSI *usalp, cdr_t *, track_t *);
int	pad_track(SCSI *usalp, cdr_t *dp, track_t *trackp, long startsec, 
					 Llong amt, BOOL dolast, Llong *bytesp);
int	write_buf(SCSI *usalp, cdr_t *dp, track_t *trackp, char *bp, 
					 long startsec, Llong amt, int secsize, BOOL dolast, 
					 Llong *bytesp);
static	void	printdata(int, track_t *);
static	void	printaudio(int, track_t *);
static	void	checkfile(int, track_t *);
static	int	checkfiles(int, track_t *);
static	void	setleadinout(int, track_t *);
static	void	setpregaps(int, track_t *);
static	long	checktsize(int, track_t *);
static	void	opentracks(track_t *);
static	void	checksize(track_t *);
static	BOOL	checkdsize(SCSI *usalp, cdr_t *dp, long tsize, int flags);
static	void	raise_fdlim(void);
static	void	raise_memlock(void);
static	int	gargs(int, char **, int *, track_t *, char **, int *, cdr_t **,
							int *, long *, int *, int *);
static	void	set_trsizes(cdr_t *, int, track_t *);
void		load_media(SCSI *usalp, cdr_t *, BOOL);
void		unload_media(SCSI *usalp, cdr_t *, int);
void		reload_media(SCSI *usalp, cdr_t *);
void		set_secsize(SCSI *usalp, int secsize);
static	int	get_dmaspeed(SCSI *usalp, cdr_t *);
static	BOOL	do_opc(SCSI *usalp, cdr_t *, int);
static	void	check_recovery(SCSI *usalp, cdr_t *, int);
void		audioread(SCSI *usalp, cdr_t *, int);
static	void	print_msinfo(SCSI *usalp, cdr_t *);
static	void	print_toc(SCSI *usalp, cdr_t *);
static	void	print_track(int, long, struct msf *, int, int, int);
#if !defined(HAVE_SYS_PRIOCNTL_H)
static	int	rt_raisepri(int);
#endif
void		raisepri(int);
static	void	wait_input(void);
static	void	checkgui(void);
static	int	getbltype(char *optstr, long *typep);
static	int	getformattype(char *optstr, long *typep);
static	void	print_drflags(cdr_t *dp);
static	void	print_wrmodes(cdr_t *dp);
static	BOOL	check_wrmode(cdr_t *dp, int wmode, int tflags);
static	void	set_wrmode(cdr_t *dp, int wmode, int tflags);
static	void	linuxcheck(void);

#ifdef __linux__
static int get_cap(cap_value_t cap_array);
#endif

struct exargs {
	SCSI	*usalp;
	cdr_t	*dp;
	int	old_secsize;
	int	flags;
	int	exflags;
} exargs;

void fifo_cleanup(void) {
   kill_faio();
}

/* shared variables */
int	scandevs = 0;
char	*msifile = NULL;

int main(int argc, char *argv[])
{
	char	*dev = NULL;
	int	timeout = 40;	/* Set default timeout to 40s CW-7502 is slow*/
	int	speed = -1;
	long	flags = 0L;
	int	blanktype = 0;
	int	formattype = 0;
	int	i;
	int	tracks = 0;
	int	trackno;
	long	tsize;
	track_t	track[MAX_TRACK+2];	/* Max tracks + track 0 + track AA */
	cdr_t	*dp = (cdr_t *)0;
	long	startsec = 0L;
	int	errs = 0;
	SCSI	*usalp = NULL;
	char	errstr[80];
	BOOL	gracedone = FALSE;
	int     ispacket;
	BOOL	is_cdwr = FALSE;
	BOOL	is_dvdwr = FALSE;

	buf=strstr(argv[0], "cdrecord");
	if(buf && '\0' == buf[8]) /* lame cheater detected */
		argv[0]="wodim";

#ifdef __EMX__
	/* This gives wildcard expansion with Non-Posix shells with EMX */
	_wildcard(&argc, &argv);
#endif
	save_args(argc, argv);
	oeuid = geteuid();		/* Remember saved set uid	*/

	fillbytes(track, sizeof (track), '\0');
	for (i = 0; i < MAX_TRACK+2; i++)
		track[i].track = track[i].trackno = i;
	track[0].tracktype = TOC_MASK;
	raise_fdlim();
	ispacket = gargs(argc, argv, &tracks, track, &dev, &timeout, &dp, &speed, &flags,
							&blanktype, &formattype);
	if ((track[0].tracktype & TOC_MASK) == TOC_MASK)
		comerrno(EX_BAD, "Internal error: Bad TOC type.\n");

	if (flags & F_VERSION) {
	   fprintf(stderr,
			 "Cdrecord-yelling-line-to-tell-frontends-to-use-it-like-version 2.01.01a03-dvd \n"
		 "Wodim " CDRKIT_VERSION "\n"
		 "Copyright (C) 2006 Cdrkit suite contributors\n"
		 "Based on works from Joerg Schilling, Copyright (C) 1995-2006, J. Schilling\n"
		 );
	   exit(0);
	}

	checkgui();

	if (debug || lverbose) {
		printf("TOC Type: %d = %s\n",
			track[0].tracktype & TOC_MASK,
			toc2name[track[0].tracktype & TOC_MASK]);
	}

	if ((flags & (F_MSINFO|F_TOC|F_PRATIP|F_FIX|F_VERSION|F_CHECKDRIVE|F_INQUIRY|F_SCANBUS|F_RESET)) == 0) {
		/*
		 * Try to lock us im memory (will only work for root)
		 * but you need access to root anyway to send SCSI commands.
		 * We need to be root to open /dev/usal? or similar devices
		 * on other OS variants and we need to be root to be able
		 * to send SCSI commands at least on AIX and
		 * Solaris (USCSI only) regardless of the permissions for
		 * opening files
		 *
		 * XXX The following test used to be
		 * XXX #if defined(HAVE_MLOCKALL) || defined(_POSIX_MEMLOCK)
		 * XXX but the definition for _POSIX_MEMLOCK did change during
		 * XXX the last 8 years and the autoconf test is better for
		 * XXX the static case. sysconf() only makes sense if we like
		 * XXX to check dynamically.
		 */
		raise_memlock();
#if defined(HAVE_MLOCKALL)
		/*
		 * XXX mlockall() needs root privilleges.
		 */
		if (mlockall(MCL_CURRENT|MCL_FUTURE) < 0) {
			if(lverbose>2)
				fprintf(stderr,
						"W: Cannot do mlockall(2). Possibly increased risk for buffer underruns.\n");
		}
#endif

		/*
		 * XXX raisepri() needs root privilleges.
		 */
		raisepri(0); /* max priority */
		/*
		 * XXX shmctl(id, SHM_LOCK, 0) needs root privilleges.
		 * XXX So if we use SysV shared memory, wee need to be root.
		 *
		 * Note that not being able to set up a FIFO bombs us
		 * back to the DOS ages. Trying to run cdrecord without
		 * root privillegs is extremely silly, it breaks most
		 * of the advanced features. We need to be at least installed
		 * suid root or called by RBACs pfexec.
		 */
		init_fifo(fs);	/* Attach shared memory (still one process) */
	}

	if ((flags & F_WAITI) != 0) {
		if (lverbose)
			printf("Waiting for data on stdin...\n");
		wait_input();
	}

	/*
	 * Call usal_remote() to force loading the remote SCSI transport library
	 * code that is located in librusal instead of the dummy remote routines
	 * that are located inside libusal.
	 */
	usal_remote();
	if (dev != NULL &&
	    ((strncmp(dev, "HELP", 4) == 0) ||
	    (strncmp(dev, "help", 4) == 0))) {
		usal_help(stderr);
		exit(0);
	}

	if( (!dev || *dev=='\0'|| 0==strcmp(dev, "-1")) && (flags & F_SCANBUS)==0 ) {
		int64_t need_size=0L;
		struct stat statbuf;
		int t;

		fprintf(stderr, "Device was not specified. Trying to find an appropriate drive...\n");

		/* estimate how much data user wants to write */
		for(t=1;t<=tracks;t++) { 
			if(track[t].tracksize>=0)
				need_size+=track[t].tracksize;
			else if(0==stat(track[t].filename, &statbuf))
				need_size+=statbuf.st_size;
		}
		usalp=open_auto(need_size, debug, lverbose);
	}

	if(!usalp)
		usalp = usal_open(dev, errstr, sizeof(errstr), debug, lverbose);

	if(!usalp)
	{
		errmsg("\nCannot open SCSI driver!\n"
				"For possible targets try 'wodim --devices' or 'wodim -scanbus'.\n"
				"For possible transport specifiers try 'wodim dev=help'.\n"
				"For IDE/ATAPI devices configuration, see the file README.ATAPI.setup from\n"
				"the wodim documentation.\n");
		exit(EX_BAD);
	}

#ifdef	HAVE_PRIV_SET
#ifdef	PRIV_DEBUG
	fprintf(stderr, "file_dac_read: %d\n", priv_ineffect(PRIV_FILE_DAC_READ));
#endif
	/*
	 * Give up privs we do not need anymore.
	 * We no longer need:
	 *	file_dac_read,proc_lock_memory,proc_priocntl,net_privaddr
	 * We still need:
	 *	sys_devices
	 */
	priv_set(PRIV_OFF, PRIV_EFFECTIVE,
		PRIV_FILE_DAC_READ, PRIV_PROC_LOCK_MEMORY,
		PRIV_PROC_PRIOCNTL, PRIV_NET_PRIVADDR, NULL);
	priv_set(PRIV_OFF, PRIV_PERMITTED,
		PRIV_FILE_DAC_READ, PRIV_PROC_LOCK_MEMORY,
		PRIV_PROC_PRIOCNTL, PRIV_NET_PRIVADDR, NULL);
	priv_set(PRIV_OFF, PRIV_INHERITABLE,
		PRIV_FILE_DAC_READ, PRIV_PROC_LOCK_MEMORY,
		PRIV_PROC_PRIOCNTL, PRIV_NET_PRIVADDR, PRIV_SYS_DEVICES, NULL);
#endif
	/*
	 * This is only for OS that do not support fine grained privs.
	 *
	 * XXX Below this point we do not need root privilleges anymore.
	 */
	if (geteuid() != getuid()) {	/* AIX does not like to do this */
					/* If we are not root		*/
#ifdef	HAVE_SETREUID
		if (setreuid(-1, getuid()) < 0)
#else
#ifdef	HAVE_SETEUID
		if (seteuid(getuid()) < 0)
#else
		if (setuid(getuid()) < 0)
#endif
#endif
			comerr("Panic cannot set back effective uid.\n");
	}

#ifdef __linux__
	/* get the rawio capability */
	if (get_cap(CAP_SYS_RAWIO) && (debug || lverbose>2)) 
	{
		perror("Warning: Cannot gain SYS_RAWIO capability");
		fprintf(stderr, "Possible reason: wodim not installed SUID root.\n");
	}
#endif

	/*
	 * WARNING: We now are no more able to do any privilleged operation
	 * unless we have been called by root.
	 *
	 * XXX It may be that we later get problems in init_faio() because
	 * XXX this function calls raisepri() to lower the priority slightly.
	 */
	usal_settimeout(usalp, timeout);
	usalp->verbose = scsi_verbose;
	usalp->silent = silent;
	usalp->debug = debug;
	usalp->kdebug = kdebug;
	usalp->cap->c_bsize = DATA_SEC_SIZE;

	if ((flags & F_MSINFO) == 0 || lverbose) {
		char	*vers;
		char	*auth;


		if(lverbose)
			fprintf(stderr, "Wodim version: " CDRKIT_VERSION "\n");

		vers = usal_version(0, SCG_VERSION);
		auth = usal_version(0, SCG_AUTHOR);
		if(lverbose >1 && auth && vers)
			fprintf(stderr, "Using libusal version '%s-%s'.\n", auth, vers);


		vers = usal_version(usalp, SCG_RVERSION);
		auth = usal_version(usalp, SCG_RAUTHOR);
		if (lverbose > 1 && vers && auth)
			fprintf(stderr, "Using remote transport code version '%s-%s'\n", auth, vers);
	}

	if (lverbose && driveropts)
		printf("Driveropts: '%s'\n", driveropts);

/*	bufsize = usal_bufsize(usalp, CDR_BUF_SIZE);*/
	bufsize = usal_bufsize(usalp, bufsize);
	if (lverbose || debug)
		fprintf(stderr, "SCSI buffer size: %ld\n", bufsize);
	if ((buf = usal_getbuf(usalp, bufsize)) == NULL)
		comerr("Cannot get SCSI I/O buffer.\n");

	if (scandevs)
		return (list_devices(usalp, stdout, 0));

	if ((flags & F_SCANBUS) != 0) {
		select_target(usalp, stdout);
		exit(0);
	}
	if ((flags & F_RESET) != 0) {
		if (usal_reset(usalp, SCG_RESET_NOP) < 0)
			comerr("Cannot reset (OS does not implement reset).\n");
		if (usal_reset(usalp, SCG_RESET_TGT) >= 0)
			exit(0);
		if (usal_reset(usalp, SCG_RESET_BUS) < 0)
			comerr("Cannot reset target.\n");
		exit(0);
	}

	/*
	 * First try to check which type of SCSI device we
	 * have.
	if (debug || lverbose)
		printf("atapi: %d\n", usal_isatapi(usalp));
	 */
	usalp->silent++;
	test_unit_ready(usalp);	/* eat up unit attention */
	usalp->silent--;
	if (!do_inquiry(usalp, (flags & F_MSINFO) == 0 || lverbose)) {
		errmsgno(EX_BAD, "Cannot do inquiry for CD/DVD-Recorder.\n");
		if (unit_ready(usalp))
			errmsgno(EX_BAD, "The unit seems to be hung and needs power cycling.\n");
		exit(EX_BAD);
	}
#ifdef	GCONF
	/*
	 * Debug only
	 */
	{
		extern	void	gconf(SCSI *);

		if (lverbose > 2)
			gconf(usalp);
	}
#endif

	if ((flags & F_PRCAP) != 0) {
		print_capabilities(usalp);
		print_capabilities_mmc4(usalp);
		exit(0);
	}
	if ((flags & F_INQUIRY) != 0)
		exit(0);

	if (dp == (cdr_t *)NULL) {	/* No driver= option specified	*/
		dp = get_cdrcmds(usalp);	/* Calls dp->cdr_identify()	*/
	}
	else if (!is_unknown_dev(usalp) && dp != get_cdrcmds(usalp)) {
		errmsgno(EX_BAD, "WARNING: Trying to use other driver on known device.\n");
	}
        is_mmc(usalp, &is_cdwr, &is_dvdwr);
	if (ispacket) {
		if (is_dvdwr) {
			track[0].flags |= TI_PACKET; 
			/*XXX put here to only affect DVD writing, should be in gargs.
			 * however if set in args for all mode, packet writing is then
			 * broken for all disc as cdrecord assume that PACKET imply TAO which  
			 * is not true at all???? */ 
			track[0].flags &= ~TI_TAO;
		}
	}

	if (dp == (cdr_t *)0)
		comerrno(EX_BAD, "Sorry, no supported CD/DVD-Recorder found on this target.\n");

	/* DVD does not support TAO */
	if (dp->is_dvd) {
	  if(lverbose>1)
		fprintf(stderr, "Using Session At Once (SAO) for DVD mode.\n");
	  dp->cdr_flags |= F_SAO;
	  for (i = 0; i <= MAX_TRACK; i++) {
		track[i].flags &= ~TI_TAO;
		track[i].flags |= TI_SAO;
	  }
	}

	if (!is_cddrive(usalp))
		comerrno(EX_BAD, "Sorry, no CD/DVD-Drive found on this target.\n");
	/*
	 * The driver is known, set up data structures...
	 */
	{
		cdr_t	*ndp;
		dstat_t	*dsp;

		ndp = malloc(sizeof (cdr_t));
		dsp = malloc(sizeof (dstat_t));
		if (ndp == NULL || dsp == NULL)
			comerr("Cannot allocate memory for driver structure.\n");
		movebytes(dp, ndp, sizeof (cdr_t));
		dp = ndp;
		dp->cdr_flags |= CDR_ALLOC;
		dp->cdr_cmdflags = flags;

		fillbytes(dsp, sizeof (*dsp), '\0');
		dsp->ds_minbuf = 0xFFFF;
		dp->cdr_dstat = dsp;
	}

	if ((flags & (F_MSINFO|F_TOC|F_LOAD|F_DLCK|F_EJECT)) == 0 ||
			tracks > 0 ||
			cuefilename != NULL)
	{
		if ((dp->cdr_flags & CDR_ISREADER) != 0) {
			errmsgno(EX_BAD,
					"Sorry, no CD/DVD-Recorder or unsupported CD/DVD-Recorder found on this target.\n");
		}

		if (!is_mmc(usalp, &is_cdwr, &is_dvdwr))
			is_cdwr = TRUE;			/* If it is not MMC, it must be a CD writer */

		if (is_dvdwr && !set_cdrcmds("mmc_mdvd", (cdr_t **)NULL)) {
			errmsgno(EX_BAD,
					"Internal error, DVD driver failure. Please report to debburn-devel@lists.alioth.debian.org.\n");
		}
		/*
		 * Only exit if this is not the ProDVD test binary.
		 */
		if (!is_cdwr)
			exit(EX_BAD);
	}

	/*
	 * Set up data structures for current drive state.
	 */
	if ((*dp->cdr_attach)(usalp, dp) != 0)
		comerrno(EX_BAD, "Cannot attach driver for CD/DVD-Recorder.\n");

	if (lverbose > 1) {
		printf("Drive current speed: %d\n", dp->cdr_dstat->ds_dr_cur_wspeed);
		printf("Drive default speed: %d\n", dp->cdr_speeddef);
		printf("Drive max speed    : %d\n", dp->cdr_speedmax);
	}
	if (speed > (int)dp->cdr_speedmax && (flags & F_FORCE) == 0)
		speed = dp->cdr_speedmax;
	if (speed < 0)
		speed = dp->cdr_speeddef;

	if (lverbose > 1) {
		printf("Selected speed     : %d\n", speed);
	}
	dp->cdr_dstat->ds_wspeed = speed; /* XXX Remove 'speed' in future */

	exargs.usalp	   = usalp;
	exargs.dp	   = dp;
	exargs.old_secsize = -1;
	exargs.flags	   = flags;

	if ((flags & F_MSINFO) == 0 || lverbose) {
		printf("Using %s (%s).\n", dp->cdr_drtext, dp->cdr_drname);
		print_drflags(dp);
		print_wrmodes(dp);
	}
	usalp->silent++;
	if ((debug || lverbose)) {
		tsize = -1;
		if ((*dp->cdr_buffer_cap)(usalp, &tsize, (long *)0) < 0 || tsize < 0) {
			if (read_buffer(usalp, buf, 4, 0) >= 0)
				tsize = a_to_u_4_byte(buf);
		}
		if (tsize > 0) {
			printf("Drive buf size : %lu = %lu KB\n",
						tsize, tsize >> 10);
		}
	}
	usalp->silent--;

	dma_speed = get_dmaspeed(usalp, dp);

	if ((debug || lverbose) && dma_speed > 0) {
		/*
		 * We do not yet know what medium type is in...
		 */
		printf("Drive DMA Speed: %d kB/s %dx CD %dx DVD\n",
			dma_speed, dma_speed/176, dma_speed/1385);
	}
	if ((tracks > 0 || cuefilename != NULL) && (debug || lverbose))
		printf("FIFO size      : %lu = %lu KB\n", fs, fs >> 10);

#ifdef	HAVE_LIB_EDC_ECC
	if ((flags & F_RAW) != 0 && (dp->cdr_dstat->ds_flags & DSF_DVD) == 0)
		raw_speed = encspeed(debug || lverbose);
#endif

	if ((flags & F_CHECKDRIVE) != 0)
		exit(0);

	if ((flags & F_ABORT) != 0) {
		/*
		 * flush cache is not supported by CD-ROMs avoid prob with -toc
		 */
		usalp->silent++;
		scsi_flush_cache(usalp, FALSE);
		(*dp->cdr_abort_session)(usalp, dp);
		usalp->silent--;
		exit(0);
	}

	if (tracks == 0 && cuefilename == NULL &&
	    (flags & (F_FIX|F_BLANK)) == 0 && (flags & F_EJECT) != 0) {
		/*
		 * Do not check if the unit is ready here to allow to open
		 * an empty unit too.
		 */
		unload_media(usalp, dp, flags);
		exit(0);
	}
	flush();

	if (cuefilename) {
		parsecue(cuefilename, track);
		tracks = track[0].tracks;
	} else {
		opentracks(track);
	}

	if (tracks > 1)
		sleep(2);	/* Let the user watch the inquiry messages */

	if (tracks > 0 && !check_wrmode(dp, flags, track[1].flags))
		comerrno(EX_BAD, "Illegal write mode for this drive.\n");

	if ((track[0].flags & TI_TEXT) == 0 &&	/* CD-Text not yet processed */
	    (track[MAX_TRACK+1].flags & TI_TEXT) != 0) {
		/*
		 * CD-Text from textfile= or from CUE CDTEXTFILE will win
		 * over CD-Text from *.inf files and over CD-Text from
		 * CUE SONGWRITER, ...
		 */
		packtext(tracks, track);
		track[0].flags |= TI_TEXT;
	}
#ifdef	CLONE_WRITE
	if (flags & F_CLONE) {
		clone_toc(track);
		clone_tracktype(track);
	}
#endif
	setleadinout(tracks, track);
	set_trsizes(dp, tracks, track);
	setpregaps(tracks, track);
	checkfiles(tracks, track);
	tsize = checktsize(tracks, track);

	/*
	 * Make wm2name[wrmode] work.
	 * This must be done after the track flags have been set up
	 * by the functions above.
	 */
	if (tracks == 0 && (flags & F_BLANK) != 0)
		dp->cdr_dstat->ds_wrmode = WM_BLANK;
	else if (tracks == 0 && (flags & F_FORMAT) != 0)
		dp->cdr_dstat->ds_wrmode = WM_FORMAT;
	else
		set_wrmode(dp, flags, track[1].flags);

	/*
	 * Debug only
	 */
	{
		void	*cp = NULL;

		(*dp->cdr_gen_cue)(track, &cp, FALSE);
		if (cp)
			free(cp);
	}

	/*
	 * Create Lead-in data. Only needed in RAW mode.
	 */
	do_leadin(track);


	/*
	 * Install exit handler before we change the drive status.
	 */
	on_comerr(exscsi, &exargs);

	if ((flags & F_FORCE) == 0)
		load_media(usalp, dp, TRUE);

	if ((flags & (F_LOAD|F_DLCK)) != 0) {
		if ((flags & F_DLCK) == 0) {
			usalp->silent++;		/* silently		*/
			scsi_prevent_removal(
				usalp, 0);	/* allow manual open	*/
			usalp->silent--;		/* if load failed...	*/
		}
		exit(0);			/* we did not change status */
	}
	exargs.old_secsize = sense_secsize(usalp, 1);
	if (exargs.old_secsize < 0)
		exargs.old_secsize = sense_secsize(usalp, 0);
	if (debug)
		printf("Current Secsize: %d\n", exargs.old_secsize);
	usalp->silent++;
	if (read_capacity(usalp) < 0) {
		if (exargs.old_secsize > 0)
			usalp->cap->c_bsize = exargs.old_secsize;
	}
	usalp->silent--;
	if (exargs.old_secsize < 0)
		exargs.old_secsize = usalp->cap->c_bsize;
	if (exargs.old_secsize != usalp->cap->c_bsize)
		errmsgno(EX_BAD, "Warning: blockdesc secsize %d differs from cap secsize %d\n",
				exargs.old_secsize, usalp->cap->c_bsize);

	if (lverbose)
		printf("Current Secsize: %d\n", exargs.old_secsize);

	if (exargs.old_secsize > 0 && exargs.old_secsize != DATA_SEC_SIZE) {
		/*
		 * Some drives (e.g. Plextor) don't like to write correctly
		 * in SAO mode if the sector size is set to 512 bytes.
		 * In addition, wodim -msinfo will not work properly
		 * if the sector size is not 2048 bytes.
		 */
		set_secsize(usalp, DATA_SEC_SIZE);
	}

	/*
	 * Is this the right place to do this ?
	 */
	check_recovery(usalp, dp, flags);

/*audioread(dp, flags);*/
/*unload_media(usalp, dp, flags);*/
/*return 0;*/
	if (flags & F_WRITE)
		dp->cdr_dstat->ds_cdrflags |= RF_WRITE;
	if (flags & F_BLANK)
		dp->cdr_dstat->ds_cdrflags |= RF_BLANK;
	if (flags & F_PRATIP || lverbose > 0) {
		dp->cdr_dstat->ds_cdrflags |= RF_PRATIP;
	}
	if (flags & F_IMMED || dminbuf > 0) {
		if (dminbuf <= 0)
			dminbuf = 50;
		if (lverbose <= 0)	/* XXX Hack needed for now */
			lverbose++;
		dp->cdr_dstat->ds_cdrflags |= RF_WR_WAIT;
	}
	if ((*dp->cdr_getdisktype)(usalp, dp) < 0) {
		errmsgno(EX_BAD, "Cannot get disk type.\n");
		if ((flags & F_FORCE) == 0)
			comexit(EX_BAD);
	}
	if (flags & F_PRATIP) {
		comexit(0);
	}
	/*
	 * The next actions should depend on the disk type.
	 */
	if (dma_speed > 0) {
		if ((dp->cdr_dstat->ds_flags & DSF_DVD) == 0)
			dma_speed /= 176;
		else
			dma_speed /= 1385;
	}

	/*
	 * Init drive to default modes:
	 *
	 * We set TAO unconditionally to make checkdsize() work
	 * currectly in SAO mode too.
	 *
	 * At least MMC drives will not return the next writable
	 * address we expect when the drive's write mode is set
	 * to SAO. We need this address for mkisofs and thus
	 * it must be the first user accessible sector and not the
	 * first sector of the pregap.
	 *
	 * XXX The ACER drive:
	 * XXX Vendor_info    : 'ATAPI   '
	 * XXX Identifikation : 'CD-R/RW 8X4X32  '
	 * XXX Revision       : '5.EW'
	 * XXX Will not return from -dummy to non-dummy without
	 * XXX opening the tray.
	 */
	usalp->silent++;
	if ((*dp->cdr_init)(usalp, dp) < 0)
		comerrno(EX_BAD, "Cannot init drive.\n");
	usalp->silent--;

	if (flags & F_SETDROPTS) {
		/*
		 * Note that the set speed function also contains
		 * drive option processing for speed related drive options.
		 */
		if ((*dp->cdr_opt1)(usalp, dp) < 0) {
			errmsgno(EX_BAD, "Cannot set up 1st set of driver options.\n");
		}
		if ((*dp->cdr_set_speed_dummy)(usalp, dp, &speed) < 0) {
			errmsgno(EX_BAD, "Cannot set speed/dummy.\n");
		}
		dp->cdr_dstat->ds_wspeed = speed; /* XXX Remove 'speed' in future */
		if ((*dp->cdr_opt2)(usalp, dp) < 0) {
			errmsgno(EX_BAD, "Cannot set up 2nd set of driver options.\n");
		}
		comexit(0);
	}
	/*
	 * XXX If dp->cdr_opt1() ever affects the result for
	 * XXX the multi session info we would need to move it here.
	 */
	if (flags & F_MSINFO) {
		print_msinfo(usalp, dp);
		comexit(0);
	}
	if (flags & F_TOC) {
		print_toc(usalp, dp);
		comexit(0);
	}
#ifdef	XXX
	if ((*dp->cdr_check_session)() < 0) {
		comexit(EX_BAD);
	}
#endif
	{
		Int32_t omb = dp->cdr_dstat->ds_maxblocks;

		if ((*dp->cdr_opt1)(usalp, dp) < 0) {
			errmsgno(EX_BAD, "Cannot set up 1st set of driver options.\n");
		}
		if (tsize > 0 && omb != dp->cdr_dstat->ds_maxblocks) {
			printf("Disk size changed by user options.\n");
			printf("Checking disk capacity according to new values.\n");
		}
	}
	if (tsize == 0) {
		if (tracks > 0) {
			errmsgno(EX_BAD,
			"WARNING: Total disk size unknown. Data may not fit on disk.\n");
		}
	} else if (tracks > 0) {
		/*
		 * XXX How do we let the user check the remaining
		 * XXX disk size witout starting the write process?
		 */
		if (!checkdsize(usalp, dp, tsize, flags))
			comexit(EX_BAD);
	}
	if (tracks > 0 && fs > 0l) {
#if defined(USE_POSIX_PRIORITY_SCHEDULING) && defined(HAVE_SETREUID)
		/*
		 * Hack to work around the POSIX design bug in real time
		 * priority handling: we need to be root even to lower
		 * our priority.
		 * Note that we need to find a more general way that works
		 * even on OS that do not support getreuid() which is *BSD
		 * and SUSv3 only.
		 */
		if (oeuid != getuid()) {
			if (setreuid(-1, oeuid) < 0)
				errmsg("Could set back effective uid.\n");
		}

#endif
		/*
		 * fork() here to start the extra process needed for
		 * improved buffering.
		 */
		if (!init_faio(track, bufsize))
			fs = 0L;
		else
			on_comerr(excdr, &exargs);

    atexit(fifo_cleanup);

#if defined(USE_POSIX_PRIORITY_SCHEDULING) && defined(HAVE_SETREUID)
		/*
		 * XXX Below this point we never need root privilleges anymore.
		 */
		if (geteuid() != getuid()) {	/* AIX does not like to do this */
						/* If we are not root		*/
			if (setreuid(-1, getuid()) < 0)
				comerr("Panic cannot set back effective uid.\n");
		}
#ifdef __linux__
		if (get_cap(CAP_SYS_RAWIO) && (debug || lverbose>2))
			perror("Error: Cannot gain SYS_RAWIO capability, is wodim installed SUID root? Reason");
#endif

#endif
	}

	if ((*dp->cdr_set_speed_dummy)(usalp, dp, &speed) < 0) {
		errmsgno(EX_BAD, "Cannot set speed/dummy.\n");
		if ((flags & F_FORCE) == 0)
			comexit(EX_BAD);
	}
	dp->cdr_dstat->ds_wspeed = speed; /* XXX Remove 'speed' in future */
	if ((flags & F_WRITE) != 0 && raw_speed >= 0) {
		int	max_raw = (flags & F_FORCE) != 0 ? raw_speed:raw_speed/2;

		if (getenv("CDR_FORCERAWSPEED"))
			max_raw = raw_speed;

		for (i = 1; i <= MAX_TRACK; i++) {
			/*
			 * Check for Clone tracks
			 */
			if ((track[i].sectype & ST_MODE_RAW) != 0)
				continue;
			/*
			 * Check for non-data tracks
			 */
			if ((track[i].sectype & ST_MODE_MASK) == ST_MODE_AUDIO)
				continue;

			if (speed > max_raw) {
				errmsgno(EX_BAD,
				"Processor too slow. Cannot write RAW data at speed %d.\n",
				speed);
				comerrno(EX_BAD, "Max RAW data speed on this processor is %d.\n",
				max_raw);
			}
			break;
		}
	}
	if (tracks > 0 && (flags & F_WRITE) != 0 && dma_speed > 0) {
		int max_dma = (dma_speed+1)*4/5; /* use an empirical formula to estimate available bandwith */

        if((flags & F_FORCE) != 0 || getenv("CDR_FORCESPEED"))
			max_dma = dma_speed;

		if (speed > max_dma) {
			errmsgno(EX_BAD,
			"DMA speed too slow (OK for %dx). Cannot write at speed %dx.\n",
					max_dma, speed);
			if ((dp->cdr_dstat->ds_cdrflags & RF_BURNFREE) == 0) {
				errmsgno(EX_BAD, "Max DMA data speed is %d.\n", max_dma);
				comerrno(EX_BAD, "Try to use 'driveropts=burnfree'.\n");
			}
		}
	}
	if ((flags & (F_WRITE|F_BLANK)) != 0 &&
				(dp->cdr_dstat->ds_flags & DSF_ERA) != 0) {
		if (xdebug) {
			printf("Current speed %d, medium low speed: %d medium high speed: %d\n",
				speed,
				dp->cdr_dstat->ds_at_min_speed,
				dp->cdr_dstat->ds_at_max_speed);
		}
		if (dp->cdr_dstat->ds_at_max_speed > 0 &&
				speed <= 8 &&
				speed > (int)dp->cdr_dstat->ds_at_max_speed) {
			/*
			 * Be careful here: 10x media may be written faster.
			 * The current code will work as long as there is no
			 * writer that can only write faster than 8x
			 */
			if ((flags & F_FORCE) == 0) {
				errmsgno(EX_BAD,
				"Write speed %d of medium not sufficient for this writer.\n",
					dp->cdr_dstat->ds_at_max_speed);
				comerrno(EX_BAD,
				"You may have used an ultra low speed medium on a high speed writer.\n");
			}
		}

		if ((dp->cdr_dstat->ds_flags & DSF_ULTRASPP_ERA) != 0 &&
		    (speed < 16 || (dp->cdr_cdrw_support & CDR_CDRW_ULTRAP) == 0)) {
			if ((dp->cdr_cdrw_support & CDR_CDRW_ULTRAP) == 0) {
				comerrno(EX_BAD,
				"Trying to use ultra high speed+ medium on a writer which is not\ncompatible with ultra high speed+ media.\n");
			} else if ((flags & F_FORCE) == 0) {
				comerrno(EX_BAD,
				"Probably trying to use ultra high speed+ medium on improper writer.\n");
			}
		} else if ((dp->cdr_dstat->ds_flags & DSF_ULTRASP_ERA) != 0 &&
		    (speed < 16 || (dp->cdr_cdrw_support & CDR_CDRW_ULTRA) == 0)) {
			if ((dp->cdr_cdrw_support & CDR_CDRW_ULTRA) == 0) {
				comerrno(EX_BAD,
				"Trying to use ultra high speed medium on a writer which is not\ncompatible with ultra high speed media.\n");
			} else if ((flags & F_FORCE) == 0) {
				comerrno(EX_BAD,
				"Probably trying to use ultra high speed medium on improper writer.\n");
			}
		}
		if (dp->cdr_dstat->ds_at_min_speed >= 4 &&
				dp->cdr_dstat->ds_at_max_speed > 4 &&
				dp->cdr_dstat->ds_dr_max_wspeed <= 4) {
			if ((flags & F_FORCE) == 0) {
				comerrno(EX_BAD,
				"Trying to use high speed medium on low speed writer.\n");
			}
		}
		if ((int)dp->cdr_dstat->ds_at_min_speed > speed) {
			if ((flags & F_FORCE) == 0) {
				errmsgno(EX_BAD,
				"Write speed %d of writer not sufficient for this medium.\n",
					speed);
				errmsgno(EX_BAD,
				"You did use a %s speed medium on an improper writer or\n",
				dp->cdr_dstat->ds_flags & DSF_ULTRASP_ERA ?
				"ultra high": "high");
				comerrno(EX_BAD,
				"you used a speed=# option with a speed too low for this medium.\n");
			}
		}
	}
	if ((flags & (F_BLANK|F_FORCE)) == (F_BLANK|F_FORCE)) {
		printf("Waiting for drive to calm down.\n");
		wait_unit_ready(usalp, 120);
		if (gracewait(dp, &gracedone) < 0) {
			/*
			 * In case kill() did not work ;-)
			 */
			errs++;
			goto restore_it;
		}
		scsi_blank(usalp, 0L, blanktype, FALSE);
	}

	/*
	 * Last chance to quit!
	 */
	if (gracewait(dp, &gracedone) < 0) {
		/*
		 * In case kill() did not work ;-)
		 */
		errs++;
		goto restore_it;
	}
 	
 	if (dp->profile == 0x2B && flags & F_SAO && tsize > 0) {
 	    printf("Preparing middle zone location for this DVD+R dual layer disc\n");
 	    if (!dp->cdr_layer_split(usalp, dp, tsize)) {
 		errmsgno(EX_BAD, "Cannot send structure for middle zone location.\n");
 		comexit(EX_BAD);
 	    }
 	}

	if (tracks > 0 && fs > 0l) {
		/*
		 * Wait for the read-buffer to become full.
		 * This should be take no extra time if the input is a file.
		 * If the input is a pipe (e.g. mkisofs) this can take a
		 * while. If mkisofs dumps core before it starts writing,
		 * we abort before the writing process started.
		 */
		if (!await_faio()) {
			comerrno(EX_BAD, "Input buffer error, aborting.\n");
		}
	}
	wait_unit_ready(usalp, 120);

	starttime.tv_sec = 0;
	wstarttime.tv_sec = 0;
	stoptime.tv_sec = 0;
	fixtime.tv_sec = 0;
	if (gettimeofday(&starttime, (struct timezone *)0) < 0)
		errmsg("Cannot get start time\n");

	/*
	 * Blank the media if we were requested to do so
	 */
	if (flags & F_BLANK) {
		/*
		 * Do not abort if OPC failes. Just give it a chance
		 * for better laser power calibration than without OPC.
		 *
		 * Ricoh drives return with a vendor unique sense code.
		 * This is most likely because they refuse to do OPC
		 * on a non blank media.
		 */
		usalp->silent++;
		do_opc(usalp, dp, flags);
		usalp->silent--;
		wait_unit_ready(usalp, 120);
		if (gettimeofday(&starttime, (struct timezone *)0) < 0)
			errmsg("Cannot get start time\n");

		if ((*dp->cdr_blank)(usalp, dp, 0L, blanktype) < 0) {
			errmsgno(EX_BAD, "Cannot blank disk, aborting.\n");
			if (blanktype != BLANK_DISC) {
				errmsgno(EX_BAD, "Some drives do not support all blank types.\n");
				errmsgno(EX_BAD, "Try again with wodim blank=all.\n");
			}
			comexit(EX_BAD);
		}
		if (gettimeofday(&fixtime, (struct timezone *)0) < 0)
			errmsg("Cannot get blank time\n");
		if (lverbose)
			prtimediff("Blanking time: ", &starttime, &fixtime);

		/*
		 * XXX Erst blank und dann format?
		 * XXX Wenn ja, dann hier (flags & F_FORMAT) testen
		 *
		 * EB: nee, besser nicht
		 */
		if (!wait_unit_ready(usalp, 240) || tracks == 0) {
			comexit(0);
		}
	}
	if (flags & F_FORMAT) {
		printf("wodim: media format asked\n");
		/*
		* Do not abort if OPC failes. Just give it a chance
		* for better laser power calibration than without OPC.
		*
		* Ricoh drives return with a vendor unique sense code.
		* This is most likely because they refuse to do OPC
		* on a non blank media.
		*/
		usalp->silent++;
		do_opc(usalp, dp, flags);
		usalp->silent--;
		wait_unit_ready(usalp, 120);
		if (gettimeofday(&starttime, (struct timezone *)0) < 0)
			errmsg("Cannot get start time\n");

		if ((*dp->cdr_format)(usalp, dp, formattype) < 0) {
			errmsgno(EX_BAD, "Cannot format disk, aborting.\n");
			comexit(EX_BAD);
		}
		if (gettimeofday(&fixtime, (struct timezone *)0) < 0)
			errmsg("Cannot get format time\n");
		if (lverbose)
			prtimediff("Formatting time: ", &starttime, &fixtime);

		if (!wait_unit_ready(usalp, 240) || tracks == 0) {
			comexit(0);
		}
		if (gettimeofday(&starttime, (struct timezone *)0) < 0)
			errmsg("Cannot get start time\n");
	}
	/*
	* Reset start time so we will not see blanking time and
	* writing time counted together.
	*/
	if (gettimeofday(&starttime, (struct timezone *)0) < 0)
		errmsg("Cannot get start time\n");
	if (tracks == 0 && (flags & F_FIX) == 0)
	comerrno(EX_BAD, "No tracks found.\n");
	/*
	 * Get the number of the next recordable track by reading the TOC and
	 * use the number the last current track number.
	 */
	usalp->silent++;
	if (read_tochdr(usalp, dp, NULL, &trackno) < 0) {
		trackno = 0;
	}
	usalp->silent--;
      
	/* If it is DVD, the information in TOC is fabricated :)
	   The real information is from read disk info command*/
	if((dp->cdr_dstat->ds_disktype&DT_DVD) && (dp->cdr_dstat->ds_trlast>0)){
		trackno=dp->cdr_dstat->ds_trlast-1;
		if (lverbose > 2)
			printf("trackno=%d\n",trackno);
	}

	if ((tracks + trackno) > MAX_TRACK) {
		/*
		 * XXX How many tracks are allowed on a DVD?
		 */
		comerrno(EX_BAD, "Too many tracks for this disk, last track number is %d.\n",
				tracks + trackno);
	}

	for (i = 0; i <= tracks+1; i++) {	/* Lead-in ... Lead-out */
		track[i].trackno = i + trackno;	/* Set up real track #	*/
	}

	if ((*dp->cdr_opt2)(usalp, dp) < 0) {
		errmsgno(EX_BAD, "Cannot set up 2nd set of driver options.\n");
	}

	/*
	 * Now we actually start writing to the CD/DVD.
	 * XXX Check total size of the tracks and remaining size of disk.
	 */
	if ((*dp->cdr_open_session)(usalp, dp, track) < 0) {
		comerrno(EX_BAD, "Cannot open new session.\n");
	}
	if (!do_opc(usalp, dp, flags))
		comexit(EX_BAD);

	/*
	 * As long as open_session() will do nothing but
	 * set up parameters, we may leave fix_it here.
	 * I case we have to add an open_session() for a drive
	 * that wants to do something that modifies the disk
	 * We have to think about a new solution.
	 */
	if (flags & F_FIX)
		goto fix_it;

	/*
	 * This call may modify trackp[i].trackstart for all tracks.
	 */
	if ((*dp->cdr_write_leadin)(usalp, dp, track) < 0)
		comerrno(EX_BAD, "Could not write Lead-in.\n");

	if (lverbose && (dp->cdr_dstat->ds_cdrflags & RF_LEADIN) != 0) {

		if (gettimeofday(&fixtime, (struct timezone *)0) < 0)
			errmsg("Cannot get lead-in write time\n");
		prtimediff("Lead-in write time: ", &starttime, &fixtime);
	}

	if (gettimeofday(&wstarttime, (struct timezone *)0) < 0)
		errmsg("Cannot get start time\n");
	for (i = 1; i <= tracks; i++) {
		startsec = 0L;

		if ((*dp->cdr_open_track)(usalp, dp, &track[i]) < 0) {
			errmsgno(EX_BAD, "Cannot open next track.\n");
			errs++;
			break;
		}

		if ((flags & (F_SAO|F_RAW)) == 0) {
			if ((*dp->cdr_next_wr_address)(usalp, &track[i], &startsec) < 0) {
				errmsgno(EX_BAD, "Cannot get next writable address.\n");
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
		if (write_track_data(usalp, dp, &track[i]) < 0) {
			if (cdr_underrun(usalp)) {
				errmsgno(EX_BAD,
				"The current problem looks like a buffer underrun.\n");
				if ((dp->cdr_dstat->ds_cdrflags & RF_BURNFREE) == 0)
					errmsgno(EX_BAD, "Try to use 'driveropts=burnfree'.\n");
				else {
					errmsgno(EX_BAD, "It looks like 'driveropts=burnfree' does not work for this drive.\n");
					errmsgno(EX_BAD, "Please report.\n");
				}

				errmsgno(EX_BAD,
				"Make sure that you are root, enable DMA and check your HW/OS set up.\n");
			} else {
				errmsgno(EX_BAD, "A write error occured.\n");
				errmsgno(EX_BAD, "Please properly read the error message above.\n");
			}
			errs++;
			sleep(5);
			unit_ready(usalp);
			(*dp->cdr_close_track)(usalp, dp, &track[i]);
			break;
		}
		if ((*dp->cdr_close_track)(usalp, dp, &track[i]) < 0) {
			/*
			 * Check for "Dummy blocks added" message first.
			 */
			if (usal_sense_key(usalp) != SC_ILLEGAL_REQUEST ||
					usal_sense_code(usalp) != 0xB5) {
				errmsgno(EX_BAD, "Cannot close track.\n");
				errs++;
				break;
			}
		}
	}
fix_it:
	if (gettimeofday(&stoptime, (struct timezone *)0) < 0)
		errmsg("Cannot get stop time\n");
	cdrstats(dp);

	if (flags & F_RAW) {
		if (lverbose) {
			printf("Writing Leadout...\n");
			flush();
		}
		write_leadout(usalp, dp, track);
	}
	if ((flags & F_NOFIX) == 0) {
		if (lverbose) {
			printf("Fixating...\n");
			flush();
		}
		if ((*dp->cdr_fixate)(usalp, dp, track) < 0) {
			/*
			 * Ignore fixating errors in dummy mode.
			 */
			if ((flags & F_DUMMY) == 0) {
				errmsgno(EX_BAD, "Cannot fixate disk.\n");
				errs++;
			}
		}
		if (gettimeofday(&fixtime, (struct timezone *)0) < 0)
			errmsg("Cannot get fix time\n");
		if (lverbose)
			prtimediff("Fixating time: ", &stoptime, &fixtime);
	}
	if ((dp->cdr_dstat->ds_cdrflags & RF_DID_CDRSTAT) == 0) {
		dp->cdr_dstat->ds_cdrflags |= RF_DID_CDRSTAT;
		(*dp->cdr_stats)(usalp, dp);
	}
	if ((flags & (F_RAW|F_EJECT)) == F_RAW) {
		/*
		 * Most drives seem to forget to reread the TOC from disk
		 * if they are in RAW mode.
		 */
		usalp->silent++;
		if (read_tochdr(usalp, dp, NULL, NULL) < 0) {
			usalp->silent--;
			if ((flags & F_DUMMY) == 0)
				reload_media(usalp, dp);
		} else {
			usalp->silent--;
		}
	}

restore_it:
	/*
	 * Try to restore the old sector size and stop FIFO.
	 */
	kill_faio();
	comexit(errs?-2:0);
	return (0);
}

static int 
gracewait(cdr_t *dp, BOOL *didgracep)
{
	int	i;
	BOOL	didgrace = FALSE;

	if (didgracep)
		didgrace = *didgracep;

	if(warn_minisize>=0) {
		fprintf(stderr,  "\nWARNING: found a microscopic small track size (%lld bytes).\n"
				"         Do you really want to write this image? Press Ctrl-C to abort...\n\n",
				warn_minisize);
		gracetime+=20;
	}
	if (gracetime < MIN_GRACE_TIME)
		gracetime = MIN_GRACE_TIME;
	if (gracetime > 999)
		gracetime = 999;

	printf("Starting to write CD/DVD at speed %5.1f in %s%s %s mode for %s session.\n",
         (float)dp->cdr_dstat->ds_wspeed,
		(dp->cdr_cmdflags & F_DUMMY) ? "dummy" : "real",
		(dp->cdr_cmdflags & F_FORCE) ? " force" : "",
		wm2name[dp->cdr_dstat->ds_wrmode],
		(dp->cdr_cmdflags & F_MULTI) ? "multi" : "single");
	if (didgrace) {
		printf("No chance to quit anymore.");
		goto grace_done;
	}
	printf("Last chance to quit, starting %s write in %4d seconds.",
		(dp->cdr_cmdflags & F_DUMMY)?"dummy":"real", gracetime);
	flush();
	signal(SIGINT, intr);
	signal(SIGHUP, intr);
	signal(SIGTERM, intr);

	for (i = gracetime; --i >= 0; ) {
		sleep(1);
		if (didintr) {
			printf("\n");
			excdr(SIGINT, &exargs);
			signal(SIGINT, SIG_DFL);
			kill(getpid(), SIGINT);
			/*
			 * In case kill() did not work ;-)
			 */
			if (didgracep)
				*didgracep = FALSE;
			return (-1);
		}
		printf("\b\b\b\b\b\b\b\b\b\b\b\b\b%4d seconds.", i);
		flush();
	}
grace_done:
	printf(" Operation starts.");
	flush();
	signal(SIGINT, SIG_DFL);
	signal(SIGHUP, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGINT, intfifo);
	signal(SIGHUP, intfifo);
	signal(SIGTERM, intfifo);
	printf("\n");

	if (didgracep)
		*didgracep = TRUE;
	return (0);
}

static void 
cdrstats(cdr_t *dp)
{
	float	secsps = 75.0;
	int	nsecs;
	float	fspeed;
	struct timeval	tcur;
	struct timeval	tlast;
	BOOL	nostop = FALSE;

	if (starttime.tv_sec == 0)
		return;

	if (stoptime.tv_sec == 0) {
		gettimeofday(&stoptime, (struct timezone *)0);
		nostop = TRUE;
	}

	if ((dp->cdr_dstat->ds_cdrflags & RF_DID_STAT) != 0)
		return;
	dp->cdr_dstat->ds_cdrflags |= RF_DID_STAT;

	if (lverbose == 0)
		return;

	if (dp->cdr_cmdflags & F_FIX)
		return;

	if ((dp->cdr_cmdflags & (F_WRITE|F_BLANK)) == F_BLANK)
		return;

	tlast = wstarttime;
	tcur = stoptime;

	prtimediff("Writing  time: ", &starttime, &stoptime);

	nsecs = dp->cdr_dstat->ds_endsec - dp->cdr_dstat->ds_startsec;

	if (dp->cdr_dstat->ds_flags & DSF_DVD)
		secsps = 676.27;

	tlast.tv_sec = tcur.tv_sec - tlast.tv_sec;
	tlast.tv_usec = tcur.tv_usec - tlast.tv_usec;
	while (tlast.tv_usec < 0) {
		tlast.tv_usec += 1000000;
		tlast.tv_sec -= 1;
	}
	if (!nostop && nsecs != 0 && dp->cdr_dstat->ds_endsec > 0) {
		/*
		 * May not be known (e.g. cdrecord -)
		 *
		 * XXX if we later allow this code to know how much has
		 * XXX actually been written, then we may remove the
		 * XXX dependance from nostop & nsecs != 0
		 */
		fspeed = (nsecs / secsps) /
			(tlast.tv_sec * 1.0 + tlast.tv_usec * 0.000001);
		if (fspeed > 999.0)
			fspeed = 999.0;
      if (dp->is_dvd) fspeed /= 9;
		printf("Average write speed %5.1fx.\n", fspeed);
	}

	if (dp->cdr_dstat->ds_minbuf <= 100) {
		printf("Min drive buffer fill was %u%%\n",
			(unsigned int)dp->cdr_dstat->ds_minbuf);
	}
	if (dp->cdr_dstat->ds_buflow > 0) {
		printf("Total of %ld possible drive buffer underruns predicted.\n",
			(long)dp->cdr_dstat->ds_buflow);
	}
}

/*
 * Short usage
 */
static void
susage(int ret)
{
	fprintf(stderr, "Usage: %s [options] track1...trackn\n", get_progname());
	fprintf(stderr, "\nUse\t%s -help\n", get_progname());
	fprintf(stderr, "to get a list of valid options.\n");
	fprintf(stderr, "\nUse\t%s blank=help\n", get_progname());
	fprintf(stderr, "to get a list of valid blanking options.\n");
	fprintf(stderr, "\nUse\t%s dev=b,t,l driveropts=help -checkdrive\n", get_progname());
	fprintf(stderr, "to get a list of drive specific options.\n");
	fprintf(stderr, "\nUse\t%s dev=help\n", get_progname());
	fprintf(stderr, "to get a list of possible SCSI transport specifiers.\n");
	exit(ret);
	/* NOTREACHED */
}

static void 
usage(int excode)
{
	fprintf(stderr, "Usage: %s [options] track1...trackn\n", get_progname());
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "\t-version	print version information and exit\n");
	fprintf(stderr, "\tdev=target	SCSI target to use as CD/DVD-Recorder\n");
	fprintf(stderr, "\tgracetime=#	set the grace time before starting to write to #.\n");
	fprintf(stderr, "\ttimeout=#	set the default SCSI command timeout to #.\n");
	fprintf(stderr, "\tdebug=#,-d	Set to # or increment misc debug level\n");
	fprintf(stderr, "\tkdebug=#,kd=#	do Kernel debugging\n");
	fprintf(stderr, "\t-verbose,-v	increment general verbose level by one\n");
	fprintf(stderr, "\t-Verbose,-V	increment SCSI command transport verbose level by one\n");
	fprintf(stderr, "\t-silent,-s	do not print status of failed SCSI commands\n");
	fprintf(stderr, "\tdriver=name	user supplied driver name, use with extreme care\n");
	fprintf(stderr, "\tdriveropts=opt	a comma separated list of driver specific options\n");
	fprintf(stderr, "\t-setdropts	set driver specific options and exit\n");
	fprintf(stderr, "\t-checkdrive	check if a driver for the drive is present\n");
	fprintf(stderr, "\t-prcap		print drive capabilities for MMC compliant drives\n");
	fprintf(stderr, "\t-inq		do an inquiry for the drive and exit\n");
 	fprintf(stderr, "\t-scanbus	scan the SCSI and IDE buses and exit\n");
	fprintf(stderr, "\t-reset		reset the SCSI bus with the cdrecorder (if possible)\n");
	fprintf(stderr, "\t-abort		send an abort sequence to the drive (may help if hung)\n");
	fprintf(stderr, "\t-overburn	allow to write more than the official size of a medium\n");
	fprintf(stderr, "\t-ignsize	ignore the known size of a medium (may cause problems)\n");
	fprintf(stderr, "\t-useinfo	use *.inf files to overwrite audio options.\n");
	fprintf(stderr, "\tspeed=#		set speed of drive\n");
	fprintf(stderr, "\tblank=type	blank a CD-RW disc (see blank=help)\n");
	fprintf(stderr, "\t-format		format a CD-RW/DVD-RW/DVD+RW disc\n");
   fprintf(stderr, "\tformattype=#	select the format method for DVD+RW disc\n");
#ifdef	FIFO
	fprintf(stderr, "\tfs=#		Set fifo size to # (0 to disable, default is %ld MB)\n",
							DEFAULT_FIFOSIZE/(1024L*1024L));
#endif
	fprintf(stderr, "\tts=#		set maximum transfer size for a single SCSI command\n");
	fprintf(stderr, "\t-load		load the disk and exit (works only with tray loader)\n");
	fprintf(stderr, "\t-lock		load and lock the disk and exit (works only with tray loader)\n");
	fprintf(stderr, "\t-eject		eject the disk after doing the work\n");
	fprintf(stderr, "\t-dummy		do everything with laser turned off\n");
	fprintf(stderr, "\t-msinfo		retrieve multi-session info for genisoimage\n");
	fprintf(stderr, "\t-msifile=path	run -msinfo and copy output to file\n");
	fprintf(stderr, "\t-toc		retrieve and print TOC/PMA data\n");
	fprintf(stderr, "\t-atip		retrieve and print ATIP data\n");
	fprintf(stderr, "\t-multi		generate a TOC that allows multi session\n");
	fprintf(stderr, "\t		In this case default track type is CD-ROM XA mode 2 form 1 - 2048 bytes\n");
	fprintf(stderr, "\t-fix		fixate a corrupt or unfixated disk (generate a TOC)\n");
	fprintf(stderr, "\t-nofix		do not fixate disk after writing tracks\n");
	fprintf(stderr, "\t-waiti		wait until input is available before opening SCSI\n");
	fprintf(stderr, "\t-immed		Try to use the SCSI IMMED flag with certain long lasting commands\n");
	fprintf(stderr, "\t-force		force to continue on some errors to allow blanking bad disks\n");
	fprintf(stderr, "\t-tao		Write disk in TAO mode.\n");
	fprintf(stderr, "\t-dao		Write disk in SAO mode.\n");
	fprintf(stderr, "\t-sao		Write disk in SAO mode.\n");
	fprintf(stderr, "\t-raw		Write disk in RAW mode.\n");
	fprintf(stderr, "\t-raw96r		Write disk in RAW/RAW96R mode.\n");
	fprintf(stderr, "\t-raw96p		Write disk in RAW/RAW96P mode.\n");
	fprintf(stderr, "\t-raw16		Write disk in RAW/RAW16 mode.\n");
#ifdef	CLONE_WRITE
	fprintf(stderr, "\t-clone		Write disk in clone write mode.\n");
#endif
	fprintf(stderr, "\ttsize=#		Length of valid data in next track\n");
	fprintf(stderr, "\tpadsize=#	Amount of padding for next track\n");
	fprintf(stderr, "\tpregap=#	Amount of pre-gap sectors before next track\n");
	fprintf(stderr, "\tdefpregap=#	Amount of pre-gap sectors for all but track #1\n");
	fprintf(stderr, "\tmcn=text	Set the media catalog number for this CD to 'text'\n");
	fprintf(stderr, "\tisrc=text	Set the ISRC number for the next track to 'text'\n");
	fprintf(stderr, "\tindex=list	Set the index list for the next track to 'list'\n");
	fprintf(stderr, "\t-text		Write CD-Text from information from *.inf or *.cue files\n");
	fprintf(stderr, "\ttextfile=name	Set the file with CD-Text data to 'name'\n");
	fprintf(stderr, "\tcuefile=name	Set the file with CDRWIN CUE data to 'name'\n");

	fprintf(stderr, "\t-audio		Subsequent tracks are CD-DA audio tracks\n");
	fprintf(stderr, "\t-data		Subsequent tracks are CD-ROM data mode 1 - 2048 bytes (default)\n");
	fprintf(stderr, "\t-mode2		Subsequent tracks are CD-ROM data mode 2 - 2336 bytes\n");
	fprintf(stderr, "\t-xa		Subsequent tracks are CD-ROM XA mode 2 form 1 - 2048 bytes\n");
	fprintf(stderr, "\t-xa1		Subsequent tracks are CD-ROM XA mode 2 form 1 - 2056 bytes\n");
	fprintf(stderr, "\t-xa2		Subsequent tracks are CD-ROM XA mode 2 form 2 - 2324 bytes\n");
	fprintf(stderr, "\t-xamix		Subsequent tracks are CD-ROM XA mode 2 form 1/2 - 2332 bytes\n");
	fprintf(stderr, "\t-cdi		Subsequent tracks are CDI tracks\n");
	fprintf(stderr, "\t-isosize	Use iso9660 file system size for next data track\n");
	fprintf(stderr, "\t-preemp		Audio tracks are mastered with 50/15 microseconds preemphasis\n");
	fprintf(stderr, "\t-nopreemp	Audio tracks are mastered with no preemphasis (default)\n");
	fprintf(stderr, "\t-copy		Audio tracks have unlimited copy permission\n");
	fprintf(stderr, "\t-nocopy		Audio tracks may only be copied once for personal use (default)\n");
	fprintf(stderr, "\t-scms		Audio tracks will not have any copy permission at all\n");
	fprintf(stderr, "\t-pad		Pad data tracks with %d zeroed sectors\n", PAD_SECS);
	fprintf(stderr, "\t		Pad audio tracks to a multiple of %d bytes\n", AUDIO_SEC_SIZE);
	fprintf(stderr, "\t-nopad		Do not pad data tracks (default)\n");
	fprintf(stderr, "\t-shorttrack	Subsequent tracks may be non Red Book < 4 seconds if in SAO or RAW mode\n");
	fprintf(stderr, "\t-noshorttrack	Subsequent tracks must be >= 4 seconds\n");
	fprintf(stderr, "\t-swab		Audio data source is byte-swapped (little-endian/Intel)\n");
	fprintf(stderr, "The type of the first track is used for the toc type.\n");
	fprintf(stderr, "Currently only form 1 tracks are supported.\n");
	exit(excode);
}

static void 
blusage(int ret)
{
	fprintf(stderr, "Blanking options:\n");
	fprintf(stderr, "\tall\t\tblank the entire disk\n");
	fprintf(stderr, "\tdisc\t\tblank the entire disk\n");
	fprintf(stderr, "\tdisk\t\tblank the entire disk\n");
	fprintf(stderr, "\tfast\t\tminimally blank the entire disk (PMA, TOC, pregap)\n");
	fprintf(stderr, "\tminimal\t\tminimally blank the entire disk (PMA, TOC, pregap)\n");
	fprintf(stderr, "\ttrack\t\tblank a track\n");
	fprintf(stderr, "\tunreserve\tunreserve a track\n");
	fprintf(stderr, "\ttrtail\t\tblank a track tail\n");
	fprintf(stderr, "\tunclose\t\tunclose last session\n");
	fprintf(stderr, "\tsession\t\tblank last session\n");

	exit(ret);
	/* NOTREACHED */
}

static void 
formattypeusage(int ret)
{
	fprintf(stderr, "Formating options:\n");
	fprintf(stderr, "\tfull\t\tstandard formating\n");
	fprintf(stderr, "\tbackground\t\tbackground formating\n");
	fprintf(stderr, "\tforce\t\tforce reformat\n");

	exit(ret);
	/* NOTREACHED */
}

/* ARGSUSED */
static void
intr(int sig)
{
	sig = 0;	/* Fake usage for gcc */

	signal(SIGINT, intr);

	didintr++;
}

static void 
catchsig(int sig)
{
	signal(sig, catchsig);
}

static int 
scsi_cb(void *arg)
{
	comexit(EX_BAD);
	/* NOTREACHED */
	return (0);	/* Keep lint happy */
}

static void 
intfifo(int sig)
{
	errmsgno(EX_BAD, "Caught interrupt.\n");
	if (exargs.usalp) {
		SCSI	*usalp = exargs.usalp;

		if (usalp->running) {
			if (usalp->cb_fun != NULL) {
				comerrno(EX_BAD, "Second interrupt. Doing hard abort.\n");
				/* NOTREACHED */
			}
			usalp->cb_fun = scsi_cb;
			usalp->cb_arg = &exargs;
			return;
		}
	}
	comexit(sig);
}

/* ARGSUSED */
static void 
exscsi(int excode, void *arg)
{
	struct exargs	*exp = (struct exargs *)arg;

	/*
	 * Try to restore the old sector size.
	 */
	if (exp != NULL && exp->exflags == 0) {
		if (exp->usalp->running) {
			return;
		}
		/*
		 * flush cache is not supported by CD-ROMs avoid prob with -toc
		 */
		exp->usalp->silent++;
		scsi_flush_cache(exp->usalp, FALSE);
		(*exp->dp->cdr_abort_session)(exp->usalp, exp->dp);
		exp->usalp->silent--;
		set_secsize(exp->usalp, exp->old_secsize);
		unload_media(exp->usalp, exp->dp, exp->flags);

		exp->exflags++;	/* Make sure that it only get called once */
	}
}

static void 
excdr(int excode, void *arg)
{
	struct exargs	*exp = (struct exargs *)arg;

	exscsi(excode, arg);

	cdrstats(exp->dp);
	if ((exp->dp->cdr_dstat->ds_cdrflags & RF_DID_CDRSTAT) == 0) {
		exp->dp->cdr_dstat->ds_cdrflags |= RF_DID_CDRSTAT;
		(*exp->dp->cdr_stats)(exp->usalp, exp->dp);
	}

#ifdef	FIFO
	kill_faio();
	wait_faio();
	if (debug || lverbose)
		fifo_stats();
#endif
}

int 
read_buf(int f, char *bp, int size)
{
	char	*p = bp;
	int	amount = 0;
	int	n;

	do {
		do {
			n = read(f, p, size-amount);
		} while (n < 0 && (geterrno() == EAGAIN || geterrno() == EINTR));
		if (n < 0)
			return (n);
		amount += n;
		p += n;

	} while (amount < size && n > 0);
	return (amount);
}

int 
fill_buf(int f, track_t *trackp, long secno, char *bp, int size)
{
	int	amount = 0;
	int	nsecs;
	int	rsize;
	int	rmod;
	int	readoffset = 0;

	nsecs = size / trackp->secsize;
	if (nsecs < trackp->secspt) {
		/*
		 * Clear buffer to prepare for last transfer.
		 * Make sure that a partial sector ends with NULs
		 */
		fillbytes(bp, trackp->secspt * trackp->secsize, '\0');
	}

	if (!is_raw(trackp)) {
		amount = read_buf(f, bp, size);
		if (amount != size) {
			if (amount < 0)
				return (amount);
			/*
			 * We got less than expected, clear rest of buf.
			 */
			fillbytes(&bp[amount], size-amount, '\0');
		}
		if (is_swab(trackp))
			swabbytes(bp, amount);
		return (amount);
	}

	rsize = nsecs * trackp->isecsize;
	rmod  = size % trackp->secsize;
	if (rmod > 0) {
		rsize += rmod;
		nsecs++;
	}

	readoffset = trackp->dataoff;
	amount = read_buf(f, bp + readoffset, rsize);
	if (is_swab(trackp))
		swabbytes(bp + readoffset, amount);

	if (trackp->isecsize == 2448 && trackp->secsize == 2368)
		subrecodesecs(trackp, (Uchar *)bp, secno, nsecs);

	scatter_secs(trackp, bp + readoffset, nsecs);

	if (amount != rsize) {
		if (amount < 0)
			return (amount);
		/*
		 * We got less than expected, clear rest of buf.
		 */
		fillbytes(&bp[amount], rsize-amount, '\0');
		nsecs = amount / trackp->isecsize;
		rmod  = amount % trackp->isecsize;
		amount = nsecs * trackp->secsize;
		if (rmod > 0) {
			nsecs++;
			amount += rmod;
		}
	} else {
		amount = size;
	}
	if ((trackp->sectype & ST_MODE_RAW) == 0) {
		encsectors(trackp, (Uchar *)bp, secno, nsecs);
		fillsubch(trackp, (Uchar *)bp, secno, nsecs);
	} else {
		scrsectors(trackp, (Uchar *)bp, secno, nsecs);
	}
	return (amount);
}

int 
get_buf(int f, track_t *trackp, long secno, char **bpp, int size)
{
	if (fs > 0) {
/*		return (faio_read_buf(f, *bpp, size));*/
		return (faio_get_buf(f, bpp, size));
	} else {
		return (fill_buf(f, trackp, secno, *bpp, size));
	}
}

int 
write_secs(SCSI *usalp, cdr_t *dp, char *bp, long startsec, int bytespt, 
        		int secspt, BOOL islast)
{
	int	amount;

again:
	usalp->silent++;
	amount = (*dp->cdr_write_trackdata)(usalp, bp, startsec, bytespt, secspt, islast);
	usalp->silent--;
	if (amount < 0) {
		if (scsi_in_progress(usalp)) {
			/*
			 * If we sleep too long, the drive buffer is empty
			 * before we start filling it again. The max. CD speed
			 * is ~ 10 MB/s (52x RAW writing). The max. DVD speed
			 * is ~ 25 MB/s (18x DVD 1385 kB/s).
			 * With 10 MB/s, a 1 MB buffer empties within 100ms.
			 * With 25 MB/s, a 1 MB buffer empties within 40ms.
			 */
			if ((dp->cdr_dstat->ds_flags & DSF_DVD) == 0) {
				usleep(60000);
			} else {
#ifndef	_SC_CLK_TCK
				usleep(20000);
#else
				if (sysconf(_SC_CLK_TCK) < 100)
					usleep(20000);
				else
					usleep(10000);

#endif
			}
			goto again;
		}
		return (-1);
	}
	return (amount);
}

static int 
write_track_data(SCSI *usalp, cdr_t *dp, track_t *trackp)
{
	int	track = trackp->trackno;
	int	f = -1;
	int	isaudio;
	long	startsec;
	Llong	bytes_read = 0;
	Llong	bytes	= 0;
	Llong	savbytes = 0;
	int	count;
	Llong	tracksize;
	int	secsize;
	int	secspt;
	int	bytespt;
	int	bytes_to_read;
	long	amount;
	int	pad;
	BOOL	neednl	= FALSE;
	BOOL	islast	= FALSE;
	char	*bp	= buf;
	struct timeval tlast;
	struct timeval tcur;
	float	secsps = 75.0;
long bsize;
long bfree;
#define	BCAP
#ifdef	BCAP
int per = 0;
#ifdef	XBCAP
int oper = -1;
#endif
#endif

	if (dp->cdr_dstat->ds_flags & DSF_DVD)
		secsps = 676.27;

	usalp->silent++;
	if ((*dp->cdr_buffer_cap)(usalp, &bsize, &bfree) < 0)
		bsize = -1L;
	if (bsize == 0)		/* If we have no (known) buffer, we cannot */
		bsize = -1L;	/* retrieve the buffer fill ratio	   */
	usalp->silent--;


	if (is_packet(trackp))	/* XXX Ugly hack for now */
		return (write_packet_data(usalp, dp, trackp));

	if (trackp->xfp != NULL)
		f = xfileno(trackp->xfp);

	isaudio = is_audio(trackp);
	tracksize = trackp->tracksize;
	startsec = trackp->trackstart;

	secsize = trackp->secsize;
	secspt = trackp->secspt;
	bytespt = secsize * secspt;

	pad = !isaudio && is_pad(trackp);	/* Pad only data tracks */

	if (debug) {
		printf("secsize:%d secspt:%d bytespt:%d audio:%d pad:%d\n",
			secsize, secspt, bytespt, isaudio, pad);
	}

	if (lverbose) {
		if (tracksize > 0)
			printf("\rTrack %02d:    0 of %4lld MB written.",
				track, tracksize >> 20);
		else
			printf("\rTrack %02d:    0 MB written.", track);
		flush();
		neednl = TRUE;
	}

	gettimeofday(&tlast, (struct timezone *)0);
	do {
		bytes_to_read = bytespt;
		if (tracksize > 0) {
			if ((tracksize - bytes_read) > bytespt)
				bytes_to_read = bytespt;
			else
				bytes_to_read = tracksize - bytes_read;
		}
		count = get_buf(f, trackp, startsec, &bp, bytes_to_read);

		if (count < 0)
			comerr("read error on input file\n");
		if (count == 0)
			break;
		bytes_read += count;
		if (tracksize >= 0 && bytes_read >= tracksize) {
			count -= bytes_read - tracksize;
			/*
			 * Paranoia: tracksize is known (trackp->tracksize >= 0)
			 * At this point, trackp->padsize should alway be set
			 * if the tracksize is less than 300 sectors.
			 */
			if (trackp->padsecs == 0 &&
			    (is_shorttrk(trackp) || (bytes_read/secsize) >= 300))
				islast = TRUE;
		}

		if (count < bytespt) {
			if (debug) {
				printf("\nNOTICE: reducing block size for last record.\n");
				neednl = FALSE;
			}

			if ((amount = count % secsize) != 0) {
				amount = secsize - amount;
				count += amount;
				printf("\nWARNING: padding up to secsize.\n");
				neednl = FALSE;
			}
			bytespt = count;
			secspt = count / secsize;
			/*
			 * If tracksize is not known (trackp->tracksize < 0)
			 * we may need to set trackp->padsize
			 * if the tracksize is less than 300 sectors.
			 */
			if (trackp->padsecs == 0 &&
			    (is_shorttrk(trackp) || (bytes_read/secsize) >= 300))
				islast = TRUE;
		}

		amount = write_secs(usalp, dp, bp, startsec, bytespt, secspt, islast);
		if (amount < 0) {
			printf("%swrite track data: error after %lld bytes\n",
							neednl?"\n":"", bytes);
			return (-1);
		}
		bytes += amount;
		startsec += amount / secsize;

		if (lverbose && (bytes >= (savbytes + 0x100000))) {
			int	fper;
			int	nsecs = (bytes - savbytes) / secsize;
			float	fspeed;

			gettimeofday(&tcur, (struct timezone *)0);
			printf("\rTrack %02d: %4lld", track, bytes >> 20);
			if (tracksize > 0)
				printf(" of %4lld MB", tracksize >> 20);
			else
				printf(" MB");
			printf(" written");
			fper = fifo_percent(TRUE);
			if (fper >= 0)
				printf(" (fifo %3d%%)", fper);
#ifdef	BCAP
			if (bsize > 0) {			/* buffer size known */
				usalp->silent++;
				per = (*dp->cdr_buffer_cap)(usalp, (long *)0, &bfree);
				usalp->silent--;
				if (per >= 0) {
					per = 100*(bsize - bfree) / bsize;
					if ((bsize - bfree) <= amount || per <= 5)
						dp->cdr_dstat->ds_buflow++;
					if (per < (int)dp->cdr_dstat->ds_minbuf &&
					    (startsec*secsize) > bsize) {
						dp->cdr_dstat->ds_minbuf = per;
					}
					printf(" [buf %3d%%]", per);
#ifdef	BCAPDBG
					printf(" %3ld %3ld", bsize >> 10, bfree >> 10);
#endif
				}
			}
#endif

			tlast.tv_sec = tcur.tv_sec - tlast.tv_sec;
			tlast.tv_usec = tcur.tv_usec - tlast.tv_usec;
			while (tlast.tv_usec < 0) {
				tlast.tv_usec += 1000000;
				tlast.tv_sec -= 1;
			}
			fspeed = (nsecs / secsps) /
				(tlast.tv_sec * 1.0 + tlast.tv_usec * 0.000001);
			if (fspeed > 999.0)
				fspeed = 999.0;
#ifdef	BCAP
			if (bsize > 0 && per > dminbuf &&
			    dp->cdr_dstat->ds_cdrflags & RF_WR_WAIT) {
				int	wsecs = (per-dminbuf)*(bsize/secsize)/100;
				int	msecs = 0x100000/secsize;
				int	wt;
				int	mt;
				int	s = dp->cdr_dstat->ds_dr_cur_wspeed;


				if (s <= 0) {
					if (dp->cdr_dstat->ds_flags & DSF_DVD)
						s = 4;
					else
						s = 50;
				}
				if (wsecs > msecs)	/* Less that 1 MB */
					wsecs = msecs;
				wt = wsecs * 1000 / secsps / fspeed;
				mt = (per-dminbuf)*(bsize/secsize)/100 * 1000 / secsps/s;

				if (wt > mt)
					wt = mt;
				if (wt > 1000)		/* Max 1 second */
					wt = 1000;
				if (wt < 20)		/* Min 20 ms */
					wt = 0;

				if (xdebug)
					printf(" |%3d %4dms %5dms|", wsecs, wt, mt);
				else
					printf(" |%3d %4dms|", wsecs, wt);
				if (wt > 0)
					usleep(wt*1000);
			}
#endif
         if (dp->is_dvd) fspeed /= 9;
			printf(" %5.1fx", fspeed);
			printf(".");
			savbytes = (bytes >> 20) << 20;
			flush();
			neednl = TRUE;
			tlast = tcur;
		}
#ifdef	XBCAP
		if (bsize > 0) {			/* buffer size known */
			(*dp->cdr_buffer_cap)(usalp, (long *)0, &bfree);
			per = 100*(bsize - bfree) / bsize;
			if (per != oper)
				printf("[buf %3d%%] %3ld %3ld\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b",
					per, bsize >> 10, bfree >> 10);
			oper = per;
			flush();
		}


#endif
	} while (tracksize < 0 || bytes_read < tracksize);

	if (!is_shorttrk(trackp) && (bytes / secsize) < 300) {
		/*
		 * If tracksize is not known (trackp->tracksize < 0) or
		 * for some strange reason we did not set padsecs properly
		 * we may need to modify trackp->padsecs if
		 * tracksize+padsecs is less than 300 sectors.
		 */
		if ((trackp->padsecs + (bytes / secsize)) < 300)
			trackp->padsecs = 300 - (bytes / secsize);
	}
	if (trackp->padsecs > 0) {
		Llong	padbytes;

		/*
		 * pad_track() is based on secsize. Compute the amount of bytes
		 * assumed by pad_track().
		 */
		padbytes = (Llong)trackp->padsecs * secsize;

		if (neednl) {
			printf("\n");
			neednl = FALSE;
		}
		if ((padbytes >> 20) > 0) {
			neednl = TRUE;
		} else if (lverbose) {
			printf("Track %02d: writing %3lld KB of pad data.\n",
					track, (Llong)(padbytes >> 10));
			neednl = FALSE;
		}
		pad_track(usalp, dp, trackp, startsec, padbytes,
					TRUE, &savbytes);
		bytes += savbytes;
		startsec += savbytes / secsize;
	}
	printf("%sTrack %02d: Total bytes read/written: %lld/%lld (%lld sectors).\n",
		neednl?"\n":"", track, bytes_read, bytes, bytes/secsize);
	flush();
	return (0);
}

int 
pad_track(SCSI *usalp, cdr_t	*dp, track_t *trackp, long startsec, Llong amt,
				BOOL dolast, Llong *bytesp)
{
	int	track = trackp->trackno;
	Llong	bytes	= 0;
	Llong	savbytes = 0;
	Llong	padsize = amt;
	int	secsize;
	int	secspt;
	int	bytespt;
	int	amount;
	BOOL	neednl	= FALSE;
	BOOL	islast	= FALSE;
	struct timeval tlast;
	struct timeval tcur;
	float	secsps = 75.0;
long bsize;
long bfree;
#define	BCAP
#ifdef	BCAP
int per;
#ifdef	XBCAP
int oper = -1;
#endif
#endif

	if (dp->cdr_dstat->ds_flags & DSF_DVD)
		secsps = 676.27;

	usalp->silent++;
	if ((*dp->cdr_buffer_cap)(usalp, &bsize, &bfree) < 0)
		bsize = -1L;
	if (bsize == 0)		/* If we have no (known) buffer, we cannot */
		bsize = -1L;	/* retrieve the buffer fill ratio	   */
	usalp->silent--;

	secsize = trackp->secsize;
	secspt = trackp->secspt;
	bytespt = secsize * secspt;

	fillbytes(buf, bytespt, '\0');

	if ((amt >> 20) > 0) {
		printf("\rTrack %02d:    0 of %4lld MB pad written.",
						track, amt >> 20);
		flush();
	}
	gettimeofday(&tlast, (struct timezone *)0);
	do {
		if (amt < bytespt) {
			bytespt = roundup(amt, secsize);
			secspt = bytespt / secsize;
		}
		if (dolast && (amt - bytespt) <= 0)
			islast = TRUE;

		if (is_raw(trackp)) {
			encsectors(trackp, (Uchar *)buf, startsec, secspt);
			fillsubch(trackp, (Uchar *)buf, startsec, secspt);
		}

		amount = write_secs(usalp, dp, buf, startsec, bytespt, secspt, islast);
		if (amount < 0) {
			printf("%swrite track pad data: error after %lld bytes\n",
							neednl?"\n":"", bytes);
			if (bytesp)
				*bytesp = bytes;
(*dp->cdr_buffer_cap)(usalp, (long *)0, (long *)0);
			return (-1);
		}
		amt -= amount;
		bytes += amount;
		startsec += amount / secsize;

		if (lverbose && (bytes >= (savbytes + 0x100000))) {
			int	nsecs = (bytes - savbytes) / secsize;
			float	fspeed;

			gettimeofday(&tcur, (struct timezone *)0);
			printf("\rTrack %02d: %4lld", track, bytes >> 20);
			if (padsize > 0)
				printf(" of %4lld MB", padsize >> 20);
			else
				printf(" MB");
			printf(" pad written");
			savbytes = (bytes >> 20) << 20;

#ifdef	BCAP
			if (bsize > 0) {			/* buffer size known */
				usalp->silent++;
				per = (*dp->cdr_buffer_cap)(usalp, (long *)0, &bfree);
				usalp->silent--;
				if (per >= 0) {
					per = 100*(bsize - bfree) / bsize;
					if ((bsize - bfree) <= amount || per <= 5)
						dp->cdr_dstat->ds_buflow++;
					if (per < (int)dp->cdr_dstat->ds_minbuf &&
					    (startsec*secsize) > bsize) {
						dp->cdr_dstat->ds_minbuf = per;
					}
					printf(" [buf %3d%%]", per);
#ifdef	BCAPDBG
					printf(" %3ld %3ld", bsize >> 10, bfree >> 10);
#endif
				}
			}
#endif
			tlast.tv_sec = tcur.tv_sec - tlast.tv_sec;
			tlast.tv_usec = tcur.tv_usec - tlast.tv_usec;
			while (tlast.tv_usec < 0) {
				tlast.tv_usec += 1000000;
				tlast.tv_sec -= 1;
			}
			fspeed = (nsecs / secsps) /
				(tlast.tv_sec * 1.0 + tlast.tv_usec * 0.000001);
			if (fspeed > 999.0)
				fspeed = 999.0;
			printf(" %5.1fx", fspeed);
			printf(".");
			flush();
			neednl = TRUE;
			tlast = tcur;
		}
	} while (amt > 0);

	if (bytesp)
		*bytesp = bytes;
	if (bytes == 0)
		return (0);
	return (bytes > 0 ? 1:-1);
}

#ifdef	USE_WRITE_BUF
int 
write_buf(SCSI *usalp, cdr_t *dp, track_t *trackp, char *bp, long startsec, 
        	  Llong amt, int secsize, BOOL dolast, Llong *bytesp)
{
	int	track = trackp->trackno;
	Llong	bytes	= 0;
	Llong	savbytes = 0;
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
		printf("\rTrack %02d:   0 of %4ld MB pad written.",
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

		amount = write_secs(usalp, dp, bp, startsec, bytespt, secspt, islast);
		if (amount < 0) {
			printf("%swrite track data: error after %ld bytes\n",
							neednl?"\n":"", bytes);
			if (bytesp)
				*bytesp = bytes;
(*dp->cdr_buffer_cap)(usalp, (long *)0, (long *)0);
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
printdata(int track, track_t *trackp)
{
	if (trackp->itracksize >= 0) {
		printf("Track %02d: data  %4lld MB        ",
					track, (Llong)(trackp->itracksize >> 20));
	} else {
		printf("Track %02d: data  unknown length",
					track);
	}
	if (trackp->padsecs > 0) {
		Llong	padbytes = (Llong)trackp->padsecs * trackp->isecsize;

		if ((padbytes >> 20) > 0)
			printf(" padsize: %4lld MB", (Llong)(padbytes >> 20));
		else
			printf(" padsize: %4lld KB", (Llong)(padbytes >> 10));
	}
	if (trackp->pregapsize != (trackp->flags & TI_DVD)? 0 : 150) {
		printf(" pregapsize: %3ld", trackp->pregapsize);
	}
	if (xdebug)
		printf(" START: %ld SECTORS: %ld INDEX0 %ld",
			trackp->trackstart, trackp->tracksecs, trackp->index0start);
	printf("\n");
}

static void 
printaudio(int track, track_t *trackp)
{
	if (trackp->itracksize >= 0) {
		printf("Track %02d: audio %4lld MB (%02d:%02d.%02d) %spreemp%s%s",
			track, (Llong)(trackp->itracksize >> 20),
			minutes(trackp->itracksize),
			seconds(trackp->itracksize),
			hseconds(trackp->itracksize),
			is_preemp(trackp) ? "" : "no ",
			is_swab(trackp) ? " swab":"",
			((trackp->itracksize < 300L*trackp->isecsize) ||
			(trackp->itracksize % trackp->isecsize)) &&
			is_pad(trackp) ? " pad" : "");
	} else {
		printf("Track %02d: audio unknown length    %spreemp%s%s",
			track, is_preemp(trackp) ? "" : "no ",
			is_swab(trackp) ? " swab":"",
			(trackp->itracksize % trackp->isecsize) && is_pad(trackp) ? " pad" : "");
	}
	if (is_scms(trackp))
		printf(" scms");
	else if (is_copy(trackp))
		printf(" copy");
	else
		printf("     ");

	if (trackp->padsecs > 0) {
		Llong	padbytes = (Llong)trackp->padsecs * trackp->isecsize;

		if ((padbytes >> 20) > 0)
			printf(" padsize: %4lld MB", (Llong)(padbytes >> 20));
		else
			printf(" padsize: %4lld KB", (Llong)(padbytes >> 10));
		printf(" (%02d:%02d.%02d)",
			Sminutes(trackp->padsecs),
			Sseconds(trackp->padsecs),
			Shseconds(trackp->padsecs));
	}
	if (trackp->pregapsize != ((trackp->flags & TI_DVD)? 0 : 150) || xdebug > 0) {
		printf(" pregapsize: %3ld", trackp->pregapsize);
	}
	if (xdebug)
		printf(" START: %ld SECTORS: %ld INDEX0 %ld",
			trackp->trackstart, trackp->tracksecs, trackp->index0start);
	printf("\n");
}

static void 
checkfile(int track, track_t *trackp)
{
	if (trackp->itracksize > 0 &&
			is_audio(trackp) &&
			((!is_shorttrk(trackp) &&
			(trackp->itracksize < 300L*trackp->isecsize)) ||
			(trackp->itracksize % trackp->isecsize)) &&
						!is_pad(trackp)) {
		errmsgno(EX_BAD, "Bad audio track size %lld for track %02d.\n",
				(Llong)trackp->itracksize, track);
		errmsgno(EX_BAD, "Audio tracks must be at least %ld bytes and a multiple of %d.\n",
				300L*trackp->isecsize, trackp->isecsize);

		if (!is_shorttrk(trackp) && (trackp->itracksize < 300L*trackp->isecsize))
			comerrno(EX_BAD, "See -shorttrack option.\n");
		if (!is_pad(trackp) && (trackp->itracksize % trackp->isecsize))
			comerrno(EX_BAD, "See -pad option.\n");
	}

	if (lverbose == 0 && xdebug == 0)
		return;

	if (is_audio(trackp))
		printaudio(track, trackp);
	else
		printdata(track, trackp);
}

static int 
checkfiles(int tracks, track_t *trackp)
{
	int	i;
	int	isaudio = 1;
	int	starttrack = 1;
	int	endtrack = tracks;

	if (xdebug) {
		/*
		 * Include Lead-in & Lead-out.
		 */
		starttrack--;
		endtrack++;
	}
	for (i = starttrack; i <= endtrack; i++) {
		if (!is_audio(&trackp[i]))
			isaudio = 0;
		if (xdebug)
			printf("SECTYPE %X ", trackp[i].sectype);
		checkfile(i, &trackp[i]);
	}
	return (isaudio);
}

static void 
setleadinout(int tracks, track_t *trackp)
{
	/*
	 * Set some values for track 0 (the lead-in)
	 */
	if (!is_clone(&trackp[0])) {
		trackp[0].sectype = trackp[1].sectype;
		trackp[0].dbtype  = trackp[1].dbtype;
		trackp[0].dataoff = trackp[1].dataoff;

		/*
		 * XXX Which other flags should be copied to Track 0 ?
		 */
		if (is_audio(&trackp[1]))
			trackp[0].flags |= TI_AUDIO;
	}

	/*
	 * Set some values for track 0xAA (the lead-out)
	 */
	trackp[tracks+1].pregapsize = 0;
	trackp[tracks+1].isecsize   = trackp[tracks].isecsize;
	trackp[tracks+1].secsize    = trackp[tracks].secsize;

	if (!is_clone(&trackp[0])) {
		trackp[tracks+1].tracktype = trackp[tracks].tracktype;
		trackp[tracks+1].sectype   = trackp[tracks].sectype;
		trackp[tracks+1].dbtype    = trackp[tracks].dbtype;
		trackp[tracks+1].dataoff   = trackp[tracks].dataoff;
	}

	trackp[tracks+1].flags = trackp[tracks].flags;
}

static void 
setpregaps(int tracks, track_t *trackp)
{
	int	i;
	int	sectype;
	long	pregapsize;
	track_t	*tp;

	sectype = trackp[1].sectype;
	sectype &= ST_MASK;

	for (i = 1; i <= tracks; i++) {
		tp = &trackp[i];
		if (tp->pregapsize == -1L) {
			tp->pregapsize = 150;		/* Default CD Pre GAP*/
			if (trackp->flags & TI_DVD) {
				tp->pregapsize = 0;
			} else if (sectype != (tp->sectype & ST_MASK)) {
				tp->pregapsize = 255;	/* Pre GAP is 255 */
				tp->flags &= ~TI_PREGAP;
			}
		}
		sectype = tp->sectype & ST_MASK;	/* Save old sectype */
	}
	trackp[tracks+1].pregapsize = 0;
	trackp[tracks+1].index0start = 0;

	for (i = 1; i <= tracks; i++) {
		/*
		 * index0start is set below tracksecks if this track contains
		 * the pregap (index 0) of the next track.
		 */
		trackp[i].index0start = trackp[i].tracksecs;

		pregapsize = trackp[i+1].pregapsize;
		if (is_pregap(&trackp[i+1]) && pregapsize > 0)
			trackp[i].index0start -= pregapsize;
	}
}

/*
 * Check total size of the medium
 */
static long 
checktsize(int tracks, track_t *trackp)
{
	int	i;
	Llong	curr;
	Llong	total = -150;	/* CD track #1 pregap compensation */
	Ullong	btotal;
	track_t	*tp;

	if (trackp->flags & TI_DVD)
		total = 0;
	for (i = 1; i <= tracks; i++) {
		tp = &trackp[i];
		if (!is_pregap(tp))
			total += tp->pregapsize;

		if (lverbose > 1) {
			printf("track: %d start: %lld pregap: %ld\n",
					i, total, tp->pregapsize);
		}
		tp->trackstart = total;
		if (tp->itracksize >= 0) {
			curr = (tp->itracksize + (tp->isecsize-1)) / tp->isecsize;
			curr += tp->padsecs;
			/*
			 * Minimum track size is 4s
			 */
			if (!is_shorttrk(tp) && curr < 300)
				curr = 300;
			if ((trackp->flags & TI_DVD) == 0) {
				/*
				 * XXX Was passiert hier bei is_packet() ???
				 */
				if (is_tao(tp) && !is_audio(tp)) {
					curr += 2;
				}
			}
			total += curr;
		} else if (is_sao(tp) || is_raw(tp)) {
			errmsgno(EX_BAD, "Track %d has unknown length.\n", i);
			comerrno(EX_BAD,
			"Use tsize= option in %s mode to specify track size.\n",
			is_sao(tp) ? "SAO" : "RAW");
		}
	}
	tp = &trackp[i];
	tp->trackstart = total;
	tp->tracksecs = 6750;		/* Size of first session Lead-Out */
	if (!lverbose)
		return (total);

	if (trackp->flags & TI_DVD)
		btotal = (Ullong)total * 2048;
	else
		btotal = (Ullong)total * 2352;
/* XXX CD Sector Size ??? */
	if (tracks > 0) {
		if (trackp->flags & TI_DVD) {
			printf("Total size:     %4llu MB = %lld sectors\n",
				btotal >> 20, total);
		} else {
			printf("Total size:     %4llu MB (%02d:%02d.%02d) = %lld sectors\n",
				btotal >> 20,
				minutes(btotal),
				seconds(btotal),
				hseconds(btotal), total);
			btotal += 150 * 2352;
			printf("Lout start:     %4llu MB (%02d:%02d/%02d) = %lld sectors\n",
				btotal >> 20,
				minutes(btotal),
				seconds(btotal),
				frames(btotal), total);
		}
	}
	return (total);
}

static void 
opentracks(track_t *trackp)
{
	track_t	*tp;
	int	i;
	int	tracks = trackp[0].tracks;

	Llong	tracksize;
	int	secsize;

	for (i = 1; i <= tracks; i++) {
		tp = &trackp[i];

		if (auinfosize(tp->filename, tp)) {
			/*
			 * open stdin
			 */
			tp->xfp = xopen(NULL, O_RDONLY|O_BINARY, 0);
		} else if (strcmp("-", tp->filename) == 0) {
			/*
			 * open stdin
			 */
			tp->xfp = xopen(NULL, O_RDONLY|O_BINARY, 0);
		} else {
			if ((tp->xfp = xopen(tp->filename,
					O_RDONLY|O_BINARY, 0)) == NULL) {
				comerr("Cannot open '%s'.\n", tp->filename);
			}
		}

		checksize(tp);
		tracksize = tp->itracksize;
		secsize = tp->isecsize;
		if (!is_shorttrk(tp) &&
		    tracksize > 0 && (tracksize / secsize) < 300) {

			tracksize = roundup(tracksize, secsize);
			if ((tp->padsecs +
			    (tracksize / secsize)) < 300) {
				tp->padsecs =
					300 - tracksize / secsize;
			}
			if (xdebug) {
				printf("TRACK %d SECTORS: %ld",
					i, tp->tracksecs);
				printf(" pasdize %lld (%ld sectors)\n",
					(Llong)tp->padsecs * secsize,
					tp->padsecs);
			}
		}
#ifdef	AUINFO
		if (tp->flags & TI_USEINFO) {
			auinfo(tp->filename, i, trackp);
			if (lverbose > 0 && i == 1)
				printf("pregap1: %ld\n", trackp[1].pregapsize);
		}
#endif
		/*
		 * tracksecks is total numbers of sectors in track (starting from
		 * index 0).
		 */
		if (tp->padsecs > 0)
			tp->tracksecs += tp->padsecs;

		if (debug) {
			printf("File: '%s' itracksize: %lld isecsize: %d tracktype: %d = %s sectype: %X = %s dbtype: %s flags %X\n",
				tp->filename, (Llong)tp->itracksize,
				tp->isecsize,
				tp->tracktype & TOC_MASK, toc2name[tp->tracktype & TOC_MASK],
				tp->sectype, st2name[tp->sectype & ST_MASK], db2name[tp->dbtype], tp->flags);
		}
	}
}

static void 
checksize(track_t *trackp)
{
	struct stat	st;
	Llong		lsize;
	int		f = -1;

	if (trackp->xfp != NULL)
		f = xfileno(trackp->xfp);

	/*
	 * If the current input file is a regular file and
	 * 'padsize=' has not been specified,
	 * use fstat() or file parser to get the size of the file.
	 */
	if (trackp->itracksize < 0 && (trackp->flags & TI_ISOSIZE) != 0) {
		lsize = isosize(f);
		trackp->itracksize = lsize;
		if (trackp->itracksize != lsize)
			comerrno(EX_BAD, "This OS cannot handle large ISO-9660 images.\n");
	}
	if (trackp->itracksize < 0 && (trackp->flags & TI_NOAUHDR) == 0) {
		lsize = ausize(f);
		trackp->itracksize = lsize;
		if (trackp->itracksize != lsize)
			comerrno(EX_BAD, "This OS cannot handle large audio images.\n");
	}
	if (trackp->itracksize < 0 && (trackp->flags & TI_NOAUHDR) == 0) {
		lsize = wavsize(f);
		trackp->itracksize = lsize;
		if (trackp->itracksize != lsize)
			comerrno(EX_BAD, "This OS cannot handle large WAV images.\n");
		if (trackp->itracksize > 0)	/* Force little endian input */
			trackp->flags |= TI_SWAB;
	}
	if (trackp->itracksize == AU_BAD_CODING) {
		comerrno(EX_BAD, "Inappropriate audio coding in '%s'.\n",
							trackp->filename);
	}
	if (trackp->itracksize < 0 &&
			fstat(f, &st) >= 0 && S_ISREG(st.st_mode)) {
		trackp->itracksize = st.st_size;
	}
	if (trackp->itracksize >= 0) {
		/*
		 * We do not allow cdrecord to start if itracksize is not
		 * a multiple of isecsize or we are allowed to pad to secsize via -pad.
		 * For this reason, we may safely always assume padding.
		 */
		trackp->tracksecs = (trackp->itracksize + trackp->isecsize -1) / trackp->isecsize;
		trackp->tracksize = (trackp->itracksize / trackp->isecsize) * trackp->secsize
					+ trackp->itracksize % trackp->isecsize;
	} else {
		trackp->tracksecs = -1L;
	}
}

static BOOL 
checkdsize(SCSI *usalp, cdr_t *dp, long tsize, int flags)
{
	long	startsec = 0L;
	long	endsec = 0L;
	dstat_t	*dsp = dp->cdr_dstat;
	int	profile;

	usalp->silent++;
	(*dp->cdr_next_wr_address)(usalp, (track_t *)0, &startsec);
	usalp->silent--;

	/*
	 * This only should happen when the drive is currently in SAO mode.
	 * We rely on the drive being in TAO mode, a negative value for
	 * startsec is not correct here it may be caused by bad firmware or
	 * by a drive in SAO mode. In SAO mode the drive will report the
	 * pre-gap as part of the writable area.
	 */
	if (startsec < 0)
		startsec = 0;

	/*
	 * Size limitations (sectors) for CD's:
	 *
	 *		404850 == 90 min	Red book calls this the
	 *					first negative time
	 *					allows lead out start up to
	 *					block 404700
	 *
	 *		449850 == 100 min	This is the first time that
	 *					is no more representable
	 *					in a two digit BCD number.
	 *					allows lead out start up to
	 *					block 449700
	 *
	 *		~540000 == 120 min	The largest CD ever made.
	 *
	 *		~650000 == 1.3 GB	a Double Density (DD) CD.
	 */

	endsec = startsec + tsize;
	dsp->ds_startsec = startsec;
	dsp->ds_endsec = endsec;


	if (dsp->ds_maxblocks > 0) {
		/*
		 * dsp->ds_maxblocks > 0 (disk capacity is known).
		 */
		if (lverbose)
			printf("Blocks total: %ld Blocks current: %ld Blocks remaining: %ld\n",
					(long)dsp->ds_maxblocks,
					(long)dsp->ds_maxblocks - startsec,
					(long)dsp->ds_maxblocks - endsec);

		if (endsec > dsp->ds_maxblocks) {
			if (dsp->ds_flags & DSF_DVD) {	/* A DVD and not a CD */
				/*
				 * There is no overburning on DVD...
				 */
				errmsgno(EX_BAD,
				"Data does not fit on current disk.\n");
				goto toolarge;
			}
			errmsgno(EX_BAD,
			"WARNING: Data may not fit on current disk.\n");

			/* XXX Check for flags & CDR_NO_LOLIMIT */
/*			goto toolarge;*/
		}
		if (lverbose && dsp->ds_maxrblocks > 0)
			printf("RBlocks total: %ld RBlocks current: %ld RBlocks remaining: %ld\n",
					(long)dsp->ds_maxrblocks,
					(long)dsp->ds_maxrblocks - startsec,
					(long)dsp->ds_maxrblocks - endsec);
		if (dsp->ds_maxrblocks > 0 && endsec > dsp->ds_maxrblocks) {
			errmsgno(EX_BAD,
			"Data does not fit on current disk.\n");
			goto toolarge;
		}
		if ((endsec > dsp->ds_maxblocks && endsec > 404700) ||
		    (dsp->ds_maxrblocks > 404700 && 449850 > dsp->ds_maxrblocks)) {
			/*
			 * Assume that this must be a CD and not a DVD.
			 * So this is a non Red Book compliant CD with a
			 * capacity between 90 and 99 minutes.
			 */
			if (dsp->ds_maxrblocks > 404700)
				printf("RedBook total: %ld RedBook current: %ld RedBook remaining: %ld\n",
					404700L,
					404700L - startsec,
					404700L - endsec);
			if (endsec > dsp->ds_maxblocks && endsec > 404700) {
				if ((flags & (F_IGNSIZE|F_FORCE)) == 0) {
					errmsgno(EX_BAD,
					"Notice: Most recorders cannot write CD's >= 90 minutes.\n");
					errmsgno(EX_BAD,
					"Notice: Use -ignsize option to allow >= 90 minutes.\n");
				}
				goto toolarge;
			}
		}
	} else {
		/*
		 * dsp->ds_maxblocks == 0 (disk capacity is unknown).
		 */
	        profile = dp->profile;
	        if (endsec >= (4200000)) {
		        errmsgno(EX_BAD,
			"ERROR: Could not manage to find medium size, and more than 8.0 GB of data.\n");
  		        goto toolarge;  
		} else if (profile != 0x2B) { 
		    if (endsec >= (2300000)) {
			errmsgno(EX_BAD,
				"ERROR: Could not manage to find medium size, and more than 4.3 GB of data for a non dual layer disc.\n");
			goto toolarge;
		    } else if (endsec >= (405000-300)) {            /*<90 min disk or DVD*/
			errmsgno(EX_BAD,
				"WARNING: Could not manage to find medium size, and more than 90 mins of data.\n");
		    } else if (endsec >= (333000-150)) {		/* 74 min disk*/
			errmsgno(EX_BAD,
				"WARNING: Data may not fit on standard 74min disk.\n");
		    }
		}
	}
	if (dsp->ds_maxblocks <= 0 || endsec <= dsp->ds_maxblocks)
		return (TRUE);
	/* FALLTHROUGH */
toolarge:
	if (dsp->ds_maxblocks > 0 && endsec > dsp->ds_maxblocks) {
		if ((flags & (F_OVERBURN|F_IGNSIZE|F_FORCE)) != 0) {
			if (dsp->ds_flags & DSF_DVD) {	/* A DVD and not a CD */
				errmsgno(EX_BAD,
				"Notice: -overburn is not expected to work with DVD media.\n");
			}
			errmsgno(EX_BAD,
				"Notice: Overburning active. Trying to write more than the official disk capacity.\n");
			return (TRUE);
		} else {
			if ((dsp->ds_flags & DSF_DVD) == 0) {	/* A CD and not a DVD */
				errmsgno(EX_BAD,
				"Notice: Use -overburn option to write more than the official disk capacity.\n");
				errmsgno(EX_BAD,
				"Notice: Most CD-writers do overburning only on SAO or RAW mode.\n");
			}
			return (FALSE);
		}
	}
	if (dsp->ds_maxblocks < 449850) {
		if ((dsp->ds_flags & DSF_DVD) == 0) {	/* A CD and not a DVD */
			if (endsec <= dsp->ds_maxblocks)
				return (TRUE);
			errmsgno(EX_BAD, "Cannot write more than remaining DVD capacity.\n");
			return (FALSE);
		}
		/*
		 * Assume that this must be a CD and not a DVD.
		 */
		if (endsec > 449700) {
			errmsgno(EX_BAD, "Cannot write CD's >= 100 minutes.\n");
			return (FALSE);
		}
	}
	if ((flags & (F_IGNSIZE|F_FORCE)) != 0)
		return (TRUE);
	return (FALSE);
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
	if (rlim.rlim_cur >= (MAX_TRACK + 10))
		return;

	rlim.rlim_cur = MAX_TRACK + 10;
	if (rlim.rlim_cur > rlim.rlim_max)
		errmsgno(EX_BAD,
			"Warning: low file descriptor limit (%lld)\n",
						(Llong)rlim.rlim_max);
	setrlimit(RLIMIT_NOFILE, &rlim);

#endif	/* RLIMIT_NOFILE */
}

static void 
raise_memlock()
{
#ifdef	RLIMIT_MEMLOCK
	struct rlimit rlim;

	rlim.rlim_cur = rlim.rlim_max = RLIM_INFINITY;

	if (setrlimit(RLIMIT_MEMLOCK, &rlim) < 0)
		errmsg("Warning: Cannot raise RLIMIT_MEMLOCK limits.\n");
#endif	/* RLIMIT_MEMLOCK */
}

char	*opts =
"help,version,checkdrive,prcap,inq,devices,scanbus,reset,abort,overburn,ignsize,useinfo,dev*,timeout#,driver*,driveropts*,setdropts,tsize&,padsize&,pregap&,defpregap&,speed#,load,lock,eject,dummy,msinfo,toc,atip,multi,fix,nofix,waiti,immed,debug#,d+,kdebug#,kd#,verbose+,v+,Verbose+,V+,x+,xd#,silent,s,audio,data,mode2,xa,xa1,xa2,xamix,cdi,isosize,nopreemp,preemp,nocopy,copy,nopad,pad,swab,fs&,ts&,blank&,format,formattype&,pktsize#,packet,noclose,force,tao,dao,sao,raw,raw96r,raw96p,raw16,clone,scms,isrc*,mcn*,index*,cuefile*,textfile*,text,shorttrack,noshorttrack,gracetime#,minbuf#,msifile*";

/*
 * Defines used to find whether a write mode has been specified.
 */
#define	M_TAO		1	/* Track at Once mode */
#define	M_SAO		2	/* Session at Once mode (also known as DAO) */
#define	M_RAW		4	/* Raw mode */
#define	M_PACKET	8	/* Packed mode */
static int 
gargs(int ac, char **av, int *tracksp, track_t *trackp, char **devp, 
		int *timeoutp, cdr_t **dpp, int *speedp, long *flagsp, int *blankp, 
		int *formatp)
{
	int	cac;
	char	* const*cav;
	char	*driver = NULL;
	char	*dev = NULL;
	char	*isrc = NULL;
	char	*mcn = NULL;
	char	*tindex = NULL;
	char	*cuefile = NULL;
	char	*textfile = NULL;
	long	bltype = -1;
	int	doformat = 0;
	int	formattype = -1;
	Llong	tracksize;
	Llong	padsize;
	long	pregapsize;
	long	defpregap = -1L;
	int	pktsize;
	int	speed = -1;
	int	help = 0;
	int	version = 0;
	int	checkdrive = 0;
	int	setdropts = 0;
	int	prcap = 0;
	int	inq = 0;
	int	scanbus = 0;
	int	reset = 0;
	int	doabort = 0;
	int	overburn = 0;
	int	ignsize = 0;
	int	useinfo = 0;
	int	load = 0;
	int	lock = 0;
	int	eject = 0;
	int	dummy = 0;
	int	msinfo = 0;
	int	toc = 0;
	int	atip = 0;
	int	multi = 0;
	int	fix = 0;
	int	nofix = 0;
	int	waiti = 0;
	int	immed = 0;
	int	audio;
	int	autoaudio = 0;
	int	data;
	int	mode2;
	int	xa;
	int	xa1;
	int	xa2;
	int	xamix;
	int	cdi;
	int	isize;
	int	ispacket = 0;
	int	noclose = 0;
	int	force = 0;
	int	tao = 0;
	int	dao = 0;
	int	raw = 0;
	int	raw96r = 0;
	int	raw96p = 0;
	int	raw16 = 0;
	int	clone = 0;
	int	scms = 0;
	int	preemp = 0;
	int	nopreemp;
	int	copy = 0;
	int	nocopy;
	int	pad = 0;
	int	bswab = 0;
	int	nopad;
	int	usetext = 0;
	int	shorttrack = 0;
	int	noshorttrack;
	int	flags;
	int	tracks = *tracksp;
	int	tracktype = TOC_ROM;
/*	int	sectype = ST_ROM_MODE1 | ST_MODE_1;*/
	int	sectype = SECT_ROM;
	int	dbtype = DB_ROM_MODE1;
	int	secsize = DATA_SEC_SIZE;
	int	dataoff = 16;
	int	ga_ret;
	int	wm = 0;

	trackp[0].flags |= TI_TAO;
	trackp[1].pregapsize = -1;
	*flagsp |= F_WRITE;

	cac = --ac;
	cav = ++av;
	for (; ; cac--, cav++) {
		tracksize = (Llong)-1L;
		padsize = (Llong)0L;
		pregapsize = defpregap;
		audio = data = mode2 = xa = xa1 = xa2 = xamix = cdi = 0;
		isize = nopreemp = nocopy = nopad = noshorttrack = 0;
		pktsize = 0;
		isrc = NULL;
		tindex = NULL;
		/*
		 * Get options up to next file type arg.
		 */
		if ((ga_ret = getargs(&cac, &cav, opts,
				&help, &version, &checkdrive, &prcap,
				&inq, &scandevs, &scanbus, &reset, &doabort, &overburn, &ignsize,
				&useinfo,
				devp, timeoutp, &driver, &driveropts, &setdropts,
				getllnum, &tracksize,
				getllnum, &padsize,
				getnum, &pregapsize,
				getnum, &defpregap,
				&speed,
				&load, &lock,
				&eject, &dummy, &msinfo, &toc, &atip,
				&multi, &fix, &nofix, &waiti, &immed,
				&debug, &debug,
				&kdebug, &kdebug,
				&lverbose, &lverbose,
				&scsi_verbose, &scsi_verbose,
				&xdebug, &xdebug,
				&silent, &silent,
				&audio, &data, &mode2,
				&xa, &xa1, &xa2, &xamix, &cdi,
				&isize,
				&nopreemp, &preemp,
				&nocopy, &copy,
				&nopad, &pad, &bswab, getnum, &fs, getnum, &bufsize,
				getbltype, &bltype, &doformat, getformattype, &formattype, &pktsize,
				&ispacket, &noclose, &force,
				&tao, &dao, &dao, &raw, &raw96r, &raw96p, &raw16,
				&clone,
				&scms, &isrc, &mcn, &tindex,
				&cuefile, &textfile, &usetext,
				&shorttrack, &noshorttrack,
				&gracetime, &dminbuf, &msifile)) < 0) {
			errmsgno(EX_BAD, "Bad Option: %s.\n", cav[0]);
			susage(EX_BAD);
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
			if (scanbus || scandevs) /* scandevs behaves similarly WRT in the legacy code, just the scan operation is different */
				*flagsp |= F_SCANBUS;
			if (reset)
				*flagsp |= F_RESET;
			if (doabort)
				*flagsp |= F_ABORT;
			if (overburn)
				*flagsp |= F_OVERBURN;
			if (ignsize)
				*flagsp |= F_IGNSIZE;
			if (load)
				*flagsp |= F_LOAD;
			if (lock)
				*flagsp |= F_DLCK;
			if (eject)
				*flagsp |= F_EJECT;
			if (dummy)
				*flagsp |= F_DUMMY;
			if (setdropts)
				*flagsp |= F_SETDROPTS;
			if(msifile)
				msinfo++;
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
				/*
				 * 2048 Bytes user data
				 */
				*flagsp |= F_MULTI;
				tracktype = TOC_XA2;
				sectype = ST_ROM_MODE2 | ST_MODE_2_FORM_1;
				sectype = SECT_MODE_2_F1;
				dbtype = DB_XA_MODE2;	/* XXX -multi nimmt DB_XA_MODE2_F1 !!! */
				secsize = DATA_SEC_SIZE;	/* 2048 */
				dataoff = 24;
			}
			if (fix)
				*flagsp |= F_FIX;
			if (nofix)
				*flagsp |= F_NOFIX;
			if (waiti)
				*flagsp |= F_WAITI;
			if (immed)
				*flagsp |= F_IMMED;
			if (force)
				*flagsp |= F_FORCE;

			if (bltype >= 0) {
				*flagsp |= F_BLANK;
				*blankp = bltype;
			}
 			if (doformat > 0) {
 				*flagsp |= F_FORMAT;
 				*formatp |= FULL_FORMAT;
 			}
 			if (formattype >= 0) {
 				*flagsp |= F_FORMAT;
 				*formatp |= formattype;
 			}
			if (ispacket)
				wm |= M_PACKET;
			if (tao)
				wm |= M_TAO;
			if (dao) {
				*flagsp |= F_SAO;
				trackp[0].flags &= ~TI_TAO;
				trackp[0].flags |= TI_SAO;
				wm |= M_SAO;

			} else if ((raw == 0) && (raw96r + raw96p + raw16) > 0)
				raw = 1;
			if ((raw != 0) && (raw96r + raw96p + raw16) == 0)
				raw96r = 1;
			if (raw96r) {
				if (!dao)
					*flagsp |= F_RAW;
				trackp[0].flags &= ~TI_TAO;
				trackp[0].flags |= TI_RAW;
				trackp[0].flags |= TI_RAW96R;
				wm |= M_RAW;
			}
			if (raw96p) {
				if (!dao)
					*flagsp |= F_RAW;
				trackp[0].flags &= ~TI_TAO;
				trackp[0].flags |= TI_RAW;
				wm |= M_RAW;
			}
			if (raw16) {
				if (!dao)
					*flagsp |= F_RAW;
				trackp[0].flags &= ~TI_TAO;
				trackp[0].flags |= TI_RAW;
				trackp[0].flags |= TI_RAW16;
				wm |= M_RAW;
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
			if ((raw96r + raw96p + raw16) > 1) {
				errmsgno(EX_BAD, "Too many raw modes.\n");
				comerrno(EX_BAD, "Only one of -raw16, -raw96p, -raw96r allowed.\n");
			}
			if ((tao + ispacket + dao + raw) > 1) {
				errmsgno(EX_BAD, "Too many write modes.\n");
				comerrno(EX_BAD, "Only one of -packet, -dao, -raw allowed.\n");
			}
			if (dao && (raw96r + raw96p + raw16) > 0) {
				if (raw16)
					comerrno(EX_BAD, "SAO RAW writing does not allow -raw16.\n");
				if (!clone)
					comerrno(EX_BAD, "SAO RAW writing only makes sense in clone mode.\n");
#ifndef	CLONE_WRITE
				comerrno(EX_BAD, "SAO RAW writing not yet implemented.\n");
#endif
				comerrno(EX_BAD, "SAO RAW writing not yet implemented.\n");
			}
			if (clone) {
				*flagsp |= F_CLONE;
				trackp[0].flags |= TI_CLONE;
#ifndef	CLONE_WRITE
				comerrno(EX_BAD, "Clone writing not compiled in.\n");
#endif
			}
			if (textfile) {
				if (!checktextfile(textfile)) {
					if ((*flagsp & F_WRITE) != 0) {
						comerrno(EX_BAD,
							"Cannot use '%s' as CD-Text file.\n",
							textfile);
					}
				}
				if ((*flagsp & F_WRITE) != 0) {
					if ((dao + raw96r + raw96p) == 0)
						comerrno(EX_BAD,
							"CD-Text needs -dao, -raw96r or -raw96p.\n");
				}
				trackp[0].flags |= TI_TEXT;
			}
			version = checkdrive = prcap = inq = scanbus = reset = doabort =
			overburn = ignsize =
			load = lock = eject = dummy = msinfo = toc = atip = multi = fix = nofix =
			waiti = immed = force = dao = setdropts = 0;
			raw96r = raw96p = raw16 = clone = 0;
		} else if ((version + checkdrive + prcap + inq + scanbus +
			    reset + doabort + overburn + ignsize +
			    load + lock + eject + dummy + msinfo + toc + atip + multi + fix + nofix +
			    waiti + immed + force + dao + setdropts +
			    raw96r + raw96p + raw16 + clone) > 0 ||
				mcn != NULL)
			comerrno(EX_BAD, "Badly placed option. Global options must be before any track.\n");

		if (nopreemp)
			preemp = 0;
		if (nocopy)
			copy = 0;
		if (nopad)
			pad = 0;
		if (noshorttrack)
			shorttrack = 0;

		if ((audio + data + mode2 + xa + xa1 + xa2 + xamix) > 1) {
			errmsgno(EX_BAD, "Too many types for track %d.\n", tracks+1);
			comerrno(EX_BAD, "Only one of -audio, -data, -mode2, -xa, -xa1, -xa2, -xamix allowed.\n");
		}
		if (ispacket && audio) {
			comerrno(EX_BAD, "Audio data cannot be written in packet mode.\n");
		}
		/*
		 * Check whether the next argument is a file type arg.
		 * If this is true, then we got a track file name.
		 * If getargs() did previously return NOTAFLAG, we may have hit
		 * an argument that has been escaped via "--", so we may not
		 * call getfiles() again in this case. If we would call
		 * getfiles() and the current arg has been escaped and looks
		 * like an option, a call to getfiles() would skip it.
		 */
		if (ga_ret != NOTAFLAG)
			ga_ret = getfiles(&cac, &cav, opts);
		if (autoaudio) {
			autoaudio = 0;
			tracktype = TOC_ROM;
			sectype = ST_ROM_MODE1 | ST_MODE_1;
			sectype = SECT_ROM;
			dbtype = DB_ROM_MODE1;
			secsize = DATA_SEC_SIZE;	/* 2048 */
			dataoff = 16;
		}
		if (ga_ret == NOTAFLAG && (is_auname(cav[0]) || is_wavname(cav[0]))) {
			/*
			 * We got a track and autodetection decided that it
			 * is an audio track.
			 */
			autoaudio++;
			audio++;
		}
		if (data) {
			/*
			 * 2048 Bytes user data
			 */
			tracktype = TOC_ROM;
			sectype = ST_ROM_MODE1 | ST_MODE_1;
			sectype = SECT_ROM;
			dbtype = DB_ROM_MODE1;
			secsize = DATA_SEC_SIZE;	/* 2048 */
			dataoff = 16;
		}
		if (mode2) {
			/*
			 * 2336 Bytes user data
			 */
			tracktype = TOC_ROM;
			sectype = ST_ROM_MODE2 | ST_MODE_2;
			sectype = SECT_MODE_2;
			dbtype = DB_ROM_MODE2;
			secsize = MODE2_SEC_SIZE;	/* 2336 */
			dataoff = 16;
		}
		if (audio) {
			/*
			 * 2352 Bytes user data
			 */
			tracktype = TOC_DA;
			sectype = preemp ? ST_AUDIO_PRE : ST_AUDIO_NOPRE;
			sectype |= ST_MODE_AUDIO;
			sectype = SECT_AUDIO;
			if (preemp)
				sectype |= ST_PREEMPMASK;
			dbtype = DB_RAW;
			secsize = AUDIO_SEC_SIZE;	/* 2352 */
			dataoff = 0;
		}
		if (xa) {
			/*
			 * 2048 Bytes user data
			 */
			if (tracktype != TOC_CDI)
				tracktype = TOC_XA2;
			sectype = ST_ROM_MODE2 | ST_MODE_2_FORM_1;
			sectype = SECT_MODE_2_F1;
			dbtype = DB_XA_MODE2;
			secsize = DATA_SEC_SIZE;	/* 2048 */
			dataoff = 24;
		}
		if (xa1) {
			/*
			 * 8 Bytes subheader + 2048 Bytes user data
			 */
			if (tracktype != TOC_CDI)
				tracktype = TOC_XA2;
			sectype = ST_ROM_MODE2 | ST_MODE_2_FORM_1;
			sectype = SECT_MODE_2_F1;
			dbtype = DB_XA_MODE2_F1;
			secsize = 2056;
			dataoff = 16;
		}
		if (xa2) {
			/*
			 * 2324 Bytes user data
			 */
			if (tracktype != TOC_CDI)
				tracktype = TOC_XA2;
			sectype = ST_ROM_MODE2 | ST_MODE_2_FORM_2;
			sectype = SECT_MODE_2_F2;
			dbtype = DB_XA_MODE2_F2;
			secsize = 2324;
			dataoff = 24;
		}
		if (xamix) {
			/*
			 * 8 Bytes subheader + 2324 Bytes user data
			 */
			if (tracktype != TOC_CDI)
				tracktype = TOC_XA2;
			sectype = ST_ROM_MODE2 | ST_MODE_2_MIXED;
			sectype = SECT_MODE_2_MIX;
			dbtype = DB_XA_MODE2_MIX;
			secsize = 2332;
			dataoff = 16;
		}
		if (cdi) {
			tracktype = TOC_CDI;
		}
		if (tracks == 0) {
			trackp[0].tracktype = tracktype;
			trackp[0].dbtype = dbtype;
			trackp[0].isecsize = secsize;
			trackp[0].secsize = secsize;
			if ((*flagsp & F_RAW) != 0) {
				trackp[0].secsize = is_raw16(&trackp[0]) ?
						RAW16_SEC_SIZE:RAW96_SEC_SIZE;
			}
			if ((*flagsp & F_DUMMY) != 0)
				trackp[0].tracktype |= TOCF_DUMMY;
			if ((*flagsp & F_MULTI) != 0)
				trackp[0].tracktype |= TOCF_MULTI;
		}

		flags = trackp[0].flags;

		if ((sectype & ST_AUDIOMASK) != 0)
			flags |= TI_AUDIO;
		if (isize) {
			flags |= TI_ISOSIZE;
			if ((*flagsp & F_MULTI) != 0)
				comerrno(EX_BAD, "Cannot get isosize for multi session disks.\n");
			/*
			 * As we do not get the padding from the ISO-9660
			 * formatting utility, we need to force padding here.
			 */
			flags |= TI_PAD;
			if (padsize == (Llong)0L)
				padsize = (Llong)PAD_SIZE;
		}

		if ((flags & TI_AUDIO) != 0) {
			if (preemp)
				flags |= TI_PREEMP;
			if (copy)
				flags |= TI_COPY;
			if (scms)
				flags |= TI_SCMS;
		}
		if (pad || ((flags & TI_AUDIO) == 0 && padsize > (Llong)0L)) {
			flags |= TI_PAD;
			if ((flags & TI_AUDIO) == 0 && padsize == (Llong)0L)
				padsize = (Llong)PAD_SIZE;
		}
		if (shorttrack && (*flagsp & (F_SAO|F_RAW)) != 0)
			flags |= TI_SHORT_TRACK;
		if (noshorttrack)
			flags &= ~TI_SHORT_TRACK;
		if (bswab)
			flags |= TI_SWAB;
		if (ispacket) {
			flags |= TI_PACKET;
			trackp[0].flags &= ~TI_TAO;
		}
		if (noclose)
			flags |= TI_NOCLOSE;
		if (useinfo)
			flags |= TI_USEINFO;

		if (ga_ret == NOARGS) {
			/*
			 * All options have already been parsed and no more
			 * file type arguments are present.
			 */
			break;
		}
		if (tracks == 0 && (wm == 0)) {
			errmsgno(EX_BAD, "No write mode specified.\n");
			errmsgno(EX_BAD, "Assuming -tao mode.\n");
			errmsgno(EX_BAD, "Future versions of wodim may have different drive dependent defaults.\n");
			tao = 1;
		}
		tracks++;

		if (tracks > MAX_TRACK)
			comerrno(EX_BAD, "Track limit (%d) exceeded\n",
								MAX_TRACK);
		/*
		 * Make 'tracks' immediately usable in track structure.
		 */
		{	register int i;
			for (i = 0; i < MAX_TRACK+2; i++)
				trackp[i].tracks = tracks;
		}

		if (strcmp("-", cav[0]) == 0)
			*flagsp |= F_STDIN;

		if (!is_auname(cav[0]) && !is_wavname(cav[0]))
			flags |= TI_NOAUHDR;

		if ((*flagsp & (F_SAO|F_RAW)) != 0 && (flags & TI_AUDIO) != 0)
			flags |= TI_PREGAP;	/* Hack for now */
		if (tracks == 1)
			flags &= ~TI_PREGAP;

		if (tracks == 1 && (pregapsize != -1L && pregapsize != 150))
			pregapsize = -1L;
		trackp[tracks].filename = cav[0];
		trackp[tracks].trackstart = 0L;
		trackp[tracks].itracksize = tracksize;
		trackp[tracks].tracksize = tracksize;
		trackp[tracks].tracksecs = -1L;
		if (tracksize >= 0) {
			trackp[tracks].tracksecs = (tracksize+secsize-1)/secsize;
			if(tracksize<616448)
				warn_minisize=tracksize;
		}
		if (trackp[tracks].pregapsize < 0)
			trackp[tracks].pregapsize = pregapsize;
		trackp[tracks+1].pregapsize = -1;
		trackp[tracks].padsecs = (padsize+2047)/2048;
		trackp[tracks].isecsize = secsize;
		trackp[tracks].secsize = secsize;
		trackp[tracks].flags = flags;
		/*
		 * XXX Dies ist falsch: auch bei SAO/RAW kann
		 * XXX secsize != isecsize sein.
		 */
		if ((*flagsp & F_RAW) != 0) {
			if (is_raw16(&trackp[tracks]))
				trackp[tracks].secsize = RAW16_SEC_SIZE;
			else
				trackp[tracks].secsize = RAW96_SEC_SIZE;
#ifndef	HAVE_LIB_EDC_ECC
			if ((sectype & ST_MODE_MASK) != ST_MODE_AUDIO) {
				errmsgno(EX_BAD,
					"EDC/ECC library not compiled in.\n");
				comerrno(EX_BAD,
					"Data sectors are not supported in RAW mode.\n");
			}
#endif
		}
		trackp[tracks].secspt = 0;	/* transfer size is set up in set_trsizes() */
		trackp[tracks].pktsize = pktsize;
		trackp[tracks].trackno = tracks;
		trackp[tracks].sectype = sectype;
#ifdef	CLONE_WRITE
		if ((*flagsp & F_CLONE) != 0) {
			trackp[tracks].isecsize = 2448;
			trackp[tracks].sectype |= ST_MODE_RAW;
			dataoff = 0;
		}
#endif
		trackp[tracks].dataoff = dataoff;
		trackp[tracks].tracktype = tracktype;
		trackp[tracks].dbtype = dbtype;
		trackp[tracks].flags = flags;
		trackp[tracks].nindex = 1;
		trackp[tracks].tindex = 0;
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
	}

	if (dminbuf >= 0) {
		if (dminbuf < 25 || dminbuf > 95)
			comerrno(EX_BAD,
			"Bad minbuf=%d option (must be between 25 and 95)\n",
				dminbuf);
	}

	if (speed < 0 && speed != -1)
		comerrno(EX_BAD, "Bad speed option.\n");

	if (fs < 0L && fs != -1L)
		comerrno(EX_BAD, "Bad fifo size option.\n");

	if (bufsize < 0L && bufsize != -1L)
		comerrno(EX_BAD, "Bad transfer size option.\n");
	if (bufsize < 0L)
		bufsize = CDR_BUF_SIZE;
	if (bufsize > CDR_MAX_BUF_SIZE)
		bufsize = CDR_MAX_BUF_SIZE;

	dev = *devp;
	cdr_defaults(&dev, &speed, &fs, &driveropts);
	if (debug) {
		printf("dev: '%s' speed: %d fs: %ld driveropts '%s'\n",
					dev, speed, fs, driveropts);
	}
	if (speed >= 0)
		*speedp = speed;

	if (fs < 0L)
		fs = DEFAULT_FIFOSIZE;
	if (fs < 2*bufsize) {
		errmsgno(EX_BAD, "Fifo size %ld too small, turning fifo off.\n", fs);
		fs = 0L;
	}

	if (dev != *devp && (*flagsp & F_SCANBUS) == 0)
		*devp = dev;

	if (*devp &&
	    ((strncmp(*devp, "HELP", 4) == 0) ||
	    (strncmp(*devp, "help", 4) == 0))) {
		*flagsp |= F_CHECKDRIVE; /* Set this for not calling mlockall() */
		return ispacket;
	}
	if (*flagsp & (F_LOAD|F_DLCK|F_SETDROPTS|F_MSINFO|F_TOC|F_PRATIP|F_FIX|F_VERSION|F_CHECKDRIVE|F_PRCAP|F_INQUIRY|F_SCANBUS|F_RESET|F_ABORT)) {
		if (tracks != 0) {
       fprintf(stderr,
             "No tracks allowed with -load, -lock, -setdropts, -msinfo, -toc, -atip, -fix,\n"
             "-version, -checkdrive, -prcap, -inq, -scanbus, --devices, -reset and -abort options.\n" );
       exit(EXIT_FAILURE);
		}
		return ispacket;
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
	if (usetext) {
		trackp[MAX_TRACK+1].flags |= TI_TEXT;
	}
	if (cuefile) {
#ifdef	FUTURE
		if ((*flagsp & F_SAO) == 0 &&
		    (*flagsp & F_RAW) == 0) {
#else
		if ((*flagsp & F_SAO) == 0) {
#endif
			errmsgno(EX_BAD, "The cuefile= option only works with -dao.\n");
			susage(EX_BAD);
		}
		if (tracks > 0) {
			errmsgno(EX_BAD, "No tracks allowed with the cuefile= option\n");
			susage(EX_BAD);
		}
		cuefilename = cuefile;
		return ispacket;
	}
	if (tracks == 0 && (*flagsp & (F_LOAD|F_DLCK|F_EJECT|F_BLANK|F_FORMAT)) == 0) {
		errmsgno(EX_BAD, "No tracks specified. Need at least one.\n");
		susage(EX_BAD);
	}
	return ispacket;
}

static void 
set_trsizes(cdr_t *dp, int tracks, track_t *trackp)
{
	int	i;
	int	secsize;
	int	secspt;

	trackp[1].flags		|= TI_FIRST;
	trackp[tracks].flags	|= TI_LAST;

	if (xdebug)
		printf("Set Transfersizes start\n");
	for (i = 0; i <= tracks+1; i++) {
		if ((dp->cdr_flags & CDR_SWABAUDIO) != 0 &&
					is_audio(&trackp[i])) {
			trackp[i].flags ^= TI_SWAB;
		}
		if (!is_audio(&trackp[i]))
			trackp[i].flags &= ~TI_SWAB;	/* Only swab audio  */

		/*
		 * Use the biggest sector size to compute how many
		 * sectors may fit into one single DMA buffer.
		 */
		secsize = trackp[i].secsize;
		if (trackp[i].isecsize > secsize)
			secsize = trackp[i].isecsize;

		/*
		 * We are using SCSI Group 0 write
		 * and cannot write more than 255 secs at once.
		 */
		secspt = bufsize/secsize;
		secspt = min(255, secspt);
		trackp[i].secspt = secspt;

		if (is_packet(&trackp[i]) && trackp[i].pktsize > 0) {
			if (trackp[i].secspt >= trackp[i].pktsize) {
				trackp[i].secspt = trackp[i].pktsize;
			} else {
				comerrno(EX_BAD,
					"Track %d packet size %d exceeds buffer limit of %d sectors",
					i, trackp[i].pktsize, trackp[i].secspt);
			}
		}
		if (xdebug) {
			printf("Track %d flags %X secspt %d secsize: %d isecsize: %d\n",
				i, trackp[i].flags, trackp[i].secspt,
				trackp[i].secsize, trackp[i].isecsize);
		}
	}
	if (xdebug)
		printf("Set Transfersizes end\n");
}

void 
load_media(SCSI *usalp, cdr_t *dp, BOOL doexit)
{
	int	code;
	int	key;
	BOOL	immed = (dp->cdr_cmdflags&F_IMMED) != 0;

	/*
	 * Do some preparation before...
	 */
	usalp->silent++;			/* Be quiet if this fails		*/
	test_unit_ready(usalp);		/* First eat up unit attention		*/
	if ((*dp->cdr_load)(usalp, dp) < 0) {	/* now try to load media and	*/
		if (!doexit)
			return;
		comerrno(EX_BAD, "Cannot load media.\n");
	}
	scsi_start_stop_unit(usalp, 1, 0, immed); /* start unit in silent mode	*/
	usalp->silent--;

	if (!wait_unit_ready(usalp, 60)) {
		code = usal_sense_code(usalp);
		key = usal_sense_key(usalp);
		usalp->silent++;
		scsi_prevent_removal(usalp, 0); /* In case someone locked it */
		usalp->silent--;

		if (!doexit)
			return;
		if (key == SC_NOT_READY && (code == 0x3A || code == 0x30))
			comerrno(EX_BAD, "No disk / Wrong disk!\n");
		comerrno(EX_BAD, "CD/DVD-Recorder not ready.\n");
	}

	scsi_prevent_removal(usalp, 1);
	scsi_start_stop_unit(usalp, 1, 0, immed);
	wait_unit_ready(usalp, 120);
	usalp->silent++;
	if(geteuid() == 0) /* EB: needed? Not allowed for non-root, that is sure. */
      rezero_unit(usalp);	/* Is this needed? Not supported by some drvives */
	usalp->silent--;
	test_unit_ready(usalp);
	scsi_start_stop_unit(usalp, 1, 0, immed);
	wait_unit_ready(usalp, 120);
}

void 
unload_media(SCSI *usalp, cdr_t *dp, int flags)
{
	scsi_prevent_removal(usalp, 0);
	if ((flags & F_EJECT) != 0) {
		if ((*dp->cdr_unload)(usalp, dp) < 0)
			errmsgno(EX_BAD, "Cannot eject media.\n");
	}
}

void 
reload_media(SCSI *usalp, cdr_t *dp)
{
	char	ans[2];
#ifdef	F_GETFL
	int	f = -1;
#endif

	errmsgno(EX_BAD, "Drive needs to reload the media to return to proper status.\n");
	unload_media(usalp, dp, F_EJECT);

	/*
	 * Note that even Notebook drives identify as CDR_TRAYLOAD
	 */
	if ((dp->cdr_flags & CDR_TRAYLOAD) != 0) {
		usalp->silent++;
		load_media(usalp, dp, FALSE);
		usalp->silent--;
	}

	usalp->silent++;
	if (((dp->cdr_flags & CDR_TRAYLOAD) == 0) ||
				!wait_unit_ready(usalp, 5)) {
		static FILE	*tty = NULL;

		printf("Re-load disk and hit <CR>");
		if (isgui)
			printf("\n");
		flush();

		if (tty == NULL) {
			tty = stdin;
			if ((dp->cdr_cmdflags & F_STDIN) != 0)
				tty = fileluopen(STDERR_FILENO, "rw");
		}
#ifdef	F_GETFL
		if (tty != NULL)
			f = fcntl(fileno(tty), F_GETFL, 0);
		if (f < 0 || (f & O_ACCMODE) == O_WRONLY) {
#ifdef	SIGUSR1
			signal(SIGUSR1, catchsig);
			printf("Controlling file not open for reading, send SIGUSR1 to continue.\n");
			flush();
			pause();
#endif
		} else
#endif
		if (rols_fgetline(tty, ans, 1) < 0)
			comerrno(EX_BAD, "Aborted by EOF on input.\n");
	}
	usalp->silent--;

	load_media(usalp, dp, TRUE);
}

void 
set_secsize(SCSI *usalp, int secsize)
{
	if (secsize > 0) {
		/*
		 * Try to restore the old sector size.
		 */
		usalp->silent++;
		select_secsize(usalp, secsize);
		usalp->silent--;
	}
}

static int 
get_dmaspeed(SCSI *usalp, cdr_t *dp)
{
	int	i;
	long	t;
	int	bs;
	int	tsize;

  if(getenv("CDR_NODMATEST"))
     return -1;

  if (debug || lverbose)
     fprintf( stderr, 
           "Beginning DMA speed test. Set CDR_NODMATEST environment variable if device\n"
           "communication breaks or freezes immediately after that.\n" );

	fillbytes((caddr_t)buf, 4, '\0');
	tsize = 0;
	usalp->silent++;
	i = read_buffer(usalp, buf, 4, 0);
	usalp->silent--;
	if (i < 0)
		return (-1);
	tsize = a_to_u_4_byte(buf);
	if (tsize <= 0)
		return (-1);

	if (gettimeofday(&starttime, (struct timezone *)0) < 0)
		return (-1);

	bs = bufsize;
	if (tsize < bs)
		bs = tsize;
	for (i = 0; i < 100; i++) {
		if (read_buffer(usalp, buf, bs, 0) < 0)
			return (-1);
	}
	if (gettimeofday(&fixtime, (struct timezone *)0) < 0) {
		errmsg("Cannot get DMA stop time\n");
		return (-1);
	}
	timevaldiff(&starttime, &fixtime);
	tsize = bs * 100;
	t = fixtime.tv_sec * 1000 + fixtime.tv_usec / 1000;
	if (t <= 0)
		return (-1);
#ifdef	DEBUG
	fprintf(stderr, "Read Speed: %lu %ld %ld kB/s %ldx CD %ldx DVD\n",
		tsize, t, tsize/t, tsize/t/176, tsize/t/1385);
#endif

	return (tsize/t);
}


static BOOL 
do_opc(SCSI *usalp, cdr_t *dp, int flags)
{
	if ((flags & F_DUMMY) == 0 && dp->cdr_opc) {
		if (debug || lverbose) {
			printf("Performing OPC...\n");
			flush();
		}
		if (dp->cdr_opc(usalp, NULL, 0, TRUE) < 0) {
			errmsgno(EX_BAD, "OPC failed.\n");
			if ((flags & F_FORCE) == 0)
				return (FALSE);
		}
	}
	return (TRUE);
}

static void 
check_recovery(SCSI *usalp, cdr_t *dp, int flags)
{
	if ((*dp->cdr_check_recovery)(usalp, dp)) {
		errmsgno(EX_BAD, "Recovery needed.\n");
		unload_media(usalp, dp, flags);
		comexit(EX_BAD);
	}
}

#ifndef	DEBUG
#define	DEBUG
#endif
void 
audioread(SCSI *usalp, cdr_t *dp, int flags)
{
#ifdef	DEBUG
	int speed = 1;
	int	oflags = dp->cdr_cmdflags;

	dp->cdr_cmdflags &= ~F_DUMMY;
	if ((*dp->cdr_set_speed_dummy)(usalp, dp, &speed) < 0)
		comexit(-1);
	dp->cdr_dstat->ds_wspeed = speed; /* XXX Remove 'speed' in future */
	dp->cdr_cmdflags = oflags;

	if ((*dp->cdr_set_secsize)(usalp, 2352) < 0)
		comexit(-1);
	usalp->cap->c_bsize = 2352;

	read_scsi(usalp, buf, 1000, 1);
	printf("XXX:\n");
	write(1, buf, 512); /* FIXME: handle return value */
	unload_media(usalp, dp, flags);
	comexit(0);
#endif
}

static void 
print_msinfo(SCSI *usalp, cdr_t *dp)
{
	long	off;
	long	fa;

	if ((*dp->cdr_session_offset)(usalp, &off) < 0) {
		errmsgno(EX_BAD, "Cannot read session offset\n");
		return;
	}
	if (lverbose)
		printf("session offset: %ld\n", off);

	if (dp->cdr_next_wr_address(usalp, (track_t *)0, &fa) < 0) {
		errmsgno(EX_BAD, "Cannot read first writable address\n");
		return;
	}
	printf("%ld,%ld\n", off, fa);
	if(msifile) {
		FILE *f = fopen(msifile, "w");
		if(f) {
			fprintf(f, "%ld,%ld", off, fa);
			fclose(f);
		}
		else {
			perror("Unable to write multi session info file");
			exit(EXIT_FAILURE);
		}
	}
}

static void 
print_toc(SCSI *usalp, cdr_t *dp)
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

	usalp->silent++;
	if (read_capacity(usalp) < 0) {
		usalp->silent--;
		errmsgno(EX_BAD, "Cannot read capacity\n");
		return;
	}
	usalp->silent--;
	if (read_tochdr(usalp, dp, &first, &last) < 0) {
		errmsgno(EX_BAD, "Cannot read TOC/PMA\n");
		return;
	}
	printf("first: %d last %d\n", first, last);
	for (i = first; i <= last; i++) {
		read_trackinfo(usalp, i, &lba, &msf, &adr, &control, &mode);
		xlba = -150 +
			msf.msf_frame + (75*msf.msf_sec) + (75*60*msf.msf_min);
		if (xlba == lba/4)
			lba = xlba;
		print_track(i, lba, &msf, adr, control, mode);
	}
	i = 0xAA;
	read_trackinfo(usalp, i, &lba, &msf, &adr, &control, &mode);
	xlba = -150 +
		msf.msf_frame + (75*msf.msf_sec) + (75*60*msf.msf_min);
	if (xlba == lba/4)
		lba = xlba;
	print_track(i, lba, &msf, adr, control, mode);
	if (lverbose > 1) {
		usalp->silent++;
		if (read_cdtext(usalp) < 0)
			errmsgno(EX_BAD, "No CD-Text or CD-Text unaware drive.\n");
		usalp->silent++;
	}
}

static void 
print_track(int track, long lba, struct msf *msp, int adr, 
				int control, int mode)
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

#ifdef	HAVE_SYS_PRIOCNTL_H	/* The preferred SYSvR4 schduler */

#include <sys/procset.h>	/* Needed for SCO Openserver */
#include <sys/priocntl.h>
#include <sys/rtpriocntl.h>

void 
raisepri(int pri)
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

	movebytes(info.pc_clinfo, &rtinfo, sizeof (rtinfo_t));

	/* set priority to max */
	rtparam.rt_pri = rtinfo.rt_maxpri - pri;
	rtparam.rt_tqsecs = 0;
	rtparam.rt_tqnsecs = RT_TQDEF;
	param.pc_cid = info.pc_cid;
	movebytes(&rtparam, param.pc_clparms, sizeof (rtparms_t));
	ret = priocntl(P_PID, pid, PC_SETPARMS, (void *)&param);
	if (ret == -1) {
		errmsg("WARNING: Cannot set priority class parameters priocntl(PC_SETPARMS)\n");
		errmsgno(EX_BAD, "WARNING: This causes a high risk for buffer underruns.\n");
	}
}

#else	/* HAVE_SYS_PRIOCNTL_H */

#if defined(_POSIX_PRIORITY_SCHEDULING) && _POSIX_PRIORITY_SCHEDULING -0 >= 0
/*
 * The second best choice: POSIX real time scheduling.
 */
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

static int 
rt_raisepri(int pri)
{
	struct sched_param scp;

	/*
	 * Verify that scheduling is available
	 */
#ifdef	_SC_PRIORITY_SCHEDULING
	if (sysconf(_SC_PRIORITY_SCHEDULING) == -1) {
		errmsg("WARNING: RR-scheduler not available, disabling.\n");
		return (-1);
	}
#endif
	fillbytes(&scp, sizeof (scp), '\0');
	scp.sched_priority = sched_get_priority_max(SCHED_RR) - pri;
	if (sched_setscheduler(0, SCHED_RR, &scp) < 0) {
		if(lverbose>2)
       errmsg("WARNING: Cannot set RR-scheduler\n");
		return (-1);
	}
	return (0);
}

#else	/* _POSIX_PRIORITY_SCHEDULING */

#ifdef	__CYGWIN32__
/*
 * Win32 specific priority settings.
 */
/*
 * NOTE: Base.h from Cygwin-B20 has a second typedef for BOOL.
 *	 We define BOOL to make all static code use BOOL
 *	 from Windows.h and use the hidden __SBOOL for
 *	 our global interfaces.
 *
 * NOTE: windows.h from Cygwin-1.x includes a structure field named sample,
 *	 so me may not define our own 'sample' or need to #undef it now.
 *	 With a few nasty exceptions, Microsoft assumes that any global
 *	 defines or identifiers will begin with an Uppercase letter, so
 *	 there may be more of these problems in the future.
 *
 * NOTE: windows.h defines interface as an alias for struct, this
 *	 is used by COM/OLE2, I guess it is class on C++
 *	 We man need to #undef 'interface'
 */
#define	BOOL	WBOOL		/* This is the Win BOOL		*/
#define	format	__format	/* Avoid format parameter hides global ... */
#include <windows.h>
#undef format
#undef interface

static int 
rt_raisepri(int pri)
{
	int prios[] = {THREAD_PRIORITY_TIME_CRITICAL, THREAD_PRIORITY_HIGHEST};

	/* set priority class */
	if (SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS) == FALSE) {
		if(debug || lverbose>2)
       errmsgno(EX_BAD, "No realtime priority class possible.\n");
		return (-1);
	}

	/* set thread priority */
	if (pri >= 0 && pri <= 1 && SetThreadPriority(GetCurrentThread(), prios[pri]) == FALSE) {
		errmsgno(EX_BAD, "Could not set realtime priority.\n");
		return (-1);
	}
	return (0);
}

#else
/*
 * This OS does not support real time scheduling.
 */
static int 
rt_raisepri(int pri)
{
	return (-1);
}

#endif	/* __CYGWIN32__ */

#endif	/* _POSIX_PRIORITY_SCHEDULING */

void 
raisepri(int pri)
{
	if (rt_raisepri(pri) >= 0)
		return;
#if	defined(HAVE_SETPRIORITY) && defined(PRIO_PROCESS)

	if (setpriority(PRIO_PROCESS, getpid(), -20 + pri) < 0) {
		if(debug || lverbose>2)
       fprintf(stderr,
             "WARNING: Cannot set priority using setpriority(),"
             "increased risk for buffer underruns.\n");
	}
#else
#ifdef	HAVE_DOSSETPRIORITY	/* RT priority on OS/2 */
	/*
	 * Set priority to timecritical 31 - pri (arg)
	 */
	DosSetPriority(0, 3, 31, 0);
	DosSetPriority(0, 3, -pri, 0);
#else
#if	defined(HAVE_NICE) && !defined(__DJGPP__) /* DOS has nice but no multitasking */
	if (nice(-20 + pri) == -1) {
		errmsg("WARNING: Cannot set priority using nice().\n");
		errmsgno(EX_BAD, "WARNING: This causes a high risk for buffer underruns.\n");
	}
#else
	errmsgno(EX_BAD, "WARNING: Cannot set priority on this OS.\n");
	errmsgno(EX_BAD, "WARNING: This causes a high risk for buffer underruns.\n");
#endif
#endif
#endif
}

#endif	/* HAVE_SYS_PRIOCNTL_H */

#ifdef	HAVE_SELECT
/*
 * sys/types.h and sys/time.h are already included.
 */
#else
#	include	<stropts.h>
#	include	<poll.h>

#ifndef	INFTIM
#define	INFTIM	(-1)
#endif
#endif

#if	defined(HAVE_SELECT) && defined(NEED_SYS_SELECT_H)
#include <sys/select.h>
#endif
#if	defined(HAVE_SELECT) && defined(NEED_SYS_SOCKET_H)
#include <sys/socket.h>
#endif

static void 
wait_input()
{
#ifdef	HAVE_SELECT
	fd_set	in;

	FD_ZERO(&in);
	FD_SET(STDIN_FILENO, &in);
	select(1, &in, NULL, NULL, 0);
#else
	struct pollfd pfd;

	pfd.fd = STDIN_FILENO;
	pfd.events = POLLIN;
	pfd.revents = 0;
	poll(&pfd, (unsigned long)1, INFTIM);
#endif
}

static void 
checkgui()
{
	struct stat st;

	if (fstat(STDERR_FILENO, &st) >= 0 && !S_ISCHR(st.st_mode)) {
		isgui = TRUE;
		if (lverbose > 1)
			printf("Using remote (pipe) mode for interactive i/o.\n");
	}
}

static int 
getbltype(char *optstr, long *typep)
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
		fprintf(stderr, "Illegal blanking type '%s'.\n", optstr);
		blusage(EX_BAD);
		return (-1);
	}
	return (TRUE);
}

static int 
getformattype(char *optstr, long *typep)
{
	if (streql(optstr, "full")) {
		*typep = FULL_FORMAT;
	} else if (streql(optstr, "background")) {
		*typep = BACKGROUND_FORMAT;
	} else if (streql(optstr, "force")) {
		*typep = FORCE_FORMAT;
	} else if (streql(optstr, "help")) {
		formattypeusage(0);
	} else {
		fprintf(stderr, "Illegal blanking type '%s'.\n", optstr);
		formattypeusage(EX_BAD);
		return (-1);
	}
	return (TRUE);
}
static void 
print_drflags(cdr_t *dp)
{
	printf("Driver flags   : ");

	if ((dp->cdr_flags & CDR_DVD) != 0)
		printf("DVD ");

	if ((dp->cdr_flags & CDR_MMC3) != 0)
		printf("MMC-3 ");
	else if ((dp->cdr_flags & CDR_MMC2) != 0)
		printf("MMC-2 ");
	else if ((dp->cdr_flags & CDR_MMC) != 0)
		printf("MMC ");

	if ((dp->cdr_flags & CDR_SWABAUDIO) != 0)
		printf("SWABAUDIO ");
	if ((dp->cdr_flags & CDR_BURNFREE) != 0)
		printf("BURNFREE ");
	if ((dp->cdr_flags & CDR_VARIREC) != 0)
		printf("VARIREC ");
	if ((dp->cdr_flags & CDR_GIGAREC) != 0)
		printf("GIGAREC ");
	if ((dp->cdr_flags & CDR_AUDIOMASTER) != 0)
		printf("AUDIOMASTER ");
	if ((dp->cdr_flags & CDR_FORCESPEED) != 0)
		printf("FORCESPEED ");
	if ((dp->cdr_flags & CDR_SPEEDREAD) != 0)
		printf("SPEEDREAD ");
	if ((dp->cdr_flags & CDR_DISKTATTOO) != 0)
		printf("DISKTATTOO ");
	if ((dp->cdr_flags & CDR_SINGLESESS) != 0)
		printf("SINGLESESSION ");
	if ((dp->cdr_flags & CDR_HIDE_CDR) != 0)
		printf("HIDECDR ");
	printf("\n");
}

static void 
print_wrmodes(cdr_t *dp)
{
	BOOL	needblank = FALSE;

	printf("Supported modes: ");
	if ((dp->cdr_flags & CDR_TAO) != 0) {
		printf("TAO");
		needblank = TRUE;
	}
	if ((dp->cdr_flags & CDR_PACKET) != 0) {
		printf("%sPACKET", needblank?" ":"");
		needblank = TRUE;
	}
	if ((dp->cdr_flags & CDR_SAO) != 0) {
		printf("%sSAO", needblank?" ":"");
		needblank = TRUE;
	}
#ifdef	__needed__
	if ((dp->cdr_flags & (CDR_SAO|CDR_SRAW16)) == (CDR_SAO|CDR_SRAW16)) {
		printf("%sSAO/R16", needblank?" ":"");
		needblank = TRUE;
	}
#endif
	if ((dp->cdr_flags & (CDR_SAO|CDR_SRAW96P)) == (CDR_SAO|CDR_SRAW96P)) {
		printf("%sSAO/R96P", needblank?" ":"");
		needblank = TRUE;
	}
	if ((dp->cdr_flags & (CDR_SAO|CDR_SRAW96R)) == (CDR_SAO|CDR_SRAW96R)) {
		printf("%sSAO/R96R", needblank?" ":"");
		needblank = TRUE;
	}
	if ((dp->cdr_flags & (CDR_RAW|CDR_RAW16)) == (CDR_RAW|CDR_RAW16)) {
		printf("%sRAW/R16", needblank?" ":"");
		needblank = TRUE;
	}
	if ((dp->cdr_flags & (CDR_RAW|CDR_RAW96P)) == (CDR_RAW|CDR_RAW96P)) {
		printf("%sRAW/R96P", needblank?" ":"");
		needblank = TRUE;
	}
	if ((dp->cdr_flags & (CDR_RAW|CDR_RAW96R)) == (CDR_RAW|CDR_RAW96R)) {
		printf("%sRAW/R96R", needblank?" ":"");
		needblank = TRUE;
	}
	printf("\n");
}

static BOOL 
check_wrmode(cdr_t *dp, int wmode, int tflags)
{
	int	cdflags = dp->cdr_flags;

	if ((tflags & TI_PACKET) != 0 && (cdflags & CDR_PACKET) == 0) {
		errmsgno(EX_BAD, "Drive does not support PACKET recording.\n");
		return (FALSE);
	}
	if ((tflags & TI_TAO) != 0 && (cdflags & CDR_TAO) == 0) {
		errmsgno(EX_BAD, "Drive does not support TAO recording.\n");
		return (FALSE);
	}
	if ((wmode & F_SAO) != 0) {
		if ((cdflags & CDR_SAO) == 0) {
			errmsgno(EX_BAD, "Drive does not support SAO recording.\n");
			if ((cdflags & CDR_RAW) != 0)
				errmsgno(EX_BAD, "Try -raw option.\n");
			return (FALSE);
		}
#ifdef	__needed__
		if ((tflags & TI_RAW16) != 0 && (cdflags & CDR_SRAW16) == 0) {
			errmsgno(EX_BAD, "Drive does not support SAO/RAW16.\n");
			goto badsecs;
		}
#endif
		if ((tflags & (TI_RAW|TI_RAW16|TI_RAW96R)) == TI_RAW && (cdflags & CDR_SRAW96P) == 0) {
			errmsgno(EX_BAD, "Drive does not support SAO/RAW96P.\n");
			goto badsecs;
		}
		if ((tflags & (TI_RAW|TI_RAW16|TI_RAW96R)) == (TI_RAW|TI_RAW96R) && (cdflags & CDR_SRAW96R) == 0) {
			errmsgno(EX_BAD, "Drive does not support SAO/RAW96R.\n");
			goto badsecs;
		}
	}
	if ((wmode & F_RAW) != 0) {
		if ((cdflags & CDR_RAW) == 0) {
			errmsgno(EX_BAD, "Drive does not support RAW recording.\n");
			return (FALSE);
		}
		if ((tflags & TI_RAW16) != 0 && (cdflags & CDR_RAW16) == 0) {
			errmsgno(EX_BAD, "Drive does not support RAW/RAW16.\n");
			goto badsecs;
		}
		if ((tflags & (TI_RAW|TI_RAW16|TI_RAW96R)) == TI_RAW && (cdflags & CDR_RAW96P) == 0) {
			errmsgno(EX_BAD, "Drive does not support RAW/RAW96P.\n");
			goto badsecs;
		}
		if ((tflags & (TI_RAW|TI_RAW16|TI_RAW96R)) == (TI_RAW|TI_RAW96R) && (cdflags & CDR_RAW96R) == 0) {
			errmsgno(EX_BAD, "Drive does not support RAW/RAW96R.\n");
			goto badsecs;
		}
	}
	return (TRUE);

badsecs:
	if ((wmode & F_SAO) != 0)
		cdflags &= ~(CDR_RAW16|CDR_RAW96P|CDR_RAW96R);
	if ((wmode & F_RAW) != 0)
		cdflags &= ~(CDR_SRAW96P|CDR_SRAW96R);

	if ((cdflags & (CDR_SRAW96R|CDR_RAW96R)) != 0)
		errmsgno(EX_BAD, "Try -raw96r option.\n");
	else if ((cdflags & (CDR_SRAW96P|CDR_RAW96P)) != 0)
		errmsgno(EX_BAD, "Try -raw96p option.\n");
	else if ((cdflags & CDR_RAW16) != 0)
		errmsgno(EX_BAD, "Try -raw16 option.\n");
	return (FALSE);
}

static void 
set_wrmode(cdr_t *dp, int wmode, int tflags)
{
	dstat_t	*dsp = dp->cdr_dstat;

	if ((tflags & TI_PACKET) != 0) {
		dsp->ds_wrmode = WM_PACKET;
		return;
	}
	if ((tflags & TI_TAO) != 0) {
		dsp->ds_wrmode = WM_TAO;
		return;
	}
	if ((wmode & F_SAO) != 0) {
		if ((tflags & (TI_RAW|TI_RAW16|TI_RAW96R)) == 0) {
			dsp->ds_wrmode = WM_SAO;
			return;
		}
		if ((tflags & TI_RAW16) != 0) {		/* Is this needed? */
			dsp->ds_wrmode = WM_SAO_RAW16;
			return;
		}
		if ((tflags & (TI_RAW|TI_RAW16|TI_RAW96R)) == TI_RAW) {
			dsp->ds_wrmode = WM_SAO_RAW96P;
			return;
		}
		if ((tflags & (TI_RAW|TI_RAW16|TI_RAW96R)) == (TI_RAW|TI_RAW96R)) {
			dsp->ds_wrmode = WM_SAO_RAW96R;
			return;
		}
	}
	if ((wmode & F_RAW) != 0) {
		if ((tflags & TI_RAW16) != 0) {
			dsp->ds_wrmode = WM_RAW_RAW16;
			return;
		}
		if ((tflags & (TI_RAW|TI_RAW16|TI_RAW96R)) == TI_RAW) {
			dsp->ds_wrmode = WM_RAW_RAW96P;
			return;
		}
		if ((tflags & (TI_RAW|TI_RAW16|TI_RAW96R)) == (TI_RAW|TI_RAW96R)) {
			dsp->ds_wrmode = WM_RAW_RAW96R;
			return;
		}
	}
	dsp->ds_wrmode = WM_NONE;
}

#if	defined(linux) || defined(__linux) || defined(__linux__)
#ifdef	HAVE_UNAME
#include <sys/utsname.h>
#endif
#endif

#ifdef __linux__
static int 
get_cap(cap_value_t cap_array)
{ 
    	  int ret;
	  cap_t capa;
	  capa = cap_get_proc();
	  cap_set_flag(capa, CAP_EFFECTIVE,  1, &cap_array, CAP_SET);
	  ret = cap_set_proc(capa);
	  cap_free(capa);
	  return ret; 
}
#endif
