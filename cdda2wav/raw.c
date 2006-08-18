/* @(#)raw.c	1.4 01/10/27 Copyright 1998,1999 Heiko Eissfeldt */
#ifndef lint
static char     sccsid[] =
"@(#)raw.c	1.4 01/10/27 Copyright 1998,1999 Heiko Eissfeldt";

#endif
#include "config.h"
#include <unixstd.h>
#include "sndfile.h"

static int InitSound __PR(( void ));

static int InitSound ( )
{
  return 0;
}

static int ExitSound __PR(( void ));

static int ExitSound ( )
{
  return 0;
}

static unsigned long GetHdrSize __PR(( void ));

static unsigned long GetHdrSize( )
{
  return 0L;
}

static unsigned long InSizeToOutSize __PR(( unsigned long BytesToDo ));

static unsigned long InSizeToOutSize ( BytesToDo )
        unsigned long BytesToDo;
{
        return BytesToDo;
}

struct soundfile rawsound =
{
  (int (*) __PR((int audio, long channels,
                 unsigned long myrate, long nBitsPerSample,
                 unsigned long expected_bytes))) InitSound,

  (int (*) __PR((int audio, unsigned long nBytesDone))) ExitSound,

  GetHdrSize,

  (int (*) __PR(( int audio, unsigned char *buf, unsigned long BytesToDo ))) write,		/* get sound samples out */

  InSizeToOutSize,	/* compressed? output file size */

  1		/* needs big endian samples */
};
