
LIST(APPEND EXTRA_LIBS "scg")

INCLUDE(CheckIncludeFiles)
CHECK_INCLUDE_FILES("stdio.h;camlib.h" HAVE_CAMLIB_H)

IF(HAVE_CAMLIB_H)

   # quick an dirty, should better become a variable used by libscg only,
   # analogous to SCG_SELF_LIBS
   ADD_DEFINITIONS(-DHAVE_CAMLIB_H)

   LIST(APPEND EXTRA_LIBS "cam")
   LIST(APPEND SCG_SELF_LIBS "cam")

ENDIF(HAVE_CAMLIB_H)

FIND_LIBRARY(HAVE_LIBVOLMGT "volmgt")
IF(HAVE_LIBVOLMGT)
   LIST(APPEND EXTRA_LIBS "volmgt")
   LIST(APPEND SCG_SELF_LIBS "volmgt")
ENDIF(HAVE_LIBVOLMGT)

IF(CMAKE_SYSTEM_NAME MATCHES "SunOS")
   LIST(APPEND EXTRA_LIBS -lrt -lsocket)
      # reason below, FIXME: add proper checks
      ##   CMakeFiles/cdda2wav.dir/cdda2wav.o(.text+0x19cc): In function
      ##   `switch_to_realtime_priority':
      ##   : undefined reference to `sched_get_priority_min'
      ##   CMakeFiles/cdda2wav.dir/cdda2wav.o(.text+0x19e0): In function
      ##   `switch_to_realtime_priority':
      ##   : undefined reference to `sched_get_priority_max'
      ##   CMakeFiles/cdda2wav.dir/cdda2wav.o(.text+0x1a2c): In function
      ##   `switch_to_realtime_priority':
      ##   : undefined reference to `sched_setscheduler'
      ##   CMakeFiles/cdda2wav.dir/toc.o(.text+0x3fd8): In function
      ##   `request_titles':
      ##   : undefined reference to `socket'
      ##   CMakeFiles/cdda2wav.dir/toc.o(.text+0x403c): In function
      ##   `request_titles':
      ##   : undefined reference to `gethostbyname'
      ##   CMakeFiles/cdda2wav.dir/toc.o(.text+0x405c): In function
      ##   `request_titles':
      ##   : undefined reference to `gethostbyname'
      ##   CMakeFiles/cdda2wav.dir/toc.o(.text+0x418c): In function
      ##   `request_titles':
      ##   : undefined reference to `getservbyname'
      ##   CMakeFiles/cdda2wav.dir/toc.o(.text+0x41d4): In function
      ##   `request_titles':
      ##   : undefined reference to `getservbyname'
      ##   CMakeFiles/cdda2wav.dir/toc.o(.text+0x427c): In function
      ##   `request_titles':
      ##   : undefined reference to `connect'
      ENDIF(CMAKE_SYSTEM_NAME MATCHES "SunOS")

