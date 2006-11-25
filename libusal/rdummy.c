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

/* @(#)rdummy.c	1.1 00/08/26 Copyright 2000 J. Schilling */
#ifndef lint
static	char _sccsid[] =
	"@(#)rdummy.c	1.1 00/08/26 Copyright 2000 J. Schilling";
#endif
/*
 *	usal Library 
 *	dummy remote ops
 *
 *	Copyright (c) 2000 J. Schilling
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
#include <schily.h>

#include <usal/scsitransp.h>

usal_ops_t *usal_remote(void);

EXPORT usal_ops_t *
usal_remote()
{
extern	usal_ops_t usal_remote_ops;

	return (&usal_remote_ops);
}