/*
 * Program boot-alpha.c - Handle Linux alpha boot extensions to iso9660.
 *
 * Written by Steve McIntyre <steve@einval.com> June 2004
 *
 * Heavily inspired by isomarkboot by David Mosberger in 1996.
 *
 * Copyright 2004 Steve McIntyre
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
#include "mkisofs.h"
#include <fctldefs.h>
#include <utypes.h>
#include <intcvt.h>
#include "match.h"
#include "diskmbr.h"
#include "bootinfo.h"
#include <schily.h>
#include "endian.h"

        int     add_boot_alpha_filename   __PR((char *filename));
static  int     boot_alpha_write          __PR((FILE *outfile));
static  char   *boot_file_name = NULL;

#define BOOT_STRING "Linux/Alpha aboot for ISO filesystem."

/* Simple function: store the filename to be used later when we need
   to find the boot file */
extern int add_boot_alpha_filename(filename)
    char *filename;
{
    boot_file_name = filename;
    return 0;
}

static int boot_alpha_write(outfile)
    FILE *outfile;
{
    unsigned long long boot_sector[256]; /* One (ISO) sector */
    unsigned long long sum = 0;
	struct directory_entry	*boot_file;	/* Boot file we need to search for */
    unsigned long length = 0;
    unsigned long extent = 0;
    int i = 0;

    memset(boot_sector, 0, sizeof(boot_sector));    

    /* Write the text header into the boot sector */
    strcpy((char *)boot_sector, BOOT_STRING);

    /* Find the dir entry for the boot file by walking our file list */
    boot_file = search_tree_file(root, boot_file_name);
    if (!boot_file) {
#ifdef	USE_LIBSCHILY
		comerrno(EX_BAD, "Uh oh, I cant find the Alpha boot file '%s'!\n",
							boot_file_name);
#else
		fprintf(stderr, "Uh oh, I cant find the Alpha boot file '%s'!\n",
							boot_file_name);
		exit(1);
#endif
    }

    /* Grab the ISO start sector and length from the dir entry. ISO
       uses 2048-byte sectors, but we convert to 512-byte sectors here
       for the sake of the firmware */
    extent = get_733(boot_file->isorec.extent);
    extent *= 4;
    
    length = get_733(boot_file->isorec.size);
    length /= 512; /* I'm sure we should take account of any overlap
                      here, but I'm copying what isomarkboot
                      does. Maybe the boot files are specified to be
                      exact multiples of 512 bytes? */

    fprintf(stderr, "Found alpha boot image %s: using extent %lu, #blocks %lu\n",
            boot_file_name, extent, length);

    /* Now write those values into the appropriate area of the boot
       sector in LITTLE ENDIAN format. */
    write_le64(length, (unsigned char *)(unsigned long long *)&boot_sector[60]);
    write_le64(extent, (unsigned char *)&boot_sector[61]);

    /* Now generate a checksum of the first 504 bytes of the boot
       sector and place it in boot_sector[63]. Isomarkboot currently
       gets this wrong and will not work on big-endian systems! */
    for (i = 0; i < 63; i++)
        sum += read_le64((unsigned char *)&boot_sector[i]);

    write_le64(sum, (unsigned char *)&boot_sector[63]);

    xfwrite(boot_sector, sizeof(boot_sector), 1, outfile, 0, FALSE);
    last_extent_written++;

    return 0;
}

struct output_fragment alphaboot_desc = {NULL, oneblock_size, NULL, boot_alpha_write, "alpha boot block"};
