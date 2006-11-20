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

/* @(#)scgops.h	1.5 02/10/19 Copyright 2000 J. Schilling */
/*
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

#ifndef	_SCG_SCGOPS_H
#define	_SCG_SCGOPS_H

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct scg_ops {
	int	(*scgo_send)(SCSI *scgp);

	char *	(*scgo_version)(SCSI *scgp, int what);
#ifdef	EOF	/* stdio.h has been included */
	int	(*scgo_help)(SCSI *scgp, FILE *f);
#else
	int	(*scgo_help)(SCSI *scgp, void *f);
#endif
	int	(*scgo_open)(SCSI *scgp, char *device);
	int	(*scgo_close)(SCSI *scgp);
	long	(*scgo_maxdma)(SCSI *scgp, long amt);
	void *	(*scgo_getbuf)(SCSI *scgp, long amt);
	void	(*scgo_freebuf)(SCSI *scgp);


	BOOL	(*scgo_havebus)(SCSI *scgp, int busno);
	int	(*scgo_fileno)(SCSI *scgp, int busno, int tgt, int tlun);
	int	(*scgo_initiator_id)(SCSI *scgp);
	int	(*scgo_isatapi)(SCSI *scgp);
	int	(*scgo_reset)(SCSI *scgp, int what);
} scg_ops_t;

#define	SCGO_SEND(scgp)				(*(scgp)->ops->scgo_send)(scgp)
#define	SCGO_VERSION(scgp, what)		(*(scgp)->ops->scgo_version)(scgp, what)
#define	SCGO_HELP(scgp, f)			(*(scgp)->ops->scgo_help)(scgp, f)
#define	SCGO_OPEN(scgp, device)			(*(scgp)->ops->scgo_open)(scgp, device)
#define	SCGO_CLOSE(scgp)			(*(scgp)->ops->scgo_close)(scgp)
#define	SCGO_MAXDMA(scgp, amt)			(*(scgp)->ops->scgo_maxdma)(scgp, amt)
#define	SCGO_GETBUF(scgp, amt)			(*(scgp)->ops->scgo_getbuf)(scgp, amt)
#define	SCGO_FREEBUF(scgp)			(*(scgp)->ops->scgo_freebuf)(scgp)
#define	SCGO_HAVEBUS(scgp, busno)		(*(scgp)->ops->scgo_havebus)(scgp, busno)
#define	SCGO_FILENO(scgp, busno, tgt, tlun)	(*(scgp)->ops->scgo_fileno)(scgp, busno, tgt, tlun)
#define	SCGO_INITIATOR_ID(scgp)			(*(scgp)->ops->scgo_initiator_id)(scgp)
#define	SCGO_ISATAPI(scgp)			(*(scgp)->ops->scgo_isatapi)(scgp)
#define	SCGO_RESET(scgp, what)			(*(scgp)->ops->scgo_reset)(scgp, what)

#ifdef	__cplusplus
}
#endif

#endif	/* _SCG_SCGOPS_H */
