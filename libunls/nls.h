/* @(#)nls.h	1.7 05/05/01 2000 J. Schilling */
/*
 *	Modifications to make the code portable Copyright (c) 2000 J. Schilling
 *	Thanks to Georgy Salnikov <sge@nmr.nioch.nsc.ru>
 *
 *	Code taken from the Linux kernel.
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

#ifndef	_NLS_H
#define	_NLS_H

#include <unls.h>

#ifndef	NULL
#define	NULL ((void *)0)
#endif

#define	MOD_INC_USE_COUNT
#define	MOD_DEC_USE_COUNT

#define	CONFIG_NLS_CODEPAGE_437
#define	CONFIG_NLS_CODEPAGE_737
#define	CONFIG_NLS_CODEPAGE_775
#define	CONFIG_NLS_CODEPAGE_850
#define	CONFIG_NLS_CODEPAGE_852
#define	CONFIG_NLS_CODEPAGE_855
#define	CONFIG_NLS_CODEPAGE_857
#define	CONFIG_NLS_CODEPAGE_860
#define	CONFIG_NLS_CODEPAGE_861
#define	CONFIG_NLS_CODEPAGE_862
#define	CONFIG_NLS_CODEPAGE_863
#define	CONFIG_NLS_CODEPAGE_864
#define	CONFIG_NLS_CODEPAGE_865
#define	CONFIG_NLS_CODEPAGE_866
#define	CONFIG_NLS_CODEPAGE_869
#define	CONFIG_NLS_CODEPAGE_874
#define	CONFIG_NLS_CODEPAGE_1250
#define	CONFIG_NLS_CODEPAGE_1251
#define	CONFIG_NLS_ISO8859_1
#define	CONFIG_NLS_ISO8859_2
#define	CONFIG_NLS_ISO8859_3
#define	CONFIG_NLS_ISO8859_4
#define	CONFIG_NLS_ISO8859_5
#define	CONFIG_NLS_ISO8859_6
#define	CONFIG_NLS_ISO8859_7
#define	CONFIG_NLS_ISO8859_8
#define	CONFIG_NLS_ISO8859_9
#define	CONFIG_NLS_ISO8859_14
#define	CONFIG_NLS_ISO8859_15
#define	CONFIG_NLS_KOI8_R
#define	CONFIG_NLS_KOI8_U

#define	CONFIG_NLS_CODEPAGE_10000
#define	CONFIG_NLS_CODEPAGE_10006
#define	CONFIG_NLS_CODEPAGE_10007
#define	CONFIG_NLS_CODEPAGE_10029
#define	CONFIG_NLS_CODEPAGE_10079
#define	CONFIG_NLS_CODEPAGE_10081

extern int init_unls_iso8859_1	__PR((void));
extern int init_unls_iso8859_2	__PR((void));
extern int init_unls_iso8859_3	__PR((void));
extern int init_unls_iso8859_4	__PR((void));
extern int init_unls_iso8859_5	__PR((void));
extern int init_unls_iso8859_6	__PR((void));
extern int init_unls_iso8859_7	__PR((void));
extern int init_unls_iso8859_8	__PR((void));
extern int init_unls_iso8859_9	__PR((void));
extern int init_unls_iso8859_14	__PR((void));
extern int init_unls_iso8859_15	__PR((void));
extern int init_unls_cp437	__PR((void));
extern int init_unls_cp737	__PR((void));
extern int init_unls_cp775	__PR((void));
extern int init_unls_cp850	__PR((void));
extern int init_unls_cp852	__PR((void));
extern int init_unls_cp855	__PR((void));
extern int init_unls_cp857	__PR((void));
extern int init_unls_cp860	__PR((void));
extern int init_unls_cp861	__PR((void));
extern int init_unls_cp862	__PR((void));
extern int init_unls_cp863	__PR((void));
extern int init_unls_cp864	__PR((void));
extern int init_unls_cp865	__PR((void));
extern int init_unls_cp866	__PR((void));
extern int init_unls_cp869	__PR((void));
extern int init_unls_cp874	__PR((void));
extern int init_unls_cp1250	__PR((void));
extern int init_unls_cp1251	__PR((void));
extern int init_unls_koi8_r	__PR((void));
extern int init_unls_koi8_u	__PR((void));

extern int init_unls_cp10000	__PR((void));
extern int init_unls_cp10006	__PR((void));
extern int init_unls_cp10007	__PR((void));
extern int init_unls_cp10029	__PR((void));
extern int init_unls_cp10079	__PR((void));
extern int init_unls_cp10081	__PR((void));
extern int init_unls_file	__PR((char * name));

#endif	/* _NLS_H */
