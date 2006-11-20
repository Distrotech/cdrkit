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

/* @(#)dmaresid.c	1.7 06/02/05 Copyright 1998,2001 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)dmaresid.c	1.7 06/02/05 Copyright 1998,2001 J. Schilling";
#endif
/*
 *	Copyright (c) 1998,2001 J. Schilling
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
#include <unixstd.h>
#include <strdefs.h>
#include <schily.h>
#include <standard.h>

#include <utypes.h>
#include <btorder.h>
#include <scg/scgcmd.h>
#include <scg/scsidefs.h>
#include <scg/scsireg.h>
#include <scg/scsitransp.h>

#include "cdrecord.h"
#include "scgcheck.h"

void	dmaresid(SCSI *scgp);
static	int	xtinquiry(SCSI *scgp, int cnt, int dmacnt);
static	int	tinquiry(SCSI *scgp, caddr_t bp, int cnt, int dmacnt);


extern	char	*buf;			/* The transfer buffer */
extern	long	bufsize;		/* The size of the transfer buffer */

extern	FILE	*logfile;
extern	char	unavail[];

void
dmaresid(SCSI *scgp)
{
	char	abuf[2];
	int	cnt = sizeof (struct scsi_inquiry);
	int	dmacnt;
	int	ret;
	BOOL	passed;

	printf("Ready to start test for working DMA residual count? Enter <CR> to continue: ");
	fprintf(logfile, "**********> Testing for working DMA residual count.\n");
	flushit();
	(void) getline(abuf, sizeof (abuf));

	printf("**********> Testing for working DMA residual count == 0.\n");
	fprintf(logfile, "**********> Testing for working DMA residual count == 0.\n");
	passed = TRUE;
	dmacnt = cnt;
	ret = xtinquiry(scgp, cnt, dmacnt);
	if (ret == dmacnt) {
		printf("---------->	Wanted %d bytes, got it.\n", dmacnt);
		fprintf(logfile, "---------->	Wanted %d bytes, got it.\n", dmacnt);
	}
	if (ret != dmacnt) {
		printf("---------->	Wanted %d bytes, got (%d)\n", dmacnt, ret);
		fprintf(logfile, "---------->	Wanted %d bytes, got (%d)\n", dmacnt, ret);
	}
	if (ret != scg_getdmacnt(scgp)) {
		passed = FALSE;
		printf("---------->	Libscg says %d bytes but got (%d)\n", scg_getdmacnt(scgp), ret);
		fprintf(logfile, "---------->	Libscg says %d bytes but got (%d)\n", scg_getdmacnt(scgp), ret);
	}
	if (passed && ret == dmacnt) {
		printf("----------> SCSI DMA residual count == 0 test PASSED\n");
		fprintf(logfile, "----------> SCSI DMA residual count == 0 test PASSED\n");
	} else {
		printf("----------> SCSI DMA residual count == 0 test FAILED\n");
		fprintf(logfile, "----------> SCSI DMA residual count == 0 test FAILED\n");
	}

	printf("Ready to start test for working DMA residual count == DMA count? Enter <CR> to continue: ");
	fprintf(logfile, "**********> Testing for working DMA residual count == DMA count.\n");
	flushit();
	(void) getline(abuf, sizeof (abuf));
	passed = TRUE;
	dmacnt = cnt;
	ret = xtinquiry(scgp, 0, dmacnt);
	if (ret == 0) {
		printf("---------->	Wanted 0 bytes, got it.\n");
		fprintf(logfile, "---------->	Wanted 0 bytes, got it.\n");
	}
	if (ret != 0) {
		printf("---------->	Wanted %d bytes, got (%d)\n", 0, ret);
		fprintf(logfile, "---------->	Wanted %d bytes, got (%d)\n", 0, ret);
	}
	if (ret != scg_getdmacnt(scgp)) {
		passed = FALSE;
		printf("---------->	Libscg says %d bytes but got (%d)\n", scg_getdmacnt(scgp), ret);
		fprintf(logfile, "---------->	Libscg says %d bytes but got (%d)\n", scg_getdmacnt(scgp), ret);
	}
	if (passed && ret == 0) {
		printf("----------> SCSI DMA residual count == DMA count test PASSED\n");
		fprintf(logfile, "----------> SCSI DMA residual count == DMA count test PASSED\n");
	} else {
		passed = FALSE;
		printf("----------> SCSI DMA residual count == DMA count test FAILED\n");
		fprintf(logfile, "----------> SCSI DMA residual count == DMA count test FAILED\n");
	}

	if (!passed) {
		printf("----------> SCSI DMA residual count not working - no further tests\n");
		fprintf(logfile, "----------> SCSI DMA residual count not working - no further tests\n");
		return;
	}

	printf("Ready to start test for working DMA residual count == 1? Enter <CR> to continue: ");
	fprintf(logfile, "**********> Testing for working DMA residual count == 1.\n");
	flushit();
	(void) getline(abuf, sizeof (abuf));
	passed = TRUE;
	dmacnt = cnt+1;
	ret = xtinquiry(scgp, cnt, dmacnt);
	if (ret == cnt) {
		printf("---------->	Wanted %d bytes, got it.\n", cnt);
		fprintf(logfile, "---------->	Wanted %d bytes, got it.\n", cnt);
	}
	if (ret != cnt) {
		printf("---------->	Wanted %d bytes, got (%d)\n", cnt, ret);
		fprintf(logfile, "---------->	Wanted %d bytes, got (%d)\n", cnt, ret);
	}
	if (ret != scg_getdmacnt(scgp)) {
		passed = FALSE;
		printf("---------->	Libscg says %d bytes but got (%d)\n", scg_getdmacnt(scgp), ret);
		fprintf(logfile, "---------->	Libscg says %d bytes but got (%d)\n", scg_getdmacnt(scgp), ret);
	}
	if (passed && ret == cnt) {
		printf("----------> SCSI DMA residual count == 1 test PASSED\n");
		fprintf(logfile, "----------> SCSI DMA residual count == 1 test PASSED\n");
	} else {
		passed = FALSE;
		printf("----------> SCSI DMA residual count == 1 test FAILED\n");
		fprintf(logfile, "----------> SCSI DMA residual count == 1 test FAILED\n");
	}

	printf("**********> Testing for working DMA overrun test.\n");
	fprintf(logfile, "**********> Testing for working DMA overrun test.\n");
	passed = TRUE;
	dmacnt = cnt-1;
	ret = xtinquiry(scgp, cnt, dmacnt);
	if (ret == cnt) {
		passed = FALSE;
		printf("---------->	Wanted %d bytes, got it - DMA overrun not blocked.\n", cnt);
		fprintf(logfile, "---------->	Wanted %d bytes, got it - DMA overrun not blocked.\n", cnt);
	}
	if (ret != dmacnt) {
		passed = FALSE;
		printf("---------->	Wanted %d bytes, got (%d)\n", dmacnt, ret);
		fprintf(logfile, "---------->	Wanted %d bytes, got (%d)\n", dmacnt, ret);
	}
	if (ret != scg_getdmacnt(scgp)) {
		passed = FALSE;
		printf("---------->	Libscg says %d bytes but got (%d)\n", scg_getdmacnt(scgp), ret);
		fprintf(logfile, "---------->	Libscg says %d bytes but got (%d)\n", scg_getdmacnt(scgp), ret);
	}
	if (passed && scg_getresid(scgp) < 0) {
		printf("----------> SCSI DMA overrun test PASSED\n");
		fprintf(logfile, "----------> SCSI DMA overrun test PASSED\n");
	} else {
		printf("----------> SCSI DMA overrun test FAILED\n");
		fprintf(logfile, "----------> SCSI DMA overrun test FAILED\n");
	}
}

static int
xtinquiry(SCSI *scgp, int cnt, int dmacnt)
{
	Uchar	ibuf[1024];
	struct scsi_inquiry	*ip;
	int	maxcnt;
	int	rescnt;
	int	i;

	ip = (struct scsi_inquiry *)ibuf;

	fillbytes(ibuf, sizeof (ibuf), '\0');
	tinquiry(scgp, (caddr_t)ibuf, cnt, dmacnt);
	for (i = sizeof (ibuf)-1; i >= 0; i--) {
		if (ibuf[i] != '\0') {
			break;
		}
	}
	i++;
	maxcnt = i;
	rescnt = dmacnt - scg_getresid(scgp);
	printf("CDB cnt: %d DMA cnt: %d got really: %d (System says: RDMA cnt: %d resid %d)\n",
				cnt, dmacnt, i, rescnt, scg_getresid(scgp));

	fillbytes(ibuf, sizeof (ibuf), 0xFF);
	tinquiry(scgp, (caddr_t)ibuf, cnt, dmacnt);
	for (i = sizeof (ibuf)-1; i >= 0; i--) {
		if (ibuf[i] != 0xFF) {
			break;
		}
	}
	i++;
	if (i > maxcnt)
		maxcnt = i;
	rescnt = dmacnt - scg_getresid(scgp);
	printf("CDB cnt: %d DMA cnt: %d got really: %d (System says: RDMA cnt: %d resid %d)\n",
				cnt, dmacnt, i, rescnt, scg_getresid(scgp));

	return (maxcnt);
}

static int
tinquiry(SCSI *scgp, caddr_t bp, int cnt, int dmacnt)
{
	register struct	scg_cmd	*scmd = scgp->scmd;

/*	fillbytes(bp, cnt, '\0');*/
	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = bp;
	scmd->size = dmacnt;
	scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
	scmd->cdb_len = SC_G0_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g0_cdb.cmd = SC_INQUIRY;
	scmd->cdb.g0_cdb.lun = scg_lun(scgp);
	scmd->cdb.g0_cdb.count = cnt;

	scgp->cmdname = "inquiry";

	if (scg_cmd(scgp) < 0)
		return (-1);
	if (scgp->verbose)
		scg_prbytes("Inquiry Data   :", (Uchar *)bp, cnt - scg_getresid(scgp));
	return (0);
}
