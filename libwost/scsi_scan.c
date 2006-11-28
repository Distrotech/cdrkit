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

#include <usal/usalcmd.h>
#include <usal/scsidefs.h>
#include <usal/scsireg.h>
#include <usal/scsitransp.h>

#include "scsi_scan.h"
#include "wodim.h"

static	void	print_product(FILE *f, struct scsi_inquiry *ip);
int	select_target(SCSI *usalp, FILE *f);
static	int	select_unit(SCSI *usalp, FILE *f);

static void
print_product(FILE *f, struct  scsi_inquiry *ip)
{
	fprintf(f, "'%.8s' ", ip->vendor_info);
	fprintf(f, "'%.16s' ", ip->prod_ident);
	fprintf(f, "'%.4s' ", ip->prod_revision);
	if (ip->add_len < 31) {
		fprintf(f, "NON CCS ");
	}
	usal_fprintdev(f, ip);
}

int
select_target(SCSI *usalp, FILE *f)
{
	int	initiator;
#ifdef	FMT
	int	cscsibus = usal_scsibus(usalp);
	int	ctarget  = usal_target(usalp);
	int	clun	 = usal_lun(usalp);
#endif
	int	n;
	int	low	= -1;
	int	high	= -1;
	int	amt	= 0;
	int	bus;
	int	tgt;
	int	lun = 0;
	BOOL	have_tgt;

	usalp->silent++;

	for (bus = 0; bus < 256; bus++) {
		usal_settarget(usalp, bus, 0, 0);

		if (!usal_havebus(usalp, bus))
			continue;

		initiator = usal_initiator_id(usalp);
		fprintf(f, "scsibus%d:\n", bus);

		for (tgt = 0; tgt < 16; tgt++) {
			n = bus*100 + tgt;

			usal_settarget(usalp, bus, tgt, lun);
			have_tgt = unit_ready(usalp) || usalp->scmd->error != SCG_FATAL;

			if (!have_tgt && tgt > 7) {
				if (usalp->scmd->ux_errno == EINVAL)
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
				fprintf(f, "%c\n", usalp->fd == -2 ? '?':'*');
				continue;
			}
			amt++;
			if (low < 0)
				low = n;
			high = n;

			getdev(usalp, FALSE);
			print_product(f, usalp->inq);
		}
	}
	usalp->silent--;

	if (low < 0) {
		errmsgno(EX_BAD, "No target found.\n");
		return (0);
	}
	n = -1;
#ifdef	FMT
	getint("Select target", &n, low, high);
	bus = n/100;
	tgt = n%100;
	usal_settarget(usalp, bus, tgt, lun);
	return (select_unit(usalp));

	usal_settarget(usalp, cscsibus, ctarget, clun);
#endif
	return (amt);
}

static int
select_unit(SCSI *usalp, FILE *f)
{
	int	initiator;
	int	clun	= usal_lun(usalp);
	int	low	= -1;
	int	high	= -1;
	int	lun;

	usalp->silent++;

	fprintf(f, "scsibus%d target %d:\n", usal_scsibus(usalp), usal_target(usalp));

	initiator = usal_initiator_id(usalp);
	for (lun = 0; lun < 8; lun++) {

#ifdef	FMT
		if (print_disknames(usal_scsibus(usalp), usal_target(usalp), lun) < 8)
			fprintf(f, "\t");
		else
			fprintf(f, " ");
#else
		fprintf(f, "\t");
#endif
		if (fprintf(f, "%d,%d,%d", usal_scsibus(usalp), usal_target(usalp), lun) < 8)
			fprintf(f, "\t");
		else
			fprintf(f, " ");
		fprintf(f, "%3d) ", lun);
		if (usal_target(usalp) == initiator) {
			fprintf(f, "HOST ADAPTOR\n");
			continue;
		}
		usal_settarget(usalp, usal_scsibus(usalp), usal_target(usalp), lun);
		if (!unit_ready(usalp) && usalp->scmd->error == SCG_FATAL) {
			fprintf(f, "*\n");
			continue;
		}
		if (unit_ready(usalp)) {
			/* non extended sense illegal lun */
			if (usalp->scmd->sense.code == 0x25) {
				fprintf(f, "BAD UNIT\n");
				continue;
			}
		}
		if (low < 0)
			low = lun;
		high = lun;

		getdev(usalp, FALSE);
		print_product(f, usalp->inq);
	}
	usalp->silent--;

	if (low < 0) {
		errmsgno(EX_BAD, "No lun found.\n");
		return (0);
	}
	lun = -1;
#ifdef	FMT
	getint("Select lun", &lun, low, high);
	usal_settarget(usalp, usal_scsibus(usalp), usal_target(usalp), lun);
	format_one(usalp);
	return (1);
#endif

	usal_settarget(usalp, usal_scsibus(usalp), usal_target(usalp), clun);
	return (1);
}
