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

/* @(#)scsi_cmds.c	1.29 03/03/31 Copyright 1998-2002 Heiko Eissfeldt */
/* file for all SCSI commands
 * FUA (Force Unit Access) bit handling copied from Monty's cdparanoia.
 */
#undef	DEBUG_FULLTOC
#undef	WARN_FULLTOC
#define TESTSUBQFALLBACK	0

#include "config.h"
#include <stdio.h>
#include <standard.h>
#include <stdxlib.h>
#include <strdefs.h>
#include <schily.h>

#include <btorder.h>

#define        g5x_cdblen(cdb, len)    ((cdb)->count[0] = ((len) >> 16L)& 0xFF,\
                                (cdb)->count[1] = ((len) >> 8L) & 0xFF,\
                                (cdb)->count[2] = (len) & 0xFF)


#include <usal/usalcmd.h>
#include <usal/scsidefs.h>
#include <usal/scsireg.h>

#include <usal/scsitransp.h>

#include "mytype.h"
#include "icedax.h"
#include "interface.h"
#include "byteorder.h"
#include "global.h"
#include "wodim.h"
#include "toc.h"
#include "scsi_cmds.h"
#include "exitcodes.h"

unsigned char *bufferTOC;
subq_chnl *SubQbuffer;
unsigned char *cmd;

static unsigned ReadFullTOCSony(SCSI *usalp);
static unsigned ReadFullTOCMMC(SCSI *usalp);


int SCSI_emulated_ATAPI_on(SCSI *usalp)
{
/*	return is_atapi;*/
	if (usal_isatapi(usalp) > 0)
		return (TRUE);

	(void) allow_atapi(usalp, TRUE);
	return (allow_atapi(usalp, TRUE));
}

int heiko_mmc(SCSI *usalp)
{
        unsigned char	mode[0x100];
	int		was_atapi;
        struct  cd_mode_page_2A *mp;
	int retval;

        fillbytes((caddr_t)mode, sizeof(mode), '\0');

        was_atapi = allow_atapi(usalp, 1);
        usalp->silent++;
        mp = mmc_cap(usalp, mode);
        usalp->silent--;
        allow_atapi(usalp, was_atapi);
        if (mp == NULL)
                return (0);

        /* have a look at the capabilities */
	if (mp->cd_da_supported == 0) {
	  retval = -1;
	} else {
	  retval = 1 + mp->cd_da_accurate;
        }
	return retval;
}


int accepts_fua_bit;
unsigned char density = 0;
unsigned char orgmode4 = 0;
unsigned char orgmode10, orgmode11;

/* get current sector size from SCSI cdrom drive */
unsigned int 
get_orig_sectorsize(SCSI *usalp, unsigned char *m4, unsigned char *m10, 
                    unsigned char *m11)
{
      /* first get current values for density, etc. */

      static unsigned char *modesense = NULL;
   
      if (modesense == NULL) {
        modesense = malloc(12);
        if (modesense == NULL) {
          fprintf(stderr, "Cannot allocate memory for mode sense command in line %d\n", __LINE__);
          return 0;
        }
      }

      /* do the scsi cmd */
      if (usalp->verbose) fprintf(stderr, "\nget density and sector size...");
      if (mode_sense(usalp, modesense, 12, 0x01, 0) < 0)
	  fprintf(stderr, "get_orig_sectorsize mode sense failed\n");

      /* FIXME: some drives dont deliver block descriptors !!! */
      if (modesense[3] == 0)
        return 0;

#if	0
	modesense[4] = 0x81;
	modesense[10] = 0x08;
	modesense[11] = 0x00;
#endif

      if (m4 != NULL)                       /* density */
        *m4 = modesense[4];
      if (m10 != NULL)                      /* MSB sector size */
        *m10 = modesense[10];
      if (m11 != NULL)                      /* LSB sector size */
        *m11 = modesense[11];

      return (modesense[10] << 8) + modesense[11];
}



/* switch CDROM scsi drives to given sector size  */
int set_sectorsize(SCSI *usalp, unsigned int secsize)
{
  static unsigned char mode [4 + 8];
  int retval;

  if (orgmode4 == 0xff) {
    get_orig_sectorsize(usalp, &orgmode4, &orgmode10, &orgmode11);
  }
  if (orgmode4 == 0x82 && secsize == 2048)
    orgmode4 = 0x81;

  /* prepare to read cds in the previous mode */

  fillbytes((caddr_t)mode, sizeof(mode), '\0');
  mode[ 3] = 8; 	       /* Block Descriptor Length */
  mode[ 4] = orgmode4; 	       /* normal density */
  mode[10] =  secsize >> 8;   /* block length "msb" */
  mode[11] =  secsize & 0xFF; /* block length lsb */

  if (usalp->verbose) fprintf(stderr, "\nset density and sector size...");
  /* do the scsi cmd */
  if ((retval = mode_select(usalp, mode, 12, 0, usalp->inq->data_format >= 2)) < 0)
        fprintf (stderr, "setting sector size failed\n");

  return retval;
}


/* switch Toshiba/DEC and HP drives from/to cdda density */
void EnableCddaModeSelect(SCSI *usalp, int fAudioMode, unsigned uSectorsize)
{
  /* reserved, Medium type=0, Dev spec Parm = 0, block descriptor len 0 oder 8,
     Density (cd format) 
     (0=YellowBook, XA Mode 2=81h, XA Mode1=83h and raw audio via SCSI=82h),
     # blks msb, #blks, #blks lsb, reserved,
     blocksize, blocklen msb, blocklen lsb,
   */

  /* MODE_SELECT, page = SCSI-2  save page disabled, reserved, reserved, 
     parm list len, flags */
  static unsigned char mode [4 + 8] = { 
       /* mode section */
			    0, 
                            0, 0, 
                            8,       /* Block Descriptor Length */
       /* block descriptor */
                            0,       /* Density Code */
                            0, 0, 0, /* # of Blocks */
                            0,       /* reserved */
                            0, 0, 0};/* Blocklen */

  if (orgmode4 == 0 && fAudioMode) {
    if (0 == get_orig_sectorsize(usalp, &orgmode4, &orgmode10, &orgmode11)) {
        /* cannot retrieve density, sectorsize */
	orgmode10 = (CD_FRAMESIZE >> 8L);
	orgmode11 = (CD_FRAMESIZE & 0xFF);
    }
  }

  if (fAudioMode) {
    /* prepare to read audio cdda */
    mode [4] = density;  			/* cdda density */
    mode [10] = (uSectorsize >> 8L);   /* block length "msb" */
    mode [11] = (uSectorsize & 0xFF);  /* block length "lsb" */
  } else {
    /* prepare to read cds in the previous mode */
    mode [4] = orgmode4; /* 0x00; 			\* normal density */
    mode [10] = orgmode10; /* (CD_FRAMESIZE >> 8L);  \* block length "msb" */
    mode [11] = orgmode11; /* (CD_FRAMESIZE & 0xFF); \* block length lsb */
  }

  if (usalp->verbose) fprintf(stderr, "\nset density/sector size (EnableCddaModeSelect)...\n");
  /* do the scsi cmd */
  if (mode_select(usalp, mode, 12, 0, usalp->inq->data_format >= 2) < 0)
        fprintf (stderr, "Audio mode switch failed\n");
}


/* read CD Text information from the table of contents */
void ReadTocTextSCSIMMC(SCSI *usalp)
{
    short datalength;

#if 1
  /* READTOC, MSF, format, res, res, res, Start track/session, len msb,
     len lsb, control */
	unsigned char *p = bufferTOC;
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof(*scmd), '\0');
        scmd->addr = (caddr_t)bufferTOC;
        scmd->size = 4;
        scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
        scmd->cdb_len = SC_G1_CDBLEN;
        scmd->sense_len = CCS_SENSE_LEN;
        scmd->cdb.g1_cdb.cmd = 0x43;		/* Read TOC command */
        scmd->cdb.g1_cdb.lun = usal_lun(usalp);
        scmd->cdb.g1_cdb.addr[0] = 5;		/* format field */
        scmd->cdb.g1_cdb.res6 = 0;	/* track/session is reserved */
        g1_cdblen(&scmd->cdb.g1_cdb, 4);

        usalp->silent++;
        if (usalp->verbose) fprintf(stderr, "\nRead TOC CD Text size ...");

	usalp->cmdname = "read toc size (text)";

        if (usal_cmd(usalp) < 0) {
          usalp->silent--;
	  if (global.quiet != 1)
            fprintf (stderr, "Read TOC CD Text failed (probably not supported).\n");
	  p[0] = p[1] = '\0';
          return ;
        }
        usalp->silent--;

	datalength  = (p[0] << 8) | (p[1]);
	if (datalength <= 2)
		return;

	fillbytes((caddr_t)scmd, sizeof(*scmd), '\0');
        scmd->addr = (caddr_t)bufferTOC;
        scmd->size = 2+datalength;
        scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
        scmd->cdb_len = SC_G1_CDBLEN;
        scmd->sense_len = CCS_SENSE_LEN;
        scmd->cdb.g1_cdb.cmd = 0x43;		/* Read TOC command */
        scmd->cdb.g1_cdb.lun = usal_lun(usalp);
        scmd->cdb.g1_cdb.addr[0] = 5;		/* format field */
        scmd->cdb.g1_cdb.res6 = 0;	/* track/session is reserved */
        g1_cdblen(&scmd->cdb.g1_cdb, 2+datalength);

        usalp->silent++;
        if (usalp->verbose) fprintf(stderr, "\nRead TOC CD Text data (length %hd)...", 2+datalength);

	usalp->cmdname = "read toc data (text)";

        if (usal_cmd(usalp) < 0) {
          usalp->silent--;
	  if (global.quiet != 1)
            fprintf (stderr,  "Read TOC CD Text data failed (probably not supported).\n");
	  p[0] = p[1] = '\0';
          return ;
        }
        usalp->silent--;
#else
	{ FILE *fp;
	int read_;
	/*fp = fopen("PearlJam.cdtext", "rb");*/
	/*fp = fopen("celine.cdtext", "rb");*/
	fp = fopen("japan.cdtext", "rb");
	if (fp == NULL) { perror(""); return; }
	fillbytes(bufferTOC, CD_FRAMESIZE, '\0');
	read_ = fread(bufferTOC, 1, CD_FRAMESIZE, fp );
fprintf(stderr, "read %d bytes. sizeof(bufferTOC)=%u\n", read_, CD_FRAMESIZE);
        datalength  = (bufferTOC[0] << 8) | (bufferTOC[1]);
	fclose(fp);
	}
#endif
}

/* read the full TOC */
static unsigned ReadFullTOCSony(SCSI *usalp)
{
  /* READTOC, MSF, format, res, res, res, Start track/session, len msb,
     len lsb, control */
	register struct	usal_cmd	*scmd = usalp->scmd;
	unsigned tracks = 99;

	fillbytes((caddr_t)scmd, sizeof(*scmd), '\0');
        scmd->addr = (caddr_t)bufferTOC;
        scmd->size = 4 + (3 + tracks + 6) * 11;
        scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
        scmd->cdb_len = SC_G1_CDBLEN;
        scmd->sense_len = CCS_SENSE_LEN;
        scmd->cdb.g1_cdb.cmd = 0x43;		/* Read TOC command */
        scmd->cdb.g1_cdb.lun = usal_lun(usalp);
        scmd->cdb.g1_cdb.res6 = 1;    		/* session */
        g1_cdblen(&scmd->cdb.g1_cdb, 4 + (3 + tracks + 6) * 11);
        scmd->cdb.g1_cdb.vu_97 = 1;   		/* format */

        usalp->silent++;
        if (usalp->verbose) fprintf(stderr, "\nRead Full TOC Sony ...");

	usalp->cmdname = "read full toc sony";

        if (usal_cmd(usalp) < 0) {
          usalp->silent--;
	  if (global.quiet != 1)
            fprintf (stderr, "Read Full TOC Sony failed (probably not supported).\n");
          return 0;
        }
        usalp->silent--;

	return (unsigned)((bufferTOC[0] << 8) | bufferTOC[1]);
}

struct msf_address {
	unsigned char	mins;
	unsigned char	secs;
	unsigned char	frame;
};

struct zmsf_address {
	unsigned char	zero;
	unsigned char	mins;
	unsigned char	secs;
	unsigned char	frame;
};

#ifdef	WARN_FULLTOC
static unsigned lba(struct msf_address *ad);

static unsigned lba(struct msf_address *ad)
{
	return	ad->mins*60*75 + ad->secs*75 + ad->frame;
}
#endif

static unsigned dvd_lba(struct zmsf_address *ad);

static unsigned dvd_lba(struct zmsf_address *ad)
{
	return	ad->zero*1053696 + ad->mins*60*75 + ad->secs*75 + ad->frame;
}

struct tocdesc {
	unsigned char	session;
	unsigned char	adrctl;
	unsigned char	tno;
	unsigned char	point;
	struct msf_address	adr1;
	struct zmsf_address	padr2;
};

struct outer {
	unsigned char	len_msb;
	unsigned char	len_lsb;
	unsigned char	first_track;
	unsigned char	last_track;
	struct tocdesc ent[1];
};

static unsigned long first_session_leadout = 0;

static unsigned collect_tracks(struct outer *po, unsigned entries, 
										 BOOL bcd_flag);

static unsigned collect_tracks(struct outer *po, unsigned entries, 
                               BOOL bcd_flag)
{
	unsigned tracks = 0;
	int i;
	unsigned session;
	unsigned last_start;
	unsigned leadout_start_orig;
	unsigned leadout_start;
	unsigned max_leadout = 0;

#ifdef	DEBUG_FULLTOC
	for (i = 0; i < entries; i++) {
fprintf(stderr, "%3d: %d %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n" 
	,i
	,bufferTOC[4+0 + (i * 11)]
	,bufferTOC[4+1 + (i * 11)]
	,bufferTOC[4+2 + (i * 11)]
	,bufferTOC[4+3 + (i * 11)]
	,bufferTOC[4+4 + (i * 11)]
	,bufferTOC[4+5 + (i * 11)]
	,bufferTOC[4+6 + (i * 11)]
	,bufferTOC[4+7 + (i * 11)]
	,bufferTOC[4+8 + (i * 11)]
	,bufferTOC[4+9 + (i * 11)]
	,bufferTOC[4+10 + (i * 11)]
	);
	}
#endif
	/* reformat to standard toc format */

	bufferTOC[2] = 0;
	bufferTOC[3] = 0;
	session = 0;
	last_start = 0;
	leadout_start_orig = 0;
	leadout_start = 0;

	for (i = 0; i < entries; i++) {
#ifdef	WARN_FULLTOC
		if (po->ent[i].tno != 0) {
			fprintf(stderr,
"entry %d, tno is not 0: %d!\n",
i, po->ent[i].tno);
		}
#endif
		if (bcd_flag) {
			po->ent[i].session     = from_bcd(po->ent[i].session);
			po->ent[i].adr1.mins    = from_bcd(po->ent[i].adr1.mins);
			po->ent[i].adr1.secs    = from_bcd(po->ent[i].adr1.secs);
			po->ent[i].adr1.frame  = from_bcd(po->ent[i].adr1.frame);
			po->ent[i].padr2.mins   = from_bcd(po->ent[i].padr2.mins);
			po->ent[i].padr2.secs   = from_bcd(po->ent[i].padr2.secs);
			po->ent[i].padr2.frame = from_bcd(po->ent[i].padr2.frame);
		}
		switch (po->ent[i].point) {
		case	0xa0:

			/* check if session is monotonous increasing */

			if (session+1 == po->ent[i].session) {
				session = po->ent[i].session;
			}
#ifdef	WARN_FULLTOC
			else fprintf(stderr,
"entry %d, session anomaly %d != %d!\n",
i, session+1, po->ent[i].session);

			/* check the adrctl field */
			if (0x10 != (po->ent[i].adrctl & 0x10)) {
				fprintf(stderr,
"entry %d, incorrect adrctl field %x!\n",
i, po->ent[i].adrctl);
			}
#endif
			/* first track number */
			if (bufferTOC[2] < po->ent[i].padr2.mins
			    && bufferTOC[3] < po->ent[i].padr2.mins) {
				bufferTOC[2] = po->ent[i].padr2.mins;
			}
#ifdef	WARN_FULLTOC
			else
				fprintf(stderr,
"entry %d, session %d: start tracknumber anomaly: %d <= %d,%d(last)!\n",
i, session, po->ent[i].padr2.mins, bufferTOC[2], bufferTOC[3]);
#endif
			break;

		case	0xa1:
#ifdef	WARN_FULLTOC
			/* check if session is constant */
			if (session != po->ent[i].session) {
				fprintf(stderr,
"entry %d, session anomaly %d != %d!\n",
i, session, po->ent[i].session);
			}

			/* check the adrctl field */
			if (0x10 != (po->ent[i].adrctl & 0x10)) {
				fprintf(stderr,
"entry %d, incorrect adrctl field %x!\n",
i, po->ent[i].adrctl);
			}
#endif
			/* last track number */
			if (bufferTOC[2] <= po->ent[i].padr2.mins
			    && bufferTOC[3] < po->ent[i].padr2.mins) {
				bufferTOC[3] = po->ent[i].padr2.mins;
			}
#ifdef	WARN_FULLTOC
			else
				fprintf(stderr,
"entry %d, session %d: end tracknumber anomaly: %d <= %d,%d(last)!\n",
i, session, po->ent[i].padr2.mins, bufferTOC[2], bufferTOC[3]);
#endif
			break;

		case	0xa2:
#ifdef	WARN_FULLTOC
			/* check if session is constant */
			if (session != po->ent[i].session) {
				fprintf(stderr,
"entry %d, session anomaly %d != %d!\n",
i, session, po->ent[i].session);
			}

			/* check the adrctl field */
			if (0x10 != (po->ent[i].adrctl & 0x10)) {
				fprintf(stderr,
"entry %d, incorrect adrctl field %x!\n",
i, po->ent[i].adrctl);
			}
#endif
			/* register leadout position */
		{
			unsigned leadout_start_tmp  = 
				dvd_lba(&po->ent[i].padr2);

			if (first_session_leadout  == 0)
				first_session_leadout = leadout_start_tmp - 150;

			if (leadout_start_tmp > leadout_start) {
				leadout_start_orig = leadout_start_tmp;
				leadout_start = leadout_start_tmp;
			}
#ifdef	WARN_FULLTOC
			else
				fprintf(stderr,
"entry %d, leadout position anomaly %u!\n",
i, leadout_start_tmp);
#endif
		}
			break;

		case	0xb0:
#ifdef	WARN_FULLTOC
			/* check if session is constant */
			if (session != po->ent[i].session) {
				fprintf(stderr,
"entry %d, session anomaly %d != %d!\n",
i, session, po->ent[i].session);
			}

			/* check the adrctl field */
			if (0x50 != (po->ent[i].adrctl & 0x50)) {
				fprintf(stderr,
"entry %d, incorrect adrctl field %x!\n",
i, po->ent[i].adrctl);
			}

			/* check the next program area */
			if (lba(&po->ent[i].adr1) < 6750 + leadout_start) {
				fprintf(stderr,
"entry %d, next program area %u < leadout_start + 6750 = %u!\n",
i, lba(&po->ent[i].adr1), 6750 + leadout_start);
			}

			/* check the maximum leadout_start */
			if (max_leadout != 0 && dvd_lba(&po->ent[i].padr2) != max_leadout) {
				fprintf(stderr,
"entry %d, max leadout_start %u != last max_leadout_start %u!\n",
i, dvd_lba(&po->ent[i].padr2), max_leadout);
			}
#endif
			if (max_leadout == 0)
				max_leadout = dvd_lba(&po->ent[i].padr2);

			break;
		case	0xb1:
		case	0xb2:
		case	0xb3:
		case	0xb4:
		case	0xb5:
		case	0xb6:
			break;
		case	0xc0:
		case	0xc1:
			break;
		default:
			/* check if session is constant */
			if (session != po->ent[i].session) {
#ifdef	WARN_FULLTOC
				fprintf(stderr,
"entry %d, session anomaly %d != %d!\n",
i, session, po->ent[i].session);
#endif
				continue;
			}

			/* check tno */
			if (bcd_flag)
				po->ent[i].point  = from_bcd(po->ent[i].point);

			if (po->ent[i].point < bufferTOC[2]
			    || po->ent[i].point > bufferTOC[3]) {
#ifdef	WARN_FULLTOC
				fprintf(stderr,
"entry %d, track number anomaly %d - %d - %d!\n",
i, bufferTOC[2], po->ent[i].point, bufferTOC[3]);
#endif
			} else {
				/* check start position */
				unsigned trackstart = dvd_lba(&po->ent[i].padr2);

				/* correct illegal leadouts */
				if (leadout_start < trackstart) {
					leadout_start = trackstart+1;
				}
				if (trackstart < last_start || trackstart >= leadout_start) {
#ifdef	WARN_FULLTOC
					fprintf(stderr,
"entry %d, track %d start position anomaly %d - %d - %d!\n",
i, po->ent[i].point, last_start, trackstart, leadout_start);
#endif
				} else {
					last_start = trackstart;
					memcpy(&po->ent[tracks], &po->ent[i], sizeof(struct tocdesc));
					tracks++;
				}
			}
		}	/* switch */
	}	/* for */

	/* patch leadout track */
	po->ent[tracks].session = session;
	po->ent[tracks].adrctl = 0x10;
	po->ent[tracks].tno = 0;
	po->ent[tracks].point = 0xAA;
	po->ent[tracks].adr1.mins = 0;
	po->ent[tracks].adr1.secs = 0;
	po->ent[tracks].adr1.frame = 0;
	po->ent[tracks].padr2.zero = leadout_start_orig / (1053696);
	po->ent[tracks].padr2.mins = (leadout_start_orig / (60*75)) % 100;
	po->ent[tracks].padr2.secs = (leadout_start_orig / 75) % 60;
	po->ent[tracks].padr2.frame = leadout_start_orig % 75;
	tracks++;

	/* length */
	bufferTOC[0] = ((tracks * 8) + 2) >> 8;
	bufferTOC[1] = ((tracks * 8) + 2) & 0xff;


	/* reformat 11 byte blocks to 8 byte entries */

	/* 1: Session	\	/	reserved
	   2: adr ctrl	|	|	adr ctrl
	   3: TNO	|	|	track number
	   4: Point	|	|	reserved
	   5: Min	+-->----+	0
	   6: Sec	|	|	Min
	   7: Frame	|	|	Sec
	   8: Zero	|	\	Frame
	   9: PMin	|
	   10: PSec	|
	   11: PFrame	/
	*/
	for (i = 0; i < tracks; i++) {
		bufferTOC[4+0 + (i << 3)] = 0;
		bufferTOC[4+1 + (i << 3)] = bufferTOC[4+1 + (i*11)];
		bufferTOC[4+1 + (i << 3)] = (bufferTOC[4+1 + (i << 3)] >> 4) | (bufferTOC[4+1 + (i << 3)] << 4);
		bufferTOC[4+2 + (i << 3)] = bufferTOC[4+3 + (i*11)];
		bufferTOC[4+3 + (i << 3)] = 0;
		bufferTOC[4+4 + (i << 3)] = bufferTOC[4+7 + (i*11)];
		bufferTOC[4+5 + (i << 3)] = bufferTOC[4+8 + (i*11)];
		bufferTOC[4+6 + (i << 3)] = bufferTOC[4+9 + (i*11)];
		bufferTOC[4+7 + (i << 3)] = bufferTOC[4+10 + (i*11)];
#ifdef	DEBUG_FULLTOC
fprintf(stderr, "%02x %02x %02x %02x %02x %02x\n"
	,bufferTOC[4+ 1 + i*8]
	,bufferTOC[4+ 2 + i*8]
	,bufferTOC[4+ 4 + i*8]
	,bufferTOC[4+ 5 + i*8]
	,bufferTOC[4+ 6 + i*8]
	,bufferTOC[4+ 7 + i*8]
);
#endif
	}

	TOC_entries(tracks, NULL, bufferTOC+4, 0);
	return tracks;
}

/* read the table of contents from the cd and fill the TOC array */
unsigned ReadTocSony(SCSI *usalp)
{
	unsigned tracks = 0;
	unsigned return_length;

	struct outer *po = (struct outer *)bufferTOC;

	return_length = ReadFullTOCSony(usalp);

	/* Check if the format was understood */
	if ((return_length & 7) == 2 && (bufferTOC[3] - bufferTOC[2]) == (return_length >> 3)) {
		/* The extended format seems not be understood, fallback to
		 * the classical format. */
		return ReadTocSCSI( usalp );
	}

	tracks = collect_tracks(po, ((return_length - 2) / 11), TRUE);

	return --tracks;           /* without lead-out */
}

/* read the start of the lead-out from the first session TOC */
unsigned ReadFirstSessionTOCSony(SCSI *usalp)
{
	unsigned return_length;
	
	if (first_session_leadout != 0)
		return first_session_leadout;

	return_length = ReadFullTOCSony(usalp);
        if (return_length >= 4 + (3 * 11) -2) {
          unsigned off;

          /* We want the entry with POINT = 0xA2, which has the start position
             of the first session lead out */
          off = 4 + 2 * 11 + 3;
          if (bufferTOC[off-3] == 1 && bufferTOC[off] == 0xA2) {
            unsigned retval;

            off = 4 + 2 * 11 + 8;
            retval = bufferTOC[off] >> 4;
	    retval *= 10; retval += bufferTOC[off] & 0xf;
	    retval *= 60;
	    off++;
            retval += 10 * (bufferTOC[off] >> 4) + (bufferTOC[off] & 0xf);
	    retval *= 75;
	    off++;
            retval += 10 * (bufferTOC[off] >> 4) + (bufferTOC[off] & 0xf);
	    retval -= 150;

            return retval;
          }
        }
        return 0;
}

/* read the full TOC */
static unsigned ReadFullTOCMMC(SCSI *usalp)
{

  /* READTOC, MSF, format, res, res, res, Start track/session, len msb,
     len lsb, control */
	register struct	usal_cmd	*scmd = usalp->scmd;
	unsigned tracks = 99;

	fillbytes((caddr_t)scmd, sizeof(*scmd), '\0');
        scmd->addr = (caddr_t)bufferTOC;
        scmd->size = 4 + (tracks + 8) * 11;
        scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
        scmd->cdb_len = SC_G1_CDBLEN;
        scmd->sense_len = CCS_SENSE_LEN;
        scmd->cdb.g1_cdb.cmd = 0x43;		/* Read TOC command */
        scmd->cdb.g1_cdb.lun = usal_lun(usalp);
        scmd->cdb.g1_cdb.addr[0] = 2;		/* format */
        scmd->cdb.g1_cdb.res6 = 1;		/* session */
        g1_cdblen(&scmd->cdb.g1_cdb, 4 + (tracks + 8) * 11);

        usalp->silent++;
        if (usalp->verbose) fprintf(stderr, "\nRead Full TOC MMC...");

	usalp->cmdname = "read full toc mmc";

        if (usal_cmd(usalp) < 0) {
	  if (global.quiet != 1)
            fprintf (stderr, "Read Full TOC MMC failed (probably not supported).\n");
#ifdef	B_BEOS_VERSION
#else
          usalp->silent--;
          return 0;
#endif
        }
        usalp->silent--;

	return (unsigned)((bufferTOC[0] << 8) | bufferTOC[1]);
}

/* read the start of the lead-out from the first session TOC */
unsigned ReadFirstSessionTOCMMC(SCSI *usalp)
{
        unsigned off;
	unsigned return_length;

	if (first_session_leadout != 0)
		return first_session_leadout;

	return_length = ReadFullTOCMMC(usalp);

        /* We want the entry with POINT = 0xA2, which has the start position
             of the first session lead out */
        off = 4 + 3;
        while (off < return_length && bufferTOC[off] != 0xA2) {
          off += 11;
        }
        if (off < return_length) {
          off += 5;
          return (bufferTOC[off]*60 + bufferTOC[off+1])*75 + bufferTOC[off+2] - 150;
        }
        return 0;
}

/* read the table of contents from the cd and fill the TOC array */
unsigned ReadTocMMC(SCSI *usalp)
{
	unsigned tracks = 0;
	unsigned return_length;

	struct outer *po = (struct outer *)bufferTOC;

	return_length = ReadFullTOCMMC(usalp);
	if (return_length - 2 < 4*11 || ((return_length - 2) % 11) != 0)
		return ReadTocSCSI(usalp);

	tracks = collect_tracks(po, ((return_length - 2) / 11), FALSE);
	return --tracks;           /* without lead-out */
}

/* read the table of contents from the cd and fill the TOC array */
unsigned ReadTocSCSI(SCSI *usalp)
{
    unsigned tracks;
    int	result;
    unsigned char bufferTOCMSF[CD_FRAMESIZE];

    /* first read the first and last track number */
    /* READTOC, MSF format flag, res, res, res, res, Start track, len msb,
       len lsb, flags */
    register struct	usal_cmd	*scmd = usalp->scmd;

    fillbytes((caddr_t)scmd, sizeof(*scmd), '\0');
    scmd->addr = (caddr_t)bufferTOC;
    scmd->size = 4;
    scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
    scmd->cdb_len = SC_G1_CDBLEN;
    scmd->sense_len = CCS_SENSE_LEN;
    scmd->cdb.g1_cdb.cmd = 0x43;		/* read TOC command */
    scmd->cdb.g1_cdb.lun = usal_lun(usalp);
    scmd->cdb.g1_cdb.res6 = 1;		/* start track */
    g1_cdblen(&scmd->cdb.g1_cdb, 4);

    if (usalp->verbose) fprintf(stderr, "\nRead TOC size (standard)...");
    /* do the scsi cmd (read table of contents) */

    usalp->cmdname = "read toc size";
    if (usal_cmd(usalp) < 0)
	FatalError ("Read TOC size failed.\n");
    

    tracks = ((bufferTOC [3] ) - bufferTOC [2] + 2) ;
    if (tracks > MAXTRK) return 0;
    if (tracks == 0) return 0;
    
    
    memset(bufferTOCMSF, 0, sizeof(bufferTOCMSF));
    fillbytes((caddr_t)scmd, sizeof(*scmd), '\0');
    scmd->addr = (caddr_t)bufferTOCMSF;
    scmd->size = 4 + tracks * 8;
    scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
    scmd->cdb_len = SC_G1_CDBLEN;
    scmd->sense_len = CCS_SENSE_LEN;
    scmd->cdb.g1_cdb.cmd = 0x43;		/* read TOC command */
    scmd->cdb.g1_cdb.lun = usal_lun(usalp);
    scmd->cdb.g1_cdb.res = 1;		/* MSF format */
    scmd->cdb.g1_cdb.res6 = 1;		/* start track */
    g1_cdblen(&scmd->cdb.g1_cdb, 4 + tracks * 8);

    if (usalp->verbose) fprintf(stderr, "\nRead TOC tracks (standard MSF)...");
    /* do the scsi cmd (read table of contents) */

    usalp->cmdname = "read toc tracks ";
    result = usal_cmd(usalp);

    if (result < 0) {
	/* MSF format did not succeeded */
	memset(bufferTOCMSF, 0, sizeof(bufferTOCMSF));
    } else {
	int	i;
	for (i = 0; i < tracks; i++) {
		bufferTOCMSF[4+1 + (i << 3)] = (bufferTOCMSF[4+1 + (i << 3)] >> 4) | (bufferTOCMSF[4+1 + (i << 3)] << 4);
#if	0
fprintf(stderr, "MSF %d %02x %02x %02x %02x %02x %02x %02x %02x\n" 
	,i
	,bufferTOCMSF[4+0 + (i * 8)]
	,bufferTOCMSF[4+1 + (i * 8)]
	,bufferTOCMSF[4+2 + (i * 8)]
	,bufferTOCMSF[4+3 + (i * 8)]
	,bufferTOCMSF[4+4 + (i * 8)]
	,bufferTOCMSF[4+5 + (i * 8)]
	,bufferTOCMSF[4+6 + (i * 8)]
	,bufferTOCMSF[4+7 + (i * 8)]
	);
#endif
	}
    }

    /* LBA format for cd burners like Philips CD-522 */
    fillbytes((caddr_t)scmd, sizeof(*scmd), '\0');
    scmd->addr = (caddr_t)bufferTOC;
    scmd->size = 4 + tracks * 8;
    scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
    scmd->cdb_len = SC_G1_CDBLEN;
    scmd->sense_len = CCS_SENSE_LEN;
    scmd->cdb.g1_cdb.cmd = 0x43;		/* read TOC command */
    scmd->cdb.g1_cdb.lun = usal_lun(usalp);
    scmd->cdb.g1_cdb.res = 0;		/* LBA format */
    scmd->cdb.g1_cdb.res6 = 1;		/* start track */
    g1_cdblen(&scmd->cdb.g1_cdb, 4 + tracks * 8);

    if (usalp->verbose) fprintf(stderr, "\nRead TOC tracks (standard LBA)...");
    /* do the scsi cmd (read table of contents) */

    usalp->cmdname = "read toc tracks ";
    if (usal_cmd(usalp) < 0) {
	FatalError ("Read TOC tracks (lba) failed.\n");
    }
    {
	int	i;
	for (i = 0; i < tracks; i++) {
		bufferTOC[4+1 + (i << 3)] = (bufferTOC[4+1 + (i << 3)] >> 4) | (bufferTOC[4+1 + (i << 3)] << 4);
#if	0
fprintf(stderr, "LBA %d %02x %02x %02x %02x %02x %02x %02x %02x\n" 
	,i
	,bufferTOC[4+0 + (i * 8)]
	,bufferTOC[4+1 + (i * 8)]
	,bufferTOC[4+2 + (i * 8)]
	,bufferTOC[4+3 + (i * 8)]
	,bufferTOC[4+4 + (i * 8)]
	,bufferTOC[4+5 + (i * 8)]
	,bufferTOC[4+6 + (i * 8)]
	,bufferTOC[4+7 + (i * 8)]
	);
#endif
	}
    }
    TOC_entries(tracks, bufferTOC+4, bufferTOCMSF+4, result);
    return --tracks;           /* without lead-out */
}

/* ---------------- Read methods ------------------------------ */

/* Read max. SectorBurst of cdda sectors to buffer
   via standard SCSI-2 Read(10) command */
static int ReadStandardLowlevel(SCSI *usalp, UINT4 *p, unsigned lSector, 
										  unsigned SectorBurstVal, unsigned secsize);

static int ReadStandardLowlevel(SCSI *usalp, UINT4 *p, unsigned lSector, 
										  unsigned SectorBurstVal, unsigned secsize)
{
  /* READ10, flags, block1 msb, block2, block3, block4 lsb, reserved, 
     transfer len msb, transfer len lsb, block addressing mode */
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof(*scmd), '\0');
        scmd->addr = (caddr_t)p;
        scmd->size = SectorBurstVal * secsize;
        scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
        scmd->cdb_len = SC_G1_CDBLEN;
        scmd->sense_len = CCS_SENSE_LEN;
        scmd->cdb.g1_cdb.cmd = 0x28;		/* read 10 command */
        scmd->cdb.g1_cdb.lun = usal_lun(usalp);
	scmd->cdb.g1_cdb.res |= (accepts_fua_bit == 1 ? 1 << 2 : 0);
        g1_cdbaddr(&scmd->cdb.g1_cdb, lSector);
        g1_cdblen(&scmd->cdb.g1_cdb, SectorBurstVal);
        if (usalp->verbose) fprintf(stderr, "\nReadStandard10 %s (%u)...", secsize > 2048 ? "CDDA" : "CD_DATA", secsize);

	usalp->cmdname = "ReadStandard10";

	if (usal_cmd(usalp)) return 0;

	/* has all or something been read? */
	return SectorBurstVal - usal_getresid(usalp)/secsize;
}


int 
ReadStandard(SCSI *usalp, UINT4 *p, unsigned lSector, unsigned SectorBurstVal)
{
	return ReadStandardLowlevel(usalp, p, lSector, SectorBurstVal, CD_FRAMESIZE_RAW);
}

int 
ReadStandardData(SCSI *usalp, UINT4 *p, unsigned lSector, unsigned SectorBurstVal)
{
	return ReadStandardLowlevel(usalp, p, lSector, SectorBurstVal, CD_FRAMESIZE);
}

/* Read max. SectorBurst of cdda sectors to buffer
   via vendor-specific ReadCdda(10) command */
int ReadCdda10(SCSI *usalp, UINT4 *p, unsigned lSector, unsigned SectorBurstVal)
{
  /* READ10, flags, block1 msb, block2, block3, block4 lsb, reserved, 
     transfer len msb, transfer len lsb, block addressing mode */
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof(*scmd), '\0');
        scmd->addr = (caddr_t)p;
        scmd->size = SectorBurstVal*CD_FRAMESIZE_RAW;
        scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
        scmd->cdb_len = SC_G1_CDBLEN;
        scmd->sense_len = CCS_SENSE_LEN;
        scmd->cdb.g1_cdb.cmd = 0xd4;		/* Read audio command */
        scmd->cdb.g1_cdb.lun = usal_lun(usalp);
	scmd->cdb.g1_cdb.res |= (accepts_fua_bit == 1 ? 1 << 2 : 0);
        g1_cdbaddr(&scmd->cdb.g1_cdb, lSector);
        g1_cdblen(&scmd->cdb.g1_cdb, SectorBurstVal);
        if (usalp->verbose) fprintf(stderr, "\nReadNEC10 CDDA...");

	usalp->cmdname = "Read10 NEC";

	if (usal_cmd(usalp)) return 0;

	/* has all or something been read? */
	return SectorBurstVal - usal_getresid(usalp)/CD_FRAMESIZE_RAW;
}


/* Read max. SectorBurst of cdda sectors to buffer
   via vendor-specific ReadCdda(12) command */
int ReadCdda12(SCSI *usalp, UINT4 *p, unsigned lSector, unsigned SectorBurstVal)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof(*scmd), '\0');
        scmd->addr = (caddr_t)p;
        scmd->size = SectorBurstVal*CD_FRAMESIZE_RAW;
        scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
        scmd->cdb_len = SC_G5_CDBLEN;
        scmd->sense_len = CCS_SENSE_LEN;
        scmd->cdb.g5_cdb.cmd = 0xd8;		/* read audio command */
        scmd->cdb.g5_cdb.lun = usal_lun(usalp);
	scmd->cdb.g5_cdb.res |= (accepts_fua_bit == 1 ? 1 << 2 : 0);
        g5_cdbaddr(&scmd->cdb.g5_cdb, lSector);
        g5_cdblen(&scmd->cdb.g5_cdb, SectorBurstVal);

        if (usalp->verbose) fprintf(stderr, "\nReadSony12 CDDA...");

	usalp->cmdname = "Read12";

	if (usal_cmd(usalp)) return 0;

	/* has all or something been read? */
	return SectorBurstVal - usal_getresid(usalp)/CD_FRAMESIZE_RAW;
}

/* Read max. SectorBurst of cdda sectors to buffer
   via vendor-specific ReadCdda(12) command */
/*
> It uses a 12 Byte CDB with 0xd4 as opcode, the start sector is coded as
> normal and the number of sectors is coded in Byte 8 and 9 (begining with 0).
*/

int ReadCdda12Matsushita(SCSI *usalp, UINT4 *p, unsigned lSector, 
                         unsigned SectorBurstVal)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

        fillbytes((caddr_t)scmd, sizeof(*scmd), '\0');
        scmd->addr = (caddr_t)p;
        scmd->size = SectorBurstVal*CD_FRAMESIZE_RAW;
        scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
        scmd->cdb_len = SC_G5_CDBLEN;
        scmd->sense_len = CCS_SENSE_LEN;
        scmd->cdb.g5_cdb.cmd = 0xd4;		/* read audio command */
        scmd->cdb.g5_cdb.lun = usal_lun(usalp);
	scmd->cdb.g5_cdb.res |= (accepts_fua_bit == 1 ? 1 << 2 : 0);
        g5_cdbaddr(&scmd->cdb.g5_cdb, lSector);
        g5_cdblen(&scmd->cdb.g5_cdb, SectorBurstVal);

        if (usalp->verbose) fprintf(stderr, "\nReadMatsushita12 CDDA...");

	usalp->cmdname = "Read12Matsushita";

	if (usal_cmd(usalp)) return 0;

	/* has all or something been read? */
	return SectorBurstVal - usal_getresid(usalp)/CD_FRAMESIZE_RAW;
}

/* Read max. SectorBurst of cdda sectors to buffer
   via MMC standard READ CD command */
int ReadCddaMMC12(SCSI *usalp, UINT4 *p, unsigned lSector, 
                  unsigned SectorBurstVal)
{
	register struct	usal_cmd	*scmd;
	scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof(*scmd), '\0');
        scmd->addr = (caddr_t)p;
        scmd->size = SectorBurstVal*CD_FRAMESIZE_RAW;
        scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
        scmd->cdb_len = SC_G5_CDBLEN;
        scmd->sense_len = CCS_SENSE_LEN;
        scmd->cdb.g5_cdb.cmd = 0xbe;		/* read cd command */
        scmd->cdb.g5_cdb.lun = usal_lun(usalp);
        scmd->cdb.g5_cdb.res = 1 << 1; /* expected sector type field CDDA */
        g5_cdbaddr(&scmd->cdb.g5_cdb, lSector);
        g5x_cdblen(&scmd->cdb.g5_cdb, SectorBurstVal);
	scmd->cdb.g5_cdb.count[3] = 1 << 4;	/* User data */

        if (usalp->verbose) fprintf(stderr, "\nReadMMC12 CDDA...");

	usalp->cmdname = "ReadCD MMC 12";

	if (usal_cmd(usalp)) return 0;

	/* has all or something been read? */
	return SectorBurstVal - usal_getresid(usalp)/CD_FRAMESIZE_RAW;
}

int ReadCddaFallbackMMC(SCSI *usalp, UINT4 *p, unsigned lSector, 
                        unsigned SectorBurstVal)
{
	static int ReadCdda12_unknown = 0;
	int retval = -999;

	usalp->silent++;
	if (ReadCdda12_unknown 
	    || ((retval = ReadCdda12(usalp, p, lSector, SectorBurstVal)) <= 0)) {
		/* if the command is not available, use the regular
		 * MMC ReadCd 
		 */
		if (retval <= 0 && usal_sense_key(usalp) == 0x05) {
			ReadCdda12_unknown = 1;
		}
		usalp->silent--;
		ReadCdRom = ReadCddaMMC12;
		ReadCdRomSub = ReadCddaSubMMC12;
		return ReadCddaMMC12(usalp, p, lSector, SectorBurstVal);
	}
	usalp->silent--;
	return retval;
}

/* Read the Sub-Q-Channel to SubQbuffer. This is the method for
 * drives that do not support subchannel parameters. */
#ifdef	PROTOTYPES
static subq_chnl *ReadSubQFallback (SCSI *usalp, unsigned char sq_format, unsigned char track)
#else
static subq_chnl *
ReadSubQFallback(SCSI *usalp, unsigned char sq_format, unsigned char track)
#endif
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof(*scmd), '\0');
        scmd->addr = (caddr_t)SubQbuffer;
        scmd->size = 24;
        scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
        scmd->cdb_len = SC_G1_CDBLEN;
        scmd->sense_len = CCS_SENSE_LEN;
        scmd->cdb.g1_cdb.cmd = 0x42;		/* Read SubQChannel */
						/* use LBA */
        scmd->cdb.g1_cdb.lun = usal_lun(usalp);
        scmd->cdb.g1_cdb.addr[0] = 0x40; 	/* SubQ info */
        scmd->cdb.g1_cdb.addr[1] = 0;	 	/* parameter list: all */
        scmd->cdb.g1_cdb.res6 = track;		/* track number */
        g1_cdblen(&scmd->cdb.g1_cdb, 24);

        if (usalp->verbose) fprintf(stderr, "\nRead Subchannel_dumb...");

	usalp->cmdname = "Read Subchannel_dumb";

	if (usal_cmd(usalp) < 0) {
	  fprintf( stderr, "Read SubQ failed\n");
	}

	/* check, if the requested format is delivered */
	{ unsigned char *p = (unsigned char *) SubQbuffer;
	  if ((((unsigned)p[2] << 8) | p[3]) /* LENGTH */ > ULONG_C(11) &&
	    (p[5] >> 4) /* ADR */ == sq_format) {
	    if (sq_format == GET_POSITIONDATA)
		p[5] = (p[5] << 4) | (p[5] >> 4);
	    return SubQbuffer;
	  }
	}

	/* FIXME: we might actively search for the requested info ... */
	return NULL;
}

/* Read the Sub-Q-Channel to SubQbuffer */
#ifdef	PROTOTYPES
subq_chnl *ReadSubQSCSI (SCSI *usalp, unsigned char sq_format, unsigned char track)
#else
subq_chnl *
ReadSubQSCSI(SCSI *usalp, unsigned char sq_format, unsigned char track)
#endif
{
        int resp_size;
	register struct	usal_cmd	*scmd = usalp->scmd;

        switch (sq_format) {
          case GET_POSITIONDATA:
            resp_size = 16;
	    track = 0;
          break;
          case GET_CATALOGNUMBER:
            resp_size = 24;
	    track = 0;
          break;
          case GET_TRACK_ISRC:
            resp_size = 24;
          break;
          default:
                fprintf(stderr, "ReadSubQSCSI: unknown format %d\n", sq_format);
                return NULL;
        }

	fillbytes((caddr_t)scmd, sizeof(*scmd), '\0');
        scmd->addr = (caddr_t)SubQbuffer;
        scmd->size = resp_size;
        scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
        scmd->cdb_len = SC_G1_CDBLEN;
        scmd->sense_len = CCS_SENSE_LEN;
        scmd->cdb.g1_cdb.cmd = 0x42;
						/* use LBA */
        scmd->cdb.g1_cdb.lun = usal_lun(usalp);
        scmd->cdb.g1_cdb.addr[0] = 0x40; 	/* SubQ info */
        scmd->cdb.g1_cdb.addr[1] = sq_format;	/* parameter list: all */
        scmd->cdb.g1_cdb.res6 = track;		/* track number */
        g1_cdblen(&scmd->cdb.g1_cdb, resp_size);

        if (usalp->verbose) fprintf(stderr, "\nRead Subchannel...");

	usalp->cmdname = "Read Subchannel";

  if (usal_cmd(usalp) < 0) {
    /* in case of error do a fallback for dumb firmwares */
    return ReadSubQFallback(usalp, sq_format, track);
  }

	if (sq_format == GET_POSITIONDATA)
		SubQbuffer->control_adr = (SubQbuffer->control_adr << 4) | (SubQbuffer->control_adr >> 4);
  return SubQbuffer;
}

static subq_chnl sc;

static subq_chnl* fill_subchannel(unsigned char bufferwithQ[]);
static subq_chnl* fill_subchannel(unsigned char bufferwithQ[])
{
	sc.subq_length = 0;
	sc.control_adr = bufferwithQ[CD_FRAMESIZE_RAW + 0];
	sc.track = bufferwithQ[CD_FRAMESIZE_RAW + 1];
	sc.index = bufferwithQ[CD_FRAMESIZE_RAW + 2];
	return &sc;
}

int 
ReadCddaSubSony(SCSI *usalp, UINT4 *p, unsigned lSector, unsigned SectorBurstVal)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof(*scmd), '\0');
        scmd->addr = (caddr_t)p;
	scmd->size = SectorBurstVal*(CD_FRAMESIZE_RAW + 16);
        scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
        scmd->cdb_len = SC_G5_CDBLEN;
        scmd->sense_len = CCS_SENSE_LEN;
        scmd->cdb.g5_cdb.cmd = 0xd8;		/* read audio command */
        scmd->cdb.g5_cdb.lun = usal_lun(usalp);
	scmd->cdb.g5_cdb.res |= (accepts_fua_bit == 1 ? 1 << 2 : 0);
	scmd->cdb.g5_cdb.res10 = 0x01;	/* subcode 1 -> cdda + 16 * q sub */
        g5_cdbaddr(&scmd->cdb.g5_cdb, lSector);
        g5_cdblen(&scmd->cdb.g5_cdb, SectorBurstVal);

        if (usalp->verbose) fprintf(stderr, "\nReadSony12 CDDA + SubChannels...");

	usalp->cmdname = "Read12SubChannelsSony";

	if (usal_cmd(usalp)) return -1;

	/* has all or something been read? */
	return usal_getresid(usalp) != 0;
}

int ReadCddaSub96Sony(SCSI *usalp, UINT4 *p, unsigned lSector, 
							 unsigned SectorBurstVal);

int ReadCddaSub96Sony(SCSI *usalp, UINT4 *p, unsigned lSector, 
                      unsigned SectorBurstVal)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof(*scmd), '\0');
        scmd->addr = (caddr_t)p;
	scmd->size = SectorBurstVal*(CD_FRAMESIZE_RAW + 96);
        scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
        scmd->cdb_len = SC_G5_CDBLEN;
        scmd->sense_len = CCS_SENSE_LEN;
        scmd->cdb.g5_cdb.cmd = 0xd8;		/* read audio command */
        scmd->cdb.g5_cdb.lun = usal_lun(usalp);
	scmd->cdb.g5_cdb.res |= (accepts_fua_bit == 1 ? 1 << 2 : 0);
	scmd->cdb.g5_cdb.res10 = 0x02;	/* subcode 2 -> cdda + 96 * q sub */
        g5_cdbaddr(&scmd->cdb.g5_cdb, lSector);
        g5_cdblen(&scmd->cdb.g5_cdb, SectorBurstVal);

        if (usalp->verbose) fprintf(stderr, "\nReadSony12 CDDA + 96 byte SubChannels...");

	usalp->cmdname = "Read12SubChannelsSony";

	if (usal_cmd(usalp)) return -1;

	/* has all or something been read? */
	return usal_getresid(usalp) != 0;
}

subq_chnl *ReadSubChannelsSony(SCSI *usalp, unsigned lSector)
{
	/*int retval = ReadCddaSub96Sony(usalp, (UINT4 *)bufferTOC, lSector, 1);*/
	int retval = ReadCddaSubSony(usalp, (UINT4 *)bufferTOC, lSector, 1);
	if (retval != 0) return NULL;

	return fill_subchannel(bufferTOC);
}

/* Read max. SectorBurst of cdda sectors to buffer
   via MMC standard READ CD command */
int ReadCddaSubMMC12(SCSI *usalp, UINT4 *p, unsigned lSector, unsigned SectorBurstVal)
{
	register struct	usal_cmd	*scmd;
	scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof(*scmd), '\0');
        scmd->addr = (caddr_t)p;
        scmd->size = SectorBurstVal*(CD_FRAMESIZE_RAW + 16);
        scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
        scmd->cdb_len = SC_G5_CDBLEN;
        scmd->sense_len = CCS_SENSE_LEN;
        scmd->cdb.g5_cdb.cmd = 0xbe;		/* read cd command */
        scmd->cdb.g5_cdb.lun = usal_lun(usalp);
        scmd->cdb.g5_cdb.res = 1 << 1; /* expected sector type field CDDA */
        g5_cdbaddr(&scmd->cdb.g5_cdb, lSector);
        g5x_cdblen(&scmd->cdb.g5_cdb, SectorBurstVal);
	scmd->cdb.g5_cdb.count[3] = 1 << 4;	/* User data */
	scmd->cdb.g5_cdb.res10 = 0x02;	/* subcode 2 -> cdda + 16 * q sub */

        if (usalp->verbose) fprintf(stderr, "\nReadMMC12 CDDA + SUB...");

	usalp->cmdname = "ReadCD Sub MMC 12";

	if (usal_cmd(usalp)) return -1;

	/* has all or something been read? */
	return usal_getresid(usalp) != 0;
}

static subq_chnl *ReadSubChannelsMMC(SCSI *usalp, unsigned lSector);
static subq_chnl *ReadSubChannelsMMC(SCSI *usalp, unsigned lSector)
{
	int retval = ReadCddaSubMMC12(usalp, (UINT4 *)bufferTOC, lSector, 1);
	if (retval != 0) return NULL;

	return fill_subchannel(bufferTOC);
}

subq_chnl *ReadSubChannelsFallbackMMC(SCSI *usalp, unsigned lSector)
{
	static int ReadSubSony_unknown = 0;
	subq_chnl *retval = NULL;

	usalp->silent++;
	if (ReadSubSony_unknown 
	    || ((retval = ReadSubChannelsSony(usalp, lSector)) == NULL)) {
		/* if the command is not available, use the regular
		 * MMC ReadCd 
		 */
		if (retval == NULL && usal_sense_key(usalp) == 0x05) {
			ReadSubSony_unknown = 1;
		}
		usalp->silent--;
		return ReadSubChannelsMMC(usalp, lSector);
	}
	usalp->silent--;
	return retval;
}

subq_chnl *ReadStandardSub(usalp, lSector)
	SCSI *usalp;
	unsigned lSector;
{
	if (0 == ReadStandardLowlevel (usalp, (UINT4 *)bufferTOC, lSector, 1, CD_FRAMESIZE_RAW + 16 )) {
		return NULL;
	}
#if	0
fprintf(stderr, "Subchannel Sec %x: %02x %02x %02x %02x\n"
	,lSector
	,bufferTOC[CD_FRAMESIZE_RAW + 0]
	,bufferTOC[CD_FRAMESIZE_RAW + 1]
	,bufferTOC[CD_FRAMESIZE_RAW + 2]
	,bufferTOC[CD_FRAMESIZE_RAW + 3]
	);
#endif
	sc.control_adr = (bufferTOC[CD_FRAMESIZE_RAW + 0] << 4)
		| bufferTOC[CD_FRAMESIZE_RAW + 1];
	sc.track = from_bcd(bufferTOC[CD_FRAMESIZE_RAW + 2]);
	sc.index = from_bcd(bufferTOC[CD_FRAMESIZE_RAW + 3]);
	return &sc;
}
/********* non standardized speed selects ***********************/

void SpeedSelectSCSIToshiba(SCSI *usalp, unsigned speed)
{
  static unsigned char mode [4 + 3];
  unsigned char *page = mode + 4;
  int retval;

  fillbytes((caddr_t)mode, sizeof(mode), '\0');
  /* the first 4 mode bytes are zero. */
  page[0] = 0x20;
  page[1] = 1;
  page[2] = speed;   /* 0 for single speed, 1 for double speed (3401) */

  if (usalp->verbose) fprintf(stderr, "\nspeed select Toshiba...");

  usalp->silent++;
  /* do the scsi cmd */
  if ((retval = mode_select(usalp, mode, 7, 0, usalp->inq->data_format >= 2)) < 0)
        fprintf (stderr, "speed select Toshiba failed\n");
  usalp->silent--;
}

void SpeedSelectSCSINEC(SCSI *usalp, unsigned speed)
{
  static unsigned char mode [4 + 8];
  unsigned char *page = mode + 4;
  int retval;
	register struct	usal_cmd	*scmd = usalp->scmd;

  fillbytes((caddr_t)mode, sizeof(mode), '\0');
  /* the first 4 mode bytes are zero. */
  page [0] = 0x0f; /* page code */
  page [1] = 6;    /* parameter length */
  /* bit 5 == 1 for single speed, otherwise double speed */
  page [2] = speed == 1 ? 1 << 5 : 0;

	fillbytes((caddr_t)scmd, sizeof(*scmd), '\0');
  scmd->addr = (caddr_t)mode;
  scmd->size = 12;
  scmd->flags = SCG_DISRE_ENA;
  scmd->cdb_len = SC_G1_CDBLEN;
  scmd->sense_len = CCS_SENSE_LEN;
  scmd->cdb.g1_cdb.cmd = 0xC5;
  scmd->cdb.g1_cdb.lun = usal_lun(usalp);
  scmd->cdb.g1_cdb.addr[0] = 0 ? 1 : 0 | 1 ? 0x10 : 0;
  g1_cdblen(&scmd->cdb.g1_cdb, 12);

  if (usalp->verbose) fprintf(stderr, "\nspeed select NEC...");
  /* do the scsi cmd */

	usalp->cmdname = "speed select NEC";

  if ((retval = usal_cmd(usalp)) < 0)
        fprintf(stderr ,"speed select NEC failed\n");
}

void SpeedSelectSCSIPhilipsCDD2600(SCSI *usalp, unsigned speed)
{
  /* MODE_SELECT, page = SCSI-2  save page disabled, reserved, reserved,
     parm list len, flags */
  static unsigned char mode [4 + 8];
  unsigned char *page = mode + 4;
  int retval;

  fillbytes((caddr_t)mode, sizeof(mode), '\0');
  /* the first 4 mode bytes are zero. */
  page[0] = 0x23;
  page[1] = 6;
  page[2] = page [4] = speed;
  page[3] = 1;

  if (usalp->verbose) fprintf(stderr, "\nspeed select Philips...");
  /* do the scsi cmd */
  if ((retval = mode_select(usalp, mode, 12, 0, usalp->inq->data_format >= 2)) < 0)
        fprintf (stderr, "speed select PhilipsCDD2600 failed\n");
}

void SpeedSelectSCSISony(SCSI *usalp, unsigned speed)
{
  static unsigned char mode [4 + 4];
  unsigned char *page = mode + 4;
  int retval;

  fillbytes((caddr_t)mode, sizeof(mode), '\0');
  /* the first 4 mode bytes are zero. */
  page[0] = 0x31;
  page[1] = 2;
  page[2] = speed;

  if (usalp->verbose) fprintf(stderr, "\nspeed select Sony...");
  /* do the scsi cmd */
  usalp->silent++;
  if ((retval = mode_select(usalp, mode, 8, 0, usalp->inq->data_format >= 2)) < 0)
        fprintf (stderr, "speed select Sony failed\n");
  usalp->silent--;
}

void SpeedSelectSCSIYamaha (usalp, speed)
	SCSI *usalp;
	unsigned speed;
{
  static unsigned char mode [4 + 4];
  unsigned char *page = mode + 4;
  int retval;

  fillbytes((caddr_t)mode, sizeof(mode), '\0');
  /* the first 4 mode bytes are zero. */
  page[0] = 0x31;
  page[1] = 2;
  page[2] = speed;

  if (usalp->verbose) fprintf(stderr, "\nspeed select Yamaha...");
  /* do the scsi cmd */
  if ((retval = mode_select(usalp, mode, 8, 0, usalp->inq->data_format >= 2)) < 0)
        fprintf (stderr, "speed select Yamaha failed\n");
}

void SpeedSelectSCSIMMC(SCSI *usalp, unsigned speed)
{
  int spd;
	register struct	usal_cmd	*scmd = usalp->scmd;

   if (speed == 0 || speed == 0xFFFF) {
      spd = 0xFFFF;
   } else {
      spd = (1764 * speed) / 10;
   }
	fillbytes((caddr_t)scmd, sizeof(*scmd), '\0');
        scmd->flags = SCG_DISRE_ENA;
        scmd->cdb_len = SC_G5_CDBLEN;
        scmd->sense_len = CCS_SENSE_LEN;
        scmd->cdb.g5_cdb.cmd = 0xBB;
        scmd->cdb.g5_cdb.lun = usal_lun(usalp);
        i_to_2_byte(&scmd->cdb.g5_cdb.addr[0], spd);
        i_to_2_byte(&scmd->cdb.g5_cdb.addr[2], 0xffff);

        if (usalp->verbose) fprintf(stderr, "\nspeed select MMC...");

	usalp->cmdname = "set cd speed";

	usalp->silent++;
        if (usal_cmd(usalp) < 0) {
		if (usal_sense_key(usalp) == 0x05 &&
		    usal_sense_code(usalp) == 0x20 &&
		    usal_sense_qual(usalp) == 0x00) {
			/* this optional command is not implemented */
		} else {
			usal_printerr(usalp);
                	fprintf (stderr, "speed select MMC failed\n");
		}
	}
	usalp->silent--;
}

/* request vendor brand and model */
unsigned char *Inquiry(SCSI *usalp)
{
  static unsigned char *Inqbuffer = NULL;
	register struct	usal_cmd	*scmd = usalp->scmd;

  if (Inqbuffer == NULL) {
    Inqbuffer = malloc(36);
    if (Inqbuffer == NULL) {
      fprintf(stderr, "Cannot allocate memory for inquiry command in line %d\n", __LINE__);
        return NULL;
    }
  }

  fillbytes(Inqbuffer, 36, '\0');
	fillbytes((caddr_t)scmd, sizeof(*scmd), '\0');
  scmd->addr = (caddr_t)Inqbuffer;
  scmd->size = 36;
  scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
  scmd->cdb_len = SC_G0_CDBLEN;
  scmd->sense_len = CCS_SENSE_LEN;
  scmd->cdb.g0_cdb.cmd = SC_INQUIRY;
  scmd->cdb.g0_cdb.lun = usal_lun(usalp);
  scmd->cdb.g0_cdb.count = 36;
        
	usalp->cmdname = "inquiry";

  if (usal_cmd(usalp) < 0)
     return (NULL);

  /* define structure with inquiry data */
  memcpy(usalp->inq, Inqbuffer, sizeof(*usalp->inq)); 

  if (usalp->verbose)
     usal_prbytes("Inquiry Data   :", (Uchar *)Inqbuffer, 22 - scmd->resid);

  return (Inqbuffer);
}

#define SC_CLASS_EXTENDED_SENSE 0x07
#define TESTUNITREADY_CMD 0
#define TESTUNITREADY_CMDLEN 6

#define ADD_SENSECODE 12
#define ADD_SC_QUALIFIER 13
#define NO_MEDIA_SC 0x3a
#define NO_MEDIA_SCQ 0x00

int TestForMedium(SCSI *usalp)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

  if (interface != GENERIC_SCSI) {
    return 1;
  }

  /* request READY status */
	fillbytes((caddr_t)scmd, sizeof(*scmd), '\0');
  scmd->addr = (caddr_t)0;
  scmd->size = 0;
  scmd->flags = SCG_DISRE_ENA | (1 ? SCG_SILENT:0);
  scmd->cdb_len = SC_G0_CDBLEN;
  scmd->sense_len = CCS_SENSE_LEN;
  scmd->cdb.g0_cdb.cmd = SC_TEST_UNIT_READY;
  scmd->cdb.g0_cdb.lun = usal_lun(usalp);
        
  if (usalp->verbose) fprintf(stderr, "\ntest unit ready...");
  usalp->silent++;

	usalp->cmdname = "test unit ready";

  if (usal_cmd(usalp) >= 0) {
    usalp->silent--;
    return 1;
  }
  usalp->silent--;

  if (scmd->sense.code >= SC_CLASS_EXTENDED_SENSE) {
    return 
      scmd->u_sense.cmd_sense[ADD_SENSECODE] != NO_MEDIA_SC ||
      scmd->u_sense.cmd_sense[ADD_SC_QUALIFIER] != NO_MEDIA_SCQ;
  } else {
    /* analyse status. */
    /* 'check condition' is interpreted as not ready. */
    return (scmd->u_scb.cmd_scb[0] & 0x1e) != 0x02;
  }
}

int StopPlaySCSI(SCSI *usalp)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof(*scmd), '\0');
  scmd->addr = NULL;
  scmd->size = 0;
  scmd->flags = SCG_DISRE_ENA;
  scmd->cdb_len = SC_G0_CDBLEN;
  scmd->sense_len = CCS_SENSE_LEN;
  scmd->cdb.g0_cdb.cmd = 0x1b;
  scmd->cdb.g0_cdb.lun = usal_lun(usalp);

  if (usalp->verbose) fprintf(stderr, "\nstop audio play");
  /* do the scsi cmd */

	usalp->cmdname = "stop audio play";

  return usal_cmd(usalp) >= 0 ? 0 : -1;
}

int Play_atSCSI(SCSI *usalp, unsigned int from_sector, unsigned int sectors)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof(*scmd), '\0');
  scmd->addr = NULL;
  scmd->size = 0;
  scmd->flags = SCG_DISRE_ENA;
  scmd->cdb_len = SC_G1_CDBLEN;
  scmd->sense_len = CCS_SENSE_LEN;
  scmd->cdb.g1_cdb.cmd = 0x47;
  scmd->cdb.g1_cdb.lun = usal_lun(usalp);
  scmd->cdb.g1_cdb.addr[1] = (from_sector + 150) / (60*75);
  scmd->cdb.g1_cdb.addr[2] = ((from_sector + 150) / 75) % 60;
  scmd->cdb.g1_cdb.addr[3] = (from_sector + 150) % 75;
  scmd->cdb.g1_cdb.res6 = (from_sector + 150 + sectors) / (60*75);
  scmd->cdb.g1_cdb.count[0] = ((from_sector + 150 + sectors) / 75) % 60;
  scmd->cdb.g1_cdb.count[1] = (from_sector + 150 + sectors) % 75;

  if (usalp->verbose) fprintf(stderr, "\nplay sectors...");
  /* do the scsi cmd */

	usalp->cmdname = "play sectors";

  return usal_cmd(usalp) >= 0 ? 0 : -1;
}

static caddr_t scsibuffer;	/* page aligned scsi transfer buffer */

void init_scsibuf(SCSI *scsp, unsigned amt);

void init_scsibuf(SCSI *usalp, unsigned amt)
{
	if (scsibuffer != NULL) {
		fprintf(stderr, "the SCSI transfer buffer has already been allocated!\n");
		exit(SETUPSCSI_ERROR);
	}
	scsibuffer = usal_getbuf(usalp, amt);
	if (scsibuffer == NULL) {
		fprintf(stderr, "could not get SCSI transfer buffer!\n");
		exit(SETUPSCSI_ERROR);
	}
}
