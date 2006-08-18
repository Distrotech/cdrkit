/* @(#)gap.h	1.10 04/02/18 J. Schilling from cdparanoia-III-alpha9.8 */
/*
 *	Modifications to make the code portable Copyright (c) 2002 J. Schilling
 */
/*
 * CopyPolicy: GNU Public License 2 applies
 * Copyright (C) by Monty (xiphmont@mit.edu)
 */

#ifndef	_GAP_H_
#define	_GAP_H_

extern long	i_paranoia_overlap_r	__PR((Int16_t * buffA, Int16_t * buffB,
						long offsetA, long offsetB));
extern long	i_paranoia_overlap_f	__PR((Int16_t * buffA, Int16_t * buffB,
						long offsetA, long offsetB,
						long sizeA, long sizeB));
extern int	i_stutter_or_gap	__PR((Int16_t * A, Int16_t * B,
						long offA, long offB,
						long gap));
extern void	i_analyze_rift_f	__PR((Int16_t * A, Int16_t * B,
						long sizeA, long sizeB,
						long aoffset, long boffset,
						long *matchA, long *matchB,
						long *matchC));
extern void	i_analyze_rift_r	__PR((Int16_t * A, Int16_t * B,
						long sizeA, long sizeB,
						long aoffset, long boffset,
						long *matchA, long *matchB,
						long *matchC));

extern void	analyze_rift_silence_f	__PR((Int16_t * A, Int16_t * B,
						long sizeA, long sizeB,
						long aoffset, long boffset,
						long *matchA, long *matchB));

#endif
