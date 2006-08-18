/* @(#)defaults.h	1.1 04/02/22 Copyright 1998-2004 J. Schilling */
/*
 *	The cdrecord defaults (/etc/default/cdrecord) interface
 *
 *	Copyright (c) 1998-2004 J. Schilling
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

#ifndef	_DEFAULTS_H
#define	_DEFAULTS_H
/*
 * defaults.c
 */
extern	void	cdr_defaults	__PR((char **devp, int *speedp, long *fsp, char **drvoptp));

#endif	/* _DEFAULTS_H */
