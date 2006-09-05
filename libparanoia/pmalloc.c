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

/* @(#)pmalloc.c	1.3 04/05/15 Copyright 2004 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)pmalloc.c	1.3 04/05/15 Copyright 2004 J. Schilling";
#endif
/*
 *	Paranoia malloc() functions
 *
 *	Copyright (c) 2004 J. Schilling
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
#include <stdxlib.h>
#include <standard.h>
#include <schily.h>
#include "pmalloc.h"

#ifdef	PM_ADD_DEBUG
LOCAL int madd = 8192;
LOCAL int cadd = 8192;
LOCAL int radd = 8192;
#else
LOCAL int madd = 0;
/*LOCAL int cadd = 0;*/
LOCAL int radd = 0;
#endif

EXPORT void
_pfree(ptr)
	void	*ptr;
{
	free(ptr);
}

EXPORT void *
_pmalloc(size)
	size_t	size;
{
	void	*p;

	p = malloc(size + madd);
	if (p == NULL)
		raisecond("NO MEM", 0L);
	return (p);
}

EXPORT void *
_pcalloc(nelem, elsize)
	size_t	nelem;
	size_t	elsize;
{
	void	*p;
#ifdef	PM_ADD_DEBUG
	size_t	n = nelem * elsize;

	n += cadd;
	p = calloc(1, n);
#else
	p = calloc(nelem, elsize);
#endif
	if (p == NULL)
		raisecond("NO MEM", 0L);
	return (p);
}

EXPORT void *
_prealloc(ptr, size)
	void	*ptr;
	size_t	size;
{
	void	*p;

	p = realloc(ptr, size + radd);
	if (p == NULL)
		raisecond("NO MEM", 0L);
	return (p);
}
