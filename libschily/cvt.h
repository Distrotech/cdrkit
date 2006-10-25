/*
	cvt

	Numeric conversion routines. libshily provides replacements for
	ecvt, fcvt, gcvt, __dtoa and strtod if those are not available
	in the standard C library.  #include "cvt.h" to use them.

	This file is part of the cdrkit suite.

	(C) 2006, the cdrkit project. License: GPL.

 */

#ifndef CVT_H
#define CVT_H

#include <stdlib.h>

#include "prototyp.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef HAVE_ECVT
char	*ecvt __PR((double, int, int *, int *));
#endif
#ifndef HAVE_FCVT
char	*fcvt __PR((double, int, int *, int *));
#endif
#ifndef	HAVE_GCVT
char	*gcvt __PR((double value, int ndigit, char *buf));
#endif

#ifdef	HAVE_DTOA	/* 4.4BSD floating point implementation */
#ifdef	HAVE_DTOA_R
char *__dtoa __PR((double value, int mode, int ndigit, int *decpt,
				int *sign, char **ep, char **resultp));
#else
char *__dtoa __PR((double value, int mode, int ndigit, int *decpt, int *sign,
							char **ep));
#endif
#endif

double strtod(const char *s00, char **se);

#ifdef __cplusplus
}
#endif

#endif /* CVT_H */
