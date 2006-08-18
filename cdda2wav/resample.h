/* @(#)resample.h	1.3 02/08/02 Copyright 1998,1999 Heiko Eissfeldt */
#define SYNC_SIZE	600	/* has to be smaller than CD_FRAMESAMPLES */

extern int waitforsignal;	/* flag: wait for any audio response */
extern int any_signal;

extern short undersampling;	/* conversion factor */
extern short samples_to_do;	/* loop variable for conversion */
extern int Halved;		/* interpolate due to non integral divider */
extern int jitterShift;		/* track accumulated jitter */
long SaveBuffer __PR((UINT4 *p,
                 unsigned long SecsToDo, unsigned long *BytesDone));
unsigned char *synchronize __PR((UINT4 *p, unsigned SamplesToDo, unsigned TotSamplesDone));
void	handle_inputendianess __PR((UINT4 *p, unsigned SamplesToDo));
