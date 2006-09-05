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

/* @(#)scsi_scan.c	1.19 04/04/16 Copyright 1997-2004 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)scsi_scan.c	1.19 04/04/16 Copyright 1997-2004 J. Schilling";
#endif
/*
 *	Scan SCSI Bus.
 *	Stolen from sformat. Need a more general form to
 *	re-use it in sformat too.
 *
 *	Copyright (c) 1997-2004 J. Schilling
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
#include <stdxlib.h>
#include <standard.h>
#include <btorder.h>
#include <errno.h>
#include <schily.h>

#include <scg/scgcmd.h>
#include <scg/scsidefs.h>
#include <scg/scsireg.h>
#include <scg/scsitransp.h>

#include "scsi_scan.h"
#include "cdrecord.h"

LOCAL	void	print_product		__PR((FILE *f, struct scsi_inquiry *ip));
EXPORT	int	select_target		__PR((SCSI *scgp, FILE *f));
LOCAL	int	select_unit		__PR((SCSI *scgp, FILE *f));

LOCAL void
print_product(f, ip)
	FILE			*f;
	struct	scsi_inquiry	*ip;
{
	fprintf(f, "'%.8s' ", ip->vendor_info);
	fprintf(f, "'%.16s' ", ip->prod_ident);
	fprintf(f, "'%.4s' ", ip->prod_revision);
	if (ip->add_len < 31) {
		fprintf(f, "NON CCS ");
	}
	scg_fprintdev(f, ip);
}

EXPORT int
select_target(scgp, f)
	SCSI	*scgp;
	FILE	*f;
{
	int	initiator;
#ifdef	FMT
	int	cscsibus = scg_scsibus(scgp);
	int	ctarget  = scg_target(scgp);
	int	clun	 = scg_lun(scgp);
#endif
	int	n;
	int	low	= -1;
	int	high	= -1;
	int	amt	= 0;
	int	bus;
	int	tgt;
	int	lun = 0;
	BOOL	have_tgt;

	scgp->silent++;

	for (bus = 0; bus < 256; bus++) {
		scg_settarget(scgp, bus, 0, 0);

		if (!scg_havebus(scgp, bus))
			continue;

		initiator = scg_initiator_id(scgp);
		fprintf(f, "scsibus%d:\n", bus);

		for (tgt = 0; tgt < 16; tgt++) {
			n = bus*100 + tgt;

			scg_settarget(scgp, bus, tgt, lun);
			have_tgt = unit_ready(scgp) || scgp->scmd->error != SCG_FATAL;

			if (!have_tgt && tgt > 7) {
				if (scgp->scmd->ux_errno == EINVAL)
					break;
				continue;
			}

#ifdef	FMT
			if (print_disknames(bus, tgt, -1) < 8)
				fprintf(f, "\t");
			else
				fprintf(f, " ");
#else
			fprintf(f, "\t");
#endif
			if (fprintf(f, "%d,%d,%d", bus, tgt, lun) < 8)
				fprintf(f, "\t");
			else
				fprintf(f, " ");
			fprintf(f, "%3d) ", n);
			if (tgt == initiator) {
				fprintf(f, "HOST ADAPTOR\n");
				continue;
			}
			if (!have_tgt) {
				/*
				 * Hack: fd -> -2 means no access
				 */
				fprintf(f, "%c\n", scgp->fd == -2 ? '?':'*');
				continue;
			}
			amt++;
			if (low < 0)
				low = n;
			high = n;

			getdev(scgp, FALSE);
			print_product(f, scgp->inq);
		}
	}
	scgp->silent--;

	if (low < 0) {
		errmsgno(EX_BAD, "No target found.\n");
		return (0);
	}
	n = -1;
#ifdef	FMT
	getint("Select target", &n, low, high);
	bus = n/100;
	tgt = n%100;
	scg_settarget(scgp, bus, tgt, lun);
	return (select_unit(scgp));

	scg_settarget(scgp, cscsibus, ctarget, clun);
#endif
	return (amt);
}

LOCAL int
select_unit(scgp, f)
	SCSI	*scgp;
	FILE	*f;
{
	int	initiator;
	int	clun	= scg_lun(scgp);
	int	low	= -1;
	int	high	= -1;
	int	lun;

	scgp->silent++;

	fprintf(f, "scsibus%d target %d:\n", scg_scsibus(scgp), scg_target(scgp));

	initiator = scg_initiator_id(scgp);
	for (lun = 0; lun < 8; lun++) {

#ifdef	FMT
		if (print_disknames(scg_scsibus(scgp), scg_target(scgp), lun) < 8)
			fprintf(f, "\t");
		else
			fprintf(f, " ");
#else
		fprintf(f, "\t");
#endif
		if (fprintf(f, "%d,%d,%d", scg_scsibus(scgp), scg_target(scgp), lun) < 8)
			fprintf(f, "\t");
		else
			fprintf(f, " ");
		fprintf(f, "%3d) ", lun);
		if (scg_target(scgp) == initiator) {
			fprintf(f, "HOST ADAPTOR\n");
			continue;
		}
		scg_settarget(scgp, scg_scsibus(scgp), scg_target(scgp), lun);
		if (!unit_ready(scgp) && scgp->scmd->error == SCG_FATAL) {
			fprintf(f, "*\n");
			continue;
		}
		if (unit_ready(scgp)) {
			/* non extended sense illegal lun */
			if (scgp->scmd->sense.code == 0x25) {
				fprintf(f, "BAD UNIT\n");
				continue;
			}
		}
		if (low < 0)
			low = lun;
		high = lun;

		getdev(scgp, FALSE);
		print_product(f, scgp->inq);
	}
	scgp->silent--;

	if (low < 0) {
		errmsgno(EX_BAD, "No lun found.\n");
		return (0);
	}
	lun = -1;
#ifdef	FMT
	getint("Select lun", &lun, low, high);
	scg_settarget(scgp, scg_scsibus(scgp), scg_target(scgp), lun);
	format_one(scgp);
	return (1);
#endif

	scg_settarget(scgp, scg_scsibus(scgp), scg_target(scgp), clun);
	return (1);
}
