/* @(#)semshm.h	1.3 03/08/29 Copyright 1998,1999 Heiko Eissfeldt */
#undef DEBUG_SHM
#ifdef DEBUG_SHM
extern char *start_of_shm;
extern char *end_of_shm;
#endif

#define FREE_SEM	0
#define DEF_SEM	1

#if defined (HAVE_SEMGET) && defined(USE_SEMAPHORES)
extern int sem_id;
#else
#define sem_id 42	/* nearly any other number would do it too */
void init_pipes __PR((void));
void init_parent __PR((void));
void init_child __PR((void));
#endif


#ifdef	HAVE_AREAS
/* the name of the shared memory mapping for the FIFO under BeOS. */
#define	AREA_NAME	"shmfifo"
#endif

void free_sem __PR((void));
int semrequest __PR((int semid, int semnum));
int semrelease __PR((int semid, int semnum, int amount));
int flush_buffers __PR((void));
void * request_shm_sem __PR((unsigned amount_of_sh_mem, unsigned char **pointer));

