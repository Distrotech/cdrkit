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

/* @(#)scsi_cmds.h	1.11 03/03/02 Copyright 1998,1999 Heiko Eissfeldt */
/* header file for scsi_cmds.c */

extern unsigned char *cmd;
struct TOC;
int SCSI_emulated_ATAPI_on(SCSI *scgp);
unsigned char *Inquiry(SCSI *scgp);
int TestForMedium(SCSI *scgp);
void SpeedSelectSCSIMMC(SCSI *scgp, unsigned speed);
void SpeedSelectSCSIYamaha(SCSI *scgp, unsigned speed);
void SpeedSelectSCSISony(SCSI *scgp, unsigned speed);
void SpeedSelectSCSIPhilipsCDD2600(SCSI *scgp, unsigned speed);
void SpeedSelectSCSINEC(SCSI *scgp, unsigned speed);
void SpeedSelectSCSIToshiba(SCSI *scgp, unsigned speed);
subq_chnl *ReadSubQSCSI(SCSI *scgp, unsigned char sq_format, 
								unsigned char ltrack);
subq_chnl *ReadSubChannelsSony(SCSI *scgp, unsigned lSector);
subq_chnl *ReadSubChannelsFallbackMMC(SCSI *scgp, unsigned lSector);
subq_chnl *ReadStandardSub(SCSI *scgp, unsigned lSector);
int ReadCddaMMC12(SCSI *scgp, UINT4 *p, unsigned lSector, 
						unsigned SectorBurstVal);
int ReadCdda12Matsushita(SCSI *scgp, UINT4 *p, unsigned lSector, 
								 unsigned SectorBurstVal);
int ReadCdda12(SCSI *scgp, UINT4 *p, unsigned lSector, unsigned SecorBurstVal);
int ReadCdda10(SCSI *scgp, UINT4 *p, unsigned lSector, unsigned SecorBurstVal);
int ReadStandard(SCSI *scgp, UINT4 *p, unsigned lSector, 
					  unsigned SctorBurstVal);
int ReadStandardData(SCSI *scgp, UINT4 *p, unsigned lSector, 
							unsigned SctorBurstVal);
int ReadCddaFallbackMMC(SCSI *scgp, UINT4 *p, unsigned lSector, 
								unsigned SctorBurstVal);
int ReadCddaSubSony(SCSI *scgp, UINT4 *p, unsigned lSector, 
						  unsigned SectorBurstVal);
int ReadCddaSub96Sony(SCSI *scgp, UINT4 *p, unsigned lSector, 
							 unsigned SectorBurstVal);
int ReadCddaSubMMC12(SCSI *scgp, UINT4 *p, unsigned lSector, 
							unsigned SectorBurstVal);
unsigned ReadTocSony(SCSI *scgp);
unsigned ReadTocMMC(SCSI *scgp);
unsigned ReadTocSCSI(SCSI *scgp);
unsigned ReadFirstSessionTOCSony(SCSI *scgp);
unsigned ReadFirstSessionTOCMMC(SCSI *scgp);
void ReadTocTextSCSIMMC(SCSI *scgp);
int Play_atSCSI(SCSI *scgp, unsigned int from_sector, unsigned int sectors);
int StopPlaySCSI(SCSI *scgp);
void EnableCddaModeSelect(SCSI *scgp, int fAudioMode, unsigned uSectorsize);
int set_sectorsize(SCSI *scgp, unsigned int secsize);
unsigned int
get_orig_sectorsize(SCSI *scgp, unsigned char *m4, unsigned char *m10,
                    unsigned char *m11);
int heiko_mmc(SCSI *scgp);
void init_scsibuf(SCSI *scgp, unsigned amt);
int	myscsierr(SCSI *scgp);

extern int accepts_fua_bit;
extern unsigned char density;
extern unsigned char orgmode4;
extern unsigned char orgmode10, orgmode11;

