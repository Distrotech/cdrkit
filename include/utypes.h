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

/* @(#)utypes.h	1.15 05/11/06 Copyright 1997 J. Schilling */
/*
 *	Definitions for some user defined types
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

#ifndef	_UTYPES_H
#define	_UTYPES_H

#ifndef	_MCONFIG_H
#include <mconfig.h>
#endif

/*
 * Include limits.h for CHAR_BIT
 */
#ifdef	HAVE_LIMITS_H
#ifndef	_INCL_LIMITS_H
#include <limits.h>
#define	_INCL_LIMITS_H
#endif
#endif

/*
 * Include sys/param.h for NBBY
 */
#ifdef	HAVE_SYS_PARAM_H
#ifndef	_INCL_SYS_PARAM_H
#include	<sys/param.h>
#define	_INCL_SYS_PARAM_H
#endif
#endif

#ifndef	CHAR_BIT
#ifdef	NBBY
#define	CHAR_BIT	NBBY 
#endif
#endif

#ifndef	CHAR_BIT
#define	CHAR_BIT	8
#endif

/*
 * These macros may not work on all platforms but as we depend
 * on two's complement in many places, they do not reduce portability.
 * The macros below work with 2s complement and ones complement machines.
 * Verify with this table...
 *
 *	Bits	1's c.	2's complement.
 * 	100	-3	-4
 * 	101	-2	-3
 * 	110	-1	-2
 * 	111	-0	-1
 * 	000	+0	 0
 * 	001	+1	+1
 * 	010	+2	+2
 * 	011	+3	+3
 *
 * Computing -TYPE_MINVAL(type) will not work on 2's complement machines
 * if 'type' is int or more. Use -(UIntmax_t)TYPE_MINVAL(type), it works
 * for both 1's complement and 2's complement machines.
 */
#define	TYPE_ISSIGNED(t)	(((t)-1) < ((t)0))
#define	TYPE_ISUNSIGNED(t)	(!TYPE_ISSIGNED(t))
#define	TYPE_MSBVAL(t)		((t)(~((t)0) << (sizeof (t)*CHAR_BIT - 1)))
#define	TYPE_MINVAL(t)		(TYPE_ISSIGNED(t)			\
				    ? TYPE_MSBVAL(t)			\
				    : ((t)0))
#define	TYPE_MAXVAL(t)		((t)(~((t)0) - TYPE_MINVAL(t)))

/*
 * Let us include system defined types too.
 */
#ifndef	_INCL_SYS_TYPES_H
#include <sys/types.h>
#define	_INCL_SYS_TYPES_H
#endif

/*
 * Several unsigned cardinal types
 */
typedef	unsigned long	Ulong;
typedef	unsigned int	Uint;
typedef	unsigned short	Ushort;
typedef	unsigned char	Uchar;

/*
 * This is a definition for a compiler dependant 64 bit type.
 * It currently is silently a long if the compiler does not
 * support it. Check if this is the right way.
 */
#ifndef	NO_LONGLONG
#	if	defined(HAVE_LONGLONG)
#		define	USE_LONGLONG
#	endif
#endif

#ifdef	USE_LONGLONG

typedef	long long		Llong;
typedef	unsigned long long	Ullong;	/* We should avoid this */
typedef	unsigned long long	ULlong;

#define	SIZEOF_LLONG		SIZEOF_LONG_LONG
#define	SIZEOF_ULLONG		SIZEOF_UNSIGNED_LONG_LONG

#else

typedef	long			Llong;
typedef	unsigned long		Ullong;	/* We should avoid this */
typedef	unsigned long		ULlong;

#define	SIZEOF_LLONG		SIZEOF_LONG
#define	SIZEOF_ULLONG		SIZEOF_UNSIGNED_LONG

#endif
#ifndef	LLONG_MIN
#define	LLONG_MIN	TYPE_MINVAL(Llong)
#endif
#ifndef	LLONG_MAX
#define	LLONG_MAX	TYPE_MAXVAL(Llong)
#endif
#ifndef	ULLONG_MAX
#define	ULLONG_MAX	TYPE_MAXVAL(Ullong)
#endif

/*
 * The IBM AIX C-compiler seems to be the only compiler on the world
 * which does not allow to use unsigned char bit fields as a hint
 * for packed bit fields. Define a pesical type to avoid warnings.
 * The packed attribute is honored wit unsigned int in this case too.
 */
#if	defined(_AIX) && !defined(__GNUC__)

typedef unsigned int	Ucbit;

#else

typedef unsigned char	Ucbit;

#endif

/*
 * Start inttypes.h emulation.
 *
 * Thanks to Solaris 2.4 and even recent 1999 Linux versions, we
 * cannot use the official UNIX-98 names here. Old Solaris versions
 * define parts of the types in some exotic include files.
 * Linux even defines incompatible types in <sys/types.h>.
 */

#ifdef	HAVE_INTTYPES_H
#	ifndef	_INCL_INTTYPES_H
#	include <inttypes.h>
#	define	_INCL_INTTYPES_H
#	endif
#	define	HAVE_INT64_T
#	define	HAVE_UINT64_T

#define	Int8_t			int8_t
#define	Int16_t			int16_t
#define	Int32_t			int32_t
#define	Int64_t			int64_t
#define	Intmax_t		intmax_t
#define	UInt8_t			uint8_t
#define	UInt16_t		uint16_t
#define	UInt32_t		uint32_t
#define	UInt64_t		uint64_t
#define	UIntmax_t		uintmax_t

#define	Intptr_t		intptr_t
#define	UIntptr_t		uintptr_t

#else	/* !HAVE_INTTYPES_H */

#if SIZEOF_CHAR != 1 || SIZEOF_UNSIGNED_CHAR != 1
/*
 * #error will not work for all compilers (e.g. sunos4)
 * The following line will abort compilation on all compilers
 * if the above is true. And that's what we want.
 */
error  Sizeof char is not equal 1
#endif

typedef signed char			Int8_t;

#if SIZEOF_SHORT_INT == 2
	typedef	short			Int16_t;
#else
	error		No int16_t found
#endif

#if SIZEOF_INT == 4
	typedef	int			Int32_t;
#else
	error		No int32_t found
#endif

#if SIZEOF_LONG_INT == 8
	typedef		long		Int64_t;
#	define	HAVE_INT64_T
#else
#if SIZEOF_LONG_LONG == 8
	typedef		long long	Int64_t;
#	define	HAVE_INT64_T
#else
/*	error		No int64_t found*/
#endif
#endif

#if SIZEOF_CHAR_P == SIZEOF_INT
	typedef		int		Intptr_t;
#else
#if SIZEOF_CHAR_P == SIZEOF_LONG_INT
	typedef		long		Intptr_t;
#else
	error		No intptr_t found
#endif
#endif

typedef	unsigned char		UInt8_t;

#if SIZEOF_UNSIGNED_SHORT_INT == 2
	typedef	unsigned short		UInt16_t;
#else
	error		No uint16_t found
#endif

#if SIZEOF_UNSIGNED_INT == 4
	typedef	unsigned int		UInt32_t;
#else
	error		No int32_t found
#endif

#if SIZEOF_UNSIGNED_LONG_INT == 8
	typedef	unsigned long		UInt64_t;
#	define	HAVE_UINT64_T
#else
#if SIZEOF_UNSIGNED_LONG_LONG == 8
	typedef	unsigned long long	UInt64_t;
#	define	HAVE_UINT64_T
#else
/*	error		No uint64_t found*/
#endif
#endif

#define	Intmax_t	Llong
#define	UIntmax_t	Ullong

#if SIZEOF_CHAR_P == SIZEOF_UNSIGNED_INT
	typedef		unsigned int	UIntptr_t;
#else
#if SIZEOF_CHAR_P == SIZEOF_UNSIGNED_LONG_INT
	typedef		unsigned long	UIntptr_t;
#else
	error		No uintptr_t found
#endif
#endif

#endif	/* HAVE_INTTYPES_H */

#ifndef	CHAR_MIN
#define	CHAR_MIN	TYPE_MINVAL(char)
#endif
#ifndef	CHAR_MAX
#define	CHAR_MAX	TYPE_MAXVAL(char)
#endif
#ifndef	UCHAR_MAX
#define	UCHAR_MAX	TYPE_MAXVAL(unsigned char)
#endif

#ifndef	SHRT_MIN
#define	SHRT_MIN	TYPE_MINVAL(short)
#endif
#ifndef	SHRT_MAX
#define	SHRT_MAX	TYPE_MAXVAL(short)
#endif
#ifndef	USHRT_MAX
#define	USHRT_MAX	TYPE_MAXVAL(unsigned short)
#endif

#ifndef	INT_MIN
#define	INT_MIN		TYPE_MINVAL(int)
#endif
#ifndef	INT_MAX
#define	INT_MAX		TYPE_MAXVAL(int)
#endif
#ifndef	UINT_MAX
#define	UINT_MAX	TYPE_MAXVAL(unsigned int)
#endif

#ifndef	LONG_MIN
#define	LONG_MIN	TYPE_MINVAL(long)
#endif
#ifndef	LONG_MAX
#define	LONG_MAX	TYPE_MAXVAL(long)
#endif
#ifndef	ULONG_MAX
#define	ULONG_MAX	TYPE_MAXVAL(unsigned long)
#endif

#ifndef	INT8_MIN
#define	INT8_MIN	TYPE_MINVAL(Int8_t)
#endif
#ifndef	INT8_MAX
#define	INT8_MAX	TYPE_MAXVAL(Int8_t)
#endif
#ifndef	UINT8_MAX
#define	UINT8_MAX	TYPE_MAXVAL(UInt8_t)
#endif

#ifndef	INT16_MIN
#define	INT16_MIN	TYPE_MINVAL(Int16_t)
#endif
#ifndef	INT16_MAX
#define	INT16_MAX	TYPE_MAXVAL(Int16_t)
#endif
#ifndef	UINT16_MAX
#define	UINT16_MAX	TYPE_MAXVAL(UInt16_t)
#endif

#ifndef	INT32_MIN
#define	INT32_MIN	TYPE_MINVAL(Int32_t)
#endif
#ifndef	INT32_MAX
#define	INT32_MAX	TYPE_MAXVAL(Int32_t)
#endif
#ifndef	UINT32_MAX
#define	UINT32_MAX	TYPE_MAXVAL(UInt32_t)
#endif

#ifdef	HAVE_INT64_T
#ifndef	INT64_MIN
#define	INT64_MIN	TYPE_MINVAL(Int64_t)
#endif
#ifndef	INT64_MAX
#define	INT64_MAX	TYPE_MAXVAL(Int64_t)
#endif
#endif
#ifdef	HAVE_UINT64_T
#ifndef	UINT64_MAX
#define	UINT64_MAX	TYPE_MAXVAL(UInt64_t)
#endif
#endif

#ifndef	INTMAX_MIN
#define	INTMAX_MIN	TYPE_MINVAL(Intmax_t)
#endif
#ifndef	INTMAX_MAX
#define	INTMAX_MAX	TYPE_MAXVAL(Intmax_t)
#endif
#ifndef	UINTMAX_MAX
#define	UINTMAX_MAX	TYPE_MAXVAL(UIntmax_t)
#endif

#define	SIZE_T_MIN	TYPE_MINVAL(size_t)
#ifndef SIZE_T_MAX
#define	SIZE_T_MAX	TYPE_MAXVAL(size_t)
#endif

#define	SSIZE_T_MIN	TYPE_MINVAL(ssize_t)
#define	SSIZE_T_MAX	TYPE_MAXVAL(ssize_t)

#define	OFF_T_MIN	TYPE_MINVAL(off_t)
#define	OFF_T_MAX	TYPE_MAXVAL(off_t)

#define	UID_T_MIN	TYPE_MINVAL(uid_t)
#define	UID_T_MAX	TYPE_MAXVAL(uid_t)

#define	GID_T_MIN	TYPE_MINVAL(gid_t)
#define	GID_T_MAX	TYPE_MAXVAL(gid_t)

#define	PID_T_MIN	TYPE_MINVAL(pid_t)
#define	PID_T_MAX	TYPE_MAXVAL(pid_t)

#define	MODE_T_MIN	TYPE_MINVAL(mode_t)
#define	MODE_T_MAX	TYPE_MAXVAL(mode_t)

#define	TIME_T_MIN	TYPE_MINVAL(time_t)
#define	TIME_T_MAX	TYPE_MAXVAL(time_t)

#define	CADDR_T_MIN	TYPE_MINVAL(caddr_t)
#define	CADDR_T_MAX	TYPE_MAXVAL(caddr_t)

#define	DADDR_T_MIN	TYPE_MINVAL(daddr_t)
#define	DADDR_T_MAX	TYPE_MAXVAL(daddr_t)

#define	DEV_T_MIN	TYPE_MINVAL(dev_t)
#define	DEV_T_MAX	TYPE_MAXVAL(dev_t)

#define	MAJOR_T_MIN	TYPE_MINVAL(major_t)
#define	MAJOR_T_MAX	TYPE_MAXVAL(major_t)

#define	MINOR_T_MIN	TYPE_MINVAL(minor_t)
#define	MINOR_T_MAX	TYPE_MAXVAL(minor_t)

#define	INO_T_MIN	TYPE_MINVAL(ino_t)
#define	INO_T_MAX	TYPE_MAXVAL(ino_t)

#define	NLINK_T_MIN	TYPE_MINVAL(nlink_t)
#define	NLINK_T_MAX	TYPE_MAXVAL(nlink_t)

#define	BLKCNT_T_MIN	TYPE_MINVAL(blkcnt_t)
#define	BLKCNT_T_MAX	TYPE_MAXVAL(blkcnt_t)

#define	CLOCK_T_MIN	TYPE_MINVAL(clock_t)
#define	CLOCK_T_MAX	TYPE_MAXVAL(clock_t)

#define	SOCKLEN_T_MIN	TYPE_MINVAL(socklen_t)
#define	SOCKLEN_T_MAX	TYPE_MAXVAL(socklen_t)

#endif	/* _UTYPES_H */
