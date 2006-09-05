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

/* @(#)default.c	1.5 04/09/04 Copyright 1997 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)default.c	1.5 04/09/04 Copyright 1997 J. Schilling";
#endif
/*
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

#include <mconfig.h>
#include <standard.h>
#include <stdio.h>
#include <strdefs.h>
#include <deflts.h>

#define	MAXLINE	512

static	FILE	*dfltfile	= (FILE *)NULL;

EXPORT	int	defltopen	__PR((const char *name));
EXPORT	int	defltclose	__PR((void));
EXPORT	void	defltfirst	__PR((void));
EXPORT	char	*defltread	__PR((const char *name));
EXPORT	char	*defltnext	__PR((const char *name));
EXPORT	int	defltcntl	__PR((int cmd, int flags));

EXPORT int
defltopen(name)
	const char	*name;
{
	if (dfltfile != (FILE *)NULL)
		fclose(dfltfile);

	if (name == (char *)NULL) {
		fclose(dfltfile);
		dfltfile = NULL;
		return (0);
	}

	if ((dfltfile = fopen(name, "r")) == (FILE *)NULL) {
		return (-1);
	}
	return (0);
}

EXPORT int
defltclose()
{
	int	ret;

	if (dfltfile != (FILE *)NULL) {
		ret = fclose(dfltfile);
		dfltfile = NULL;
		return (ret);
	}
	return (0);
}

EXPORT void
defltfirst()
{
	if (dfltfile == (FILE *)NULL) {
		return;
	}
	rewind(dfltfile);
}

EXPORT char *
defltread(name)
	const char	*name;
{
	if (dfltfile == (FILE *)NULL) {
		return ((char *)NULL);
	}
	rewind(dfltfile);
	return (defltnext(name));
}

EXPORT char *
defltnext(name)
	const char	*name;
{
	register int	len;
	register int	namelen;
	static	 char	buf[MAXLINE];

	if (dfltfile == (FILE *)NULL) {
		return ((char *)NULL);
	}
	namelen = strlen(name);

	while (fgets(buf, sizeof (buf), dfltfile)) {
		len = strlen(buf);
		if (buf[len-1] == '\n') {
			buf[len-1] = 0;
		} else {
			return ((char *)NULL);
		}
		if (strncmp(name, buf, namelen) == 0) {
			return (&buf[namelen]);
		}
	}
	return ((char *)NULL);
}

EXPORT int
defltcntl(cmd, flags)
	int	cmd;
	int	flags;
{
	int  oldflags = 0;

	return (oldflags);
}
