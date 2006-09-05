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

/* @(#)setuid.h	1.2 99/12/19 Copyright 1998,1999 Heiko Eissfeldt */
/* Security functions */
void initsecurity __PR((void));

void needroot __PR((int necessary));
void dontneedroot __PR((void));
void neverneedroot __PR((void));

void needgroup __PR((int necessary));
void dontneedgroup __PR((void));
void neverneedgroup __PR((void));

#if defined (HPUX)
#define HAVE_SETREUID
#define HAVE_SETREGID
int seteuid __PR((uid_t uid));
int setreuid __PR((uid_t uid1, uid_t uid2));
int setregid __PR((gid_t gid1, gid_t gid2));
#endif
