/* @(#)scsi_cdr_mmc4.c	1.1 05/05/16 Copyright 1995-2005 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)scsi_cdr_mmc4.c	1.1 05/05/16 Copyright 1995-2005 J. Schilling";
#endif
/*
 *	SCSI command functions for cdrecord
 *	covering MMC-4
 *
 *	Copyright (c) 1995-2005 J. Schilling
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

EXPORT	void	print_capabilities_mmc4 __PR((SCSI *scgp));

#define	DOES(what, flag)	printf("  Does %s%s\n", flag?"":"not ", what)


EXPORT void
print_capabilities_mmc4(scgp)
	SCSI	*scgp;
{
	int	cdrw_types;

	if (scgp->inq->type != INQ_ROMD)
		return;

	cdrw_types = get_supported_cdrw_media_types(scgp);
	if (cdrw_types != -1) {
		printf("\nSupported CD-RW media types according to MMC-4 feature 0x37:\n");
		DOES("write multi speed       CD-RW media", (cdrw_types & CDR_CDRW_MULTI));
		DOES("write high  speed       CD-RW media", (cdrw_types & CDR_CDRW_HIGH));
		DOES("write ultra high speed  CD-RW media", (cdrw_types & CDR_CDRW_ULTRA));
		DOES("write ultra high speed+ CD-RW media", (cdrw_types & CDR_CDRW_ULTRAP));
	}
}
