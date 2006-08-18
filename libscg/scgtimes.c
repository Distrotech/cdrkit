/* @(#)scgtimes.c	1.1 00/08/25 Copyright 1995,2000 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)scgtimes.c	1.1 00/08/25 Copyright 1995,2000 J. Schilling";
#endif
/*
 *	SCSI user level command timing
 *
 *	Copyright (c) 1995,2000 J. Schilling
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
#include <standard.h>
#include <timedefs.h>
#include <schily.h>

#include <scg/scsitransp.h>
#include "scgtimes.h"

EXPORT	void	__scg_times	__PR((SCSI *scgp));

/*
 * We don't like to make this a public interface to prevent bad users
 * from making our timing incorrect.
 */
EXPORT void
__scg_times(scgp)
	SCSI	*scgp;
{
	struct timeval	*stp = scgp->cmdstop;

	gettimeofday(stp, (struct timezone *)0);
	stp->tv_sec -= scgp->cmdstart->tv_sec;
	stp->tv_usec -= scgp->cmdstart->tv_usec;
	while (stp->tv_usec < 0) {
		stp->tv_sec -= 1;
		stp->tv_usec += 1000000;
	}
}
