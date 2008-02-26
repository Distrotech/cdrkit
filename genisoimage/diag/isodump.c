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

/* @(#)isodump.c	1.27 05/05/15 joerg */
/*
 * File isodump.c - dump iso9660 directory information.
 *
 *
 * Written by Eric Youngdale (1993).
 *
 * Copyright 1993 Yggdrasil Computing, Incorporated
 * Copyright (c) 1999-2004 J. Schilling
 *
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
#include <stdxlib.h>
#include <unixstd.h>
#include <strdefs.h>
#include <utypes.h>

#include <stdio.h>
#include <standard.h>
#include <ttydefs.h>
#include <signal.h>
#include <schily.h>

#include "../scsi.h"
#include "../../wodim/defaults.h"

/*
 * XXX JS: Some structures have odd lengths!
 * Some compilers (e.g. on Sun3/mc68020) padd the structures to even length.
 * For this reason, we cannot use sizeof (struct iso_path_table) or
 * sizeof (struct iso_directory_record) to compute on disk sizes.
 * Instead, we use offsetof(..., name) and add the name size.
 * See iso9660.h
 */
#ifndef	offsetof
#define	offsetof(TYPE, MEMBER)	((size_t) &((TYPE *)0)->MEMBER)
#endif

/*
 * Note: always use these macros to avoid problems.
 *
 * ISO_ROUND_UP(X)	may cause an integer overflow and thus give
 *			incorrect results. So avoid it if possible.
 *
 * ISO_BLOCKS(X)	is overflow safe. Prefer this when ever it is possible.
 */
#define	SECTOR_SIZE	(2048)
#define	ISO_ROUND_UP(X)	(((X) + (SECTOR_SIZE - 1)) & ~(SECTOR_SIZE - 1))
#define	ISO_BLOCKS(X)	(((X) / SECTOR_SIZE) + (((X)%SECTOR_SIZE)?1:0))

#define	infile	in_image
FILE	*infile = NULL;
off_t	file_addr;
unsigned char buffer[2048];
unsigned char search[64];
int blocksize;

#define	PAGE	sizeof (buffer)

#define	ISODCL(from, to)	(to - from + 1)

struct iso_primary_descriptor {
	unsigned char type			[ISODCL(1,   1)]; /* 711 */
	unsigned char id			[ISODCL(2,   6)];
	unsigned char version			[ISODCL(7,   7)]; /* 711 */
	unsigned char unused1			[ISODCL(8,   8)];
	unsigned char system_id			[ISODCL(9,   40)]; /* aunsigned chars */
	unsigned char volume_id			[ISODCL(41,  72)]; /* dunsigned chars */
	unsigned char unused2			[ISODCL(73,  80)];
	unsigned char volume_space_size		[ISODCL(81,  88)]; /* 733 */
	unsigned char unused3			[ISODCL(89,  120)];
	unsigned char volume_set_size		[ISODCL(121, 124)]; /* 723 */
	unsigned char volume_sequence_number	[ISODCL(125, 128)]; /* 723 */
	unsigned char logical_block_size	[ISODCL(129, 132)]; /* 723 */
	unsigned char path_table_size		[ISODCL(133, 140)]; /* 733 */
	unsigned char type_l_path_table		[ISODCL(141, 144)]; /* 731 */
	unsigned char opt_type_l_path_table	[ISODCL(145, 148)]; /* 731 */
	unsigned char type_m_path_table		[ISODCL(149, 152)]; /* 732 */
	unsigned char opt_type_m_path_table	[ISODCL(153, 156)]; /* 732 */
	unsigned char root_directory_record	[ISODCL(157, 190)]; /* 9.1 */
	unsigned char volume_set_id		[ISODCL(191, 318)]; /* dunsigned chars */
	unsigned char publisher_id		[ISODCL(319, 446)]; /* achars */
	unsigned char preparer_id		[ISODCL(447, 574)]; /* achars */
	unsigned char application_id		[ISODCL(575, 702)]; /* achars */
	unsigned char copyright_file_id		[ISODCL(703, 739)]; /* 7.5 dchars */
	unsigned char abstract_file_id		[ISODCL(740, 776)]; /* 7.5 dchars */
	unsigned char bibliographic_file_id	[ISODCL(777, 813)]; /* 7.5 dchars */
	unsigned char creation_date		[ISODCL(814, 830)]; /* 8.4.26.1 */
	unsigned char modification_date		[ISODCL(831, 847)]; /* 8.4.26.1 */
	unsigned char expiration_date		[ISODCL(848, 864)]; /* 8.4.26.1 */
	unsigned char effective_date		[ISODCL(865, 881)]; /* 8.4.26.1 */
	unsigned char file_structure_version	[ISODCL(882, 882)]; /* 711 */
	unsigned char unused4			[ISODCL(883, 883)];
	unsigned char application_data		[ISODCL(884, 1395)];
	unsigned char unused5			[ISODCL(1396, 2048)];
};

struct iso_directory_record {
	unsigned char length			[ISODCL(1, 1)]; /* 711 */
	unsigned char ext_attr_length		[ISODCL(2, 2)]; /* 711 */
	unsigned char extent			[ISODCL(3, 10)]; /* 733 */
	unsigned char size			[ISODCL(11, 18)]; /* 733 */
	unsigned char date			[ISODCL(19, 25)]; /* 7 by 711 */
	unsigned char flags			[ISODCL(26, 26)];
	unsigned char file_unit_size		[ISODCL(27, 27)]; /* 711 */
	unsigned char interleave		[ISODCL(28, 28)]; /* 711 */
	unsigned char volume_sequence_number	[ISODCL(29, 32)]; /* 723 */
	unsigned char name_len			[ISODCL(33, 33)]; /* 711 */
	unsigned char name			[1];
};

static int	isonum_731(char * p);
static int	isonum_72(char * p);
static int	isonum_723(char * p);
static int	isonum_733(unsigned char * p);
static void	reset_tty(void);
static void	set_tty(void);
static void	onsusp(int signo);
static void	crsr2(int row, int col);
static int	parse_rr(unsigned char * pnt, int len, int cont_flag);
static void	dump_rr(struct iso_directory_record * idr);
static void	showblock(int flag);
static int	getbyte(void);
static void	usage(int excode);

static int
isonum_731(char *p)
{
	return ((p[0] & 0xff)
		| ((p[1] & 0xff) << 8)
		| ((p[2] & 0xff) << 16)
		| ((p[3] & 0xff) << 24));
}

static int
isonum_721(char *p)
{
	return ((p[0] & 0xff) | ((p[1] & 0xff) << 8));
}

static int
isonum_723(char *p)
{
#if 0
	if (p[0] != p[3] || p[1] != p[2]) {
#ifdef	USE_LIBSCHILY
		comerrno(EX_BAD, "invalid format 7.2.3 number\n");
#else
		fprintf(stderr, "invalid format 7.2.3 number\n");
		exit(1);
#endif
	}
#endif
	return (isonum_721(p));
}


static int
isonum_733(unsigned char *p)
{
	return (isonum_731((char *)p));
}

#ifdef	USE_V7_TTY
static	struct sgttyb	savetty;
static	struct sgttyb	newtty;
#else
static	struct termios savetty;
static	struct termios newtty;
#endif

static void
reset_tty()
{
#ifdef USE_V7_TTY
	if (ioctl(STDIN_FILENO, TIOCSETN, &savetty) == -1) {
#else
#ifdef TCSANOW
	if (tcsetattr(STDIN_FILENO, TCSANOW, &savetty) == -1)  {
#else
	if (ioctl(STDIN_FILENO, TCSETAF, &savetty) == -1) {
#endif
#endif
#ifdef	USE_LIBSCHILY
		comerr("Cannot put tty into normal mode\n");
#else
		printf("Cannot put tty into normal mode\n");
		exit(1);
#endif
	}
}

static void
set_tty()
{
#ifdef USE_V7_TTY
	if (ioctl(STDIN_FILENO, TIOCSETN, &newtty) == -1) {
#else
#ifdef TCSANOW
	if (tcsetattr(STDIN_FILENO, TCSANOW, &newtty) == -1) {
#else
	if (ioctl(STDIN_FILENO, TCSETAF, &newtty) == -1) {
#endif
#endif
#ifdef	USE_LIBSCHILY
		comerr("Cannot put tty into raw mode\n");
#else
		printf("Cannot put tty into raw mode\n");
		exit(1);
#endif
	}
}

/* Come here when we get a suspend signal from the terminal */

static void
onsusp(int signo)
{
#ifdef	SIGTTOU
	/* ignore SIGTTOU so we don't get stopped if csh grabs the tty */
	signal(SIGTTOU, SIG_IGN);
#endif
	reset_tty();
	fflush(stdout);
#ifdef	SIGTTOU
	signal(SIGTTOU, SIG_DFL);
	/* Send the TSTP signal to suspend our process group */
	signal(SIGTSTP, SIG_DFL);
/*	sigsetmask(0);*/
	kill(0, SIGTSTP);
	/* Pause for station break */

	/* We're back */
	signal(SIGTSTP, onsusp);
#endif
	set_tty();
}



static void
crsr2(int row, int col)
{
	printf("\033[%d;%dH", row, col);
}

static int
parse_rr(unsigned char *pnt, int len, int cont_flag)
{
	int		slen;
	int		ncount;
	int		extent;
	off_t		cont_extent;
	int		cont_offset;
	int		cont_size;
	int		flag1;
	int		flag2;
	unsigned char	*pnts;
	char		symlinkname[1024];
	char		name[1024];
	int		goof;

/*	printf(" RRlen=%d ", len); */

	symlinkname[0] = 0;

	cont_extent = (off_t)0;
	cont_offset = cont_size = 0;

	ncount = 0;
	flag1 = flag2 = 0;
	while (len >= 4) {
		if (ncount)
			printf(",");
		else
			printf("[");
		printf("%c%c", pnt[0], pnt[1]);
		if (pnt[3] != 1 && pnt[3] != 2) {
			printf("**BAD RRVERSION (%d) for %c%c\n", pnt[3], pnt[0], pnt[1]);
			return (0);	/* JS ??? Is this right ??? */
		} else if (pnt[0] == 'R' && pnt[1] == 'R') {
			printf("=%d", pnt[3]);
		}
		ncount++;
		if (pnt[0] == 'R' && pnt[1] == 'R') flag1 = pnt[4] & 0xff;
		if (strncmp((char *)pnt, "PX", 2) == 0) flag2 |= 1;
		if (strncmp((char *)pnt, "PN", 2) == 0) flag2 |= 2;
		if (strncmp((char *)pnt, "SL", 2) == 0) flag2 |= 4;
		if (strncmp((char *)pnt, "NM", 2) == 0) {
			slen = pnt[2] - 5;
			pnts = pnt+5;
			if ((pnt[4] & 6) != 0) {
				printf("*");
			}
			memset(name, 0, sizeof (name));
			memcpy(name, pnts, slen);
			printf("=%s", name);
			flag2 |= 8;
		}
		if (strncmp((char *)pnt, "CL", 2) == 0) flag2 |= 16;
		if (strncmp((char *)pnt, "PL", 2) == 0) flag2 |= 32;
		if (strncmp((char *)pnt, "RE", 2) == 0) flag2 |= 64;
		if (strncmp((char *)pnt, "TF", 2) == 0) flag2 |= 128;

		if (strncmp((char *)pnt, "PX", 2) == 0) {
			extent = isonum_733(pnt+12);
			printf("=%x", extent);
		}

		if (strncmp((char *)pnt, "CE", 2) == 0) {
			cont_extent = (off_t)isonum_733(pnt+4);
			cont_offset = isonum_733(pnt+12);
			cont_size = isonum_733(pnt+20);
			printf("=[%x,%x,%d]", (int)cont_extent, cont_offset,
								cont_size);
		}

		if (strncmp((char *)pnt, "PL", 2) == 0 || strncmp((char *)pnt, "CL", 2) == 0) {
			extent = isonum_733(pnt+4);
			printf("=%x", extent);
		}

		if (strncmp((char *)pnt, "SL", 2) == 0) {
			int	cflag;

			cflag = pnt[4];
			pnts = pnt+5;
			slen = pnt[2] - 5;
			while (slen >= 1) {
				switch (pnts[0] & 0xfe) {
				case 0:
					strncat(symlinkname, (char *)(pnts+2), pnts[1]);
					break;
				case 2:
					strcat(symlinkname, ".");
					break;
				case 4:
					strcat(symlinkname, "..");
					break;
				case 8:
					if ((pnts[0] & 1) == 0)
						strcat(symlinkname, "/");
					break;
				case 16:
					strcat(symlinkname, "/mnt");
					printf("Warning - mount point requested");
					break;
				case 32:
					strcat(symlinkname, "kafka");
					printf("Warning - host_name requested");
					break;
				default:
					printf("Reserved bit setting in symlink");
					goof++;
					break;
				}
				if ((pnts[0] & 0xfe) && pnts[1] != 0) {
					printf("Incorrect length in symlink component");
				}
				if ((pnts[0] & 1) == 0)
					strcat(symlinkname, "/");

				slen -= (pnts[1] + 2);
				pnts += (pnts[1] + 2);
			}
			if (cflag)
				strcat(symlinkname, "+");
			printf("=%s", symlinkname);
			symlinkname[0] = 0;
		}

		len -= pnt[2];
		pnt += pnt[2];
		if (len <= 3 && cont_extent) {
			unsigned char sector[2048];

#ifdef	USE_SCG
			readsecs(cont_extent * blocksize / 2048, sector, ISO_BLOCKS(sizeof (sector)));
#else
			lseek(fileno(infile), cont_extent * blocksize, SEEK_SET);
			read(fileno(infile), sector, sizeof (sector));
#endif
			flag2 |= parse_rr(&sector[cont_offset], cont_size, 1);
		}
	}
	if (ncount)
		printf("]");
	if (!cont_flag && flag1 != flag2) {
		printf("Flag %x != %x", flag1, flag2);
		goof++;
	}
	return (flag2);
}

static void
dump_rr(struct iso_directory_record *idr)
{
	int		len;
	unsigned char	*pnt;

	len = idr->length[0] & 0xff;
	len -= offsetof(struct iso_directory_record, name[0]);
	len -= idr->name_len[0];
	pnt = (unsigned char *) idr;
	pnt += offsetof(struct iso_directory_record, name[0]);
	pnt += idr->name_len[0];
	if ((idr->name_len[0] & 1) == 0) {
		pnt++;
		len--;
	}
	parse_rr(pnt, len, 0);
}


static void
showblock(int flag)
{
	int	i;
	int	j;
	int	line;
	struct iso_directory_record	*idr;

#ifdef	USE_SCG
	readsecs(file_addr / 2048, buffer, ISO_BLOCKS(sizeof (buffer)));
#else
	lseek(fileno(infile), file_addr, SEEK_SET);
	read(fileno(infile), buffer, sizeof (buffer));
#endif
	for (i = 0; i < 60; i++)
		printf("\n");
	fflush(stdout);
	i = line = 0;
	if (flag) {
		while (1 == 1) {
			crsr2(line+3, 1);
			idr = (struct iso_directory_record *) &buffer[i];
			if (idr->length[0] == 0)
				break;
			printf("%3d ", idr->length[0]);
			printf("[%2d] ", idr->volume_sequence_number[0]);
			printf("%5x ", isonum_733(idr->extent));
			printf("%8d ", isonum_733(idr->size));
			printf("%02x/", idr->flags[0]);
			printf((idr->flags[0] & 2) ? "*" : " ");
			if (idr->name_len[0] == 1 && idr->name[0] == 0)
				printf(".             ");
			else if (idr->name_len[0] == 1 && idr->name[0] == 1)
				printf("..            ");
			else {
				for (j = 0; j < (int)idr->name_len[0]; j++) printf("%c", idr->name[j]);
				for (j = 0; j < (14 - (int)idr->name_len[0]); j++) printf(" ");
			}
			dump_rr(idr);
			printf("\n");
			i += buffer[i];
			if (i > 2048 - offsetof(struct iso_directory_record, name[0]))
				break;
			line++;
		}
	}
	printf("\n");
	if (sizeof (file_addr) > sizeof (long)) {
		printf(" Zone, zone offset: %14llx %12.12llx  ",
			(Llong)file_addr / blocksize,
			(Llong)file_addr & (Llong)(blocksize - 1));
	} else {
		printf(" Zone, zone offset: %6lx %4.4lx  ",
			(long) (file_addr / blocksize),
			(long) file_addr & (blocksize - 1));
	}
	fflush(stdout);
}

static int
getbyte()
{
	char	c1;

	c1 = buffer[file_addr & (blocksize-1)];
	file_addr++;
	if ((file_addr & (blocksize-1)) == 0)
		showblock(0);
	return (c1);
}

static void
usage(int excode)
{
	errmsgno(EX_BAD, "Usage: %s [options] image\n",
						get_progname());

	fprintf(stderr, "Options:\n");
	fprintf(stderr, "\t-help, -h	Print this help\n");
	fprintf(stderr, "\t-version	Print version info and exit\n");
	fprintf(stderr, "\t-i filename	Filename to read ISO-9660 image from\n");
	fprintf(stderr, "\tdev=target	SCSI target to use as CD/DVD-Recorder\n");
	fprintf(stderr, "\nIf neither -i nor dev= are speficied, <image> is needed.\n");
	exit(excode);
}

int
main(int argc, char *argv[])
{
	int	cac;
	char	* const *cav;
	char	*opts = "help,h,version,i*,dev*";
	BOOL	help = FALSE;
	BOOL	prvers = FALSE;
	char	*filename = NULL;
	char	*devname = NULL;
	char	c;
	int	i;
	struct iso_primary_descriptor	ipd;
	struct iso_directory_record	*idr;

	save_args(argc, argv);

	cac = argc - 1;
	cav = argv + 1;
	if (getallargs(&cac, &cav, opts, &help, &help, &prvers,
			&filename, &devname) < 0) {
		errmsgno(EX_BAD, "Bad Option: '%s'\n", cav[0]);
		usage(EX_BAD);
	}
	if (help)
		usage(0);
	if (prvers) {
		printf("isodump %s (%s)\n", CDRKIT_VERSION, HOST_SYSTEM);
		exit(0);
	}
	cac = argc - 1;
	cav = argv + 1;
	if (filename == NULL && devname == NULL) {
		if (getfiles(&cac, &cav, opts) != 0) {
			filename = cav[0];
			cac--, cav++;
		}
	}
	if (getfiles(&cac, &cav, opts) != 0) {
		errmsgno(EX_BAD, "Bad Argument: '%s'\n", cav[0]);
		usage(EX_BAD);
	}
	if (filename != NULL && devname != NULL) {
		errmsgno(EX_BAD, "Only one of -i or dev= allowed\n");
		usage(EX_BAD);
	}
#ifdef	USE_SCG
	if (filename == NULL && devname == NULL)
		cdr_defaults(&devname, NULL, NULL, NULL);
#endif
	if (filename == NULL && devname == NULL) {
#ifdef	USE_LIBSCHILY
		errmsgno(EX_BAD, "ISO-9660 image not specified\n");
#else
		fprintf(stderr, "ISO-9660 image not specified\n");
#endif
		usage(EX_BAD);
	}

	if (filename != NULL)
		infile = fopen(filename, "rb");
	else
		filename = devname;

	if (infile != NULL) {
		/* EMPTY */;
#ifdef	USE_SCG
	} else if (scsidev_open(filename) < 0) {
#else
	} else {
#endif
#ifdef	USE_LIBSCHILY
		comerr("Cannot open '%s'\n", filename);
#else
		fprintf(stderr, "Cannot open '%s'\n", filename);
		exit(1);
#endif
	}

	file_addr = (off_t) (16 << 11);
#ifdef	USE_SCG
	readsecs(file_addr / 2048, &ipd, ISO_BLOCKS(sizeof (ipd)));
#else
	lseek(fileno(infile), file_addr, SEEK_SET);
	read(fileno(infile), &ipd, sizeof (ipd));
#endif
	idr = (struct iso_directory_record *)ipd.root_directory_record;

	blocksize = isonum_723((char *)ipd.logical_block_size);
	if (blocksize != 512 && blocksize != 1024 && blocksize != 2048) {
		blocksize = 2048;
	}

	file_addr = (off_t)isonum_733(idr->extent);
	file_addr = file_addr * blocksize;

/* Now setup the keyboard for single character input. */
#ifdef USE_V7_TTY
	if (ioctl(STDIN_FILENO, TIOCGETP, &savetty) == -1) {
#else
#ifdef TCSANOW
	if (tcgetattr(STDIN_FILENO, &savetty) == -1) {
#else
	if (ioctl(STDIN_FILENO, TCGETA, &savetty) == -1) {
#endif
#endif
#ifdef	USE_LIBSCHILY
		comerr("Stdin must be a tty\n");
#else
		printf("Stdin must be a tty\n");
		exit(1);
#endif
	}
	newtty = savetty;
#ifdef USE_V7_TTY
	newtty.sg_flags  &= ~(ECHO|CRMOD);
	newtty.sg_flags  |= CBREAK;
#else
	newtty.c_lflag   &= ~ICANON;
	newtty.c_lflag   &= ~ECHO;
	newtty.c_cc[VMIN] = 1;
#endif
	set_tty();
#ifdef	SIGTSTP
	signal(SIGTSTP, onsusp);
#endif
	on_comerr((void(*)(int, void *))reset_tty, NULL);

	do {
		if (file_addr < 0)
			file_addr = (off_t)0;
		showblock(1);
		read(STDIN_FILENO, &c, 1); /* FIXME: check return value */
		if (c == 'a')
			file_addr -= blocksize;
		if (c == 'b')
			file_addr += blocksize;
		if (c == 'g') {
			crsr2(20, 1);
			printf("Enter new starting block (in hex):");
			if (sizeof (file_addr) > sizeof (long)) {
				Llong	ll;
				scanf("%llx", &ll); /* FIXME: check return value */
				file_addr = (off_t)ll;
			} else {
				long	l;
				scanf("%lx", &l); /* FIXME: check return value */
				file_addr = (off_t)l;
			}
			file_addr = file_addr * blocksize;
			crsr2(20, 1);
			printf("                                     ");
		}
		if (c == 'f') {
			crsr2(20, 1);
			printf("Enter new search string:");
			fgets((char *)search, sizeof (search), stdin); /* FIXME: check return value */
			while (search[strlen((char *)search)-1] == '\n')
				search[strlen((char *)search)-1] = 0;
			crsr2(20, 1);
			printf("                                     ");
		}
		if (c == '+') {
			while (1 == 1) {
				int	slen;

				while (1 == 1) {
					c = getbyte();
					if (c == search[0])
						break;
				}
				slen = (int)strlen((char *)search);
				for (i = 1; i < slen; i++) {
					if (search[i] != getbyte())
						break;
				}
				if (i == slen)
					break;
			}
			file_addr &= ~(blocksize-1);
			showblock(1);
		}
		if (c == 'q')
			break;
	} while (1 == 1);
	reset_tty();
	if (infile != NULL)
		fclose(infile);
	return (0);
}
