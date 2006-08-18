/* @(#)toc.h	1.9 06/02/19 Copyright 1998,1999 Heiko Eissfeldt, Copyright 2006 J. Schilling */

#define	MAXTRK	100	/* maximum of audio tracks (without a hidden track) */

extern	unsigned cdtracks;
extern	int	have_multisession;
extern	int	have_CD_extra;
extern	int	have_CD_text;
extern	int	have_CDDB;

#if	!defined(HAVE_NETDB_H)
#undef	USE_REMOTE
#else
#define	USE_REMOTE	1
extern	int		request_titles	__PR((void));
#endif

extern	int		ReadToc		__PR((void));
extern	void		Check_Toc	__PR((void));
extern	int		TOC_entries	__PR((unsigned tracks, unsigned char *a, unsigned char *b,
						int bvalid));
extern	void		toc_entry	__PR((unsigned nr, unsigned flag, unsigned tr,
						unsigned char *ISRC,
						unsigned long lba, int m, int s, int f));
extern	int		patch_real_end	__PR((unsigned long sector));
extern	int		no_disguised_audiotracks __PR((void));

extern	int		Get_Track	__PR((unsigned long sector));
extern	long		FirstTrack	__PR((void));
extern	long		LastTrack	__PR((void));
extern	long		FirstAudioTrack	__PR((void));
extern	long		FirstDataTrack	__PR((void));
extern	long		LastAudioTrack	__PR((void));
extern	long		Get_EndSector	__PR((unsigned long p_track));
extern	long		Get_StartSector	__PR((unsigned long p_track));
extern	long		Get_AudioStartSector	__PR((unsigned long p_track));
extern	long		Get_LastSectorOnCd	__PR((unsigned long p_track));
extern	int		CheckTrackrange	__PR((unsigned long from, unsigned long upto));

extern	int		Get_Preemphasis	__PR((unsigned long p_track));
extern	int		Get_Channels	__PR((unsigned long p_track));
extern	int		Get_Copyright	__PR((unsigned long p_track));
extern	int		Get_Datatrack	__PR((unsigned long p_track));
extern	int		Get_Tracknumber	__PR((unsigned long p_track));
extern	unsigned char *	Get_MCN		__PR((void));
extern	unsigned char *	Get_ISRC	__PR((unsigned long p_track));

extern	unsigned	find_an_off_sector __PR((unsigned lSector, unsigned SectorBurstVal));
extern	void		DisplayToc	__PR((void));
extern	unsigned	FixupTOC	__PR((unsigned no_tracks));
extern	void		calc_cddb_id	__PR((void));
extern	void		calc_cdindex_id	__PR((void));
extern	void		Read_MCN_ISRC	__PR((void));
extern	unsigned	ScanIndices	__PR((unsigned trackval, unsigned indexval, int bulk));
extern	int		handle_cdtext	__PR((void));
extern	int		lba_2_msf	__PR((long lba, int *m, int *s, int *f));
