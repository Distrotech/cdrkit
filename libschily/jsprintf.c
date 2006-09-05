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

/* @(#)jsprintf.c	1.15 04/09/25 Copyright 1985, 1995-2003 J. Schilling */
/*
 *	Copyright (c) 1985, 1995-2003 J. Schilling
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
#include <vadefs.h>
#include <standard.h>
#include <schily.h>

#define	BFSIZ	256

typedef struct {
	short	cnt;
	char	*ptr;
	char	buf[BFSIZ];
	int	count;
	FILE	*f;
} *BUF, _BUF;

LOCAL	void	_bflush		__PR((BUF));
LOCAL	void	_bput		__PR((char, long));
EXPORT	int	js_fprintf	__PR((FILE *, const char *, ...));
EXPORT	int	js_printf	__PR((const char *, ...));

LOCAL void
_bflush(bp)
	register BUF	bp;
{
	bp->count += bp->ptr - bp->buf;
	if (filewrite(bp->f, bp->buf, bp->ptr - bp->buf) < 0)
		bp->count = EOF;
	bp->ptr = bp->buf;
	bp->cnt = BFSIZ;
}

#ifdef	PROTOTYPES
LOCAL void
_bput(char c, long l)
#else
LOCAL void
_bput(c, l)
		char	c;
		long	l;
#endif
{
	register BUF	bp = (BUF)l;

	*bp->ptr++ = c;
	if (--bp->cnt <= 0)
		_bflush(bp);
}

/* VARARGS1 */
#ifdef	PROTOTYPES
EXPORT int
js_printf(const char *form, ...)
#else
EXPORT int
js_printf(form, va_alist)
	char	*form;
	va_dcl
#endif
{
	va_list	args;
	_BUF	bb;

	bb.ptr = bb.buf;
	bb.cnt = BFSIZ;
	bb.count = 0;
	bb.f = stdout;
#ifdef	PROTOTYPES
	va_start(args, form);
#else
	va_start(args);
#endif
	format(_bput, (long)&bb, form, args);
	va_end(args);
	if (bb.cnt < BFSIZ)
		_bflush(&bb);
	return (bb.count);
}

/* VARARGS2 */
#ifdef	PROTOTYPES
EXPORT int
js_fprintf(FILE *file, const char *form, ...)
#else
EXPORT int
js_fprintf(file, form, va_alist)
	FILE	*file;
	char	*form;
	va_dcl
#endif
{
	va_list	args;
	_BUF	bb;

	bb.ptr = bb.buf;
	bb.cnt = BFSIZ;
	bb.count = 0;
	bb.f = file;
#ifdef	PROTOTYPES
	va_start(args, form);
#else
	va_start(args);
#endif
	format(_bput, (long)&bb, form, args);
	va_end(args);
	if (bb.cnt < BFSIZ)
		_bflush(&bb);
	return (bb.count);
}
