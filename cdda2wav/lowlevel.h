/* @(#)lowlevel.h	1.2 99/12/19 Copyright 1998,1999 Heiko Eissfeldt */
/* os dependent functions */

#ifndef LOWLEVEL
# define LOWLEVEL 1

# if defined(__linux__)
#  include <linux/version.h>
#  include <linux/major.h>

# endif /* defined __linux__ */

#endif /* ifndef LOWLEVEL */
