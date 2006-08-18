/* @(#)sndconfig.h	1.2 99/12/19 Copyright 1998,1999 Heiko Eissfeldt */
#define NONBLOCKING_AUDIO
int set_snd_device __PR((const char *devicename));
int init_soundcard __PR((double rate, int bits));

int open_snd_device __PR((void));
int close_snd_device __PR((void));
int write_snd_device __PR((char *buffer, unsigned length));
