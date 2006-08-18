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
