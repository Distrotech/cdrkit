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

/* @(#)astoll.c	1.3 03/06/15 Copyright 1985, 2000-2005 J. Schilling */
/*
 *	astoll() converts a string to long long
 *
 *	Leading tabs and spaces are ignored.
 *	Both return pointer to the first char that has not been used.
 *	Caller must check if this means a bad conversion.
 *
 *	leading "+" is ignored
 *	leading "0"  makes conversion octal (base 8)
 *	leading "0x" makes conversion hex   (base 16)
 *
 *	Llong is silently reverted to long if the compiler does not
 *	support long long.
 *
 *	Copyright (c) 1985, 2000-2005 J. Schilling
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
#include <errno.h>
#ifndef	HAVE_ERRNO_DEF
extern	int	errno;
#endif

#define	is_space(c)	 ((c) == ' ' || (c) == '\t')
#define	is_digit(c)	 ((c) >= '0' && (c) <= '9')
#define	is_hex(c)	(\
			((c) >= 'a' && (c) <= 'f') || \
			((c) >= 'A' && (c) <= 'F'))

#define	is_lower(c)	((c) >= 'a' && (c) <= 'z')
#define	is_upper(c)	((c) >= 'A' && (c) <= 'Z')
#define	to_lower(c)	(((c) >= 'A' && (c) <= 'Z') ? (c) - 'A'+'a' : (c))

#if	('i' + 1) < 'j'
#define	BASE_MAX	('i' - 'a' + 10 + 1)	/* This is EBCDIC */
#else
#define	BASE_MAX	('z' - 'a' + 10 + 1)	/* This is ASCII */
#endif



EXPORT	char *astoull	__PR((const char *s, Ullong *l));
EXPORT	char *astoullb	__PR((const char *s, Ullong *l, int base));

EXPORT char *
astoull(s, l)
	register const char *s;
	Ullong *l;
{
	return (astoullb(s, l, 0));
}

EXPORT char *
astoullb(s, l, base)
	register const char *s;
	Ullong *l;
	register int base;
{
#ifdef	DO_SIGNED
	int neg = 0;
#endif
	register Ullong ret = (Ullong)0;
		Ullong maxmult;
	register int digit;
	register char c;

	if (base > BASE_MAX || base == 1 || base < 0) {
		errno = EINVAL;
		return ((char *)s);
	}

	while (is_space(*s))
		s++;

	if (*s == '+') {
		s++;
	} else if (*s == '-') {
#ifndef	DO_SIGNED
		errno = EINVAL;
		return ((char *)s);
#else
		s++;
		neg++;
#endif
	}

	if (base == 0) {
		if (*s == '0') {
			base = 8;
			s++;
			if (*s == 'x' || *s == 'X') {
				s++;
				base = 16;
			}
		} else {
			base = 10;
		}
	}
	maxmult = TYPE_MAXVAL(Ullong) / base;
	for (; (c = *s) != 0; s++) {

		if (is_digit(c)) {
			digit = c - '0';
#ifdef	OLD
		} else if (is_hex(c)) {
			digit = to_lower(c) - 'a' + 10;
#else
		} else if (is_lower(c)) {
			digit = c - 'a' + 10;
		} else if (is_upper(c)) {
			digit = c - 'A' + 10;
#endif
		} else {
			break;
		}

		if (digit < base) {
			if (ret > maxmult)
				goto overflow;
			ret *= base;
			if (TYPE_MAXVAL(Ullong) - ret < digit)
				goto overflow;
			ret += digit;
		} else {
			break;
		}
	}
#ifdef	DO_SIGNED
	if (neg)
		ret = -ret;
#endif
	*l = ret;
	return ((char *)s);
overflow:
	for (; (c = *s) != 0; s++) {

		if (is_digit(c)) {
			digit = c - '0';
		} else if (is_lower(c)) {
			digit = c - 'a' + 10;
		} else if (is_upper(c)) {
			digit = c - 'A' + 10;
		} else {
			break;
		}
		if (digit >= base)
			break;
	}
	*l = TYPE_MAXVAL(Ullong);
	errno = ERANGE;
	return ((char *)s);
}
