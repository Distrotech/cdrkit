/* @(#)sun.c	1.4 01/10/27 Copyright 1998,1999 Heiko Eissfeldt */
#ifndef lint
static char     sccsid[] =
"@(#)sun.c	1.4 01/10/27 Copyright 1998,1999 Heiko Eissfeldt";

#endif
/***
 * CopyPolicy: GNU Public License 2 applies
 * Copyright (C) by Heiko Eissfeldt
 *
 *
 * ---------------------------------------------------------------------
 *  definitions for sun pcm output
 * ---------------------------------------------------------------------
 */

#include "config.h"
#include "mytype.h"
#include <stdio.h>
#include <unixstd.h>
#include "byteorder.h"
#include "sndfile.h"

typedef struct SUNHDR {
  unsigned int magic;			/* dns. a la .snd */
  unsigned int data_location;		/* offset to data */
  unsigned int size;			/* # of data bytes */
  unsigned int format;			/* format code */
  unsigned int sample_rate;		/* in Hertz */
  unsigned int channelcount;		/* 1 for mono, 2 for stereo */
  char info[8];				/* comments */
} SUNHDR;

static SUNHDR sunHdr;

static int InitSound __PR(( int audio, long channels, unsigned long rate, long nBitsPerSample, unsigned long expected_bytes ));

static int InitSound ( audio, channels, rate, nBitsPerSample, expected_bytes)
	int audio;
	long channels;
	unsigned long rate;
	long nBitsPerSample;
	unsigned long expected_bytes;
{
  unsigned long format = nBitsPerSample > 8 ? 0x03 : 0x02;

  sunHdr.magic         = cpu_to_le32(UINT4_C(0x646e732e));
  sunHdr.data_location = cpu_to_be32(0x20);
  sunHdr.size          = cpu_to_be32(expected_bytes);
  sunHdr.format        = cpu_to_be32(format);
  sunHdr.sample_rate   = cpu_to_be32(rate);
  sunHdr.channelcount  = cpu_to_be32(channels);

  return write (audio, &sunHdr, sizeof (sunHdr));
}

static int ExitSound __PR(( int audio, unsigned long nBytesDone ));

static int ExitSound ( audio, nBytesDone )
	int audio;
	unsigned long nBytesDone;
{
  sunHdr.size = cpu_to_be32(nBytesDone);

  /* goto beginning */
  if (lseek(audio, 0L, SEEK_SET) == -1) {
    return 0;
  }
  return write (audio, &sunHdr, sizeof (sunHdr));
}

static unsigned long GetHdrSize __PR(( void ));

static unsigned long GetHdrSize( )
{
  return sizeof( sunHdr );
}

static unsigned long InSizeToOutSize __PR(( unsigned long BytesToDo ));

static unsigned long InSizeToOutSize ( BytesToDo )
        unsigned long BytesToDo;
{
        return BytesToDo;
}

struct soundfile sunsound =
{
	InitSound,		/* init header method */
	ExitSound,		/* exit header method */
	GetHdrSize,		/* report header size method */
	(int (*) __PR(( int audio, unsigned char *buf, unsigned long BytesToDo ))) write,			/* get sound samples out */
	InSizeToOutSize,	/* compressed? output file size */
	1			/* needs big endian samples */
};
