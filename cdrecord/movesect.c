/* @(#)movesect.c	1.3 04/03/02 Copyright 2001, 2004 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)movesect.c	1.3 04/03/02 Copyright 2001, 2004 J. Schilling";
#endif
/*
 *	Copyright (c) 2001, 2004 J. Schilling
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
#include <utypes.h>
#include <schily.h>

#include "cdrecord.h"
#include "movesect.h"

EXPORT	void	scatter_secs	__PR((track_t *trackp, char *bp, int nsecs));

/*
 * Scatter input sector size records over buffer to make them
 * output sector size.
 *
 * If input sector size is less than output sector size,
 *
 *	| sector_0 || sector_1 || ... || sector_n ||
 *
 * will be convterted into:
 *
 *	| sector_0 |grass|| sector_1 |grass|| ... || sector_n |grass||
 *
 *	Sector_n must me moved n * grass_size forward,
 *	Sector_1 must me moved 1 * grass_size forward
 *
 *
 * If output sector size is less than input sector size,
 *
 *	| sector_0 |grass|| sector_1 |grass|| ... || sector_n |grass||
 *
 * will be convterted into:
 *
 *	| sector_0 || sector_1 || ... || sector_n ||
 *
 *	Sector_1 must me moved 1 * grass_size backward,
 *	Sector_n must me moved n * grass_size backward,
 *
 *	Sector_0 must never be moved.
 */
EXPORT void
scatter_secs(trackp, bp, nsecs)
	track_t	*trackp;
	char	*bp;
	int	nsecs;
{
	char	*from;
	char	*to;
	int	isecsize = trackp->isecsize;
	int	secsize = trackp->secsize;
	int	i;

	if (secsize == isecsize)
		return;

	nsecs -= 1;	/* we do not move sector # 0 */

	if (secsize < isecsize) {
		from = bp + isecsize;
		to   = bp + secsize;

		for (i = nsecs; i > 0; i--) {
			movebytes(from, to, secsize);
			from += isecsize;
			to   += secsize;
		}
	} else {
		from = bp + (nsecs * isecsize);
		to   = bp + (nsecs * secsize);

		for (i = nsecs; i > 0; i--) {
			movebytes(from, to, isecsize);
			from -= isecsize;
			to   -= secsize;
		}
	}
}
