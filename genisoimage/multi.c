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

/* @(#)multi.c	1.68 05/05/15 joerg */
/*
 * File multi.c - scan existing iso9660 image and merge into
 * iso9660 filesystem.  Used for multisession support.
 *
 * Written by Eric Youngdale (1996).
 * Copyright (c) 1999-2003 J. Schilling
 *
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <mconfig.h>
#include "genisoimage.h"
#include <timedefs.h>
#include <errno.h>
#include <utypes.h>
#include <schily.h>
#include <ctype.h>			/* Needed for printasc()	*/

#ifdef VMS

#include <sys/file.h>
#include <vms/fabdef.h>
#include "vms.h"
#endif

#ifndef howmany
#define	howmany(x, y)   (((x)+((y)-1))/(y))
#endif
#ifndef roundup
#define	roundup(x, y)   ((((x)+((y)-1))/(y))*(y))
#endif

/*
 * Cannot debug memset() with gdb on Linux, so use fillbytes()
 */
/*#define	memset(s, c, n)	fillbytes(s, n, c)*/

#define	TF_CREATE 1
#define	TF_MODIFY 2
#define	TF_ACCESS 4
#define	TF_ATTRIBUTES 8

static	int	isonum_711(unsigned char *p);
static	int	isonum_721(unsigned char *p);
static	int	isonum_723(unsigned char *p);
static	int	isonum_731(unsigned char *p);

static	void	printasc(char *txt, unsigned char *p, int len);
static	void	prbytes(char *txt, unsigned char *p, int len);
unsigned char	*parse_xa(unsigned char *pnt, int *lenp,
								 struct directory_entry *dpnt);
int	rr_flags(struct iso_directory_record *idr);
static	int	parse_rrflags(Uchar *pnt, int len, int cont_flag);
static	BOOL	find_rr(struct iso_directory_record *idr, Uchar **pntp, 
							  int *lenp);
static	 int	parse_rr(unsigned char *pnt, int len, 
								struct directory_entry *dpnt);
static	int	check_rr_dates(struct directory_entry *dpnt,
										struct directory_entry *current,
										struct stat *statbuf,
										struct stat *lstatbuf);
static	struct directory_entry **
		read_merging_directory(struct iso_directory_record *, int *);
static	int	free_mdinfo(struct directory_entry **, int len);
static	void	free_directory_entry(struct directory_entry * dirp);
static	void	merge_remaining_entries(struct directory *,
													struct directory_entry **, int);

static	int	merge_old_directory_into_tree(struct directory_entry *,
															struct directory *);
static	void	check_rr_relocation(struct directory_entry *de);

static int
isonum_711(unsigned char *p)
{
	return (*p & 0xff);
}

static int
isonum_721(unsigned char *p)
{
	return ((p[0] & 0xff) | ((p[1] & 0xff) << 8));
}

static int
isonum_723(unsigned char *p)
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
isonum_731(unsigned char *p)
{
	return ((p[0] & 0xff)
		| ((p[1] & 0xff) << 8)
		| ((p[2] & 0xff) << 16)
		| ((p[3] & 0xff) << 24));
}

int
isonum_733(unsigned char *p)
{
	return (isonum_731(p));
}

FILE	*in_image = NULL;

#ifndef	USE_SCG
/*
 * Don't define readsecs if genisoimage is linked with
 * the SCSI library.
 * readsecs() will be implemented as SCSI command in this case.
 *
 * Use global var in_image directly in readsecs()
 * the SCSI equivalent will not use a FILE* for I/O.
 *
 * The main point of this pointless abstraction is that Solaris won't let
 * you read 2K sectors from the cdrom driver.  The fact that 99.9% of the
 * discs out there have a 2K sectorsize doesn't seem to matter that much.
 * Anyways, this allows the use of a scsi-generics type of interface on
 * Solaris.
 */
static int
readsecs(int startsecno, void *buffer, int sectorcount)
{
	int		f = fileno(in_image);

	if (lseek(f, (off_t) startsecno * SECTOR_SIZE, SEEK_SET) == (off_t) - 1) {
#ifdef	USE_LIBSCHILY
		comerr(" Seek error on old image\n");
#else
		fprintf(stderr, " Seek error on old image\n");
		exit(10);
#endif
	}
	if (read(f, buffer, (sectorcount * SECTOR_SIZE))
		!= (sectorcount * SECTOR_SIZE)) {
#ifdef	USE_LIBSCHILY
		comerr(" Read error on old image\n");
#else
		fprintf(stderr, " Read error on old image\n");
		exit(10);
#endif
	}
	return (sectorcount * SECTOR_SIZE);
}

#endif

static void
printasc(char *txt, unsigned char *p, int len)
{
	int		i;

	fprintf(stderr, "%s ", txt);
	for (i = 0; i < len; i++) {
		if (isprint(p[i]))
			fprintf(stderr, "%c", p[i]);
		else
			fprintf(stderr, ".");
	}
	fprintf(stderr, "\n");
}

static void
prbytes(char *txt, register Uchar *p, register int len)
{
	fprintf(stderr, "%s", txt);
	while (--len >= 0)
		fprintf(stderr, " %02X", *p++);
	fprintf(stderr, "\n");
}

unsigned char *
parse_xa(unsigned char *pnt, int *lenp, struct directory_entry *dpnt)
{
	struct iso_xa_dir_record *xadp;
	int		len = *lenp;
static	int		did_xa = 0;

/*fprintf(stderr, "len: %d\n", len);*/

	if (len >= 14) {
		xadp = (struct iso_xa_dir_record *)pnt;

/*		if (dpnt) prbytes("XA ", pnt, len);*/
		if (xadp->signature[0] == 'X' && xadp->signature[1] == 'A' &&
				xadp->reserved[0] == '\0') {
			len -= 14;
			pnt += 14;
			*lenp = len;
			if (!did_xa) {
				did_xa = 1;
				errmsgno(EX_BAD, "Found XA directory extension record.\n");
			}
		} else if (pnt[2] == 0) {
			char *cp = NULL;

			if (dpnt)
				cp = (char *)&dpnt->isorec;
			if (cp) {
				prbytes("ISOREC:", (Uchar *)cp, 33+cp[32]);
				printasc("ISOREC:", (Uchar *)cp, 33+cp[32]);
				prbytes("XA REC:", pnt, len);
				printasc("XA REC:", pnt, len);
			}
			if (no_rr == 0) {
				errmsgno(EX_BAD, "Disabling RR / XA / AA.\n");
				no_rr = 1;
			}
			*lenp = 0;
			if (cp) {
				errmsgno(EX_BAD, "Problems with old ISO directory entry for file: '%s'.\n", &cp[33]);
			}
			errmsgno(EX_BAD, "Illegal extended directory attributes found (bad XA disk?).\n");
/*			errmsgno(EX_BAD, "Disabling Rock Ridge for old session.\n");*/
			comerrno(EX_BAD, "Try again using the -no-rr option.\n");
		}
	}
	if (len >= 4 && pnt[3] != 1 && pnt[3] != 2) {
		prbytes("BAD RR ATTRIBUTES:", pnt, len);
		printasc("BAD RR ATTRIBUTES:", pnt, len);
	}
	return (pnt);
}

static BOOL
find_rr(struct iso_directory_record *idr, Uchar **pntp, int *lenp)
{
	struct iso_xa_dir_record *xadp;
	int		len;
	unsigned char	*pnt;
	BOOL		ret = FALSE;

	len = idr->length[0] & 0xff;
	len -= sizeof (struct iso_directory_record);
	len += sizeof (idr->name);
	len -= idr->name_len[0];

	pnt = (unsigned char *) idr;
	pnt += sizeof (struct iso_directory_record);
	pnt -= sizeof (idr->name);
	pnt += idr->name_len[0];
	if ((idr->name_len[0] & 1) == 0) {
		pnt++;
		len--;
	}
	if (len >= 14) {
		xadp = (struct iso_xa_dir_record *)pnt;

		if (xadp->signature[0] == 'X' && xadp->signature[1] == 'A' &&
				xadp->reserved[0] == '\0') {
			len -= 14;
			pnt += 14;
			ret = TRUE;
		}
	}
	*pntp = pnt;
	*lenp = len;
	return (ret);
}

static int
parse_rrflags(Uchar *pnt, int len, int cont_flag)
{
	int	ncount;
	int	cont_extent;
	int	cont_offset;
	int	cont_size;
	int	flag1;
	int	flag2;

	cont_extent = cont_offset = cont_size = 0;

	ncount = 0;
	flag1 = flag2 = 0;
	while (len >= 4) {
		if (pnt[3] != 1 && pnt[3] != 2) {
#ifdef USE_LIBSCHILY
			errmsgno(EX_BAD,
				"**BAD RRVERSION (%d) for %c%c\n",
				pnt[3], pnt[0], pnt[1]);
#else
			fprintf(stderr,
				"**BAD RRVERSION (%d) for %c%c\n",
				pnt[3], pnt[0], pnt[1]);
#endif
			return (0);	/* JS ??? Is this right ??? */
		}
		ncount++;
		if (pnt[0] == 'R' && pnt[1] == 'R')
			flag1 = pnt[4] & 0xff;

		if (strncmp((char *) pnt, "PX", 2) == 0)	/* POSIX attributes */
			flag2 |= 1;
		if (strncmp((char *) pnt, "PN", 2) == 0)	/* POSIX device number */
			flag2 |= 2;
		if (strncmp((char *) pnt, "SL", 2) == 0)	/* Symlink */
			flag2 |= 4;
		if (strncmp((char *) pnt, "NM", 2) == 0)	/* Alternate Name */
			flag2 |= 8;
		if (strncmp((char *) pnt, "CL", 2) == 0)	/* Child link */
			flag2 |= 16;
		if (strncmp((char *) pnt, "PL", 2) == 0)	/* Parent link */
			flag2 |= 32;
		if (strncmp((char *) pnt, "RE", 2) == 0)	/* Relocated Direcotry */
			flag2 |= 64;
		if (strncmp((char *) pnt, "TF", 2) == 0)	/* Time stamp */
			flag2 |= 128;
		if (strncmp((char *) pnt, "SP", 2) == 0) {	/* SUSP record */
			flag2 |= 1024;
/*			su_version = pnt[3] & 0xff;*/
		}
		if (strncmp((char *) pnt, "AA", 2) == 0) {	/* Apple Signature record */
			flag2 |= 2048;
/*			aa_version = pnt[3] & 0xff;*/
		}

		if (strncmp((char *)pnt, "CE", 2) == 0) {	/* Continuation Area */
			cont_extent = isonum_733(pnt+4);
			cont_offset = isonum_733(pnt+12);
			cont_size = isonum_733(pnt+20);
		}

		len -= pnt[2];
		pnt += pnt[2];
		if (len <= 3 && cont_extent) {
			unsigned char   sector[SECTOR_SIZE];

			readsecs(cont_extent, sector, 1);
			flag2 |= parse_rrflags(&sector[cont_offset], cont_size, 1);
		}
	}
	return (flag2);
}

int
rr_flags(struct iso_directory_record *idr)
{
	int		len;
	unsigned char	*pnt;
	int		ret = 0;

	if (find_rr(idr, &pnt, &len))
		ret |= 4096;
	ret |= parse_rrflags(pnt, len, 0);
	return (ret);
}

/*
 * Parse the RR attributes so we can find the file name.
 */
static int
parse_rr(unsigned char *pnt, int len, struct directory_entry *dpnt)
{
	int		cont_extent;
	int		cont_offset;
	int		cont_size;
	char		name_buf[256];

	cont_extent = cont_offset = cont_size = 0;

	pnt = parse_xa(pnt, &len, dpnt /* 0 */);

	while (len >= 4) {
		if (pnt[3] != 1 && pnt[3] != 2) {
#ifdef	USE_LIBSCHILY
			errmsgno(EX_BAD,
				"**BAD RRVERSION (%d) for %c%c\n",
				pnt[3], pnt[0], pnt[1]);
#else
			fprintf(stderr,
				"**BAD RRVERSION (%d) for %c%c\n",
				pnt[3], pnt[0], pnt[1]);
#endif
			return (-1);
		}
		if (strncmp((char *) pnt, "NM", 2) == 0) {
			strncpy(name_buf, (char *) pnt + 5, pnt[2] - 5);
			name_buf[pnt[2] - 5] = 0;
			if (dpnt->name) {
				size_t nlen = strlen(dpnt->name);

				/*
				 * append to name from previous NM records
				 */
				dpnt->name = realloc(dpnt->name, nlen +
							strlen(name_buf) + 1);
				strcpy(dpnt->name + nlen, name_buf);
			} else {
				dpnt->name = strdup(name_buf);
				dpnt->got_rr_name = 1;
			}
			/* continue searching for more NM records */
		} else if (strncmp((char *) pnt, "CE", 2) == 0) {
			cont_extent = isonum_733(pnt + 4);
			cont_offset = isonum_733(pnt + 12);
			cont_size = isonum_733(pnt + 20);
		}

		len -= pnt[2];
		pnt += pnt[2];
		if (len <= 3 && cont_extent) {
			unsigned char   sector[SECTOR_SIZE];

			readsecs(cont_extent, sector, 1);
			if (parse_rr(&sector[cont_offset],
							cont_size, dpnt) == -1)
				return (-1);
		}
	}

	/* Fall back to the iso name if no RR name found */
	if (dpnt->name == NULL) {
		char	*cp;

		strcpy(name_buf, dpnt->isorec.name);
		cp = strchr(name_buf, ';');
		if (cp != NULL) {
			*cp = '\0';
		}
		dpnt->name = strdup(name_buf);
	}
	return (0);
}/* parse_rr */


/*
 * Returns 1 if the two files are identical
 * Returns 0 if the two files differ
 */
static int
check_rr_dates(struct directory_entry *dpnt, 
					struct directory_entry *current, 
					struct stat *statbuf, 
					struct stat *lstatbuf)
{
	int		cont_extent;
	int		cont_offset;
	int		cont_size;
	int		offset;
	unsigned char	*pnt;
	int		len;
	int		same_file;
	int		same_file_type;
	mode_t		mode;
	char		time_buf[7];


	cont_extent = cont_offset = cont_size = 0;
	same_file = 1;
	same_file_type = 1;

	pnt = dpnt->rr_attributes;
	len = dpnt->rr_attr_size;
	/*
	 * We basically need to parse the rr attributes again, and dig out the
	 * dates and file types.
	 */
	pnt = parse_xa(pnt, &len, /* dpnt */ 0);
	while (len >= 4) {
		if (pnt[3] != 1 && pnt[3] != 2) {
#ifdef	USE_LIBSCHILY
			errmsgno(EX_BAD,
				"**BAD RRVERSION (%d) for %c%c\n",
				pnt[3], pnt[0], pnt[1]);
#else
			fprintf(stderr,
				"**BAD RRVERSION (%d) for %c%c\n",
				pnt[3], pnt[0], pnt[1]);
#endif
			return (-1);
		}

		/*
		 * If we have POSIX file modes, make sure that the file type is
		 * the same.  If it isn't, then we must always write the new
		 * file.
		 */
		if (strncmp((char *) pnt, "PX", 2) == 0) {
			mode = isonum_733(pnt + 4);
			if ((lstatbuf->st_mode & S_IFMT) != (mode & S_IFMT)) {
				same_file_type = 0;
				same_file = 0;
			}
		}
		if (strncmp((char *) pnt, "TF", 2) == 0) {
			offset = 5;
			if (pnt[4] & TF_CREATE) {
				iso9660_date((char *) time_buf,
							lstatbuf->st_ctime);
				if (memcmp(time_buf, pnt + offset, 7) != 0)
					same_file = 0;
				offset += 7;
			}
			if (pnt[4] & TF_MODIFY) {
				iso9660_date((char *) time_buf,
							lstatbuf->st_mtime);
				if (memcmp(time_buf, pnt + offset, 7) != 0)
					same_file = 0;
				offset += 7;
			}
		}
		if (strncmp((char *) pnt, "CE", 2) == 0) {
			cont_extent = isonum_733(pnt + 4);
			cont_offset = isonum_733(pnt + 12);
			cont_size = isonum_733(pnt + 20);
		}

		len -= pnt[2];
		pnt += pnt[2];
		if (len <= 3 && cont_extent) {
			unsigned char   sector[SECTOR_SIZE];

			readsecs(cont_extent, sector, 1);
			/*
			 * Continue to scan the extension record.
			 * Note that this has not been tested yet, but it is
			 * definitely more correct that calling parse_rr()
			 * as done in Eric's old code.
			 */
			pnt = &sector[cont_offset];
			len = cont_size;
			/*
			 * Clear the "pending extension record" state as
			 * we did already read it now.
			 */
			cont_extent = cont_offset = cont_size = 0;
		}
	}

	/*
	 * If we have the same fundamental file type, then it is clearly safe
	 * to reuse the TRANS.TBL entry.
	 */
	if (same_file_type) {
		current->de_flags |= SAFE_TO_REUSE_TABLE_ENTRY;
	}
	return (same_file);
}

static struct directory_entry **
read_merging_directory(struct iso_directory_record *mrootp, int *nentp)
{
	unsigned char	*cpnt;
	unsigned char	*cpnt1;
	char		*p;
	char		*dirbuff;
	int		i;
	struct iso_directory_record *idr;
	int		len;
	int		nbytes;
	int		nent;
	struct directory_entry **pnt;
	int		rlen;
	struct directory_entry **rtn;
	int		seen_rockridge;
	unsigned char	*tt_buf;
	int		tt_extent;
	int		tt_size;

	static int	warning_given = 0;

	/*
	 * This is the number of sectors we will need to read.  We need to
	 * round up to get the last fractional sector - we are asking for the
	 * data in terms of a number of sectors.
	 */
	nbytes = roundup(isonum_733((unsigned char *) mrootp->size),
								SECTOR_SIZE);

	/*
	 * First, allocate a buffer large enough to read in the entire
	 * directory.
	 */
	dirbuff = (char *) e_malloc(nbytes);

	readsecs(isonum_733((unsigned char *) mrootp->extent), dirbuff,
		nbytes / SECTOR_SIZE);

	/*
	 * Next look over the directory, and count up how many entries we have.
	 */
	len = isonum_733((unsigned char *) mrootp->size);
	i = 0;
	*nentp = 0;
	nent = 0;
	while (i < len) {
		idr = (struct iso_directory_record *) & dirbuff[i];
		if (idr->length[0] == 0) {
			i = ISO_ROUND_UP(i);
			continue;
		}
		nent++;
		i += idr->length[0];
	}

	/*
	 * Now allocate the buffer which will hold the array we are about to
	 * return.
	 */
	rtn = (struct directory_entry **) e_malloc(nent * sizeof (*rtn));

	/*
	 * Finally, scan the directory one last time, and pick out the relevant
	 * bits of information, and store it in the relevant bits of the
	 * structure.
	 */
	i = 0;
	pnt = rtn;
	tt_extent = 0;
	seen_rockridge = 0;
	tt_size = 0;
	while (i < len) {
		idr = (struct iso_directory_record *) & dirbuff[i];
		if (idr->length[0] == 0) {
			i = ISO_ROUND_UP(i);
			continue;
		}
		*pnt = (struct directory_entry *) e_malloc(sizeof (**rtn));
		(*pnt)->next = NULL;
#ifdef	DEBUG
		fprintf(stderr, "IDR name: '%s' ist: %d soll: %d\n",
			idr->name, strlen(idr->name), idr->name_len[0]);
#endif
		(*pnt)->isorec = *idr;
		(*pnt)->starting_block =
				isonum_733((unsigned char *) idr->extent);
		(*pnt)->size = isonum_733((unsigned char *) idr->size);
		(*pnt)->priority = 0;
		(*pnt)->name = NULL;
		(*pnt)->got_rr_name = 0;
		(*pnt)->table = NULL;
		(*pnt)->whole_name = NULL;
		(*pnt)->filedir = NULL;
		(*pnt)->parent_rec = NULL;
		/*
		 * Set this information so that we correctly cache previous
		 * session bits of information.
		 */
		(*pnt)->inode = (*pnt)->starting_block;
		(*pnt)->dev = PREV_SESS_DEV;
		(*pnt)->rr_attributes = NULL;
		(*pnt)->rr_attr_size = 0;
		(*pnt)->total_rr_attr_size = 0;
		(*pnt)->de_flags = SAFE_TO_REUSE_TABLE_ENTRY;
#ifdef APPLE_HYB
		(*pnt)->assoc = NULL;
		(*pnt)->hfs_ent = NULL;
#endif	/* APPLE_HYB */

		/*
		 * Check for and parse any RR attributes for the file. All we
		 * are really looking for here is the original name of the
		 * file.
		 */
		rlen = idr->length[0] & 0xff;
		cpnt = (unsigned char *) idr;

		rlen -= offsetof(struct iso_directory_record, name[0]);
		cpnt += offsetof(struct iso_directory_record, name[0]);

		rlen -= idr->name_len[0];
		cpnt += idr->name_len[0];

		if ((idr->name_len[0] & 1) == 0) {
			cpnt++;
			rlen--;
		}

		if (no_rr)
			rlen = 0;
		if (rlen > 0) {
			(*pnt)->total_rr_attr_size =
						(*pnt)->rr_attr_size = rlen;
			(*pnt)->rr_attributes = e_malloc(rlen);
			memcpy((*pnt)->rr_attributes, cpnt, rlen);
			seen_rockridge = 1;
		}
#ifdef	DEBUG
		fprintf(stderr, "INT name: '%s' ist: %d soll: %d\n",
			(*pnt)->isorec.name, strlen((*pnt)->isorec.name),
			idr->name_len[0]);
#endif

		if (idr->name_len[0] < sizeof ((*pnt)->isorec.name)) {
			/*
			 * Now zero out the remainder of the name field.
			 */
			cpnt = (unsigned char *) (*pnt)->isorec.name;
			cpnt += idr->name_len[0];
			memset(cpnt, 0,
				sizeof ((*pnt)->isorec.name) - idr->name_len[0]);
		} else {
			/*
			 * Simple sanity work to make sure that we have no
			 * illegal data structures in our tree.
			 */
			(*pnt)->isorec.name[MAX_ISONAME] = '\0';
			(*pnt)->isorec.name_len[0] = MAX_ISONAME;
		}
		/*
		 * If the filename len from the old session is more
		 * then 31 chars, there is a high risk of hard violations
		 * if the ISO9660 standard.
		 * Run it through our name canonication machine....
		 */
		if (idr->name_len[0] > LEN_ISONAME || check_oldnames) {
			iso9660_check(idr, *pnt);
		}

		if (parse_rr((*pnt)->rr_attributes, rlen, *pnt) == -1) {
#ifdef	USE_LIBSCHILY
			comerrno(EX_BAD,
			    "Cannot parse Rock Ridge attributes for '%s'.\n",
								idr->name);
#else
			fprintf(stderr,
			    "Cannot parse Rock Ridge attributes for '%s'.\n",
								idr->name);
			exit(1);
#endif
		}
		if (((*pnt)->isorec.name_len[0] == 1) &&
		    (((*pnt)->isorec.name[0] == 0) ||	/* "."  entry */
		    ((*pnt)->isorec.name[0] == 1))) {	/* ".." entry */

			if ((*pnt)->name != NULL) {
				free((*pnt)->name);
			}
			if ((*pnt)->whole_name != NULL) {
				free((*pnt)->whole_name);
			}
			if ((*pnt)->isorec.name[0] == 0) {
				(*pnt)->name = strdup(".");
			} else {
				(*pnt)->name = strdup("..");
			}
		}
#ifdef DEBUG
		fprintf(stderr, "got DE name: %s\n", (*pnt)->name);
#endif

		if (strncmp(idr->name, trans_tbl, strlen(trans_tbl)) == 0) {
			if ((*pnt)->name != NULL) {
				free((*pnt)->name);
			}
			if ((*pnt)->whole_name != NULL) {
				free((*pnt)->whole_name);
			}
/*			(*pnt)->name = strdup("<translation table>");*/
			(*pnt)->name = strdup(trans_tbl);
			tt_extent = isonum_733((unsigned char *) idr->extent);
			tt_size = isonum_733((unsigned char *) idr->size);
			if (tt_extent == 0)
				tt_size = 0;
		}
		pnt++;
		i += idr->length[0];
	}
#ifdef APPLE_HYB
	/*
	 * If we find an associated file, check if there is a file
	 * with same ISO name and link it to this entry
	 */
	for (pnt = rtn, i = 0; i < nent; i++, pnt++) {
		int	j;

		rlen = isonum_711((*pnt)->isorec.name_len);
		if ((*pnt)->isorec.flags[0] & ISO_ASSOCIATED) {
			for (j = 0; j < nent; j++) {
				if (strncmp(rtn[j]->isorec.name,
				    (*pnt)->isorec.name, rlen) == 0 &&
				    (rtn[j]->isorec.flags[0] & ISO_ASSOCIATED) == 0) {
					rtn[j]->assoc = *pnt;

					/*
					 * don't want this entry to be
					 * in the Joliet tree
					 */
					(*pnt)->de_flags |= INHIBIT_JOLIET_ENTRY;

					/*
					 * as we have associated files, then
					 * assume we are are dealing with
					 * Apple's extensions - if not already
					 * set
					 */
					if (apple_both == 0) {
						apple_both = apple_ext = 1;
					}
					break;
				}
			}
		}
	}
#endif	/* APPLE_HYB */

	/*
	 * If there was a TRANS.TBL;1 entry, then grab it, read it, and use it
	 * to get the filenames of the files.  Also, save the table info, just
	 * in case we need to use it.
	 *
	 * The entries look something like: F ISODUMP.;1 isodump
	 */
	if (tt_extent != 0 && tt_size != 0) {
		nbytes = roundup(tt_size, SECTOR_SIZE);
		tt_buf = (unsigned char *) e_malloc(nbytes);
		readsecs(tt_extent, tt_buf, nbytes / SECTOR_SIZE);

		/*
		 * Loop through the file, examine each entry, and attempt to
		 * attach it to the correct entry.
		 */
		cpnt = tt_buf;
		cpnt1 = tt_buf;
		while (cpnt - tt_buf < tt_size) {
			/* Skip to a line terminator, or end of the file. */
			while ((cpnt1 - tt_buf < tt_size) &&
				(*cpnt1 != '\n') &&
				(*cpnt1 != '\0')) {
				cpnt1++;
			}
			/* Zero terminate this particular line. */
			if (cpnt1 - tt_buf < tt_size) {
				*cpnt1 = '\0';
			}
			/*
			 * Now dig through the actual directories, and try and
			 * find the attachment for this particular filename.
			 */
			for (pnt = rtn, i = 0; i < nent; i++, pnt++) {
				rlen = isonum_711((*pnt)->isorec.name_len);

				/*
				 * If this filename is so long that it would
				 * extend past the end of the file, it cannot
				 * be the one we want.
				 */
				if (cpnt + 2 + rlen - tt_buf >= tt_size) {
					continue;
				}
				/*
				 * Now actually compare the name, and make sure
				 * that the character at the end is a ' '.
				 */
				if (strncmp((char *) cpnt + 2,
					(*pnt)->isorec.name, rlen) == 0 &&
					cpnt[2 + rlen] == ' ' &&
					(p = strchr((char *)&cpnt[2 + rlen], '\t'))) {
					p++;
					/*
					 * This is a keeper. Now determine the
					 * correct table entry that we will
					 * use on the new image.
					 */
					if (strlen(p) > 0) {
						(*pnt)->table =
						    e_malloc(strlen(p) + 4);
						sprintf((*pnt)->table,
							"%c\t%s\n",
							*cpnt, p);
					}
					if (!(*pnt)->got_rr_name) {
						if ((*pnt)->name != NULL) {
							free((*pnt)->name);
						}
						(*pnt)->name = strdup(p);
					}
					break;
				}
			}
			cpnt = cpnt1 + 1;
			cpnt1 = cpnt;
		}

		free(tt_buf);
	} else if (!seen_rockridge && !warning_given) {
		/*
		 * Warn the user that iso-9660 names were used because neither
		 * Rock Ridge (-R) nor TRANS.TBL (-T) name translations were
		 * found.
		 */
		fprintf(stderr,
		    "Warning: Neither Rock Ridge (-R) nor TRANS.TBL (-T) \n");
		fprintf(stderr,
		    "name translations were found on previous session.\n");
		fprintf(stderr,
		    "ISO-9660 file names have been used instead.\n");
		warning_given = 1;
	}
	if (dirbuff != NULL) {
		free(dirbuff);
	}
	*nentp = nent;
	return (rtn);
}/* read_merging_directory */

/*
 * Free any associated data related to the structures.
 */
static int
free_mdinfo(struct directory_entry **ptr, int len)
{
	int		i;
	struct directory_entry **p;

	p = ptr;
	for (i = 0; i < len; i++, p++) {
		/*
		 * If the tree-handling code decided that it needed an entry, it
		 * will have removed it from the list.  Thus we must allow for
		 * null pointers here.
		 */
		if (*p == NULL) {
			continue;
		}
		free_directory_entry(*p);
	}

	free(ptr);
	return (0);
}

static void
free_directory_entry(struct directory_entry *dirp)
{
	if (dirp->name != NULL)
		free(dirp->name);

	if (dirp->whole_name != NULL)
		free(dirp->whole_name);

	if (dirp->rr_attributes != NULL)
		free(dirp->rr_attributes);

	if (dirp->table != NULL)
		free(dirp->table);

	free(dirp);
}

/*
 * Search the list to see if we have any entries from the previous
 * session that match this entry.  If so, copy the extent number
 * over so we don't bother to write it out to the new session.
 */
int
check_prev_session(struct directory_entry **ptr, int len, 
						 struct directory_entry *curr_entry, 
						 struct stat *statbuf, 
						 struct stat *lstatbuf, 
						 struct directory_entry **odpnt)
{
	int		i;
	int		rr;
	int		retcode = 0;	/* Default not found */

	for (i = 0; i < len; i++) {
		if (ptr[i] == NULL) {	/* Used or empty entry skip */
			continue;
		}
#if 0
		if (ptr[i]->name != NULL && ptr[i]->isorec.name_len[0] == 1 &&
		    ptr[i]->name[0] == '\0') {
			continue;
		}
		if (ptr[i]->name != NULL && ptr[i]->isorec.name_len[0] == 1 &&
		    ptr[i]->name[0] == 1) {
			continue;
		}
#else
		if (ptr[i]->name != NULL && strcmp(ptr[i]->name, ".") == 0) {
			continue;
		}
		if (ptr[i]->name != NULL && strcmp(ptr[i]->name, "..") == 0) {
			continue;
		}
#endif

		if (ptr[i]->name != NULL &&
		    strcmp(ptr[i]->name, curr_entry->name) != 0) {
			/* Not the same name continue */
			continue;
		}
		/*
		 * It's a directory so we must always merge it with the new
		 * session. Never ever reuse directory extents.  See comments
		 * in tree.c for an explaination of why this must be the case.
		 */
		if ((curr_entry->isorec.flags[0] & ISO_DIRECTORY) != 0) {
			retcode = 2;	/* Flag directory case */
			goto found_it;
		}
		/*
		 * We know that the files have the same name.  If they also
		 * have the same file type (i.e. file, dir, block, etc), then
		 * we can safely reuse the TRANS.TBL entry for this file. The
		 * check_rr_dates() function will do this for us.
		 *
		 * Verify that the file type and dates are consistent. If not,
		 * we probably have a different file, and we need to write it
		 * out again.
		 */
		retcode = 1;	/* We found a non directory */

		if (ptr[i]->rr_attributes != NULL) {
			if ((rr = check_rr_dates(ptr[i], curr_entry, statbuf,
							lstatbuf)) == -1)
				return (-1);

			if (rr == 0) {	/* Different files */
				goto found_it;
			}
		}
		/*
		 * Verify size and timestamp.  If rock ridge is in use, we
		 * need to compare dates from RR too.  Directories are special,
		 * we calculate their size later.
		 */
		if (ptr[i]->size != curr_entry->size) {
			/* Different files */
			goto found_it;
		}
		if (memcmp(ptr[i]->isorec.date,
					curr_entry->isorec.date, 7) != 0) {
			/* Different files */
			goto found_it;
		}
		/* We found it and we can reuse the extent */
		memcpy(curr_entry->isorec.extent, ptr[i]->isorec.extent, 8);
		curr_entry->starting_block = isonum_733((unsigned char *)ptr[i]->isorec.extent);
		curr_entry->de_flags |= SAFE_TO_REUSE_TABLE_ENTRY;
		goto found_it;
	}
	return (retcode);

found_it:
	if (odpnt != NULL) {
		*odpnt = ptr[i];
	} else {
		free(ptr[i]);
	}
	ptr[i] = NULL;
	return (retcode);
}

/*
 * open_merge_image:  Open an existing image.
 */
int
open_merge_image(char *path)
{
#ifndef	USE_SCG
	in_image = fopen(path, "rb");
	if (in_image == NULL) {
		return (-1);
	}
#else
	in_image = fopen(path, "rb");
	if (in_image == NULL) {
		if (scsidev_open(path) < 0)
			return (-1);
	}
#endif
	return (0);
}

/*
 * close_merge_image:  Close an existing image.
 */
int
close_merge_image()
{
#ifdef	USE_SCG
	return (scsidev_close());
#else
	return (fclose(in_image));
#endif
}

/*
 * merge_isofs:  Scan an existing image, and return a pointer
 * to the root directory for this image.
 */
struct iso_directory_record *
merge_isofs(char *path)
{
	char		buffer[SECTOR_SIZE];
	int		file_addr;
	int		i;
	struct iso_primary_descriptor *pri = NULL;
	struct iso_directory_record *rootp;
	struct iso_volume_descriptor *vdp;

	/*
	 * Start by searching for the volume header. Ultimately, we need to
	 * search for volume headers in multiple places because we might be
	 * starting with a multisession image. FIXME(eric).
	 */
	get_session_start(&file_addr);

	for (i = 0; i < 100; i++) {
		if (readsecs(file_addr, buffer,
				sizeof (buffer) / SECTOR_SIZE) != sizeof (buffer)) {
#ifdef	USE_LIBSCHILY
			comerr(" Read error on old image %s\n", path);
#else
			fprintf(stderr, " Read error on old image %s\n", path);
			exit(10);
#endif
		}
		vdp = (struct iso_volume_descriptor *) buffer;

		if ((strncmp(vdp->id, ISO_STANDARD_ID, sizeof (vdp->id)) == 0) &&
		    (isonum_711((unsigned char *) vdp->type) == ISO_VD_PRIMARY)) {
			break;
		}
		file_addr += 1;
	}

	if (i == 100) {
		return (NULL);
	}
	pri = (struct iso_primary_descriptor *) vdp;

	/* Check the blocksize of the image to make sure it is compatible. */
	if (isonum_723((unsigned char *) pri->logical_block_size) != SECTOR_SIZE) {
		errmsgno(EX_BAD,
			"Previous session has incompatible sector size %d.\n",
			isonum_723((unsigned char *) pri->logical_block_size));
		return (NULL);
	}
	if (isonum_723((unsigned char *) pri->volume_set_size) != 1) {
		errmsgno(EX_BAD,
			"Previous session has volume set size %d (must be 1).\n",
			isonum_723((unsigned char *) pri->volume_set_size));
		return (NULL);
	}
	/* Get the location and size of the root directory. */
	rootp = (struct iso_directory_record *)
		e_malloc(sizeof (struct iso_directory_record));

	memcpy(rootp, pri->root_directory_record, sizeof (*rootp));

	return (rootp);
}

static void
merge_remaining_entries(struct directory *this_dir, 
								struct directory_entry **pnt, int n_orig)
{
	int		i;
	struct directory_entry *s_entry;
	unsigned int	ttbl_extent = 0;
	unsigned int	ttbl_index = 0;
	char		whole_path[PATH_MAX];

	/*
	 * Whatever is leftover in the list needs to get merged back into the
	 * directory.
	 */
	for (i = 0; i < n_orig; i++) {
		if (pnt[i] == NULL) {
			continue;
		}
		if (pnt[i]->name != NULL && pnt[i]->whole_name == NULL) {
			/* Set the name for this directory. */
			strcpy(whole_path, this_dir->de_name);
			strcat(whole_path, SPATH_SEPARATOR);
			strcat(whole_path, pnt[i]->name);

			pnt[i]->whole_name = strdup(whole_path);
		}
		if (pnt[i]->name != NULL &&
/*			strcmp(pnt[i]->name, "<translation table>") == 0 )*/
			strcmp(pnt[i]->name, trans_tbl) == 0) {
			ttbl_extent =
			    isonum_733((unsigned char *)pnt[i]->isorec.extent);
			ttbl_index = i;
			continue;
		}

		/*
		 * Skip directories for now - these need to be treated
		 * differently.
		 */
		if ((pnt[i]->isorec.flags[0] & ISO_DIRECTORY) != 0) {
			/*
			 * FIXME - we need to insert this directory into the
			 * tree, so that the path tables we generate will be
			 * correct.
			 */
			if ((strcmp(pnt[i]->name, ".") == 0) ||
				(strcmp(pnt[i]->name, "..") == 0)) {
				free_directory_entry(pnt[i]);
				pnt[i] = NULL;
				continue;
			} else {
				merge_old_directory_into_tree(pnt[i], this_dir);
			}
		}
		pnt[i]->next = this_dir->contents;
		pnt[i]->filedir = this_dir;
		this_dir->contents = pnt[i];
		pnt[i] = NULL;
	}


	/*
	 * If we don't have an entry for the translation table, then don't
	 * bother trying to copy the starting extent over. Note that it is
	 * possible that if we are copying the entire directory, the entry for
	 * the translation table will have already been inserted into the
	 * linked list and removed from the old entries list, in which case we
	 * want to leave the extent number as it was before.
	 */
	if (ttbl_extent == 0) {
		return;
	}
	/*
	 * Finally, check the directory we are creating to see whether there
	 * are any new entries in it.  If there are not, we can reuse the same
	 * translation table.
	 */
	for (s_entry = this_dir->contents; s_entry; s_entry = s_entry->next) {
		/*
		 * Don't care about '.' or '..'.  They are never in the table
		 * anyways.
		 */
		if (s_entry->name != NULL && strcmp(s_entry->name, ".") == 0) {
			continue;
		}
		if (s_entry->name != NULL && strcmp(s_entry->name, "..") == 0) {
			continue;
		}
/*		if (strcmp(s_entry->name, "<translation table>") == 0)*/
		if (strcmp(s_entry->name, trans_tbl) == 0) {
			continue;
		}
		if ((s_entry->de_flags & SAFE_TO_REUSE_TABLE_ENTRY) == 0) {
			return;
		}
	}

	/*
	 * Locate the translation table, and re-use the same extent. It isn't
	 * clear that there should ever be one in there already so for now we
	 * try and muddle through the best we can.
	 */
	for (s_entry = this_dir->contents; s_entry; s_entry = s_entry->next) {
/*		if (strcmp(s_entry->name, "<translation table>") == 0)*/
		if (strcmp(s_entry->name, trans_tbl) == 0) {
			fprintf(stderr, "Should never get here\n");
			set_733(s_entry->isorec.extent, ttbl_extent);
			return;
		}
	}

	pnt[ttbl_index]->next = this_dir->contents;
	pnt[ttbl_index]->filedir = this_dir;
	this_dir->contents = pnt[ttbl_index];
	pnt[ttbl_index] = NULL;
}


/*
 * Here we have a case of a directory that has completely disappeared from
 * the face of the earth on the tree we are mastering from.  Go through and
 * merge it into the tree, as well as everything beneath it.
 *
 * Note that if a directory has been moved for some reason, this will
 * incorrectly pick it up and attempt to merge it back into the old
 * location.  FIXME(eric).
 */
static int
merge_old_directory_into_tree(struct directory_entry *dpnt, 
										struct directory *parent)
{
	struct directory_entry **contents = NULL;
	int		i;
	int		n_orig;
	struct directory *this_dir,
			*next_brother;
	char		whole_path[PATH_MAX];

	this_dir = (struct directory *) e_malloc(sizeof (struct directory));
	memset(this_dir, 0, sizeof (struct directory));
	this_dir->next = NULL;
	this_dir->subdir = NULL;
	this_dir->self = dpnt;
	this_dir->contents = NULL;
	this_dir->size = 0;
	this_dir->extent = 0;
	this_dir->depth = parent->depth + 1;
	this_dir->parent = parent;
	if (!parent->subdir)
		parent->subdir = this_dir;
	else {
		next_brother = parent->subdir;
		while (next_brother->next)
			next_brother = next_brother->next;
		next_brother->next = this_dir;
	}

	/* Set the name for this directory. */
	strcpy(whole_path, parent->de_name);
	strcat(whole_path, SPATH_SEPARATOR);
	strcat(whole_path, dpnt->name);
	this_dir->de_name = strdup(whole_path);
	this_dir->whole_name = strdup(whole_path);

	/*
	 * Now fill this directory using information from the previous session.
	 */
	contents = read_merging_directory(&dpnt->isorec, &n_orig);
	/*
	 * Start by simply copying the '.', '..' and non-directory entries to
	 * this directory.  Technically we could let merge_remaining_entries
	 * handle this, but it gets rather confused by the '.' and '..' entries
	 */
	for (i = 0; i < n_orig; i++) {
		/*
		 * We can always reuse the TRANS.TBL in this particular case.
		 */
		contents[i]->de_flags |= SAFE_TO_REUSE_TABLE_ENTRY;

		if (((contents[i]->isorec.flags[0] & ISO_DIRECTORY) != 0) &&
							(i >= 2)) {
			continue;
		}
		/* If we have a directory, don't reuse the extent number. */
		if ((contents[i]->isorec.flags[0] & ISO_DIRECTORY) != 0) {
			memset(contents[i]->isorec.extent, 0, 8);

			if (strcmp(contents[i]->name, ".") == 0)
				this_dir->dir_flags |= DIR_HAS_DOT;

			if (strcmp(contents[i]->name, "..") == 0)
				this_dir->dir_flags |= DIR_HAS_DOTDOT;
		}
		/*
		 * for regilar files, we do it here.
		 * If it has CL or RE attributes, remember its extent
		 */
		check_rr_relocation(contents[i]);

		/*
		 * Set the whole name for this file.
		 */
		strcpy(whole_path, this_dir->whole_name);
		strcat(whole_path, SPATH_SEPARATOR);
		strcat(whole_path, contents[i]->name);

		contents[i]->whole_name = strdup(whole_path);

		contents[i]->next = this_dir->contents;
		contents[i]->filedir = this_dir;
		this_dir->contents = contents[i];
		contents[i] = NULL;
	}

	/*
	 * and for directories, we do it here.
	 * If it has CL or RE attributes, remember its extent
	 */
	check_rr_relocation(dpnt);

	/*
	 * Zero the extent number for ourselves.
	 */
	memset(dpnt->isorec.extent, 0, 8);

	/*
	 * Anything that is left are other subdirectories that need to be
	 * merged.
	 */
	merge_remaining_entries(this_dir, contents, n_orig);
	free_mdinfo(contents, n_orig);
#if 0
	/*
	 * This is no longer required.  The post-scan sort will handle all of
	 * this for us.
	 */
	sort_n_finish(this_dir);
#endif

	return (0);
}


char	*cdrecord_data = NULL;

int
get_session_start(int *file_addr)
{
	char		*pnt;

#ifdef CDRECORD_DETERMINES_FIRST_WRITABLE_ADDRESS
	/*
	 * FIXME(eric).  We need to coordinate with cdrecord to obtain the
	 * parameters.  For now, we assume we are writing the 2nd session, so
	 * we start from the session that starts at 0.
	 */
	if (file_addr != NULL)
		*file_addr = 16;

	/*
	 * We need to coordinate with cdrecord to get the next writable address
	 * from the device.  Here is where we use it.
	 */
	session_start = last_extent = last_extent_written = cdrecord_result();
#else

	if (file_addr != NULL)
		*file_addr = 0L;
	session_start = last_extent = last_extent_written = 0L;
	if (check_session && cdrecord_data == NULL)
		return (0);

	if (cdrecord_data == NULL) {
#ifdef	USE_LIBSCHILY
		comerrno(EX_BAD,
		    "Special parameters for cdrecord not specified with -C\n");
#else
		fprintf(stderr,
		    "Special parameters for cdrecord not specified with -C\n");
		exit(1);
#endif
	}
	/*
	 * Next try and find the ',' in there which delimits the two numbers.
	 */
	pnt = strchr(cdrecord_data, ',');
	if (pnt == NULL) {
#ifdef	USE_LIBSCHILY
		comerrno(EX_BAD, "Malformed cdrecord parameters\n");
#else
		fprintf(stderr, "Malformed cdrecord parameters\n");
		exit(1);
#endif
	}

	*pnt = '\0';
	if (file_addr != NULL) {
		*file_addr = atol(cdrecord_data);
	}
	pnt++;

	session_start = last_extent = last_extent_written = atol(pnt);

	pnt--;
	*pnt = ',';

#endif
	return (0);
}

/*
 * This function scans the directory tree, looking for files, and it makes
 * note of everything that is found.  We also begin to construct the ISO9660
 * directory entries, so that we can determine how large each directory is.
 */
int
merge_previous_session(struct directory *this_dir, 
							  struct iso_directory_record *mrootp, 
							  char *reloc_root, 
							  char *reloc_old_root)
{
	struct directory_entry **orig_contents = NULL;
	struct directory_entry *odpnt = NULL;
	int		n_orig;
	struct directory_entry *s_entry;
	int		status;
	int		lstatus;
	struct stat	statbuf,
			lstatbuf;
	int		retcode;

	/* skip leading slash */
	while (reloc_old_root && reloc_old_root[0] == PATH_SEPARATOR) {
		reloc_old_root++;
	}
	while (reloc_root && reloc_root[0] == PATH_SEPARATOR) {
		reloc_root++;
	}

	/*
	 * Parse the same directory in the image that we are merging for
	 * multisession stuff.
	 */
	orig_contents = read_merging_directory(mrootp, &n_orig);
	if (orig_contents == NULL) {
		if (reloc_old_root) {
#ifdef	USE_LIBSCHILY
			comerrno(EX_BAD,
			"Reading old session failed, cannot execute -old-root.\n");
#else
			fprintf(stderr,
			"Reading old session failed, cannot execute -old-root.\n");
			exit(1);
#endif
		}
		return (0);
	}

	if (reloc_old_root && reloc_old_root[0]) {
		struct directory_entry	**new_orig_contents = orig_contents;
		int			new_n_orig = n_orig;

		/* decend until we reach the original root */
		while (reloc_old_root[0]) {
			int	i;
			char	*next;
			int	last;

			for (next = reloc_old_root; *next && *next != PATH_SEPARATOR; next++);
			if (*next) {
				last = 0;
				*next = 0;
				next++;
			} else {
				last = 1;
			}
			while (*next == PATH_SEPARATOR) {
				next++;
			}

			for (i = 0; i < new_n_orig; i++) {
				struct iso_directory_record subroot;

				if (new_orig_contents[i]->name != NULL &&
				    strcmp(new_orig_contents[i]->name, reloc_old_root) != 0) {
					/* Not the same name continue */
					continue;
				}
				/*
				 * enter directory, free old one only if not the top level,
				 * which is still needed
				 */
				subroot = new_orig_contents[i]->isorec;
				if (new_orig_contents != orig_contents) {
					free_mdinfo(new_orig_contents, new_n_orig);
				}
				new_orig_contents = read_merging_directory(&subroot, &new_n_orig);

				if (!new_orig_contents) {
#ifdef	USE_LIBSCHILY
					comerrno(EX_BAD,
					"Reading directory %s in old session failed, cannot execute -old-root.\n",
							reloc_old_root);
#else
					fprintf(stderr,
					"Reading directory %s in old session failed, cannot execute -old-root.\n",
							reloc_old_root);
					exit(1);
#endif
				}
				i = -1;
				break;
			}

			if (i == new_n_orig) {
#ifdef	USE_LIBSCHILY
				comerrno(EX_BAD,
				"-old-root (sub)directory %s not found in old session.\n",
						reloc_old_root);
#else
				fprintf(stderr,
				"-old-root (sub)directory %s not found in old session.\n",
						reloc_old_root);
				exit(1);
#endif
			}

			/* restore string, proceed to next sub directory */
			if (!last) {
				reloc_old_root[strlen(reloc_old_root)] = PATH_SEPARATOR;
			}
			reloc_old_root = next;
		}

		/*
		 * preserve the old session, skipping those dirs/files that are found again
		 * in the new root
		 */
		for (s_entry = this_dir->contents; s_entry; s_entry = s_entry->next) {
			status = stat_filter(s_entry->whole_name, &statbuf);
			lstatus = lstat_filter(s_entry->whole_name, &lstatbuf);

			/*
			 * check_prev_session() will search for s_entry and remove it from
			 * orig_contents if found
			 */
			retcode = check_prev_session(orig_contents, n_orig, s_entry,
			    &statbuf, &lstatbuf, NULL);
			if (retcode == -1)
				return (-1);
		}
		merge_remaining_entries(this_dir, orig_contents, n_orig);

		/* use new directory */
		free_mdinfo(orig_contents, n_orig);
		orig_contents = new_orig_contents;
		n_orig = new_n_orig;

		if (reloc_root && reloc_root[0]) {
			/* also decend into new root before searching for files */
			this_dir = find_or_create_directory(this_dir, reloc_root, NULL, TRUE, NULL);
			if (!this_dir) {
				return (-1);
			}
		}
	}


	/*
	 * Now we scan the directory itself, and look at what is inside of it.
	 */
	for (s_entry = this_dir->contents; s_entry; s_entry = s_entry->next) {
		status = stat_filter(s_entry->whole_name, &statbuf);
		lstatus = lstat_filter(s_entry->whole_name, &lstatbuf);

		/*
		 * We always should create an entirely new directory tree
		 * whenever we generate a new session, unless there were
		 * *no* changes whatsoever to any of the directories, in which
		 * case it would be kind of pointless to generate a new
		 * session.
		 * I believe it is possible to rigorously prove that any change
		 * anywhere in the filesystem will force the entire tree to be
		 * regenerated because the modified directory will get a new
		 * extent number.  Since each subdirectory of the changed
		 * directory has a '..' entry, all of them will need to be
		 * rewritten too, and since the parent directory of the
		 * modified directory will have an extent pointer to the
		 * directory it too will need to be rewritten.  Thus we will
		 * never be able to reuse any directory information when
		 * writing new sessions.
		 *
		 * We still check the previous session so we can mark off the
		 * equivalent entry in the list we got from the original disc,
		 * however.
		 */

		/*
		 * The check_prev_session function looks for an identical
		 * entry in the previous session.  If we see it, then we copy
		 * the extent number to s_entry, and cross it off the list.
		 * It returns 2 if it's a directory
		 */
		retcode = check_prev_session(orig_contents, n_orig, s_entry,
			&statbuf, &lstatbuf, &odpnt);
		if (retcode == -1)
			return (-1);

		if (retcode == 2 && odpnt != NULL) {
			int	dflag;

			if (strcmp(s_entry->name, ".") != 0 &&
					strcmp(s_entry->name, "..") != 0) {
				struct directory *child;

				/*
				 * XXX It seems that the tree that has been
				 * XXX read from the previous session does not
				 * XXX carry whole_name entries. We provide a
				 * XXX hack in
				 * XXX multi.c:find_or_create_directory()
				 * XXX that should be removed when a
				 * XXX reasonable method could be found.
				 */
				child = find_or_create_directory(this_dir,
					s_entry->whole_name,
					s_entry, 1, NULL);
				dflag = merge_previous_session(child,
					&odpnt->isorec,
					NULL, reloc_old_root);
				if (dflag == -1) {
					return (-1);
				}
				free(odpnt);
				odpnt = NULL;
			}
		}
	}

	if (!reloc_old_root) {
		/*
		 * Whatever is left over, are things which are no longer in the tree on
		 * disk. We need to also merge these into the tree.
		 */
		merge_remaining_entries(this_dir, orig_contents, n_orig);
	}
	free_mdinfo(orig_contents, n_orig);
	return (1);
}

/*
 * This code deals with relocated directories which may exist
 * in the previous session.
 */
struct dir_extent_link  {
	unsigned int		extent;
	struct directory_entry	*de;
	struct dir_extent_link	*next;
};

static struct dir_extent_link	*cl_dirs = NULL;
static struct dir_extent_link	*re_dirs = NULL;

static void
check_rr_relocation(struct directory_entry *de)
{
	unsigned char	sector[SECTOR_SIZE];
	unsigned char	*pnt = de->rr_attributes;
		int	len = de->rr_attr_size;
		int	cont_extent = 0,
			cont_offset = 0,
			cont_size = 0;

	pnt = parse_xa(pnt, &len, /* dpnt */ 0);
	while (len >= 4) {
		if (pnt[3] != 1 && pnt[3] != 2) {
#ifdef USE_LIBSCHILY
			errmsgno(EX_BAD, "**BAD RRVERSION (%d) for %c%c\n", pnt[3], pnt[0], pnt[1]);
#else
			fprintf(stderr, "**BAD RRVERSION (%d) for %c%c\n", pnt[3], pnt[0], pnt[1]);
#endif
		}
		if (strncmp((char *) pnt, "CL", 2) == 0) {
			struct dir_extent_link *dlink = e_malloc(sizeof (*dlink));

			dlink->extent = isonum_733(pnt + 4);
			dlink->de = de;
			dlink->next = cl_dirs;
			cl_dirs = dlink;

		} else if (strncmp((char *) pnt, "RE", 2) == 0) {
			struct dir_extent_link *dlink = e_malloc(sizeof (*dlink));

			dlink->extent = de->starting_block;
			dlink->de = de;
			dlink->next = re_dirs;
			re_dirs = dlink;

		} else if (strncmp((char *) pnt, "CE", 2) == 0) {
			cont_extent = isonum_733(pnt + 4);
			cont_offset = isonum_733(pnt + 12);
			cont_size = isonum_733(pnt + 20);

		} else if (strncmp((char *) pnt, "ST", 2) == 0) {
			len = pnt[2];
		}
		len -= pnt[2];
		pnt += pnt[2];
		if (len <= 3 && cont_extent) {
			/* ??? What if cont_offset+cont_size > SECTOR_SIZE */
			readsecs(cont_extent, sector, 1);
			pnt = sector + cont_offset;
			len = cont_size;
			cont_extent = cont_offset = cont_size = 0;
		}
	}

}

void
match_cl_re_entries()
{
	struct dir_extent_link *re = re_dirs;

	/* for each relocated directory */
	for (; re; re = re->next) {
		struct dir_extent_link *cl = cl_dirs;

		for (; cl; cl = cl->next) {
			/* find a place where it was relocated from */
			if (cl->extent == re->extent) {
				/* set link to that place */
				re->de->parent_rec = cl->de;
				re->de->filedir = cl->de->filedir;

				/*
				 * see if it is in rr_moved
				 */
				if (reloc_dir != NULL) {
					struct directory_entry *rr_moved_e = reloc_dir->contents;

					for (; rr_moved_e; rr_moved_e = rr_moved_e->next) {
						/* yes it is */
						if (re->de == rr_moved_e) {
							/* forget it */
							re->de = NULL;
						}
					}
				}
				break;
			}
		}
	}
}

void
finish_cl_pl_for_prev_session()
{
	struct dir_extent_link *re = re_dirs;

	/* for those that were relocated, but NOT to rr_moved */
	re = re_dirs;
	for (; re; re = re->next) {
		if (re->de != NULL) {
			/*
			 * here we have hypothetical case when previous session
			 * was not created by genisoimage and contains relocations
			 */
			struct directory_entry *s_entry = re->de;
			struct directory_entry *s_entry1;
			struct directory *d_entry = reloc_dir->subdir;

			/* do the same as finish_cl_pl_entries */
			if (s_entry->de_flags & INHIBIT_ISO9660_ENTRY) {
				continue;
			}
			while (d_entry) {
				if (d_entry->self == s_entry)
					break;
				d_entry = d_entry->next;
			}
			if (!d_entry) {
#ifdef USE_LIBSCHILY
				comerrno(EX_BAD, "Unable to locate directory parent\n");
#else
				fprintf(stderr, "Unable to locate directory parent\n");
				exit(1);
#endif
			}

			if (s_entry->filedir != NULL && s_entry->parent_rec != NULL) {
				char	*rr_attr;

				/*
				 * First fix the PL pointer in the directory in the
				 * rr_reloc dir
				 */
				s_entry1 = d_entry->contents->next;
				rr_attr = find_rr_attribute(s_entry1->rr_attributes,
					s_entry1->total_rr_attr_size, "PL");
				if (rr_attr != NULL)
					set_733(rr_attr + 4, s_entry->filedir->extent);

				/* Now fix the CL pointer */
				s_entry1 = s_entry->parent_rec;

				rr_attr = find_rr_attribute(s_entry1->rr_attributes,
					s_entry1->total_rr_attr_size, "CL");
				if (rr_attr != NULL)
					set_733(rr_attr + 4, d_entry->extent);
			}
		}
	}
	/* free memory */
	re = re_dirs;
	while (re) {
		struct dir_extent_link *next = re->next;

		free(re);
		re = next;
	}
	re = cl_dirs;
	while (re) {
		struct dir_extent_link *next = re->next;

		free(re);
		re = next;
	}
}
