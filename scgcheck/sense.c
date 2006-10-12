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

/* @(#)sense.c	1.4 03/03/27 Copyright 2001 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)sense.c	1.4 03/03/27 Copyright 2001 J. Schilling";
#endif
/*
 *	Copyright (c) 2001 J. Schilling
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

extern	char	*buf;			/* The transfer buffer */
extern	long	bufsize;		/* The size of the transfer buffer */

extern	FILE	*logfile;
extern	char	unavail[];

static	BOOL	inq_nofail = FALSE;


void	sensetest(SCSI *scgp);
static	int	sensecount(SCSI *scgp, int sensecnt);
static	int	badinquiry(SCSI *scgp, caddr_t bp, int cnt, int sensecnt);
static	int	bad_unit_ready(SCSI *scgp, int sensecnt);

void
sensetest(SCSI *scgp)
{
	char	abuf[2];
	int	ret;
	int	sense_count = 0;
	BOOL	passed = TRUE;

	printf("Ready to start test for failing command? Enter <CR> to continue: ");
	fprintf(logfile, "**********> Testing for failed SCSI command.\n");
	flushit();
	(void)getline(abuf, sizeof(abuf));
/*	scgp->verbose++;*/
	fillbytes(buf, sizeof(struct scsi_inquiry), '\0');
	fillbytes((caddr_t)scgp->scmd, sizeof(*scgp->scmd), '\0');
	ret = badinquiry(scgp, buf, sizeof(struct scsi_inquiry), CCS_SENSE_LEN);
	scg_vsetup(scgp);
	scg_errfflush(scgp, logfile);
	if (ret >= 0 || !scg_cmd_err(scgp)) {
		inq_nofail = TRUE;
		printf("Inquiry did not fail.\n");
		fprintf(logfile, "Inquiry did not fail.\n");
		printf("This may be because the firmware in your drive is buggy.\n");
		printf("If the current drive is not a CD-ROM drive please restart\n");
		printf("the test utility. Otherwise remove any medium from the drive.\n");
		printf("Ready to start test for failing command? Enter <CR> to continue: ");
		flushit();
		(void)getline(abuf, sizeof(abuf));
		ret = test_unit_ready(scgp);
		if (ret >= 0 || !scg_cmd_err(scgp)) {
			printf("Test Unit Ready did not fail.\n");
			printf("Ready to eject tray? Enter <CR> to continue: ");
			flushit();
			(void)getline(abuf, sizeof(abuf));
			scsi_unload(scgp, (cdr_t *)0);
			ret = test_unit_ready(scgp);
		}
	}
	scg_vsetup(scgp);
	scg_errfflush(scgp, logfile);
/*	scgp->verbose--;*/
	if (ret < 0 &&
	    scgp->scmd->error == SCG_NO_ERROR &&
	    scgp->scmd->ux_errno != 0 &&
	    *(Uchar *)&scgp->scmd->scb != 0) {
		printf("----------> SCSI failed command test PASSED\n");
		fprintf(logfile, "----------> SCSI failed command test PASSED\n");
	} else {
		if (ret >= 0) {
			printf("---------->	scg_cmd() returns not -1 (%d)\n", ret);
			fprintf(logfile, "---------->	scg_cmd() returns not -1 (%d)\n", ret);
		}
		if (scgp->scmd->error != SCG_NO_ERROR) {
			printf("---------->	SCSI Transport return != SCG_NO_ERROR (%d)\n", scgp->scmd->error);
			fprintf(logfile, "---------->	SCSI Transport return != SCG_NO_ERROR (%d)\n", scgp->scmd->error);
		}
		if (scgp->scmd->ux_errno == 0) {
			printf("---------->	UNIX errno set to 0\n");
			fprintf(logfile, "---------->	UNIX errno set to 0\n");
		}
		if (*(Uchar *)&scgp->scmd->scb == 0) {
			printf("---------->	SCSI status byte set to 0 (0x%x)\n", *(Uchar *)&scgp->scmd->scb & 0xFF);
			fprintf(logfile, "---------->	SCSI status byte set to 0 (0x%x)\n", *(Uchar *)&scgp->scmd->scb & 0xFF);
		}
		printf("----------> SCSI failed command test FAILED\n");
		fprintf(logfile, "----------> SCSI failed command test FAILED\n");
	}


	printf("Ready to start test for sense data count? Enter <CR> to continue: ");
	fprintf(logfile, "**********> Testing for SCSI sense data count.\n");
	flushit();
	(void)getline(abuf, sizeof(abuf));
	printf("Testing if at least CCS_SENSE_LEN (%d) is supported...\n", CCS_SENSE_LEN);
	fprintf(logfile, "**********> Testing if at least CCS_SENSE_LEN (%d) is supported...\n", CCS_SENSE_LEN);
	ret = sensecount(scgp, CCS_SENSE_LEN);
	if (ret > sense_count)
		sense_count = ret;
	if (ret == CCS_SENSE_LEN) {
		printf("---------->	Wanted %d sense bytes, got it.\n", CCS_SENSE_LEN);
		fprintf(logfile, "---------->	Wanted %d sense bytes, got it.\n", CCS_SENSE_LEN);
	}
	if (ret != CCS_SENSE_LEN) {
		printf("---------->	Minimum standard (CCS) sense length failed\n");
		printf("---------->	Wanted %d sense bytes, got (%d)\n", CCS_SENSE_LEN, ret);
		fprintf(logfile, "---------->	Minimum standard (CCS) sense length failed\n");
		fprintf(logfile, "---------->	Wanted %d sense bytes, got (%d)\n", CCS_SENSE_LEN, ret);
	}
	if (ret != scgp->scmd->sense_count) {
		passed = FALSE;
		printf("---------->	Libscg says %d sense bytes but got (%d)\n", scgp->scmd->sense_count, ret);
		fprintf(logfile, "---------->	Libscg says %d sense bytes but got (%d)\n", scgp->scmd->sense_count, ret);
	}
	printf("Testing for %d bytes of sense data...\n", SCG_MAX_SENSE);
	fprintf(logfile, "**********> Testing for %d bytes of sense data...\n", SCG_MAX_SENSE);
	ret = sensecount(scgp, SCG_MAX_SENSE);
	if (ret > sense_count)
		sense_count = ret;
	if (ret == SCG_MAX_SENSE) {
		printf("---------->	Wanted %d sense bytes, got it.\n", SCG_MAX_SENSE);
		fprintf(logfile, "---------->	Wanted %d sense bytes, got it.\n", SCG_MAX_SENSE);
	}
	if (ret != SCG_MAX_SENSE) {
		printf("---------->	Wanted %d sense bytes, got (%d)\n", SCG_MAX_SENSE, ret);
		fprintf(logfile, "---------->	Wanted %d sense bytes, got (%d)\n", SCG_MAX_SENSE, ret);
	}
	if (ret != scgp->scmd->sense_count) {
		passed = FALSE;
		printf("---------->	Libscg says %d sense bytes but got (%d)\n", scgp->scmd->sense_count, ret);
		fprintf(logfile, "---------->	Libscg says %d sense bytes but got (%d)\n", scgp->scmd->sense_count, ret);
	}

	printf("----------> Got a maximum of %d sense bytes\n", sense_count);
	fprintf(logfile, "----------> Got a maximum of %d sense bytes\n", sense_count);
	if (passed && sense_count >= CCS_SENSE_LEN) {
		printf("----------> SCSI sense count test PASSED\n");
		fprintf(logfile, "----------> SCSI sense count test PASSED\n");
	} else {
		printf("----------> SCSI sense count test FAILED\n");
		fprintf(logfile, "----------> SCSI sense count test FAILED\n");
	}
}

static int
sensecount(SCSI *scgp, int sensecnt)
{
	int	maxcnt;
	int	i;
	Uchar	*p;

	if (sensecnt > SCG_MAX_SENSE)
		sensecnt = SCG_MAX_SENSE;
		
/*	scgp->verbose++;*/
	scgp->silent++;
	fillbytes(buf, sizeof(struct scsi_inquiry), '\0');
	fillbytes((caddr_t)scgp->scmd, sizeof(*scgp->scmd), '\0');
	fillbytes((caddr_t)scgp->scmd->u_sense.cmd_sense, sensecnt, 0x00);
	if (inq_nofail)
		bad_unit_ready(scgp, sensecnt);
	else
		badinquiry(scgp, buf, sizeof(struct scsi_inquiry), sensecnt);
	scg_fprbytes(stdout,  "Sense Data:", (Uchar *)scgp->scmd->u_sense.cmd_sense, sensecnt);
	scg_fprbytes(logfile, "Sense Data:", (Uchar *)scgp->scmd->u_sense.cmd_sense, sensecnt);
	p = (Uchar *)scgp->scmd->u_sense.cmd_sense;
	for (i=sensecnt-1; i >= 0; i--) {
		if (p[i] != 0x00) {
			break;
		}
	}
	i++;
	maxcnt = i;
printf("---------->     Method 0x00: expected: %d reported: %d max found: %d\n", sensecnt, scgp->scmd->sense_count, maxcnt);

	fillbytes(buf, sizeof(struct scsi_inquiry), '\0');
	fillbytes((caddr_t)scgp->scmd, sizeof(*scgp->scmd), '\0');
	fillbytes((caddr_t)scgp->scmd->u_sense.cmd_sense, sensecnt, 0xFF);
	if (inq_nofail)
		bad_unit_ready(scgp, sensecnt);
	else
		badinquiry(scgp, buf, sizeof(struct scsi_inquiry), sensecnt);
	scg_fprbytes(stdout,  "Sense Data:", (Uchar *)scgp->scmd->u_sense.cmd_sense, sensecnt);
	scg_fprbytes(logfile, "Sense Data:", (Uchar *)scgp->scmd->u_sense.cmd_sense, sensecnt);
	p = (Uchar *)scgp->scmd->u_sense.cmd_sense;
	for (i=sensecnt-1; i >= 0; i--) {
		if (p[i] != 0xFF) {
			break;
		}
	}
	i++;
	if (i > maxcnt)
		maxcnt = i;
printf("---------->     Method 0xFF: expected: %d reported: %d max found: %d\n", sensecnt, scgp->scmd->sense_count, i);

/*	scgp->verbose--;*/
	scgp->silent--;
/*	scg_vsetup(scgp);*/
/*	scg_errfflush(scgp, logfile);*/

	return (maxcnt);
}

static int
badinquiry(SCSI *scgp, caddr_t bp, int cnt, int sensecnt)
{
	register struct	scg_cmd	*scmd = scgp->scmd;

/*	fillbytes(bp, cnt, '\0');*/
/*	fillbytes((caddr_t)scmd, sizeof(*scmd), '\0');*/
	scmd->addr = bp;
	scmd->size = cnt;
	scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
	scmd->cdb_len = SC_G0_CDBLEN;
/*	scmd->sense_len = CCS_SENSE_LEN;*/
	scmd->sense_len = sensecnt;
	scmd->cdb.g0_cdb.cmd = SC_INQUIRY;
	scmd->cdb.g0_cdb.lun = scg_lun(scgp);
	scmd->cdb.g0_cdb.count = cnt;

scmd->cdb.cmd_cdb[3] = 0xFF;
	
	scgp->cmdname = "inquiry";

	if (scg_cmd(scgp) < 0)
		return (-1);
	if (scgp->verbose)
		scg_prbytes("Inquiry Data   :", (Uchar *)bp, cnt - scg_getresid(scgp));
	return (0);
}

static int
bad_unit_ready(SCSI *scgp, int sensecnt)
{
	register struct	scg_cmd	*scmd = scgp->scmd;

/*	fillbytes((caddr_t)scmd, sizeof(*scmd), '\0');*/
	scmd->addr = (caddr_t)0;
	scmd->size = 0;
	scmd->flags = SCG_DISRE_ENA | (scgp->silent ? SCG_SILENT:0);
	scmd->cdb_len = SC_G0_CDBLEN;
	scmd->sense_len = sensecnt;
	scmd->cdb.g0_cdb.cmd = SC_TEST_UNIT_READY;
	scmd->cdb.g0_cdb.lun = scg_lun(scgp);
	
	scgp->cmdname = "test unit ready";

	return (scg_cmd(scgp));
}
