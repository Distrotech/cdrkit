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

/* @(#)write.c	1.88 06/02/01 joerg */
/* Parts from @(#)write.c	1.106 07/02/17 joerg */
/*
 * Program write.c - dump memory  structures to  file for iso9660 filesystem.
 *
 * Written by Eric Youngdale (1993).
 *
 * Copyright 1993 Yggdrasil Computing, Incorporated
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

/* APPLE_HYB James Pearson j.pearson@ge.ucl.ac.uk 23/2/2000 */

#include <mconfig.h>
#include "genisoimage.h"
#include <timedefs.h>
#include <fctldefs.h>
#ifdef SORTING
#include "match.h"
#endif /* SORTING */
#include <errno.h>
#include <schily.h>
#ifdef DVD_VIDEO
#include "dvd_reader.h"
#include "dvd_file.h"
#include "ifo_read.h"
#endif
#ifdef APPLE_HYB
#include <ctype.h>
#endif

#ifdef	VMS
#include "vms.h"
#endif

/* Max number of sectors we will write at  one time */
#define	NSECT 16

/* Counters for statistics */

static int	table_size = 0;
static int	total_dir_size = 0;
static int	rockridge_size = 0;
static struct directory **pathlist;
static int	next_path_index = 1;
static int	sort_goof;

static int	is_rr_dir = 0;

struct output_fragment *out_tail;
struct output_fragment *out_list;

struct iso_primary_descriptor vol_desc;

void	set_721(char *pnt, unsigned int i);
void	set_722(char *pnt, unsigned int i);
void	set_723(char *pnt, unsigned int i);
void	set_731(char *pnt, unsigned int i);
void	set_732(char *pnt, unsigned int i);
void	set_733(char *pnt, unsigned int i);
int	get_731(char *p);
int	get_732(char *p);
int	get_733(char *p);
static	int	xawrite(void *buffer, int size, int count, FILE *file, 
							  int submode, BOOL islast);
void	xfwrite(void *buffer, int size, int count, FILE *file, int submode, 
				  BOOL islast);
static 	int	assign_directory_addresses(struct directory *node);
#ifdef APPLE_HYB
static 	void	write_one_file(char *filename, off_t size, FILE *outfile, 
										off_t off);
#else
static 	void	write_one_file(char *filename, off_t size, FILE *outfile);
#endif
static 	void	write_files(FILE *outfile);
#if 0
static 	void	dump_filelist	__PR((void));
#endif
static 	int	compare_dirs(const void *rr, const void *ll);
int	sort_directory(struct directory_entry **sort_dir, int rr);
static 	int	root_gen(void);
static 	BOOL	assign_file_addresses(struct directory *dpnt, BOOL isnest);
static 	void	free_one_directory(struct directory *dpnt);
static 	void	free_directories(struct directory *dpnt);
void	generate_one_directory(struct directory *dpnt, FILE *outfile);
static 	void	build_pathlist(struct directory *node);
static 	int	compare_paths(void const *r, void const *l);
static 	int	generate_path_tables(void);
void	memcpy_max(char *to, char *from, int max);
void	outputlist_insert(struct output_fragment *frag);
static 	int	file_write(FILE *outfile);
static 	int	pvd_write(FILE *outfile);
static 	int	xpvd_write(FILE *outfile);
static 	int	evd_write(FILE *outfile);
static 	int	vers_write(FILE *outfile);
static 	int	graftcp(char *to, char *from, char *ep);
static 	int	pathcp(char *to, char *from, char *ep);
static 	int	pathtab_write(FILE *outfile);
static 	int	exten_write(FILE *outfile);
int	oneblock_size(int starting_extent);
static 	int	pathtab_size(int starting_extent);
static 	int	startpad_size(int starting_extent);
static 	int	interpad_size(int starting_extent);
static 	int	endpad_size(int starting_extent);
static 	int	file_gen(void);
static 	int	dirtree_dump(void);
static 	int	dirtree_fixup(int starting_extent);
static 	int	dirtree_size(int starting_extent);
static 	int	ext_size(int starting_extent);
static 	int	dirtree_write(FILE *outfile);
static 	int	dirtree_cleanup(FILE *outfile);
static 	int	startpad_write(FILE *outfile);
static 	int	interpad_write(FILE *outfile);
static 	int	endpad_write(FILE *outfile);
#ifdef APPLE_HYB
static 	int	hfs_pad;
static 	int	hfs_get_parms(char *key);
static 	void	hfs_file_gen(int start_extent);
static 	void	gen_prepboot(void);
Ulong	get_adj_size(int Csize);
int	adj_size(int Csize, int start_extent, int extra);
void	adj_size_other(struct directory *dpnt);
static 	int	hfs_hce_write(FILE * outfile);
int	insert_padding_file(int size);
#endif	/* APPLE_HYB */

#ifdef SORTING
static 	int	compare_sort(const void * rr, const void * ll);
static 	void	reassign_link_addresses(struct directory * dpnt);
static 	int	sort_file_addresses(void);
#endif /* SORTING */

/*
 * Routines to actually write the disc.  We write sequentially so that
 * we could write a tape, or write the disc directly
 */
#define	FILL_SPACE(X)	memset(vol_desc.X, ' ', sizeof (vol_desc.X))

void
set_721(char *pnt, unsigned int i)
{
	pnt[0] = i & 0xff;
	pnt[1] = (i >> 8) & 0xff;
}

void
set_722(char *pnt, unsigned int i)
{
	pnt[0] = (i >> 8) & 0xff;
	pnt[1] = i & 0xff;
}

void
set_723(char *pnt, unsigned int i)
{
	pnt[3] = pnt[0] = i & 0xff;
	pnt[2] = pnt[1] = (i >> 8) & 0xff;
}

void
set_731(char *pnt, unsigned int i)
{
	pnt[0] = i & 0xff;
	pnt[1] = (i >> 8) & 0xff;
	pnt[2] = (i >> 16) & 0xff;
	pnt[3] = (i >> 24) & 0xff;
}

void
set_732(char *pnt, unsigned int i)
{
	pnt[3] = i & 0xff;
	pnt[2] = (i >> 8) & 0xff;
	pnt[1] = (i >> 16) & 0xff;
	pnt[0] = (i >> 24) & 0xff;
}

void
set_733(char *pnt, unsigned int i)
{
	pnt[7] = pnt[0] = i & 0xff;
	pnt[6] = pnt[1] = (i >> 8) & 0xff;
	pnt[5] = pnt[2] = (i >> 16) & 0xff;
	pnt[4] = pnt[3] = (i >> 24) & 0xff;
}

int
get_731(char *p)
{
	return ((p[0] & 0xff)
		| ((p[1] & 0xff) << 8)
		| ((p[2] & 0xff) << 16)
		| ((p[3] & 0xff) << 24));
}

int
get_732(char *p)
{
	return ((p[3] & 0xff)
		| ((p[2] & 0xff) << 8)
		| ((p[1] & 0xff) << 16)
		| ((p[0] & 0xff) << 24));
}

int
get_733(char *p)
{
	return ((p[0] & 0xff)
		| ((p[1] & 0xff) << 8)
		| ((p[2] & 0xff) << 16)
		| ((p[3] & 0xff) << 24));
}

void
xfwrite(void *buffer, int size, int count, FILE *file, int submode, BOOL islast)
{
	/*
	 * This is a hack that could be made better.
	 * XXXIs this the only place?
	 * It is definitely needed on Operating Systems that do not allow to
	 * write files that are > 2GB. If the system is fast enough to be able
	 * to feed 1400 KB/s writing speed of a DVD-R drive, use stdout.
	 * If the system cannot do this reliable, you need to use this hacky
	 * option.
	 */
	static int	idx = 0;

#ifdef	XFWRITE_DEBUG
	if (count != 1 || (size % 2048) != 0)
		fprintf(stderr, "Count: %d, size: %d\n", count, size);
#endif

	if (split_output != 0 &&
		(idx == 0 || ftell(file) >= ((off_t)1024 * 1024 * 1024))) {
		char		nbuf[512];
		extern char	*outfile;

		if (idx == 0)
			unlink(outfile);
		sprintf(nbuf, "%s_%02d", outfile, idx++);
		file = freopen(nbuf, "wb", file);
		if (file == NULL) {
#ifdef	USE_LIBSCHILY
			comerr("Cannot open '%s'.\n", nbuf);
#else
			fprintf(stderr, "Cannot open '%s'.\n", nbuf);
			exit(1);
#endif
		}
	}
	while (count) {
		int	got;

		seterrno(0);
		if (osecsize != 0)
			got = xawrite(buffer, size, count, file, submode, islast);
		else
			got = fwrite(buffer, size, count, file);

		if (got <= 0) {
#ifdef	USE_LIBSCHILY
			comerr("cannot fwrite %d*%d\n", size, count);
#else
			fprintf(stderr, "cannot fwrite %d*%d\n", size, count);
			exit(1);
#endif
		}
		/*
		 * This comment is in hope to prevent silly people from
		 * e.g. SuSE (who did not yet learn C but believe that
		 * they need to patch other peoples code) from changing the
		 * next cast into an illegal lhs cast expression.
		 * The cast below is the correct way to handle the problem.
		 * The (void *) cast is to avoid a GCC warning like:
		 * "warning: dereferencing type-punned pointer will break \
		 * strict-aliasing rules"
		 * which is wrong this code. (void *) introduces a compatible
		 * intermediate type in the cast list.
		 */
		count -= got;
		buffer = (void *)(((char *)buffer) + size * got);
	}
}

static int
xawrite(void *buffer, int size, int count, FILE *file, int submode, BOOL islast)
{
	register char	*p = buffer;
	register int	amt = size * count;
	register int	n;
	struct xa_subhdr subhdr[2];

	if (osecsize == 2048)
		return (fwrite(buffer, size, count, file));

	if (amt % 2048)
		comerrno(EX_BAD,
			"Trying to write %d bytes (not a multiple of 2048).\n",
			amt);

	subhdr[0].file_number		= subhdr[1].file_number		= 0;
	subhdr[0].channel_number	= subhdr[1].channel_number	= 0;
	subhdr[0].coding		= subhdr[1].coding		= 0;

	while (amt > 0) {
#ifdef	LATER
		if (submode < 0)
			subhdr[0].sub_mode = subhdr[1].sub_mode = XA_SUBH_DATA;
		else
			subhdr[0].sub_mode = subhdr[1].sub_mode = submode;
#else
		subhdr[0].sub_mode = subhdr[1].sub_mode = XA_SUBH_DATA;
#endif

		if ((amt <= 2048) && islast) {
			subhdr[0].sub_mode = subhdr[1].sub_mode
						|= (XA_SUBH_EOR|XA_SUBH_EOF);
		}
		n = fwrite(&subhdr, sizeof (subhdr), 1, file);
		if (n <= 0)
			return (n);

		n = fwrite(p, 2048, 1, file);
		if (n <= 0)
			return (n);

		p += 2048;
		amt -= 2048;
	}
	return (1);
}

#ifdef APPLE_HYB
/*
 * use the deferred_write struct to store info about the hfs_boot_file
 */
static struct deferred_write mac_boot;

#endif	/* APPLE_HYB */
static struct deferred_write	*dw_head = NULL,
				*dw_tail = NULL;

unsigned int	last_extent_written = 0;
static	Uint	path_table_index;
time_t	begun;

/*
 * We recursively walk through all of the directories and assign extent
 * numbers to them.  We have already assigned extent numbers to everything that
 * goes in front of them
 */
static int
assign_directory_addresses(struct directory *node)
{
	int		dir_size;
	struct directory *dpnt;

	dpnt = node;

	while (dpnt) {
		/* skip if it's hidden */
		if (dpnt->dir_flags & INHIBIT_ISO9660_ENTRY) {
			dpnt = dpnt->next;
			continue;
		}
		/*
		 * If we already have an extent for this (i.e. it came from a
		 * multisession disc), then don't reassign a new extent.
		 */
		dpnt->path_index = next_path_index++;
		if (dpnt->extent == 0) {
			dpnt->extent = last_extent;
			dir_size = ISO_BLOCKS(dpnt->size);

			last_extent += dir_size;

			/*
			 * Leave room for the CE entries for this directory.
			 * Keep them close to the reference directory so that
			 * access will be quick.
			 */
			if (dpnt->ce_bytes) {
				last_extent += ISO_BLOCKS(dpnt->ce_bytes);
			}
		}
		if (dpnt->subdir) {
			assign_directory_addresses(dpnt->subdir);
		}
		dpnt = dpnt->next;
	}
	return (0);
}

#ifdef APPLE_HYB
static void
write_one_file(char *filename, off_t size, FILE *outfile, off_t off)
#else
static void
write_one_file(char *filename, off_t size, FILE *outfile)
#endif	/* APPLE_HYB */
{
	/*
	 * It seems that there are still stone age C-compilers
	 * around.
	 * The Metrowerks C found on BeOS/PPC does not allow
	 * more than 32kB of local vars.
	 * As we do not need to call write_one_file() recursively
	 * we make buffer static.
	 */
static	char		buffer[SECTOR_SIZE * NSECT];
	FILE		*infile;
	off_t		remain;
	int	use;

	char *mirror_name;
	unsigned char md5[16];
	int include_in_jigdo = list_file_in_jigdo(filename, size, &mirror_name, md5);

	if ((infile = fopen(filename, "rb")) == NULL) {
#ifdef	USE_LIBSCHILY
		comerr("cannot open '%s'\n", filename);
#else
#ifndef	HAVE_STRERROR
		fprintf(stderr, "cannot open '%s': (%d)\n",
				filename, errno);
#else
		fprintf(stderr, "cannot open '%s': %s\n",
				filename, strerror(errno));
#endif
		exit(1);
#endif
	}
#ifdef APPLE_HYB
	fseek(infile, off, SEEK_SET);
#endif	/* APPLE_HYB */
	remain = size;

	if (include_in_jigdo)
		write_jt_match_record(filename, mirror_name, SECTOR_SIZE, size, md5);

	while (remain > 0) {
		int	amt;

		use = (remain > SECTOR_SIZE * NSECT - 1 ?
				NSECT * SECTOR_SIZE : remain);
		use = ISO_ROUND_UP(use);	/* Round up to nearest sector */
						/* boundary */
		memset(buffer, 0, use);
		seterrno(0);
		amt = fread(buffer, 1, use, infile);
		if (amt < use && amt < remain) {
			/*
			 * Note that genisoimage is not star and no 100% archiver.
			 * We only detect file growth if the new size does not
			 * match 'use' at the last read.
			 */
			if (geterrno() == 0) {
#ifdef	USE_LIBSCHILY
         comerrno(EX_BAD,
               "File '%s' did shrink.\n"
               "Files must not be changed while genisoimage runs!\n",
               filename);
#else
         fprintf(stderr,
               "File '%s' did shrink.\n"
               "Files must not be changed while genisoimage runs!\n",
               filename);
				exit(EX_BAD);
#endif
			}
#ifdef	USE_LIBSCHILY
			comerr("Cannot read from '%s'\n", filename);
#else
			fprintf(stderr, "Cannot read from '%s'\n", filename);
			exit(1);
#endif
		}
		if (!include_in_jigdo)
			jtwrite(buffer, use, 1,
			        XA_SUBH_DATA, remain <= (SECTOR_SIZE * NSECT));
		xfwrite(buffer, use, 1, outfile,
				XA_SUBH_DATA, remain <= (SECTOR_SIZE * NSECT));
		last_extent_written += use / SECTOR_SIZE;
#if 0
		if ((last_extent_written % 1000) < use / SECTOR_SIZE) {
			fprintf(stderr, "%d..", last_extent_written);
		}
#else
		if (verbose > 0 &&
		    (int)(last_extent_written % (gui ? 500 : 5000)) <
							use / SECTOR_SIZE) {
			time_t	now;
			time_t	the_end;
			double	frac;

			time(&now);
			frac = last_extent_written / (1.0 * last_extent);
			the_end = begun + (now - begun) / frac;
#ifndef NO_FLOATINGPOINT
			fprintf(stderr, "%6.2f%% done, estimate finish %s",
				frac * 100., ctime(&the_end));
#else
			fprintf(stderr, "%3d.%-02d%% done, estimate finish %s",
				(int)(frac * 100.),
				(int)((frac+.00005) * 10000.)%100,
				ctime(&the_end));
#endif
			fflush(stderr);
		}
#endif
		remain -= use;
	}
	fclose(infile);
}/* write_one_file(... */

static void
write_files(FILE *outfile)
{
	struct deferred_write	*dwpnt,
				*dwnext;

	dwpnt = dw_head;
	while (dwpnt) {
/*#define DEBUG*/
#ifdef DEBUG
		fprintf(stderr,
		"The file name is %s and pad is %d, size is %lld and extent is %d\n",
				dwpnt->name, dwpnt->pad,
				(Llong)dwpnt->size, dwpnt->extent);
#endif
		if (dwpnt->table) {
			jtwrite(dwpnt->table, ISO_ROUND_UP(dwpnt->size), 1, XA_SUBH_DATA, TRUE);
			xfwrite(dwpnt->table, ISO_ROUND_UP(dwpnt->size), 1,
							outfile,
							XA_SUBH_DATA, TRUE);
			last_extent_written += ISO_BLOCKS(dwpnt->size);
			table_size += dwpnt->size;
/*			fprintf(stderr, "Size %lld ", (Llong)dwpnt->size); */
			free(dwpnt->table);
			dwpnt->table = NULL;
		} else {

#ifdef VMS
			vms_write_one_file(dwpnt->name, dwpnt->size, outfile);
#else
#ifdef APPLE_HYB
			write_one_file(dwpnt->name, dwpnt->size, outfile,
								dwpnt->off);
#else
			write_one_file(dwpnt->name, dwpnt->size, outfile);
#endif	/* APPLE_HYB */
#endif
			free(dwpnt->name);
			dwpnt->name = NULL;
		}


#ifndef DVD_VIDEO
#define	dvd_video	0
#endif

#ifndef APPLE_HYB
#define	apple_hyb	0
#endif

#if	defined(APPLE_HYB) || defined(DVD_VIDEO)

		if (apple_hyb || dvd_video) {
			/*
			 * we may have to pad out ISO files to work with HFS
			 * clump sizes
			 */
			char	blk[SECTOR_SIZE];
			Uint	i;

			for (i = 0; i < dwpnt->pad; i++) {
				jtwrite(blk, SECTOR_SIZE, 1, 0, FALSE);
				xfwrite(blk, SECTOR_SIZE, 1, outfile, 0, FALSE);
				last_extent_written++;
			}
		}
#endif	/* APPLE_HYB || DVD_VIDEO */


		dwnext = dwpnt;
		dwpnt = dwpnt->next;
		free(dwnext);
		dwnext = NULL;
	}
}/* write_files(... */

#if 0
static void
dump_filelist()
{
	struct deferred_write *dwpnt;

	dwpnt = dw_head;
	while (dwpnt) {
		fprintf(stderr, "File %s\n", dwpnt->name);
		dwpnt = dwpnt->next;
	}
	fprintf(stderr, "\n");
}

#endif

static int
compare_dirs(const void *rr, const void *ll)
{
	char		*rpnt,
			*lpnt;
	struct directory_entry **r,
			**l;

	r = (struct directory_entry **) rr;
	l = (struct directory_entry **) ll;
	rpnt = (*r)->isorec.name;
	lpnt = (*l)->isorec.name;

#ifdef APPLE_HYB
	/*
	 * resource fork MUST (not sure if this is true for HFS volumes) be
	 * before the data fork - so force it here
	 */
	if ((*r)->assoc && (*r)->assoc == (*l))
		return (1);
	if ((*l)->assoc && (*l)->assoc == (*r))
		return (-1);
#endif	/* APPLE_HYB */

	/* If the entries are the same, this is an error. */
	if (strcmp(rpnt, lpnt) == 0) {
#ifdef	USE_LIBSCHILY
		errmsgno(EX_BAD,
			"Error: '%s' and '%s' have the same ISO9660 name '%s'.\n",
			(*r)->whole_name, (*l)->whole_name,
			rpnt);
#else
		fprintf(stderr,
			"Error: '%s' and '%s' have the same ISO9660 name '%s'.\n",
			(*r)->whole_name, (*l)->whole_name,
			rpnt);
#endif
		sort_goof++;
	}
	/* Check we don't have the same RR name */
	if (use_RockRidge && !is_rr_dir) {
		/*
		 * entries *can* have the same RR name in the "rr_moved"
		 * directory so skip checks if we're in reloc_dir
		 */
		if (!(strcmp((*r)->name, (*l)->name))) {
#ifdef	USE_LIBSCHILY
			errmsgno(EX_BAD,
			"Error: '%s' and '%s' have the same Rock Ridge name '%s'.\n",
				(*r)->whole_name, (*l)->whole_name,
				(*r)->name);
#else
			fprintf(stderr,
			"Error: '%s' and '%s' have the same Rock Ridge name '%s'.\n",
				(*r)->whole_name, (*l)->whole_name,
				(*r)->name);
#endif
			sort_goof++;
		}
	}
	/*
	 * Put the '.' and '..' entries on the head of the sorted list. For
	 * normal ASCII, this always happens to be the case, but out of band
	 * characters cause this not to be the case sometimes.
	 * FIXME(eric) - these tests seem redundant, in that the name is never
	 * assigned these values.  It will instead be \000 or \001, and thus
	 * should always be sorted correctly.   I need to figure out why I
	 * thought I needed this in the first place.
	 */
#if 0
	if (strcmp(rpnt, ".") == 0)
		return (-1);
	if (strcmp(lpnt, ".") == 0)
		return (1);

	if (strcmp(rpnt, "..") == 0)
		return (-1);
	if (strcmp(lpnt, "..") == 0)
		return (1);
#else
	/*
	 * The code above is wrong (as explained in Eric's comment), leading to
	 * incorrect sort order iff the -L option ("allow leading dots") is in
	 * effect and a directory contains entries that start with a dot.
	 *  (TF, Tue Dec 29 13:49:24 CET 1998)
	 */
	if ((*r)->isorec.name_len[0] == 1 && *rpnt == 0)
		return (-1);	/* '.' */
	if ((*l)->isorec.name_len[0] == 1 && *lpnt == 0)
		return (1);

	if ((*r)->isorec.name_len[0] == 1 && *rpnt == 1)
		return (-1);	/* '..' */
	if ((*l)->isorec.name_len[0] == 1 && *lpnt == 1)
		return (1);
#endif

	while (*rpnt && *lpnt) {
		if (*rpnt == ';' && *lpnt != ';')
			return (-1);
		if (*rpnt != ';' && *lpnt == ';')
			return (1);

		if (*rpnt == ';' && *lpnt == ';')
			return (0);

		if (*rpnt == '.' && *lpnt != '.')
			return (-1);
		if (*rpnt != '.' && *lpnt == '.')
			return (1);

		if ((unsigned char) *rpnt < (unsigned char) *lpnt)
			return (-1);
		if ((unsigned char) *rpnt > (unsigned char) *lpnt)
			return (1);
		rpnt++;
		lpnt++;
	}
	if (*rpnt)
		return (1);
	if (*lpnt)
		return (-1);
	return (0);
}

/*
 * Function:		sort_directory
 *
 * Purpose:		Sort the directory in the appropriate ISO9660
 *			order.
 *
 * Notes:		Returns 0 if OK, returns > 0 if an error occurred.
 */
int
sort_directory(struct directory_entry **sort_dir, int rr)
{
	int		dcount = 0;
	int		xcount = 0;
	int		j;
	int		i,
			len;
	struct directory_entry *s_entry;
	struct directory_entry **sortlist;

	/* need to keep a count of how many entries are hidden */
	s_entry = *sort_dir;
	while (s_entry) {
		if (s_entry->de_flags & INHIBIT_ISO9660_ENTRY)
			xcount++;
		dcount++;
		s_entry = s_entry->next;
	}

	if (dcount == 0) {
		return (0);
	}
	/* OK, now we know how many there are.  Build a vector for sorting. */
	sortlist = (struct directory_entry **)
		e_malloc(sizeof (struct directory_entry *) * dcount);

	j = dcount - 1;
	dcount = 0;
	s_entry = *sort_dir;
	while (s_entry) {
		if (s_entry->de_flags & INHIBIT_ISO9660_ENTRY) {
			/* put any hidden entries at the end of the vector */
			sortlist[j--] = s_entry;
		} else {
			sortlist[dcount] = s_entry;
			dcount++;
		}
		len = s_entry->isorec.name_len[0];
		s_entry->isorec.name[len] = 0;
		s_entry = s_entry->next;
	}

	/* Each directory is required to contain at least . and .. */
	if (dcount < 2) {
#ifdef	USE_LIBSCHILY
		errmsgno(EX_BAD,
			"Directory size too small (. or .. missing ??%s)\n",
			"?");	/* Try to avoid a GCC trigraph warning */
#else
		fprintf(stderr,
			"Directory size too small (. or .. missing ??%s)\n",
			"?");	/* Try to avoid a GCC trigraph warning */
#endif
		sort_goof = 1;

	} else {
		/* only sort the non-hidden entries */
		sort_goof = 0;
		is_rr_dir = rr;
#ifdef PROTOTYPES
		qsort(sortlist, dcount, sizeof (struct directory_entry *),
			(int (*) (const void *, const void *)) compare_dirs);
#else
		qsort(sortlist, dcount, sizeof (struct directory_entry *),
			compare_dirs);
#endif

		/*
		 * Now reassemble the linked list in the proper sorted order
		 * We still need the hidden entries, as they may be used in
		 * the Joliet tree.
		 */
		for (i = 0; i < dcount + xcount - 1; i++) {
			sortlist[i]->next = sortlist[i + 1];
		}

		sortlist[dcount + xcount - 1]->next = NULL;
		*sort_dir = sortlist[0];
	}

	free(sortlist);
	sortlist = NULL;
	return (sort_goof);
}

static int
root_gen()
{
	init_fstatbuf();

	root_record.length[0] = 1 +
			offsetof(struct iso_directory_record, name[0]);
	root_record.ext_attr_length[0] = 0;
	set_733((char *) root_record.extent, root->extent);
	set_733((char *) root_record.size, ISO_ROUND_UP(root->size));
	iso9660_date(root_record.date, root_statbuf.st_mtime);
	root_record.flags[0] = ISO_DIRECTORY;
	root_record.file_unit_size[0] = 0;
	root_record.interleave[0] = 0;
	set_723(root_record.volume_sequence_number, volume_sequence_number);
	root_record.name_len[0] = 1;
	return (0);
}

#ifdef SORTING
/*
 *	sorts deferred_write entries based on the sort weight
 */
static int
compare_sort(const void *rr, const void *ll)
{
	struct deferred_write	**r;
	struct deferred_write	**l;
	int			r_sort;
	int			l_sort;

	r = (struct deferred_write **) rr;
	l = (struct deferred_write **) ll;
	r_sort = (*r)->s_entry->sort;
	l_sort = (*l)->s_entry->sort;

	if (r_sort != l_sort)
		return (r_sort < l_sort ? 1 : -1);
	else
		return ((*r)->extent - (*l)->extent);
}

/*
 *	reassign start extents to files that are "hard links" to
 *	files that may have been sorted
 */
static void
reassign_link_addresses(struct directory *dpnt)
{
	struct directory_entry	*s_entry;
	struct file_hash	*s_hash;

	while (dpnt) {
		s_entry = dpnt->contents;
		for (s_entry = dpnt->contents; s_entry; s_entry = s_entry->next) {
			/* link files have already been given the weight NOT_SORTED */
			if (s_entry->sort == NOT_SORTED)
			{
				/* update the start extent */
				s_hash = find_hash(s_entry->dev, s_entry->inode);
				if (s_hash) {
					set_733((char *) s_entry->isorec.extent,
							s_hash->starting_block);
					s_entry->starting_block = s_hash->starting_block;
				}
			}

			if (verbose > 2 && s_entry->size != 0) {
				fprintf(stderr, "%8u %8u ",
					s_entry->starting_block,
					(unsigned int)(s_entry->starting_block + ISO_BLOCKS(s_entry->size) - 1));

				if (s_entry->inode != TABLE_INODE) {
					fprintf(stderr, "%s\n", s_entry->whole_name);
				} else {
					fprintf(stderr, "%s%s%s\n",
						s_entry->filedir->whole_name,
						SPATH_SEPARATOR, trans_tbl);
				}
			}
		}
		if (dpnt->subdir) {
			reassign_link_addresses(dpnt->subdir);
		}

		dpnt = dpnt->next;
	}
}

/*
 *	sort files in order of the given sort weight
 */
static int
sort_file_addresses()
{
	struct deferred_write	*dwpnt;
	struct deferred_write	**sortlist;
	struct directory_entry	*s_entry;
	int			start_extent;
	int			num = 0;
	int			i;

	/* need to store start extents for linked files */
	flush_hash();

	/* find out how many files we have */
	dwpnt = dw_head;
	while (dwpnt) {
		num++;
		dwpnt = dwpnt->next;
	}

	/* return if we have none */
	if (num == 0) {
		return (1);
	}

	/* save the start extent of the first file */
	start_extent = dw_head->extent;

	/* set up vector to store entries */
	sortlist = (struct deferred_write **)
		e_malloc(sizeof (struct deferred_write *) * num);

	for (i = 0, dwpnt = dw_head; i < num; i++, dwpnt = dwpnt->next)
		sortlist[i] = dwpnt;

	/* sort the list */
#ifdef PROTOTYPES
	qsort(sortlist, num, sizeof (struct deferred_write *),
		(int (*)(const void *, const void *))compare_sort);
#else
	qsort(sortlist, num, sizeof (struct deferred_write *), compare_sort);
#endif

	/* reconstruct the linked list */
	for (i = 0; i < num-1; i++) {
		sortlist[i]->next = sortlist[i+1];
	}

	sortlist[num-1]->next = NULL;
	dw_head = sortlist[0];

	free(sortlist);

	/* set the new start extents for the sorted list */
	for (i = 0, dwpnt = dw_head; i < num; i++, dwpnt = dwpnt->next) {
		s_entry = dwpnt->s_entry;
		dwpnt->extent = s_entry->starting_block = start_extent;
		set_733((char *) s_entry->isorec.extent, start_extent);

		start_extent += ISO_BLOCKS(s_entry->size);
#ifdef DVD_VIDEO
		/*
		 * Shouldn't this be done for every type of sort? Otherwise
		 * we will loose every pad info we add if we sort the files
		 */
		if (dvd_video) {
			start_extent += dwpnt->pad;
		}
#endif /* DVD_VIDEO */

		/* cache start extents for any linked files */
		add_hash(s_entry);
	}

	return (0);
}
#endif /* SORTING */



static BOOL
assign_file_addresses(struct directory *dpnt, BOOL isnest)
{
	struct directory *finddir;
	struct directory_entry *s_entry;
	struct file_hash *s_hash;
	struct deferred_write *dwpnt;
	char		whole_path[PATH_MAX];
#ifdef DVD_VIDEO
	char		dvd_path[PATH_MAX];
	title_set_info_t * title_set_info = NULL;
	char	*p;
#endif
	BOOL	ret = FALSE;

	while (dpnt) {
#ifdef DVD_VIDEO
		if (dvd_video && root == dpnt->parent &&
		    ((p = strstr(dpnt->whole_name, "VIDEO_TS")) != 0)&&
		    strcmp(p, "VIDEO_TS") == 0) {

			int     maxlen = strlen(dpnt->whole_name)-8;
			if (maxlen > (sizeof (dvd_path)-1))
				maxlen = sizeof (dvd_path)-1;
			strncpy(dvd_path, dpnt->whole_name, maxlen);
			dvd_path[maxlen] = '\0';

#ifdef DEBUG
			fprintf(stderr, "Found 'VIDEO_TS', the path is %s \n", dvd_path);
#endif
			title_set_info = DVDGetFileSet(dvd_path);
			if (title_set_info == 0) {
				/*
				 * Do not switch off -dvd-video but let is fail later.
				 */
/*				dvd_video = 0;*/
				errmsgno(EX_BAD, "Unable to parse DVD-Video structures.\n");
			} else {
				ret = TRUE;
			}
		}
#endif /* DVD_VIDEO */

		s_entry = dpnt->contents;
		for (s_entry = dpnt->contents; s_entry;
						s_entry = s_entry->next) {
			/*
			 * If we already have an  extent for this entry, then
			 * don't assign a new one.  It must have come from a
			 * previous session on the disc.  Note that we don't
			 * end up scheduling the thing for writing either.
			 */
			if (get_733(s_entry->isorec.extent) != 0) {
				continue;
			}
			/*
			 * This saves some space if there are symlinks present
			 */
			s_hash = find_hash(s_entry->dev, s_entry->inode);
			if (s_hash) {
				if (verbose > 2) {
					fprintf(stderr, "Cache hit for '%s%s%s'\n", s_entry->filedir->de_name,
						SPATH_SEPARATOR,
						s_entry->name);
				}
				set_733((char *) s_entry->isorec.extent,
						s_hash->starting_block);
				set_733((char *) s_entry->isorec.size,
						s_hash->size);
#ifdef SORTING
				/* check for non-directory files */
				if (do_sort && ((s_entry->isorec.flags[0] & ISO_DIRECTORY) == 0)) {
					/* make sure the real file has the highest weighting */
					s_hash->de->sort = MAX(s_entry->sort, s_hash->de->sort);
					/* flag this as a potential non-sorted file */
					s_entry->sort = NOT_SORTED;
				}
#endif /* SORTING */
				continue;
			}
			/*
			 * If this is for a directory that is not a . or
			 * a .. entry, then look up the information for the
			 * entry.  We have already assigned extents for
			 * directories, so we just need to fill in the blanks
			 * here.
			 */
			if (strcmp(s_entry->name, ".") != 0 &&
					strcmp(s_entry->name, "..") != 0 &&
					s_entry->isorec.flags[0] & ISO_DIRECTORY) {
				finddir = dpnt->subdir;
				while (1 == 1) {
					if (finddir->self == s_entry)
						break;
					finddir = finddir->next;
					if (!finddir) {
#ifdef	DVD_VIDEO
						if (title_set_info != 0) {
							DVDFreeFileSet(title_set_info);
						}
#endif
						comerrno(EX_BAD,
							"Fatal goof - could not find dir entry for '%s'\n",
							s_entry->name);
					}
				}
				set_733((char *) s_entry->isorec.extent,
						finddir->extent);
				s_entry->starting_block = finddir->extent;
				s_entry->size = ISO_ROUND_UP(finddir->size);
				total_dir_size += s_entry->size;
				add_hash(s_entry);
				set_733((char *) s_entry->isorec.size,
						ISO_ROUND_UP(finddir->size));
				continue;
			}
			/*
			 * If this is . or .., then look up the relevant info
			 * from the tables.
			 */
			if (strcmp(s_entry->name, ".") == 0) {
				set_733((char *) s_entry->isorec.extent,
								dpnt->extent);

				/*
				 * Set these so that the hash table has the
				 * correct information
				 */
				s_entry->starting_block = dpnt->extent;
				s_entry->size = ISO_ROUND_UP(dpnt->size);

				add_hash(s_entry);
				s_entry->starting_block = dpnt->extent;
				set_733((char *) s_entry->isorec.size,
						ISO_ROUND_UP(dpnt->size));
				continue;
			}
			if (strcmp(s_entry->name, "..") == 0) {
				if (dpnt == root) {
					total_dir_size += root->size;
				}
				set_733((char *) s_entry->isorec.extent,
							dpnt->parent->extent);

				/*
				 * Set these so that the hash table has the
				 * correct information
				 */
				s_entry->starting_block = dpnt->parent->extent;
				s_entry->size =
					ISO_ROUND_UP(dpnt->parent->size);

				add_hash(s_entry);
				s_entry->starting_block = dpnt->parent->extent;
				set_733((char *) s_entry->isorec.size,
					ISO_ROUND_UP(dpnt->parent->size));
				continue;
			}
			/*
			 * Some ordinary non-directory file.  Just schedule
			 * the file to be written.  This is all quite
			 * straightforward, just make a list and assign
			 * extents as we go.  Once we get through writing all
			 * of the directories, we should be ready write out
			 * these files
			 */
			if (s_entry->size) {
				dwpnt = (struct deferred_write *)
					e_malloc(sizeof (struct deferred_write));
				/* save this directory entry for later use */
				dwpnt->s_entry = s_entry;
				/* set the initial padding to zero */
				dwpnt->pad = 0;
#ifdef DVD_VIDEO
				if (dvd_video && (title_set_info != 0)) {
					int pad;

					pad = DVDGetFilePad(title_set_info, s_entry->name);
					if (pad < 0) {
						errmsgno(EX_BAD,
						"Implementation botch. Video pad for file %s is %d\n",
						s_entry->name, pad),
						comerrno(EX_BAD,
						"Either the *.IFO file is bad or you found a genisoimage bug.\n");
					}
					dwpnt->pad = pad;
					if (verbose > 0 && pad != 0) {
						fprintf(stderr,
							"The pad was %d for file %s\n", dwpnt->pad, s_entry->name);
					}
				}
#endif /* DVD_VIDEO */
#ifdef APPLE_HYB
				/*
				 * maybe an offset to start of the real
				 * file/fork
				 */
				dwpnt->off = s_entry->hfs_off;
#else
				dwpnt->off = (off_t)0;
#endif	/* APPLE_HYB */
				if (dw_tail) {
					dw_tail->next = dwpnt;
					dw_tail = dwpnt;
				} else {
					dw_head = dwpnt;
					dw_tail = dwpnt;
				}
				if (s_entry->inode == TABLE_INODE) {
					dwpnt->table = s_entry->table;
					dwpnt->name = NULL;
					sprintf(whole_path, "%s%s%s",
						s_entry->filedir->whole_name,
						SPATH_SEPARATOR, trans_tbl);
				} else {
					dwpnt->table = NULL;
					strcpy(whole_path, s_entry->whole_name);
					dwpnt->name = strdup(whole_path);
				}
				dwpnt->next = NULL;
				dwpnt->size = s_entry->size;
				dwpnt->extent = last_extent;
				set_733((char *) s_entry->isorec.extent,
								last_extent);
				s_entry->starting_block = last_extent;
				add_hash(s_entry);
				last_extent += ISO_BLOCKS(s_entry->size);
#ifdef DVD_VIDEO
				/* Shouldn't we always add the pad info? */
				if (dvd_video) {
					last_extent += dwpnt->pad;
				}
#endif /* DVD_VIDEO */
				if (verbose > 2 && !do_sort) {
					fprintf(stderr, "%8d %8u %s\n",
						s_entry->starting_block,
						last_extent - 1, whole_path);
				}
#ifdef DBG_ISO
				if (ISO_BLOCKS(s_entry->size) > 500) {
					fprintf(stderr,
						"Warning: large file '%s'\n",
						whole_path);
					fprintf(stderr,
						"Starting block is %d\n",
						s_entry->starting_block);
					fprintf(stderr,
					"Reported file size is %lld\n",
						(Llong)s_entry->size);

				}
#endif
#ifdef	NOT_NEEDED	/* Never use this code if you like to create a DVD */

				if (last_extent > (800000000 >> 11)) {
					/* More than 800Mb? Punt */
					fprintf(stderr,
					"Extent overflow processing file '%s'\n",
						whole_path);
					fprintf(stderr,
						"Starting block is %d\n",
						s_entry->starting_block);
					fprintf(stderr,
					"Reported file size is %lld\n",
							(Llong)s_entry->size);
					exit(1);
				}
#endif
				continue;
			}
			/*
			 * This is for zero-length files.  If we leave the
			 * extent 0, then we get screwed, because many readers
			 * simply drop files that have an extent of zero.
			 * Thus we leave the size 0, and just assign the
			 * extent number.
			 */
			set_733((char *) s_entry->isorec.extent, last_extent);
		}
		if (dpnt->subdir) {
			if (assign_file_addresses(dpnt->subdir, TRUE))
				ret = TRUE;
		}
		dpnt = dpnt->next;
	}
#ifdef DVD_VIDEO
	if (title_set_info != NULL) {
		DVDFreeFileSet(title_set_info);
	}
	if (dvd_video && !ret && !isnest) {
		errmsgno(EX_BAD,
			"Could not find correct 'VIDEO_TS' directory.\n");
	}
#endif /* DVD_VIDEO */
	return (ret);
} /* assign_file_addresses(... */

static void
free_one_directory(struct directory *dpnt)
{
	struct directory_entry *s_entry;
	struct directory_entry *s_entry_d;

	s_entry = dpnt->contents;
	while (s_entry) {
		s_entry_d = s_entry;
		s_entry = s_entry->next;

		if (s_entry_d->rr_attributes) {
			free(s_entry_d->rr_attributes);
			s_entry_d->rr_attributes = NULL;
		}
		if (s_entry_d->name != NULL) {
			free(s_entry_d->name);
			s_entry_d->name = NULL;
		}
		if (s_entry_d->whole_name != NULL) {
			free(s_entry_d->whole_name);
			s_entry_d->whole_name = NULL;
		}
#ifdef APPLE_HYB
		if (apple_both && s_entry_d->hfs_ent && !s_entry_d->assoc)
			free(s_entry_d->hfs_ent);
#endif	/* APPLE_HYB */

		free(s_entry_d);
		s_entry_d = NULL;
	}
	dpnt->contents = NULL;
}/* free_one_directory(... */

static void
free_directories(struct directory *dpnt)
{
	while (dpnt) {
		free_one_directory(dpnt);
		if (dpnt->subdir)
			free_directories(dpnt->subdir);
		dpnt = dpnt->next;
	}
}

void
generate_one_directory(struct directory *dpnt, FILE *outfile)
{
	unsigned int	ce_address = 0;
	char		*ce_buffer;
	unsigned int	ce_index = 0;
	unsigned int	ce_size;
	unsigned int	dir_index;
	char		*directory_buffer;
	int		new_reclen;
	struct directory_entry *s_entry;
	struct directory_entry *s_entry_d;
	unsigned int	total_size;

	total_size = ISO_ROUND_UP(dpnt->size);
	directory_buffer = (char *) e_malloc(total_size);
	memset(directory_buffer, 0, total_size);
	dir_index = 0;

	ce_size = ISO_ROUND_UP(dpnt->ce_bytes);
	ce_buffer = NULL;

	if (ce_size > 0) {
		ce_buffer = (char *) e_malloc(ce_size);
		memset(ce_buffer, 0, ce_size);

		ce_index = 0;

		/* Absolute byte address of CE entries for this directory */
		ce_address = last_extent_written + (total_size >> 11);
		ce_address = ce_address << 11;
	}
	s_entry = dpnt->contents;
	while (s_entry) {
		/* skip if it's hidden */
		if (s_entry->de_flags & INHIBIT_ISO9660_ENTRY) {
			s_entry = s_entry->next;
			continue;
		}
		/*
		 * We do not allow directory entries to cross sector
		 * boundaries. Simply pad, and then start the next entry at
		 * the next sector
		 */
		new_reclen = s_entry->isorec.length[0];
		if ((dir_index & (SECTOR_SIZE - 1)) + new_reclen >=
								SECTOR_SIZE) {
			dir_index = ISO_ROUND_UP(dir_index);
		}
		memcpy(directory_buffer + dir_index, &s_entry->isorec,
			offsetof(struct iso_directory_record, name[0]) +
			s_entry->isorec.name_len[0]);
		dir_index += offsetof(struct iso_directory_record, name[0]) +
			s_entry->isorec.name_len[0];

		/* Add the Rock Ridge attributes, if present */
		if (s_entry->rr_attr_size) {
			if (dir_index & 1) {
				directory_buffer[dir_index++] = 0;
			}
			/*
			 * If the RR attributes were too long, then write the
			 * CE records, as required.
			 */
			if (s_entry->rr_attr_size != s_entry->total_rr_attr_size) {
				struct iso_xa_dir_record *xadp;
				unsigned char	*pnt;
				int		len,
						nbytes;

				/*
				 * Go through the entire record, first skip
				 * the XA record and then fix up the
				 * CE entries so that the extent and offset
				 * are correct
				 */
				pnt = s_entry->rr_attributes;
				len = s_entry->total_rr_attr_size;

				if (len >= 14) {
					xadp = (struct iso_xa_dir_record *)pnt;

					if (xadp->signature[0] == 'X' && xadp->signature[1] == 'A' &&
									xadp->reserved[0] == '\0') {
						len -= 14;
						pnt += 14;
					}
				}

				while (len > 3) {
#ifdef DEBUG
					if (ce_size <= 0) {
						fprintf(stderr,
						"Warning: ce_index(%d) && ce_address(%d) not initialized\n",
							ce_index, ce_address);
					}
#endif

					if (pnt[0] == 'C' && pnt[1] == 'E') {
						nbytes = get_733((char *) pnt + 20);

						if ((ce_index & (SECTOR_SIZE - 1)) + nbytes >=
							SECTOR_SIZE) {
							ce_index = ISO_ROUND_UP(ce_index);
						}
						set_733((char *) pnt + 4,
							(ce_address + ce_index) >> 11);
						set_733((char *) pnt + 12,
							(ce_address + ce_index) & (SECTOR_SIZE - 1));


						/*
						 * Now store the block in the
						 * ce buffer
						 */
						memcpy(ce_buffer + ce_index,
							pnt + pnt[2], nbytes);
						ce_index += nbytes;
						if (ce_index & 1) {
							ce_index++;
						}
					}
					len -= pnt[2];
					pnt += pnt[2];
				}

			}
			rockridge_size += s_entry->total_rr_attr_size;
			memcpy(directory_buffer + dir_index,
				s_entry->rr_attributes,
				s_entry->rr_attr_size);
			dir_index += s_entry->rr_attr_size;
		}
		if (dir_index & 1) {
			directory_buffer[dir_index++] = 0;
		}
		s_entry_d = s_entry;
		s_entry = s_entry->next;

		/*
		 * Joliet doesn't use the Rock Ridge attributes, so we free
		 * it here.
		 */
		if (s_entry_d->rr_attributes) {
			free(s_entry_d->rr_attributes);
			s_entry_d->rr_attributes = NULL;
		}
	}

	if (dpnt->size != dir_index) {
#ifdef	USE_LIBSCHILY
		errmsgno(EX_BAD,
			"Unexpected directory length %lld expected: %d '%s'\n",
			(Llong)dpnt->size,
			dir_index, dpnt->de_name);
#else
		fprintf(stderr,
			"Unexpected directory length %lld expected: %d '%s'\n",
			(Llong)dpnt->size,
			dir_index, dpnt->de_name);
#endif
	}
	jtwrite(directory_buffer, total_size, 1, 0, FALSE);
	xfwrite(directory_buffer, total_size, 1, outfile, 0, FALSE);
	last_extent_written += total_size >> 11;
	free(directory_buffer);
	directory_buffer = NULL;

	if (ce_size > 0) {
		if (ce_index != dpnt->ce_bytes) {
#ifdef	USE_LIBSCHILY
			errmsgno(EX_BAD,
			"Continuation entry record length mismatch %d expected: %d.\n",
				ce_index, dpnt->ce_bytes);
#else
			fprintf(stderr,
			"Continuation entry record length mismatch %d expected: %d.\n",
				ce_index, dpnt->ce_bytes);
#endif
		}
		jtwrite(ce_buffer, ce_size, 1, 0, FALSE);
		xfwrite(ce_buffer, ce_size, 1, outfile, 0, FALSE);
		last_extent_written += ce_size >> 11;
		free(ce_buffer);
		ce_buffer = NULL;
	}
}/* generate_one_directory(... */

static void
build_pathlist(struct directory *node)
{
	struct directory *dpnt;

	dpnt = node;

	while (dpnt) {
		/* skip if it's hidden */
		if ((dpnt->dir_flags & INHIBIT_ISO9660_ENTRY) == 0)
			pathlist[dpnt->path_index] = dpnt;

		if (dpnt->subdir)
			build_pathlist(dpnt->subdir);
		dpnt = dpnt->next;
	}
}/* build_pathlist(... */

static int
compare_paths(void const *r, void const *l)
{
	struct directory const *ll = *(struct directory * const *) l;
	struct directory const *rr = *(struct directory * const *) r;

	if (rr->parent->path_index < ll->parent->path_index) {
		return (-1);
	}
	if (rr->parent->path_index > ll->parent->path_index) {
		return (1);
	}
	return (strcmp(rr->self->isorec.name, ll->self->isorec.name));

}/* compare_paths(... */

static int
generate_path_tables()
{
	struct directory_entry *de = NULL;
	struct directory *dpnt;
	int		fix;
	int		i;
	int		j;
	int		namelen;
	char		*npnt;
	char		*npnt1;
	int		tablesize;

	/* First allocate memory for the tables and initialize the memory */
	tablesize = path_blocks << 11;
	path_table_m = (char *) e_malloc(tablesize);
	path_table_l = (char *) e_malloc(tablesize);
	memset(path_table_l, 0, tablesize);
	memset(path_table_m, 0, tablesize);

	/*
	 * Now start filling in the path tables.  Start with root directory
	 */

	path_table_index = 0;
	pathlist = (struct directory **) e_malloc(sizeof (struct directory *)
		* next_path_index);
	memset(pathlist, 0, sizeof (struct directory *) * next_path_index);
	build_pathlist(root);

	do {
		fix = 0;
#ifdef PROTOTYPES
		qsort(&pathlist[1], next_path_index - 1,
			sizeof (struct directory *),
			(int (*) (const void *, const void *)) compare_paths);
#else
		qsort(&pathlist[1], next_path_index - 1,
			sizeof (struct directory *),
			compare_paths);
#endif

		for (j = 1; j < next_path_index; j++) {
			if (pathlist[j]->path_index != j) {
				pathlist[j]->path_index = j;
				fix++;
			}
		}
	} while (fix);

	for (j = 1; j < next_path_index; j++) {
		dpnt = pathlist[j];
		if (!dpnt) {
#ifdef	USE_LIBSCHILY
			comerrno(EX_BAD, "Entry %d not in path tables\n", j);
#else
			fprintf(stderr, "Entry %d not in path tables\n", j);
			exit(1);
#endif
		}
		npnt = dpnt->de_name;

		/* So the root comes out OK */
		if ((*npnt == 0) || (dpnt == root)) {
			npnt = ".";
		}
		npnt1 = strrchr(npnt, PATH_SEPARATOR);
		if (npnt1) {
			npnt = npnt1 + 1;
		}
		de = dpnt->self;
		if (!de) {
#ifdef	USE_LIBSCHILY
			comerrno(EX_BAD,
			"Fatal ISO9660 goof - directory has amnesia\n");
#else
			fprintf(stderr,
			"Fatal ISO9660 goof - directory has amnesia\n");
			exit(1);
#endif
		}
		namelen = de->isorec.name_len[0];

		path_table_l[path_table_index] = namelen;
		path_table_m[path_table_index] = namelen;
		path_table_index += 2;

		set_731(path_table_l + path_table_index, dpnt->extent);
		set_732(path_table_m + path_table_index, dpnt->extent);
		path_table_index += 4;

		if (dpnt->parent->path_index > 0xffff) {
#ifdef	USE_LIBSCHILY
			comerrno(EX_BAD,
			"Unable to generate sane path tables - too many directories (%d)\n",
				dpnt->parent->path_index);
#else
			fprintf(stderr,
			"Unable to generate sane path tables - too many directories (%d)\n",
				dpnt->parent->path_index);
			exit(1);
#endif
		}

		set_721(path_table_l + path_table_index,
			dpnt->parent->path_index);
		set_722(path_table_m + path_table_index,
			dpnt->parent->path_index);
		path_table_index += 2;

		for (i = 0; i < namelen; i++) {
			path_table_l[path_table_index] = de->isorec.name[i];
			path_table_m[path_table_index] = de->isorec.name[i];
			path_table_index++;
		}
		if (path_table_index & 1) {
			path_table_index++;	/* For odd lengths we pad */
		}
	}

	free(pathlist);
	pathlist = NULL;
	if (path_table_index != path_table_size) {
#ifdef	USE_LIBSCHILY
		errmsgno(EX_BAD,
			"Path table lengths do not match %d expected: %d\n",
			path_table_index,
			path_table_size);
#else
		fprintf(stderr,
			"Path table lengths do not match %d expected: %d\n",
			path_table_index,
			path_table_size);
#endif
	}
	return (0);
}/* generate_path_tables(... */

void
memcpy_max(char *to, char *from, int max)
{
	int	n = strlen(from);

	if (n > max) {
		n = max;
	}
	memcpy(to, from, n);

}/* memcpy_max(... */

void
outputlist_insert(struct output_fragment *frag)
{
	struct output_fragment *nfrag;

	nfrag = e_malloc(sizeof (*frag));
	movebytes(frag, nfrag, sizeof (*frag));
	nfrag->of_start_extent = 0;

	if (out_tail == NULL) {
		out_list = out_tail = nfrag;
	} else {
		out_tail->of_next = nfrag;
		out_tail = nfrag;
	}
}

static int
file_write(FILE *outfile)
{
	Uint	should_write;

#ifdef APPLE_HYB
	char	buffer[SECTOR_SIZE];

	memset(buffer, 0, sizeof (buffer));

	if (apple_hyb) {

		int	i;

		/*
		 * write out padding to round up to HFS allocation block
		 */
		for (i = 0; i < hfs_pad; i++) {
			jtwrite(buffer, sizeof (buffer), 1, 0, FALSE);
			xfwrite(buffer, sizeof (buffer), 1, outfile, 0, FALSE);
			last_extent_written++;
		}
	}
#endif	/* APPLE_HYB */

	/*
	 * OK, all done with that crap.  Now write out the directories. This is
	 * where the fur starts to fly, because we need to keep track of each
	 * file as we find it and keep track of where we put it.
	 */
	should_write = last_extent - session_start;

	if (verbose > 2) {
#ifdef DBG_ISO
		fprintf(stderr,
			"Total directory extents being written = %d\n",
							last_extent);
#endif

#ifdef APPLE_HYB
		if (apple_hyb)
			fprintf(stderr,
			"Total extents scheduled to be written (inc HFS) = %d\n",
				last_extent - session_start);
		else
#endif	/* APPLE_HYB */

			fprintf(stderr,
				"Total extents scheduled to be written = %u\n",
				last_extent - session_start);
	}
	/* Now write all of the files that we need. */
	write_files(outfile);

#ifdef APPLE_HYB
	/* write out extents/catalog/dt file */
	if (apple_hyb) {

		jtwrite(hce->hfs_ce, HFS_BLOCKSZ, hce->hfs_tot_size, 0, FALSE);
		xfwrite(hce->hfs_ce, HFS_BLOCKSZ, hce->hfs_tot_size, outfile, 0, FALSE);

		/* round up to a whole CD block */
		if (HFS_ROUND_UP(hce->hfs_tot_size) -
					hce->hfs_tot_size * HFS_BLOCKSZ) {
			jtwrite(buffer,
				HFS_ROUND_UP(hce->hfs_tot_size) -
				hce->hfs_tot_size * HFS_BLOCKSZ, 1, 0, FALSE);
			xfwrite(buffer,
				HFS_ROUND_UP(hce->hfs_tot_size) -
				hce->hfs_tot_size * HFS_BLOCKSZ, 1, outfile, 0, FALSE);
		}
		last_extent_written += ISO_ROUND_UP(hce->hfs_tot_size *
						HFS_BLOCKSZ) / SECTOR_SIZE;

		/* write out HFS boot block */
		if (mac_boot.name)
			write_one_file(mac_boot.name, mac_boot.size, outfile,
								mac_boot.off);
	}
#endif	/* APPLE_HYB */

	/* The rest is just fluff. */
	if (verbose == 0) {
		return (0);
	}
#ifdef APPLE_HYB
	if (apple_hyb) {
		fprintf(stderr,
			"Total extents actually written (inc HFS) = %d\n",
			last_extent_written - session_start);
		fprintf(stderr, "(Size of ISO volume = %d, HFS extra = %d)\n",
			last_extent_written - session_start - hfs_extra,
			hfs_extra);
	} else
#else
	fprintf(stderr, "Total extents actually written = %d\n",
		last_extent_written - session_start);
#endif	/* APPLE_HYB */

	/* Hard links throw us off here */
	if (should_write != (last_extent - session_start)) {
		fprintf(stderr,
		"Number of extents written not what was predicted.  Please fix.\n");
		fprintf(stderr, "Predicted = %d, written = %d\n",
						should_write, last_extent);
	}
	fprintf(stderr, "Total translation table size: %d\n", table_size);
	fprintf(stderr, "Total rockridge attributes bytes: %d\n",
						rockridge_size);
	fprintf(stderr, "Total directory bytes: %d\n", total_dir_size);
	fprintf(stderr, "Path table size(bytes): %d\n", path_table_size);

#ifdef DEBUG
	fprintf(stderr,
		"next extent, last_extent, last_extent_written %d %d %d\n",
		next_extent, last_extent, last_extent_written);
#endif

	return (0);

}/* iso_write(... */

/*
 * Function to write the PVD for the disc.
 */
static int
pvd_write(FILE *outfile)
{
	char		iso_time[17];
	int		should_write;
	struct tm	local;
	struct tm	gmt;


	time(&begun);

	local = *localtime(&begun);
	gmt = *gmtime(&begun);

	/*
	 * There was a comment here about breaking in the year 2000.
	 * That's not true, in 2000 tm_year == 100, so 1900+tm_year == 2000.
	 */
	sprintf(iso_time, "%4.4d%2.2d%2.2d%2.2d%2.2d%2.2d00",
		1900 + local.tm_year,
		local.tm_mon + 1, local.tm_mday,
		local.tm_hour, local.tm_min, local.tm_sec);

	local.tm_min -= gmt.tm_min;
	local.tm_hour -= gmt.tm_hour;
	local.tm_yday -= gmt.tm_yday;
	if (local.tm_yday < -2)		/* Hit new-year limit	*/
		local.tm_yday = 1;	/* Local is GMT + 1 day	*/
	iso_time[16] = (local.tm_min + 60 *
				(local.tm_hour + 24 * local.tm_yday)) / 15;

	/* Next we write out the primary descriptor for the disc */
	memset(&vol_desc, 0, sizeof (vol_desc));
	vol_desc.type[0] = ISO_VD_PRIMARY;
	memcpy(vol_desc.id, ISO_STANDARD_ID, sizeof (ISO_STANDARD_ID));
	vol_desc.version[0] = 1;

	memset(vol_desc.system_id, ' ', sizeof (vol_desc.system_id));
	memcpy_max(vol_desc.system_id, system_id, strlen(system_id));

	memset(vol_desc.volume_id, ' ', sizeof (vol_desc.volume_id));
	memcpy_max(vol_desc.volume_id, volume_id, strlen(volume_id));

	should_write = last_extent - session_start;
	set_733((char *) vol_desc.volume_space_size, should_write);
	set_723(vol_desc.volume_set_size, volume_set_size);
	set_723(vol_desc.volume_sequence_number, volume_sequence_number);
	set_723(vol_desc.logical_block_size, SECTOR_SIZE);

	/*
	 * The path tables are used by DOS based machines to cache directory
	 * locations
	 */
	set_733((char *) vol_desc.path_table_size, path_table_size);
	set_731(vol_desc.type_l_path_table, path_table[0]);
	set_731(vol_desc.opt_type_l_path_table, path_table[1]);
	set_732(vol_desc.type_m_path_table, path_table[2]);
	set_732(vol_desc.opt_type_m_path_table, path_table[3]);

	/* Now we copy the actual root directory record */
	memcpy(vol_desc.root_directory_record, &root_record,
		offsetof(struct iso_directory_record, name[0]) + 1);

	/*
	 * The rest is just fluff.  It looks nice to fill in many of these
	 * fields, though.
	 */
	FILL_SPACE(volume_set_id);
	if (volset_id)
		memcpy_max(vol_desc.volume_set_id, volset_id, strlen(volset_id));

	FILL_SPACE(publisher_id);
	if (publisher)
		memcpy_max(vol_desc.publisher_id, publisher, strlen(publisher));

	FILL_SPACE(preparer_id);
	if (preparer)
		memcpy_max(vol_desc.preparer_id, preparer, strlen(preparer));

	FILL_SPACE(application_id);
	if (appid)
		memcpy_max(vol_desc.application_id, appid, strlen(appid));

	FILL_SPACE(copyright_file_id);
	if (copyright)
		memcpy_max(vol_desc.copyright_file_id, copyright,
			strlen(copyright));

	FILL_SPACE(abstract_file_id);
	if (abstract)
		memcpy_max(vol_desc.abstract_file_id, abstract,
			strlen(abstract));

	FILL_SPACE(bibliographic_file_id);
	if (biblio)
		memcpy_max(vol_desc.bibliographic_file_id, biblio,
			strlen(biblio));

	FILL_SPACE(creation_date);
	FILL_SPACE(modification_date);
	FILL_SPACE(expiration_date);
	FILL_SPACE(effective_date);
	vol_desc.file_structure_version[0] = 1;
	FILL_SPACE(application_data);

	memcpy(vol_desc.creation_date, iso_time, 17);
	memcpy(vol_desc.modification_date, iso_time, 17);
	memcpy(vol_desc.expiration_date, "0000000000000000", 17);
	memcpy(vol_desc.effective_date, iso_time, 17);

	if (use_XA) {
		char	*xap = &((char *)&vol_desc)[1024];

		memcpy(&xap[0], "CD-XA001", 8);			/* XA Sign.  */
		memcpy(&xap[8], "\0\0", 2);			/* XA flags  */
		memcpy(&xap[10], "\0\0\0\0\0\0\0\0", 8);	/* Start dir */
		memcpy(&xap[18], "\0\0\0\0\0\0\0\0", 8);	/* Reserved  */
	}

	/* if not a bootable cd do it the old way */
	jtwrite(&vol_desc, SECTOR_SIZE, 1, 0, FALSE);
	xfwrite(&vol_desc, SECTOR_SIZE, 1, outfile, 0, FALSE);
	last_extent_written++;
	return (0);
}

/*
 * Function to write the Extended PVD for the disc.
 */
static int
xpvd_write(FILE *outfile)
{
	vol_desc.type[0] = ISO_VD_SUPPLEMENTARY;
	vol_desc.version[0] = 2;
	vol_desc.file_structure_version[0] = 2;

	/* if not a bootable cd do it the old way */
	jtwrite(&vol_desc, SECTOR_SIZE, 1, 0, FALSE);
	xfwrite(&vol_desc, SECTOR_SIZE, 1, outfile, 0, FALSE);
	last_extent_written++;
	return (0);
}

/*
 * Function to write the EVD for the disc.
 */
static int
evd_write(FILE *outfile)
{
	struct iso_primary_descriptor evol_desc;

	/*
	 * Now write the end volume descriptor.  Much simpler than the other
	 * one
	 */
	memset(&evol_desc, 0, sizeof (evol_desc));
	evol_desc.type[0] = (unsigned char) ISO_VD_END;
	memcpy(evol_desc.id, ISO_STANDARD_ID, sizeof (ISO_STANDARD_ID));
	evol_desc.version[0] = 1;
	jtwrite(&evol_desc, SECTOR_SIZE, 1, 0, TRUE);
	xfwrite(&evol_desc, SECTOR_SIZE, 1, outfile, 0, TRUE);
	last_extent_written += 1;
	return (0);
}

/*
 * Function to write the version information for the disc.
 */
static int
vers_write(FILE *outfile)
{
	char		vers[SECTOR_SIZE+1];
	int		X_ac;
	char		**X_av;
	char		*cp;
	int		i;
	int		idx = 4;
	int		len;
	extern char	version_string[];
	extern int	path_ind;

	/* Now write the version descriptor. */
	memset(vers, 0, sizeof (vers));
	strcpy(vers, "MKI ");

	cp = vers;
	X_ac = saved_ac();
	X_av = saved_av();
	strcpy(&cp[idx], ctime(&begun));
	idx += 25;
	strcpy(&cp[idx], version_string);
	idx += strlen(version_string);
	for (i = 1; i < X_ac; i++) {
		len = strlen(X_av[i]);
		if ((idx + len + 2) >= SECTOR_SIZE)
			break;
		cp[idx++] = ' ';
		/*
		 * Do not give away secret information when not in debug mode.
		 */
		if (debug)
			strcpy(&cp[idx], X_av[i]);
		else if (i >= path_ind)
			len = graftcp(&cp[idx], X_av[i], &vers[SECTOR_SIZE-1]);
		else if (X_av[i][0] == '/')
			len = pathcp(&cp[idx], X_av[i], &vers[SECTOR_SIZE-1]);
		else
			strcpy(&cp[idx], X_av[i]);
		idx += len;
	}

	cp[SECTOR_SIZE - 1] = '\0';
 	/* Per default: keep privacy. Blackout the version and arguments. */
	if(getenv("ISODEBUG")) {
		jtwrite(vers, SECTOR_SIZE, 1, 0, TRUE);
		xfwrite(vers, SECTOR_SIZE, 1, outfile, 0, TRUE);
	} else {
		jtwrite(calloc(SECTOR_SIZE, 1), SECTOR_SIZE, 1, 0, TRUE);
		xfwrite(calloc(SECTOR_SIZE, 1), SECTOR_SIZE, 1, outfile, 0, TRUE);
	}
    last_extent_written += 1;
	return (0);
}

/*
 * Avoid to write unwanted information into the version info string.
 */
static int
graftcp(char *to, char *from, char *ep)
{
	int	len = strlen(from);
	char	*node = NULL;

	if (use_graft_ptrs)
		node = findgequal(from);

	if (node == NULL) {
		len = 0;
		node = from;
	} else {
		len = node - from;
		*node = '\0';
		strncpy(to, from, ep - to);
		*node++ = '=';
		to += len++;
		*to++ = '=';
	}
	return (len + pathcp(to, node, ep));
}

static int
pathcp(char *to, char *from, char *ep)
{
	int	len = strlen(from);
	char	*p;

	p = strrchr(from, '/');
	if (p == NULL) {
		strncpy(to, from, ep - to);
	} else {
		if (p[1] == '\0') {
			--p;
			while (p > from && *p != '/')
				--p;
		}
		len = 0;
		if (*p == '/') {
			strncpy(to, "...", ep - to);
			to += 3;
			len = 3;
		}
		if (to < ep) {
			strncpy(to, p, ep - to);
			len += strlen(to);
		}
	}
	return (len);
}


/*
 * Function to write the path table for the disc.
 */
static int
pathtab_write(FILE *outfile)
{
	/* Next we write the path tables */
	jtwrite(path_table_l, path_blocks << 11, 1, 0, FALSE);
	xfwrite(path_table_l, path_blocks << 11, 1, outfile, 0, FALSE);
	last_extent_written += path_blocks;
	jtwrite(path_table_m, path_blocks << 11, 1, 0, FALSE);
	xfwrite(path_table_m, path_blocks << 11, 1, outfile, 0, FALSE);
	last_extent_written += path_blocks;
	free(path_table_l);
	free(path_table_m);
	path_table_l = NULL;
	path_table_m = NULL;
	return (0);
}

static int
exten_write(FILE *outfile)
{
	jtwrite(extension_record, SECTOR_SIZE, 1, 0, FALSE);
	xfwrite(extension_record, SECTOR_SIZE, 1, outfile, 0, FALSE);
	last_extent_written++;
	return (0);
}

/*
 * Functions to describe padding block at the start of the disc.
 */
int
oneblock_size(int starting_extent)
{
	last_extent++;
	return (0);
}

/*
 * Functions to describe path table size.
 */
static int
pathtab_size(int starting_extent)
{
	path_table[0] = starting_extent;

	path_table[1] = 0;
	path_table[2] = path_table[0] + path_blocks;
	path_table[3] = 0;
	last_extent += 2 * path_blocks;
	return (0);
}

/*
 * Functions to describe padding blocks before PVD.
 */
static int
startpad_size(int starting_extent)
{
	last_extent = session_start + 16;
	return (0);
}

/*
 * Functions to describe padding blocks between sections.
 */
static int
interpad_size(int starting_extent)
{
	int	emod = 0;

#ifdef	needed
	starting_extent += 16;	/* First add 16 pad blocks */
#endif
	if ((emod = starting_extent % 16) != 0) {
		starting_extent += 16 - emod;	/* Now pad to mod 16 #	   */
	}
	last_extent = starting_extent;
	return (0);
}

/*
 * Functions to describe padding blocks at end of disk.
 */
static int
endpad_size(int starting_extent)
{
	starting_extent += 150;			/* 150 pad blocks (post gap) */
	last_extent = starting_extent;
	return (0);
}

static int
file_gen()
{
#ifdef APPLE_HYB
	int	start_extent = last_extent;	/* orig ISO files start */

#endif	/* APPLE_HYB */

	if (!assign_file_addresses(root, FALSE)) {
#ifdef DVD_VIDEO
		if (dvd_video) {
			comerrno(EX_BAD, "Unable to make a DVD-Video image.\n"
            "Possible reasons:\n"
            "  - VIDEO_TS subdirectory was not found on specified location\n"
            "  - VIDEO_TS has invalid contents\n"
            );
		}
#else
		;	/* EMPTY */
#endif
	}


#ifdef SORTING
	if (do_sort) {
		if (sort_file_addresses() == 0)
			reassign_link_addresses(root);
	}
#endif /* SORTING */

#ifdef APPLE_HYB
	/*
	 * put this here for the time being - may when I've worked out how to
	 * use Eric's new system for creating/writing parts of the image it
	 * may move to it's own routine
	 */
	if (apple_hyb)
		hfs_file_gen(start_extent);
#ifdef PREP_BOOT
	else if (use_prep_boot || use_chrp_boot)
		gen_prepboot();
#endif	/* PREP_BOOT */
#endif	/* APPLE_HYB */

	return (0);
}

static int
dirtree_dump()
{
	if (verbose > 2) {
		dump_tree(root);
	}
	return (0);
}

static int
dirtree_fixup(int starting_extent)
{
	if (use_RockRidge && reloc_dir)
		finish_cl_pl_entries();

	if (use_RockRidge)
		update_nlink_field(root);
	return (0);
}

static int
dirtree_size(int starting_extent)
{
	assign_directory_addresses(root);
	return (0);
}

static int
ext_size(int starting_extent)
{
	extern int		extension_record_size;
	struct directory_entry *s_entry;

	extension_record_extent = starting_extent;
	s_entry = root->contents;
	set_733((char *) s_entry->rr_attributes + s_entry->rr_attr_size - 24,
		extension_record_extent);
	set_733((char *) s_entry->rr_attributes + s_entry->rr_attr_size - 8,
		extension_record_size);
	last_extent++;
	return (0);
}

static int
dirtree_write(FILE *outfile)
{
	generate_iso9660_directories(root, outfile);
	return (0);
}

static int
dirtree_cleanup(FILE *outfile)
{
	free_directories(root);
	return (0);
}

static int
startpad_write(FILE *outfile)
{
	char	buffer[SECTOR_SIZE];
	int	i;
	int	npad;

	memset(buffer, 0, sizeof (buffer));

	npad = session_start + 16 - last_extent_written;

	for (i = 0; i < npad; i++) {
		jtwrite(buffer, sizeof (buffer), 1, 0, FALSE);
		xfwrite(buffer, sizeof (buffer), 1, outfile, 0, FALSE);
		last_extent_written++;
	}

	return (0);
}

static int
interpad_write(FILE *outfile)
{
	char	buffer[SECTOR_SIZE];
	int	i;
	int	npad = 0;

	memset(buffer, 0, sizeof (buffer));

#ifdef	needed
	npad = 16;
#endif
	if ((i = last_extent_written % 16) != 0)
		npad += 16 - i;

	for (i = 0; i < npad; i++) {
		jtwrite(buffer, sizeof (buffer), 1, 0, FALSE);
		xfwrite(buffer, sizeof (buffer), 1, outfile, 0, FALSE);
		last_extent_written++;
	}

	return (0);
}

static int
endpad_write(FILE *outfile)
{
	char	buffer[SECTOR_SIZE];
	int	i;

	memset(buffer, 0, sizeof (buffer));

	for (i = 0; i < 150; i++) {
		jtwrite(buffer, sizeof (buffer), 1, 0, FALSE);
		xfwrite(buffer, sizeof (buffer), 1, outfile, 0, FALSE);
		last_extent_written++;
	}

	return (0);
}

#ifdef APPLE_HYB

/*
 *	hfs_get_parms:	get HFS parameters from the command line
 */

static int
hfs_get_parms(char *key)
{
	int	ret = 0;
	char	*p;

	if (hfs_parms == NULL)
		return (ret);

	if ((p = strstr(hfs_parms, key)) != NULL) {
		p += strlen(key) + 1;
		sscanf(p, "%d", &ret);
	}

	return (ret);
}

/*
 *	hfs_file_gen:	set up "fake" HFS volume using the ISO9660 tree
 */
static void
hfs_file_gen(int start_extent)
{
	int	Csize;	/* clump size for HFS vol */
	int	loop;
	int	last_extent_save = last_extent;
	char	*p;

	/* allocate memory for the libhfs/genisoimage extra info */
	hce = (hce_mem *) e_malloc(sizeof (hce_mem));

	hce->error = (char *) e_malloc(1024);

	/* mark as unallocated for use later */
	hce->hfs_ce = hce->hfs_hdr = hce->hfs_map = 0;

	/* reserve space for the label partition - if it is needed */
#ifdef PREP_BOOT
	/* a PReP bootable partition needs the map.. */
	if (gen_pt || use_prep_boot || use_chrp_boot)
#else
	if (gen_pt)
#endif	/* PREP_BOOT */
		hce->hfs_map_size = HFS_MAP_SIZE;
	else
		hce->hfs_map_size = 0;

	/* set the HFS parameter string to upper case */
	if (hfs_parms) {
		for (p = hfs_parms; *p; p++)
			*p = toupper(*p);
	}

	/* set the initial factor to increase Catalog file size */
	if ((hce->ctc_size = hfs_get_parms("CTC")) == 0)
		hce->ctc_size = CTC;

	/* set the max size of the Catalog file */
	if ((hce->max_XTCsize = hfs_get_parms("MAX_XTCSIZE")) == 0)
		hce->max_XTCsize = MAX_XTCSIZE;

	/* set the number of time to try to make an HFS volume */
	if ((loop = hfs_get_parms("CTC_LOOP")) == 0)
		loop = CTC_LOOP;

	/*
	 * "create" the HFS volume (just the header, catalog/extents files) if
	 * there's a problem with the Catalog file being too small, we keep on
	 * increasing the size (up to CTC_LOOP) times and try again.
	 * Unfortunately I don't know enough about the inner workings of HFS,
	 * so I can't workout the size of the Catalog file in advance (and I
	 * don't want to "grow" as is is normally allowed to), therefore, this
	 * approach is a bit over the top as it involves throwing away the
	 * "volume" we have created and trying again ...
	 */
	do {
		hce->error[0] = '\0';

		/* attempt to create the Mac volume */
		Csize = make_mac_volume(root, start_extent);

		/* if we have a problem ... */
		if (Csize < 0) {
			/*
			 * we've made too many attempts, or got some other
			 * error
			 */
			if (loop == 0 || errno != HCE_ERROR) {
				/* HCE_ERROR is not a valid errno value */
				if (errno == HCE_ERROR)
					errno = 0;

				/* exit with the error */
				if (*hce->error)
					fprintf(stderr, "%s\n", hce->error);
				perr(hfs_error);
			} else {
				/* increase Catalog file size factor */
				hce->ctc_size *= CTC;

				/*
				 * reset the initial "last_extent" and try
				 * again
				 */
				last_extent = last_extent_save;
			}
		} else {
			/* everything OK - just carry on ... */
			loop = 0;
		}
	}
	while (loop--);

	hfs_extra = HFS_ROUND_UP(hce->hfs_tot_size) / SECTOR_SIZE;

	last_extent += hfs_extra;

	/* generate the Mac label and HFS partition maps */
	mac_boot.name = hfs_boot_file;

	/*
	 * only generate the partition tables etc. if we are making a bootable
	 * CD - or if the -part option is given
	 */
	if (gen_pt) {
		if (gen_mac_label(&mac_boot)) {
			if (*hce->error)
				fprintf(stderr, "%s\n", hce->error);
			perr(hfs_error);
		}
	}
	/* set Autostart filename if required */
	if (autoname) {
		if (autostart())
			perr("Autostart filename must less than 12 characters");
	}
	/* finished with any HFS type errors */
	free(hce->error);
	hce->error = 0;

	/*
	 * the ISO files need to start on a multiple of the HFS allocation
	 * blocks, so find out how much padding we need
	 */

	/*
	 * take in accout alignment of files wrt HFS volume start - remove any
	 * previous session as well
	 */
	start_extent -= session_start;
	hfs_pad = ROUND_UP(start_extent*SECTOR_SIZE +
			(hce->hfs_hdr_size + hce->hfs_map_size) * HFS_BLOCKSZ,
							Csize) / SECTOR_SIZE;

	hfs_pad -= (start_extent + (hce->hfs_hdr_size + hce->hfs_map_size) /
							HFS_BLK_CONV);

#ifdef PREP_BOOT
	gen_prepboot_label(hce->hfs_map);
#endif	/* PREP_BOOT */

}

#ifdef PREP_BOOT
static void
gen_prepboot()
{
	/*
	 * we need to allocate the hce struct since hce->hfs_map is used to
	 * generate the fdisk partition map required for PReP booting
	 */
	hce = (hce_mem *) e_malloc(sizeof (hce_mem));

	/* mark as unallocated for use later */
	hce->hfs_ce = hce->hfs_hdr = hce->hfs_map = 0;

	/* reserve space for the label partition - if it is needed */
	hce->hfs_map_size = HFS_MAP_SIZE;

	hce->hfs_map = (unsigned char *) e_malloc(hce->hfs_map_size * HFS_BLOCKSZ);
	gen_prepboot_label(hce->hfs_map);
}

#endif	/* PREP_BOOT */

/*
 *	get_adj_size:	get the ajusted size of the volume with the HFS
 *			allocation block size for each file
 */
Ulong
get_adj_size(int Csize)
{
	struct deferred_write *dw;
	Ulong		size = 0;
	int		count = 0;

	/* loop through all the files finding the new total size */
	for (dw = dw_head; dw; dw = dw->next) {
		size += (ROUND_UP(dw->size, Csize)/HFS_BLOCKSZ);
		count++;
	}

	/*
	 * crude attempt to prevent overflows - HFS can only cope with a
	 * maximum of about 65536 forks (actually less) - this will trap cases
	 * when we have far too many files
	 */

	if (count >= 65536)
		return (-1);
	else
		return (size);
}

/*
 *	adj_size:	adjust the ISO record entries for all files
 *			based on the HFS allocation block size
 */
int
adj_size(int Csize, int start_extent, int extra)
{
	struct deferred_write *dw;
	struct directory_entry *s_entry;
	int		size;

	/* get the adjusted start_extent (with padding) */
	/* take in accout alignment of files wrt HFS volume start */

	start_extent -= session_start;

	start_extent = ROUND_UP(start_extent*SECTOR_SIZE + extra*HFS_BLOCKSZ,
						Csize) / SECTOR_SIZE;

	start_extent -= (extra / HFS_BLK_CONV);

	start_extent += session_start;

	/* initialise file hash */
	flush_hash();

	/*
	 * loop through all files changing their starting blocks and finding
	 * any padding needed to written out latter
	 */
	for (dw = dw_head; dw; dw = dw->next) {
		s_entry = dw->s_entry;
		s_entry->starting_block = dw->extent = start_extent;
		set_733((char *) s_entry->isorec.extent, start_extent);
		size = ROUND_UP(dw->size, Csize) / SECTOR_SIZE;
		dw->pad = size - ISO_ROUND_UP(dw->size) / SECTOR_SIZE;

		/*
		 * cache non-HFS files - as there may be multiple links to
		 * these files (HFS files can't have multiple links). We will
		 * need to change the starting extent of the other links later
		 */
		if (!s_entry->hfs_ent)
			add_hash(s_entry);

		start_extent += size;
	}

	return (start_extent);
}

/*
 *	adj_size_other:	adjust any non-HFS files that may be linked
 *			to an existing file (i.e. not have a deferred_write
 *			entry of it's own
 */
void
adj_size_other(struct directory *dpnt)
{
	struct directory_entry *s_entry;
	struct file_hash *s_hash;

	while (dpnt) {
		s_entry = dpnt->contents;
		for (s_entry = dpnt->contents; s_entry;
						s_entry = s_entry->next) {
			/*
			 * if it's an HFS file or a directory - then ignore
			 * (we're after non-HFS files)
			 */
			if (s_entry->hfs_ent ||
			    (s_entry->isorec.flags[0] & ISO_DIRECTORY))
				continue;

			/*
			 * find any cached entry and assign new starting
			 * extent
			 */
			s_hash = find_hash(s_entry->dev, s_entry->inode);
			if (s_hash) {
				set_733((char *) s_entry->isorec.extent,
						s_hash->starting_block);
				/* not vital - but tidy */
				s_entry->starting_block =
							s_hash->starting_block;
			}
		}
		if (dpnt->subdir) {
			adj_size_other(dpnt->subdir);
		}
		dpnt = dpnt->next;
	}

	/* clear file hash */
	flush_hash();
}

/*
 *	hfs_hce_write:	write out the HFS header stuff
 */
static int
hfs_hce_write(FILE *outfile)
{
	char	buffer[SECTOR_SIZE];
	int	n = 0;
	int	r;	/* HFS hdr output */
	int	tot_size = hce->hfs_map_size + hce->hfs_hdr_size;

	memset(buffer, 0, sizeof (buffer));

	/*
	 * hack time ... if the tot_size is greater than 32Kb then
	 * it won't fit in the first 16 blank SECTORS (64 512 byte
	 * blocks, as most of this is padding, we just truncate this
	 * data to 64x4xHFS_BLOCKSZ ... hope this is OK ...
	 */

	if (tot_size > 64) tot_size = 64;

	/* get size in CD blocks == 4xHFS_BLOCKSZ == 2048 */
	n = tot_size / HFS_BLK_CONV;
	r = tot_size % HFS_BLK_CONV;

	/* write out HFS volume header info */
	jtwrite(hce->hfs_map, HFS_BLOCKSZ, tot_size, 0, FALSE);
	xfwrite(hce->hfs_map, HFS_BLOCKSZ, tot_size, outfile, 0, FALSE);

	/* fill up to a complete CD block */
	if (r) {
		jtwrite(buffer, HFS_BLOCKSZ, HFS_BLK_CONV - r, 0, FALSE);
		xfwrite(buffer, HFS_BLOCKSZ, HFS_BLK_CONV - r, outfile, 0, FALSE);
		n++;
	}
	last_extent_written += n;
	return (0);
}

/*
 *	insert_padding_file : insert a dumy file to make volume at least
 *				800k
 *
 *	XXX If we ever need to write more then 2 GB, make size off_t
 */
int
insert_padding_file(int size)
{
	struct deferred_write *dwpnt;

	/* get the size in bytes */
	size *= HFS_BLOCKSZ;

	dwpnt = (struct deferred_write *)
		e_malloc(sizeof (struct deferred_write));
	dwpnt->s_entry = 0;
	/* set the padding to zero */
	dwpnt->pad = 0;
	/* set offset to zero */
	dwpnt->off = (off_t)0;

	/*
	 * don't need to wory about the s_entry stuff as it won't be touched#
	 * at this point onwards
	 */

	/* insert the entry in the list */
	if (dw_tail) {
		dw_tail->next = dwpnt;
		dw_tail = dwpnt;
	} else {
		dw_head = dwpnt;
		dw_tail = dwpnt;
	}

	/* aloocate memory as a "Table" file */
	dwpnt->table = e_malloc(size);
	dwpnt->name = NULL;

	dwpnt->next = NULL;
	dwpnt->size = size;
	dwpnt->extent = last_extent;
	last_extent += ISO_BLOCKS(size);

	/* retune the size in HFS blocks */
	return (ISO_ROUND_UP(size) / HFS_BLOCKSZ);
}

struct output_fragment hfs_desc		= {NULL, NULL, NULL, hfs_hce_write, "HFS volume header"};

#endif	/* APPLE_HYB */

struct output_fragment startpad_desc	= {NULL, startpad_size,	NULL,	  startpad_write, "Initial Padblock"};
struct output_fragment voldesc_desc	= {NULL, oneblock_size,	root_gen, pvd_write,	  "Primary Volume Descriptor"};
struct output_fragment xvoldesc_desc	= {NULL, oneblock_size,	NULL,	  xpvd_write,	  "Enhanced Volume Descriptor"};
struct output_fragment end_vol		= {NULL, oneblock_size,	NULL,	  evd_write,	  "End Volume Descriptor" };
struct output_fragment version_desc	= {NULL, oneblock_size,	NULL,	  vers_write,	  "Version block" };
struct output_fragment pathtable_desc	= {NULL, pathtab_size,	generate_path_tables, pathtab_write, "Path table"};
struct output_fragment dirtree_desc	= {NULL, dirtree_size,	NULL,	  dirtree_write,  "Directory tree" };
struct output_fragment dirtree_clean	= {NULL, dirtree_fixup,	dirtree_dump, dirtree_cleanup, "Directory tree cleanup" };
struct output_fragment extension_desc	= {NULL, ext_size,	NULL,	  exten_write,	  "Extension record" };
struct output_fragment files_desc	= {NULL, NULL,		file_gen, file_write,	  "The File(s)"};
struct output_fragment interpad_desc	= {NULL, interpad_size,	NULL,	  interpad_write, "Intermediate Padblock"};
struct output_fragment endpad_desc	= {NULL, endpad_size,	NULL,	  endpad_write,	  "Ending Padblock"};
