/* @(#)scsi_mmc4.c	1.1 05/05/16 Copyright 2002-2005 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)scsi_mmc4.c	1.1 05/05/16 Copyright 2002-2005 J. Schilling";
#endif
/*
 *	SCSI command functions for cdrecord
 *	covering MMC-4 level and above
 *
 *	Copyright (c) 2002-2005 J. Schilling
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

/* #define	DEBUG */
#include <mconfig.h>

#include <stdio.h>
#include <standard.h>
#include <stdxlib.h>
#include <unixstd.h>
#include <fctldefs.h>
#include <errno.h>
#include <strdefs.h>
#include <timedefs.h>

#include <utypes.h>
#include <btorder.h>
#include <intcvt.h>
#include <schily.h>

#include <scg/scgcmd.h>
#include <scg/scsidefs.h>
#include <scg/scsireg.h>
#include <scg/scsitransp.h>

#include "scsimmc.h"
#include "cdrecord.h"

EXPORT  int	get_supported_cdrw_media_types	__PR((SCSI *scgp));

/*
 * Retrieve list of supported cd-rw media types (feature 0x37)
 */
EXPORT int
get_supported_cdrw_media_types(scgp)
    SCSI    *scgp;
{
	Uchar   cbuf[16];
	int	ret;
	fillbytes(cbuf, sizeof (cbuf), '\0');

	scgp->silent++;
	ret = get_configuration(scgp, (char *)cbuf, sizeof (cbuf), 0x37, 2);
	scgp->silent--;

	if (ret < 0)
		return (-1);

	if (cbuf[3] < 12)	/* Couldn't retrieve feature 0x37	*/
		return (-1);

	return (int)(cbuf[13]);
}
