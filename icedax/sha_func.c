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

/* @(#)sha_func.c	1.3 01/10/27 Copyright 1998,1999 Heiko Eissfeldt */
/*____________________________________________________________________________
//
//   CD Index - The Internet CD Index
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation; either version 2 of the License, or
//   (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program; if not, write to the Free Software
//   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
//   $Id: sha_func.c,v 1.2 1999/06/04 14:10:07 marc Exp $
//____________________________________________________________________________
*/
/* NIST Secure Hash Algorithm */

/* heavily modified by Uwe Hollerbach <uh@alumni.caltech edu> */
/* from Peter C. Gutmann's implementation as found in */
/* Applied Cryptography by Bruce Schneier */
/* Further modifications to include the "UNRAVEL" stuff, below */
/* portability modifications Heiko Eissfeldt */

/* This code is in the public domain */

#include "config.h"
#include <strdefs.h>
#include "sha.h"

/* UNRAVEL should be fastest & biggest */
/* UNROLL_LOOPS should be just as big, but slightly slower */
/* both undefined should be smallest and slowest */

#define UNRAVEL
/* #define UNROLL_LOOPS */

/* SHA f()-functions */

#define f1(x,y,z)	((x & y) | (~x & z))
#define f2(x,y,z)	(x ^ y ^ z)
#define f3(x,y,z)	((x & y) | (x & z) | (y & z))
#define f4(x,y,z)	(x ^ y ^ z)

/* SHA constants */

#define CONST1		ULONG_C(0x5a827999)
#define CONST2		ULONG_C(0x6ed9eba1)
#define CONST3		ULONG_C(0x8f1bbcdc)
#define CONST4		ULONG_C(0xca62c1d6)

/* truncate to 32 bits -- should be a null op on 32-bit machines */

#define T32(x)	((x) & ULONG_C(0xffffffff))

/* 32-bit rotate */

#define R32(x,n)	T32(((x << n) | (x >> (32 - n))))

/* the generic case, for when the overall rotation is not unraveled */

#define FG(n)	\
    T = T32(R32(A,5) + CONCAT(f,n(B,C,D)) + E + *WP++ + CONCAT(CONST,n));	\
    E = D; D = C; C = R32(B,30); B = A; A = T

/* specific cases, for when the overall rotation is unraveled */

#define FA(n)	\
    T = T32(R32(A,5) + CONCAT(f,n(B,C,D)) + E + *WP++ + CONCAT(CONST,n)); B = R32(B,30)

#define FB(n)	\
    E = T32(R32(T,5) + CONCAT(f,n(A,B,C)) + D + *WP++ + CONCAT(CONST,n)); A = R32(A,30)

#define FC(n)	\
    D = T32(R32(E,5) + CONCAT(f,n(T,A,B)) + C + *WP++ + CONCAT(CONST,n)); T = R32(T,30)

#define FD(n)	\
    C = T32(R32(D,5) + CONCAT(f,n(E,T,A)) + B + *WP++ + CONCAT(CONST,n)); E = R32(E,30)

#define FE(n)	\
    B = T32(R32(C,5) + CONCAT(f,n(D,E,T)) + A + *WP++ + CONCAT(CONST,n)); D = R32(D,30)

#define FT(n)	\
    A = T32(R32(B,5) + CONCAT(f,n(C,D,E)) + T + *WP++ + CONCAT(CONST,n)); C = R32(C,30)

/* do SHA transformation */

static void sha_transform(SHA_INFO *sha_info);

static void sha_transform(SHA_INFO *sha_info)
{
    int i;
    BYTE *dp;
    ULONG T, A, B, C, D, E, W[80], *WP;

    dp = sha_info->data;

/*
the following makes sure that at least one code block below is
traversed or an error is reported, without the necessity for nested
preprocessor if/else/endif blocks, which are a great pain in the
nether regions of the anatomy...
*/
#undef SWAP_DONE

#if (SHA_BYTE_ORDER == 1234)
#define SWAP_DONE
    for (i = 0; i < 16; ++i) {
	T = *((ULONG *) dp);
	dp += 4;
	W[i] =  ((T << 24) & ULONG_C(0xff000000)) | ((T <<  8) & ULONG_C(0x00ff0000)) |
		((T >>  8) & ULONG_C(0x0000ff00)) | ((T >> 24) & ULONG_C(0x000000ff));
    }
#endif /* SHA_BYTE_ORDER == 1234 */

#if (SHA_BYTE_ORDER == 4321)
#define SWAP_DONE
    for (i = 0; i < 16; ++i) {
	T = *((ULONG *) dp);
	dp += 4;
	W[i] = T32(T);
    }
#endif /* SHA_BYTE_ORDER == 4321 */

#if (SHA_BYTE_ORDER == 12345678)
#define SWAP_DONE
    for (i = 0; i < 16; i += 2) {
	T = *((ULONG *) dp);
	dp += 8;
	W[i] =  ((T << 24) & ULONG_C(0xff000000)) | ((T <<  8) & ULONG_C(0x00ff0000)) |
		((T >>  8) & ULONG_C(0x0000ff00)) | ((T >> 24) & ULONG_C(0x000000ff));
	T >>= 32;
	W[i+1] = ((T << 24) & ULONG_C(0xff000000)) | ((T <<  8) & ULONG_C(0x00ff0000)) |
		 ((T >>  8) & ULONG_C(0x0000ff00)) | ((T >> 24) & ULONG_C(0x000000ff));
    }
#endif /* SHA_BYTE_ORDER == 12345678 */

#if (SHA_BYTE_ORDER == 87654321)
#define SWAP_DONE
    for (i = 0; i < 16; i += 2) {
	T = *((ULONG *) dp);
	dp += 8;
	W[i] = T32(T >> 32);
	W[i+1] = T32(T);
    }
#endif /* SHA_BYTE_ORDER == 87654321 */

#ifndef SWAP_DONE
error Unknown byte order -- you need to add code here
#endif /* SWAP_DONE */

    for (i = 16; i < 80; ++i) {
	W[i] = W[i-3] ^ W[i-8] ^ W[i-14] ^ W[i-16];
#if (SHA_VERSION == 1)
	W[i] = R32(W[i], 1);
#endif /* SHA_VERSION */
    }
    A = sha_info->digest[0];
    B = sha_info->digest[1];
    C = sha_info->digest[2];
    D = sha_info->digest[3];
    E = sha_info->digest[4];
    WP = W;
#ifdef UNRAVEL
    FA(1); FB(1); FC(1); FD(1); FE(1); FT(1); FA(1); FB(1); FC(1); FD(1);
    FE(1); FT(1); FA(1); FB(1); FC(1); FD(1); FE(1); FT(1); FA(1); FB(1);
    FC(2); FD(2); FE(2); FT(2); FA(2); FB(2); FC(2); FD(2); FE(2); FT(2);
    FA(2); FB(2); FC(2); FD(2); FE(2); FT(2); FA(2); FB(2); FC(2); FD(2);
    FE(3); FT(3); FA(3); FB(3); FC(3); FD(3); FE(3); FT(3); FA(3); FB(3);
    FC(3); FD(3); FE(3); FT(3); FA(3); FB(3); FC(3); FD(3); FE(3); FT(3);
    FA(4); FB(4); FC(4); FD(4); FE(4); FT(4); FA(4); FB(4); FC(4); FD(4);
    FE(4); FT(4); FA(4); FB(4); FC(4); FD(4); FE(4); FT(4); FA(4); FB(4);
    sha_info->digest[0] = T32(sha_info->digest[0] + E);
    sha_info->digest[1] = T32(sha_info->digest[1] + T);
    sha_info->digest[2] = T32(sha_info->digest[2] + A);
    sha_info->digest[3] = T32(sha_info->digest[3] + B);
    sha_info->digest[4] = T32(sha_info->digest[4] + C);
#else /* !UNRAVEL */
#ifdef UNROLL_LOOPS
    FG(1); FG(1); FG(1); FG(1); FG(1); FG(1); FG(1); FG(1); FG(1); FG(1);
    FG(1); FG(1); FG(1); FG(1); FG(1); FG(1); FG(1); FG(1); FG(1); FG(1);
    FG(2); FG(2); FG(2); FG(2); FG(2); FG(2); FG(2); FG(2); FG(2); FG(2);
    FG(2); FG(2); FG(2); FG(2); FG(2); FG(2); FG(2); FG(2); FG(2); FG(2);
    FG(3); FG(3); FG(3); FG(3); FG(3); FG(3); FG(3); FG(3); FG(3); FG(3);
    FG(3); FG(3); FG(3); FG(3); FG(3); FG(3); FG(3); FG(3); FG(3); FG(3);
    FG(4); FG(4); FG(4); FG(4); FG(4); FG(4); FG(4); FG(4); FG(4); FG(4);
    FG(4); FG(4); FG(4); FG(4); FG(4); FG(4); FG(4); FG(4); FG(4); FG(4);
#else /* !UNROLL_LOOPS */
    for (i =  0; i < 20; ++i) { FG(1); }
    for (i = 20; i < 40; ++i) { FG(2); }
    for (i = 40; i < 60; ++i) { FG(3); }
    for (i = 60; i < 80; ++i) { FG(4); }
#endif /* !UNROLL_LOOPS */
    sha_info->digest[0] = T32(sha_info->digest[0] + A);
    sha_info->digest[1] = T32(sha_info->digest[1] + B);
    sha_info->digest[2] = T32(sha_info->digest[2] + C);
    sha_info->digest[3] = T32(sha_info->digest[3] + D);
    sha_info->digest[4] = T32(sha_info->digest[4] + E);
#endif /* !UNRAVEL */
}

/* initialize the SHA digest */

void sha_init(SHA_INFO *sha_info);

void sha_init(SHA_INFO *sha_info)
{
    sha_info->digest[0] = ULONG_C(0x67452301);
    sha_info->digest[1] = ULONG_C(0xefcdab89);
    sha_info->digest[2] = ULONG_C(0x98badcfe);
    sha_info->digest[3] = ULONG_C(0x10325476);
    sha_info->digest[4] = ULONG_C(0xc3d2e1f0);
    sha_info->count_lo = 0L;
    sha_info->count_hi = 0L;
    sha_info->local = 0;
}

/* update the SHA digest */

void sha_update(SHA_INFO *sha_info, BYTE *buffer, int count);

void sha_update(SHA_INFO *sha_info, BYTE *buffer, int count)
{
    int i;
    ULONG clo;

    clo = T32(sha_info->count_lo + ((ULONG) count << 3));
    if (clo < sha_info->count_lo) {
	++sha_info->count_hi;
    }
    sha_info->count_lo = clo;
    sha_info->count_hi += (ULONG) count >> 29;
    if (sha_info->local) {
	i = SHA_BLOCKSIZE - sha_info->local;
	if (i > count) {
	    i = count;
	}
	memcpy(((BYTE *) sha_info->data) + sha_info->local, buffer, i);
	count -= i;
	buffer += i;
	sha_info->local += i;
	if (sha_info->local == SHA_BLOCKSIZE) {
	    sha_transform(sha_info);
	} else {
	    return;
	}
    }
    while (count >= SHA_BLOCKSIZE) {
	memcpy(sha_info->data, buffer, SHA_BLOCKSIZE);
	buffer += SHA_BLOCKSIZE;
	count -= SHA_BLOCKSIZE;
	sha_transform(sha_info);
    }
    memcpy(sha_info->data, buffer, count);
    sha_info->local = count;
}

/* finish computing the SHA digest */

void sha_final(unsigned char digest[20], SHA_INFO *sha_info);

void sha_final(unsigned char digest[20], SHA_INFO *sha_info)
{
    int count;
    ULONG lo_bit_count, hi_bit_count;

    lo_bit_count = sha_info->count_lo;
    hi_bit_count = sha_info->count_hi;
    count = (int) ((lo_bit_count >> 3) & 0x3f);
    ((BYTE *) sha_info->data)[count++] = 0x80;
    if (count > SHA_BLOCKSIZE - 8) {
	memset(((BYTE *) sha_info->data) + count, 0, SHA_BLOCKSIZE - count);
	sha_transform(sha_info);
	memset((BYTE *) sha_info->data, 0, SHA_BLOCKSIZE - 8);
    } else {
	memset(((BYTE *) sha_info->data) + count, 0,
	    SHA_BLOCKSIZE - 8 - count);
    }
    sha_info->data[56] = (unsigned char) ((hi_bit_count >> 24) & 0xff);
    sha_info->data[57] = (unsigned char) ((hi_bit_count >> 16) & 0xff);
    sha_info->data[58] = (unsigned char) ((hi_bit_count >>  8) & 0xff);
    sha_info->data[59] = (unsigned char) ((hi_bit_count >>  0) & 0xff);
    sha_info->data[60] = (unsigned char) ((lo_bit_count >> 24) & 0xff);
    sha_info->data[61] = (unsigned char) ((lo_bit_count >> 16) & 0xff);
    sha_info->data[62] = (unsigned char) ((lo_bit_count >>  8) & 0xff);
    sha_info->data[63] = (unsigned char) ((lo_bit_count >>  0) & 0xff);
    sha_transform(sha_info);
    digest[ 0] = (unsigned char) ((sha_info->digest[0] >> 24) & 0xff);
    digest[ 1] = (unsigned char) ((sha_info->digest[0] >> 16) & 0xff);
    digest[ 2] = (unsigned char) ((sha_info->digest[0] >>  8) & 0xff);
    digest[ 3] = (unsigned char) ((sha_info->digest[0]      ) & 0xff);
    digest[ 4] = (unsigned char) ((sha_info->digest[1] >> 24) & 0xff);
    digest[ 5] = (unsigned char) ((sha_info->digest[1] >> 16) & 0xff);
    digest[ 6] = (unsigned char) ((sha_info->digest[1] >>  8) & 0xff);
    digest[ 7] = (unsigned char) ((sha_info->digest[1]      ) & 0xff);
    digest[ 8] = (unsigned char) ((sha_info->digest[2] >> 24) & 0xff);
    digest[ 9] = (unsigned char) ((sha_info->digest[2] >> 16) & 0xff);
    digest[10] = (unsigned char) ((sha_info->digest[2] >>  8) & 0xff);
    digest[11] = (unsigned char) ((sha_info->digest[2]      ) & 0xff);
    digest[12] = (unsigned char) ((sha_info->digest[3] >> 24) & 0xff);
    digest[13] = (unsigned char) ((sha_info->digest[3] >> 16) & 0xff);
    digest[14] = (unsigned char) ((sha_info->digest[3] >>  8) & 0xff);
    digest[15] = (unsigned char) ((sha_info->digest[3]      ) & 0xff);
    digest[16] = (unsigned char) ((sha_info->digest[4] >> 24) & 0xff);
    digest[17] = (unsigned char) ((sha_info->digest[4] >> 16) & 0xff);
    digest[18] = (unsigned char) ((sha_info->digest[4] >>  8) & 0xff);
    digest[19] = (unsigned char) ((sha_info->digest[4]      ) & 0xff);
}

#ifdef SHA_FOR_C

/* compute the SHA digest of a FILE stream */

#define BLOCK_SIZE	8192

void sha_stream(unsigned char digest[20], SHA_INFO *sha_info, FILE *fin);

void sha_stream(unsigned char digest[20], SHA_INFO *sha_info, FILE *fin)
{
    int i;
    BYTE data[BLOCK_SIZE];

    sha_init(sha_info);
    while ((i = fread(data, 1, BLOCK_SIZE, fin)) > 0) {
	sha_update(sha_info, data, i);
    }
    sha_final(digest, sha_info);
}

/* print a SHA digest */

void sha_print(unsigned char digest[20]);

void sha_print(unsigned char digest[20])
{
    int i, j;

    for (j = 0; j < 5; ++j) {
	for (i = 0; i < 4; ++i) {
	    printf("%02x", *digest++);
	}
	printf("%c", (j < 4) ? ' ' : '\n');
    }
}

char *sha_version(void);

char *sha_version()
{
#if (SHA_VERSION == 1)
    static char *version = "SHA-1";
#else
    static char *version = "SHA";
#endif
    return(version);
}

#endif /* SHA_FOR_C */
