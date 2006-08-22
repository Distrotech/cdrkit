/* @(#)getfp.c	1.14 03/07/13 Copyright 1988-2003 J. Schilling */
/*
 *	Get frame pointer
 *
 *	Copyright (c) 1988-2003 J. Schilling
 */
/*
 * Copyright Jörg Schilling. All rights reserved.
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only.
 * See the file CDDL.Schily.txt in this distribution or
 * http://opensource.org/licenses/cddl1.php for details.
 */

#include <mconfig.h>
#include <standard.h>
#include <schily.h>

#ifndef	IS_AVOFFSET
/*
 * We usually don't like to compile a getfp() that returns junk data in case
 * we cannot scan the stack.
 * The only way to find this out is by including avoffset.h. Unfortunately, we
 * need to be able to use getfp() from avoffset.c in order to check if the
 * return value is usable or junk. To be able to do this, we include getfp.c
 * from avoffset.c and define IS_AVOFFSET before.
 */
#include <avoffset.h>

#if	!defined(AV_OFFSET) || !defined(FP_INDIR)
#	ifdef	HAVE_SCANSTACK
#	undef	HAVE_SCANSTACK
#	endif
#endif
#endif

#ifdef	HAVE_SCANSTACK
#include <stkframe.h>

#define	MAXWINDOWS	32
#define	NWINDOWS	7

#if defined(sparc) && defined(__GNUC__)
#	define	FP_OFF		0x10	/* some strange things on sparc gcc */
#else
#	define	FP_OFF		0
#endif

EXPORT	void	**___fpoff	__PR((char *cp));

EXPORT void **
getfp()
{
		long	**dummy[1];

#ifdef	sparc
	flush_reg_windows(MAXWINDOWS-2);
#endif
	return ((void **)((struct frame *)___fpoff((char *)&dummy[0]))->fr_savfp);
}

/*
 * Don't make it static to avoid inline optimization.
 *
 * We need this function to fool GCCs check for returning addresses
 * from outside the functions local address space.
 */
EXPORT void **
___fpoff(cp)
	char	*cp;
{
	long ***lp;

	lp = (long ***)(cp + FP_OFF);
	lp++;
	return ((void **)lp);
}

#ifdef	sparc
EXPORT int
flush_reg_windows(n)
	int	n;
{
	if (--n > 0)
		flush_reg_windows(n);
	return (0);
}
#endif

#endif	/* HAVE_SCANSTACK */
