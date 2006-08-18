/* @(#)gdata.c	1.1 01/01/21 Copyright 2001 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)gdata.c	1.1 01/01/21 Copyright 2001 J. Schilling";
#endif

#include <mconfig.h>
#include "internal.h"

char *hfs_error = "no error";	/* static error string */
hfsvol *hfs_mounts;		/* linked list of mounted volumes */
hfsvol *hfs_curvol;		/* current volume */
