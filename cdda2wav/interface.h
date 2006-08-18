/* @(#)interface.h	1.14 06/02/19 Copyright 1998-2001 Heiko Eissfeldt, Copyright 2005-2006 J. Schilling */

/***
 * CopyPolicy: GNU Public License 2 applies
 * Copyright (C) by Heiko Eissfeldt
 *
 * header file interface.h for cdda2wav */

#ifndef CD_FRAMESIZE
#define CD_FRAMESIZE 2048
#endif

#ifndef CD_FRAMESIZE_RAW
#define CD_FRAMESIZE_RAW 2352
#endif

#define CD_FRAMESAMPLES (CD_FRAMESIZE_RAW / 4)

extern unsigned interface;

extern int trackindex_disp;
#ifndef NSECTORS
#define NSECTORS 75
#endif

/* interface types */
#define GENERIC_SCSI	0
#define COOKED_IOCTL	1

/* constants for sub-q-channel info */
#define GET_ALL			0
#define GET_POSITIONDATA	1
#define GET_CATALOGNUMBER	2
#define GET_TRACK_ISRC		3

typedef struct subq_chnl {
    unsigned char reserved;
    unsigned char audio_status;
    unsigned short subq_length;
    unsigned char format;
    unsigned char control_adr;
    unsigned char track;
    unsigned char index;
    unsigned char data[40];	/* this has subq_all, subq_position,
				   subq_catalog or subq_track_isrc format */
} subq_chnl;

typedef struct subq_all {
    unsigned char abs_min;
    unsigned char abs_sec;
    unsigned char abs_frame;
    unsigned char abs_reserved;
    unsigned char trel_min;
    unsigned char trel_sec;
    unsigned char trel_frame;
    unsigned char trel_reserved;
    unsigned char mc_valid;     /* MSB */
    unsigned char media_catalog_number[13];
    unsigned char zero;
    unsigned char aframe;
    unsigned char tc_valid;	/* MSB */
    unsigned char track_ISRC[15];
} subq_all;

typedef struct subq_position {
    unsigned char abs_reserved;
    unsigned char abs_min;
    unsigned char abs_sec;
    unsigned char abs_frame;
    unsigned char trel_reserved;
    unsigned char trel_min;
    unsigned char trel_sec;
    unsigned char trel_frame;
} subq_position;

typedef struct subq_catalog {
    unsigned char mc_valid;	/* MSB */
    unsigned char media_catalog_number[13];
    unsigned char zero;
    unsigned char aframe;
} subq_catalog;

typedef struct subq_track_isrc {
    unsigned char tc_valid;	/* MSB */
    unsigned char track_isrc[15];
} subq_track_isrc;

#if	!defined	NO_SCSI_STUFF

struct TOC;

/* cdrom access function pointer */
extern void     (*EnableCdda) __PR((SCSI *scgp, int Switch, unsigned uSectorsize));
extern unsigned (*doReadToc) __PR(( SCSI *scgp ));
extern void	(*ReadTocText) __PR(( SCSI *scgp ));
extern unsigned (*ReadLastAudio) __PR(( SCSI *scgp ));
extern int      (*ReadCdRom) __PR((SCSI *scgp, UINT4 *p, unsigned lSector, unsigned SectorBurstVal ));
extern int      (*ReadCdRomSub) __PR((SCSI *scgp, UINT4 *p, unsigned lSector, unsigned SectorBurstVal ));
extern int      (*ReadCdRomData) __PR((SCSI *scgp, unsigned char *p, unsigned lSector, unsigned SectorBurstVal ));
extern subq_chnl *(*ReadSubQ) __PR(( SCSI *scgp, unsigned char sq_format, unsigned char track ));
extern subq_chnl *(*ReadSubChannels) __PR(( SCSI *scgp, unsigned lSector ));
extern void     (*SelectSpeed) __PR(( SCSI *scgp, unsigned speed ));
extern int	(*Play_at) __PR(( SCSI *scgp, unsigned from_sector, unsigned sectors));
extern int	(*StopPlay) __PR(( SCSI *scgp));
extern void	(*trash_cache) __PR((UINT4 *p, unsigned lSector, unsigned SectorBurstVal));

SCSI    *get_scsi_p __PR(( void ));
#endif

extern unsigned char *bufferTOC;
extern subq_chnl *SubQbuffer;


void SetupInterface __PR(( void ));
int	Toshiba3401 __PR(( void ));

EXPORT	void	priv_init	__PR((void));
EXPORT	void	priv_on		__PR((void));
EXPORT	void	priv_off	__PR((void));
