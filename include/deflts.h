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

/* @(#)deflts.h	1.6 02/08/26 Copyright 1997 J. Schilling */
/*
 *	Definitions for reading program defaults.
 *
 *	Copyright (c) 1997 J. Schilling
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

#ifndef	_DEFLTS_H
#define	_DEFLTS_H

#ifndef _MCONFIG_H
#include <mconfig.h>
#endif
#ifndef _PROTOTYP_H
#include <prototyp.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

#define	DEFLT	"/etc/default"

/*
 * cmd's to defltcntl()
 */
#define	DC_GETFLAGS	0	/* Get actual flags	*/
#define	DC_SETFLAGS	1	/* Set new flags	*/

/*
 * flags to defltcntl()
 *
 * Make sure that when adding features, the default behaviour
 * is the same as old behaviour.
 */
#define	DC_CASE		0x0001	/* Don't ignore case	*/

#define	DC_STD		DC_CASE	/* Default flags	*/

/*
 * Macros to handle flags
 */
#ifndef	TURNON
#define	TURNON(flags, mask)	flags |= mask
#define	TURNOFF(flags, mask)	flags &= ~(mask)
#define	ISON(flags, mask)	(((flags) & (mask)) == (mask))
#define	ISOFF(flags, mask)	(((flags) & (mask)) != (mask))
#endif

extern	int	defltopen	__PR((const char *name));
extern	int	defltclose	__PR((void));
extern	void	defltfirst	__PR((void));
extern	char	*defltread	__PR((const char *name));
extern	char	*defltnext	__PR((const char *name));
extern	int	defltcntl	__PR((int cmd, int flags));

#ifdef	__cplusplus
}
#endif

#endif	/* _DEFLTS_H */
