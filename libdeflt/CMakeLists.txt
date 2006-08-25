PROJECT (LIBdeflt)
INCLUDE_DIRECTORIES(../include ../libhfs_iso . ${CMAKE_BINARY_DIR} ../cdrecord)
ADD_DEFINITIONS(-DHAVE_CONFIG_H)

SET(LIBdeflt_SRCS default.c)

ADD_LIBRARY (deflt STATIC ${LIBdeflt_SRCS})
