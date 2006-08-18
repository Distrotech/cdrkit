/* @(#)sndfile.h	1.4 06/02/19 Copyright 1998,1999 Heiko Eissfeldt, Copyright 2006 J. Schilling */

/*
 * generic soundfile structure
 */

#ifndef	_SNDFILE_H
#define	_SNDFILE_H

#include <utypes.h>

struct soundfile {
	int	(* InitSound)		__PR((int audio, long channels,
						Ulong rate,
						long nBitsPerSample,
						Ulong expected_bytes));
	int	(* ExitSound)		__PR((int audio, Ulong nBytesDone));
	Ulong	(* GetHdrSize)		__PR((void));
	int	(* WriteSound)		__PR((int audio, unsigned char *buf,
						Ulong BytesToDo));
	Ulong	(* InSizeToOutSize)	__PR((Ulong BytesToDo));

	int	need_big_endian;
};

#endif	/* _SNDFILE_H */
