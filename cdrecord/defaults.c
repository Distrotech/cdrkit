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

/* @(#)defaults.c	1.17 06/02/15 Copyright 1998-2005 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)defaults.c	1.17 06/02/15 Copyright 1998-2005 J. Schilling";
#endif
/*
 *	Copyright (c) 1998-2005 J. Schilling
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
#include <unixstd.h>
#include <strdefs.h>
#include <stdio.h>
#include <standard.h>
#include <deflts.h>
#include <utypes.h>
#include <schily.h>
#include "cdrecord.h"	/* only for getnum() */
#include "defaults.h"

LOCAL	int	open_cdrdefaults __PR((void));
EXPORT	void	cdr_defaults	__PR((char **devp, int *speedp, long *fsp, char **drvoptp));
LOCAL	void	cdr_xdefaults	__PR((char **devp, int *speedp, long *fsp, char **drvoptp));
LOCAL	char *	strsv		__PR((char *s));

LOCAL int
open_cdrdefaults()
{
	/*
	 * WARNING you are only allowed to change this filename if you also
	 * change the documentation and add a statement that makes clear
	 * where the official location of the file is why you did choose a
	 * nonstandard location and that the nonstandard location only refers
	 * to inofficial cdrecord versions.
	 *
	 * I was forced to add this because some people change cdrecord without
	 * rational reason and then publish the result. As those people
	 * don't contribute work and don't give support, they are causing extra
	 * work for me and this way slow down the cdrecord development.
	 */
	return (defltopen("/etc/default/wodim"));
}

EXPORT void
cdr_defaults(devp, speedp, fsp, drvoptp)
	char	**devp;
	int	*speedp;
	long	*fsp;
	char	**drvoptp;
{
	char	*dev	= NULL;
	int	speed	= 0;
	long	fs	= 0L;

	if (devp != NULL)
		dev = *devp;
	if (speedp != NULL)
		speed = *speedp;
	if (fsp != NULL)
		fs = *fsp;

	if (!dev && devp != NULL) {
		*devp = getenv("CDR_DEVICE");

		if (!*devp && open_cdrdefaults() == 0) {
			dev = defltread("CDR_DEVICE=");
			if (dev != NULL)
				*devp = strsv(dev);
		}
	}
	if (devp != NULL && *devp)
		cdr_xdefaults(devp, &speed, &fs, drvoptp);

	if (speed < 0) {
		char	*p = getenv("CDR_SPEED");

		if (!p) {
			if (open_cdrdefaults() == 0) {
				p = defltread("CDR_SPEED=");
			}
		}
		if (p) {
			speed = atoi(p);
			if (speed < 0 && speed != -1) {
				comerrno(EX_BAD,
					"Bad speed environment (%s).\n", p);
			}
		}
	}
	if (speed >= 0 && speedp != NULL)
		*speedp = speed;

	if (fs < 0L) {
		char	*p = getenv("CDR_FIFOSIZE");

		if (!p) {
			if (open_cdrdefaults() == 0) {
				p = defltread("CDR_FIFOSIZE=");
			}
		}
		if (p) {
			if (getnum(p, &fs) != 1) {
				comerrno(EX_BAD,
					"Bad fifo size environment (%s).\n", p);
			}
		}
	}
	if (fs > 0L && fsp != NULL) {
		char	*p = NULL;
		long	maxfs;

		if (open_cdrdefaults() == 0) {
			p = defltread("CDR_MAXFIFOSIZE=");
		}
		if (p) {
			if (getnum(p, &maxfs) != 1) {
				comerrno(EX_BAD,
					"Bad max fifo size default (%s).\n", p);
			}
			if (fs > maxfs)
				fs = maxfs;
		}
		*fsp = fs;
	}


	defltclose();
}

/*
 * All args execpt "drvoptp" are granted to be non NULL pointers.
 */
LOCAL void
cdr_xdefaults(devp, speedp, fsp, drvoptp)
	char	**devp;
	int	*speedp;
	long	*fsp;
	char	**drvoptp;
{
	char	dname[256];
	char	*p = *devp;
	char	*x = ",:/@";

	while (*x) {
		if (strchr(p, *x))
			return;
		x++;
	}
	js_snprintf(dname, sizeof (dname), "%s=", p);
	if (open_cdrdefaults() != 0)
		return;

	p = defltread(dname);
	if (p != NULL) {
		while (*p == '\t' || *p == ' ')
			p++;
		if ((x = strchr(p, '\t')) != NULL)
			*x = '\0';
		else if ((x = strchr(p, ' ')) != NULL)
			*x = '\0';
		*devp = strsv(p);
		if (x) {
			p = ++x;
			while (*p == '\t' || *p == ' ')
				p++;
			if ((x = strchr(p, '\t')) != NULL)
				*x = '\0';
			else if ((x = strchr(p, ' ')) != NULL)
				*x = '\0';
			if (*speedp < 0)
				*speedp = atoi(p);
			if (*speedp < 0 && *speedp != -1) {
				comerrno(EX_BAD,
					"Bad speed in defaults (%s).\n", p);
			}
		}
		if (x) {
			p = ++x;
			while (*p == '\t' || *p == ' ')
				p++;
			if ((x = strchr(p, '\t')) != NULL)
				*x = '\0';
			else if ((x = strchr(p, ' ')) != NULL)
				*x = '\0';
			if (*fsp < 0L) {
				if (getnum(p, fsp) != 1) {
					comerrno(EX_BAD,
					"Bad fifo size in defaults (%s).\n",
					p);
				}
			}
		}
		if (x) {
			p = ++x;
			while (*p == '\t' || *p == ' ')
				p++;
			if ((x = strchr(p, '\t')) != NULL)
				*x = '\0';
			else if ((x = strchr(p, ' ')) != NULL)
				*x = '\0';
			if (strcmp(p, "\"\"") != '\0') {
				/*
				 * Driver opts found.
				 */
				if (drvoptp && *drvoptp == NULL)
					*drvoptp = strsv(p);
			}
		}
	}
}

LOCAL char *
strsv(s)
	char	*s;
{
	char	*p;
	int len = strlen(s);

	p = malloc(len+1);
	if (p)
		strcpy(p, s);
	return (p);
}
