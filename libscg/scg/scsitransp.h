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

/* @(#)scsitransp.h	1.54 03/05/03 Copyright 1995 J. Schilling */
/*
 *	Definitions for commands that use functions from scsitransp.c
 *
 *	Copyright (c) 1995 J. Schilling
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

#ifndef	_SCG_SCSITRANSP_H
#define	_SCG_SCSITRANSP_H

#ifdef	__cplusplus
extern "C" {
#endif

typedef	struct scg_scsi	SCSI;

typedef struct {
	int	scsibus;	/* SCSI bus #    for next I/O		*/
	int	target;		/* SCSI target # for next I/O		*/
	int	lun;		/* SCSI lun #    for next I/O		*/
} scg_addr_t;

#ifndef	_SCG_SCGOPS_H
#include <scg/scgops.h>
#endif

typedef	int	(*scg_cb_t)(void *);

struct scg_scsi {
	scg_ops_t *ops;		/* Ptr to low level SCSI transport ops	*/
	int	fd;		/* File descriptor for next I/O		*/
	scg_addr_t	addr;	/* SCSI address for next I/O		*/
	int	flags;		/* Libscg flags (see below)		*/
	int	dflags;		/* Drive specific flags (see below)	*/
	int	kdebug;		/* Kernel debug value for next I/O	*/
	int	debug;		/* Debug value for SCSI library		*/
	int	silent;		/* Be silent if value > 0		*/
	int	verbose;	/* Be verbose if value > 0		*/
	int	overbose;	/* Be verbose in open() if value > 0	*/
	int	disre_disable;
	int	deftimeout;
	int	noparity;	/* Do not use SCSI parity fo next I/O	*/
	int	dev;		/* from scsi_cdr.c			*/
	struct scg_cmd *scmd;
	char	*cmdname;
	char	*curcmdname;
	BOOL	running;
	int	error;		/* libscg error number			*/

	long	maxdma;		/* Max DMA limit for this open instance	*/
	long	maxbuf;		/* Cur DMA buffer limit for this inst.	*/
				/* This is the size behind bufptr	*/
	struct timeval	*cmdstart;
	struct timeval	*cmdstop;
	const char	**nonstderrs;
	void	*local;		/* Local data from the low level code	*/
	void	*bufbase;	/* needed for scsi_freebuf()		*/
	void	*bufptr;	/* DMA buffer pointer for appl. use	*/
	char	*errstr;	/* Error string for scsi_open/sendmcd	*/
	char	*errbeg;	/* Pointer to begin of not flushed data	*/
	char	*errptr;	/* Actual write pointer into errstr	*/
	void	*errfile;	/* FILE to write errors to. NULL for not*/
				/* writing and leaving errs in errstr	*/
	scg_cb_t cb_fun;
	void	*cb_arg;

	struct scsi_inquiry *inq;
	struct scsi_capacity *cap;
};

/*
 * Macros for accessing members of the scg address structure.
 * scg_settarget() is the only function that is allowed to modify
 * the values of the SCSI address.
 */
#define	scg_scsibus(scgp)	(scgp)->addr.scsibus
#define	scg_target(scgp)	(scgp)->addr.target
#define	scg_lun(scgp)		(scgp)->addr.lun

/*
 * Flags for struct SCSI:
 */
/* NONE yet */

/*
 * Drive specific flags for struct SCSI:
 */
#define	DRF_MODE_DMA_OVR	0x0001		/* Drive gives DMA overrun */
						/* on mode sense	   */

#define	SCSI_ERRSTR_SIZE	4096

/*
 * Libscg error codes:
 */
#define	SCG_ERRBASE		1000000
#define	SCG_NOMEM		1000001

/*
 * Function codes for scg_version():
 */
#define	SCG_VERSION		0	/* libscg or transport version */
#define	SCG_AUTHOR		1	/* Author of above */
#define	SCG_SCCS_ID		2	/* SCCS id of above */
#define	SCG_RVERSION		10	/* Remote transport version */
#define	SCG_RAUTHOR		11	/* Remote transport author */
#define	SCG_RSCCS_ID		12	/* Remote transport SCCS ID */
#define	SCG_KVERSION		20	/* Kernel transport version */

/*
 * Function codes for scg_reset():
 */
#define	SCG_RESET_NOP		0	/* Test if reset is supported */
#define	SCG_RESET_TGT		1	/* Reset Target only */
#define	SCG_RESET_BUS		2	/* Reset complete SCSI Bus */

/*
 * Helpers for the error buffer in SCSI*
 */
#define	scg_errsize(scgp)	((scgp)->errptr - (scgp)->errstr)
#define	scg_errrsize(scgp)	(SCSI_ERRSTR_SIZE - scg_errsize(scgp))

/*
 * From scsitransp.c:
 */
extern	char	*scg_version(SCSI *scgp, int what);
extern	int	scg__open(SCSI *scgp, char *device);
extern	int	scg__close(SCSI *scgp);
extern	BOOL	scg_havebus(SCSI *scgp, int);
extern	int	scg_initiator_id(SCSI *scgp);
extern	int	scg_isatapi(SCSI *scgp);
extern	int	scg_reset(SCSI *scgp, int what);
extern	void	*scg_getbuf(SCSI *scgp, long);
extern	void	scg_freebuf(SCSI *scgp);
extern	long	scg_bufsize(SCSI *scgp, long);
extern	void	scg_setnonstderrs(SCSI *scgp, const char **);
extern	BOOL	scg_yes(char *);
extern	int	scg_cmd(SCSI *scgp);
extern	void	scg_vhead(SCSI *scgp);
extern	int	scg_svhead(SCSI *scgp, char *buf, int maxcnt);
extern	int	scg_vtail(SCSI *scgp);
extern	int	scg_svtail(SCSI *scgp, int *retp, char *buf, int maxcnt);
extern	void	scg_vsetup(SCSI *scgp);
extern	int	scg_getresid(SCSI *scgp);
extern	int	scg_getdmacnt(SCSI *scgp);
extern	BOOL	scg_cmd_err(SCSI *scgp);
extern	void	scg_printerr(SCSI *scgp);
#ifdef	EOF	/* stdio.h has been included */
extern	void	scg_fprinterr(SCSI *scgp, FILE *f);
#endif
extern	int	scg_sprinterr(SCSI *scgp, char *buf, int maxcnt);
extern	int	scg__sprinterr(SCSI *scgp, char *buf, int maxcnt);
extern	void	scg_printcdb(SCSI *scgp);
extern	int	scg_sprintcdb(SCSI *scgp, char *buf, int maxcnt);
extern	void	scg_printwdata(SCSI *scgp);
extern	int	scg_sprintwdata(SCSI *scgp, char *buf, int maxcnt);
extern	void	scg_printrdata(SCSI *scgp);
extern	int	scg_sprintrdata(SCSI *scgp, char *buf, int maxcnt);
extern	void	scg_printresult(SCSI *scgp);
extern	int	scg_sprintresult(SCSI *scgp, char *buf, int maxcnt);
extern	void	scg_printstatus(SCSI *scgp);
extern	int	scg_sprintstatus(SCSI *scgp, char *buf, int maxcnt);
#ifdef	EOF	/* stdio.h has been included */
extern	void	scg_fprbytes(FILE *, char *, unsigned char *, int);
extern	void	scg_fprascii(FILE *, char *, unsigned char *, int);
#endif
extern	void	scg_prbytes(char *, unsigned char *, int);
extern	void	scg_prascii(char *, unsigned char *, int);
extern	int	scg_sprbytes(char *buf, int maxcnt, char *, unsigned char *, int);
extern	int	scg_sprascii(char *buf, int maxcnt, char *, unsigned char *, int);
#ifdef	EOF	/* stdio.h has been included */
extern	void	scg_fprsense(FILE *f, unsigned char *, int);
#endif
extern	void	scg_prsense(unsigned char *, int);
extern	int	scg_sprsense(char *buf, int maxcnt, unsigned char *, int);
extern	int	scg_cmd_status(SCSI *scgp);
extern	int	scg_sense_key(SCSI *scgp);
extern	int	scg_sense_code(SCSI *scgp);
extern	int	scg_sense_qual(SCSI *scgp);
#ifdef	_SCG_SCSIREG_H
#ifdef	EOF	/* stdio.h has been included */
extern	void	scg_fprintdev(FILE *, struct scsi_inquiry *);
#endif
extern	void	scg_printdev(struct scsi_inquiry *);
#endif
extern	int	scg_printf(SCSI *scgp, const char *form, ...);
extern	int	scg_errflush(SCSI *scgp);
#ifdef	EOF	/* stdio.h has been included */
extern	int	scg_errfflush(SCSI *scgp, FILE *f);
#endif

/*
 * From scsierrmsg.c:
 */
extern	const char	*scg_sensemsg(int, int, int, const char **, char *, 
											  int maxcnt);
#ifdef	_SCG_SCSISENSE_H
extern	int		scg__errmsg(SCSI *scgp, char *obuf, int maxcnt,
										struct scsi_sense *, struct scsi_status *, int);
#endif

/*
 * From scsiopen.c:
 */
#ifdef	EOF	/* stdio.h has been included */
extern	int	scg_help(FILE *f);
#endif
extern	SCSI	*scg_open(char *scsidev, char *errs, int slen, int odebug, 
								 int be_verbose);
extern	int	scg_close(SCSI * scgp);
extern	void	scg_settimeout(SCSI * scgp, int timeout);
extern	SCSI	*scg_smalloc(void);
extern	void	scg_sfree(SCSI *scgp);

/*
 * From scgsettarget.c:
 */
extern	int	scg_settarget(SCSI *scgp, int scsibus, int target, int lun);

/*
 * From scsi-remote.c:
 */
extern	scg_ops_t *scg_remote(void);

/*
 * From scsihelp.c:
 */
#ifdef	EOF	/* stdio.h has been included */
extern	void	__scg_help(FILE *f, char *name, char *tcomment, char *tind,
								  char *tspec, char *texample, BOOL mayscan, 
								  BOOL bydev);
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SCG_SCSITRANSP_H */
