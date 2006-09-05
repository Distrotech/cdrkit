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

