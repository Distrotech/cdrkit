#include <sys/types.h>
#include <sys/mman.h>
#include <errno.h>
#ifndef HAVE_ERRNO_DEF
extern  int     errno;
#endif

   int
main()
{
   if (mlockall(MCL_CURRENT|MCL_FUTURE) < 0) {
      if (errno == EINVAL || errno ==  ENOMEM ||
            errno == EPERM  || errno ==  EACCES)
         return(0);
      return(-1);
   }
   return(0);
}

