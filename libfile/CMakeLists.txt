PROJECT (LIBfile)
INCLUDE_DIRECTORIES(../include ../libhfs_iso . ../cdrecord)
ADD_DEFINITIONS(-DHAVE_CONFIG_H)
SET(LIBfile_SRCS apprentice.c file.c softmagic.c )

ADD_LIBRARY (file STATIC ${LIBfile_SRCS})
