PROJECT (INCLUDES)

# auto-generated header with sizeof magic
INCLUDE(CheckTypeSize)
CHECK_TYPE_SIZE(short SIZE_SHORT)
MATH(EXPR SHORTLESS ${SIZE_SHORT}-1)
CHECK_TYPE_SIZE(int SIZE_INT)
MATH(EXPR INTLESS ${SIZE_INT}-1)
CHECK_TYPE_SIZE(long SIZE_LONG)
MATH(EXPR LONGLESS ${SIZE_LONG}-1)
CHECK_TYPE_SIZE("long long" SIZE_LLONG)
MATH(EXPR LLONGLESS ${SIZE_LLONG}-1)
CHECK_TYPE_SIZE(float SIZE_FLOAT)
MATH(EXPR FLOATLESS ${SIZE_FLOAT}-1)
CHECK_TYPE_SIZE(double SIZE_DOUBLE)
MATH(EXPR DOUBLELESS ${SIZE_DOUBLE}-1)
CHECK_TYPE_SIZE("char*" SIZE_PTR)
MATH(EXPR PTRLESS ${SIZE_PTR}-1)
CONFIGURE_FILE( align.h.in align.h)
#SET_SOURCE_FILES_PROPERTIES(align.h PROPERTIES GENERATED 1)


TRY_COMPILE(VA_LIST_IS_ARRAY ${CMAKE_BINARY_DIR}  ${CMAKE_CURRENT_SOURCE_DIR}/test_VA_LIST_IS_ARRAY.c)
# invert the result, has the condition IFF compiler fails. There must be a simple way to say a=!b... but how?
IF(VA_LIST_IS_ARRAY) 
   SET(VA_LIST_IS_ARRAY FALSE)
else(VA_LIST_IS_ARRAY) 
   SET(VA_LIST_IS_ARRAY TRUE)
endif(VA_LIST_IS_ARRAY)

TRY_COMPILE(HAVE_MLOCKALL ${CMAKE_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/test_HAVE_MLOCKALL.c)

INCLUDE(TestBigEndian)
TEST_BIG_ENDIAN(WORDS_BIGENDIAN)

TRY_RUN(BITFIELDS_HTOL TEST_DUMMY ${CMAKE_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/test_BITFIELDS_HTOL.c)

FIND_FILE(HAVE_WAIT_H "wait.h")
FIND_FILE(HAVE_SYS_WAIT_H "sys/wait.h")
TRY_COMPILE(HAVE_UNION_WAIT ${CMAKE_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/test_UNION_WAIT.c )
# disabled, though the alternative works on cygwin and both headers are present
# there.
#IF(NOT HAVE_UNION_WAIT)
#	TRY_COMPILE(HAVE_UNION_WAIT ${CMAKE_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/test_SYS_UNION_WAIT.c )
#MESSAGE("syswait: ${HAVE_UNION_WAIT}")
#ENDIF(NOT HAVE_UNION_WAIT)

FIND_FILE(HAVE_SYS_DKIO_H "sys/dkio.h")
FIND_FILE(HAVE_SUN_DKIO_H "sun/dkio.h")
FIND_FILE(HAVE_STDARG_H "stdarg.h")
FIND_FILE(HAVE_STDLIB_H "stdlib.h")
FIND_FILE(HAVE_STRING_H "string.h")
FIND_FILE(HAVE_STRINGS_H "strings.h")
FIND_FILE(HAVE_UNISTD_H "unistd.h")
FIND_FILE(HAVE_GETOPT_H "getopt.h")
FIND_FILE(HAVE_LIMITS_H "limits.h")
FIND_FILE(HAVE_FCNTL_H "fcntl.h")
FIND_FILE(HAVE_SYS_FILE_H "sys/file.h")
FIND_FILE(HAVE_INTTYPES_H "inttypes.h")
FIND_FILE(HAVE_DIRENT_H "dirent.h")
FIND_FILE(HAVE_ALLOCA_H "alloca.h")
FIND_FILE(HAVE_TERMIOS_H "termios.h")
FIND_FILE(HAVE_TERMIO_H "termio.h")
FIND_FILE(HAVE_PWD_H "pwd.h")
FIND_FILE(HAVE_SYS_TIME_H "sys/time.h")
FIND_FILE(HAVE_UTIME_H "utime.h")
FIND_FILE(HAVE_SYS_IOCTL_H "sys/ioctl.h")
FIND_FILE(HAVE_SYS_PARAM_H "sys/param.h")
FIND_FILE(HAVE_WAIT_H "wait.h")
FIND_FILE(HAVE_SYS_WAIT_H "sys/wait.h")
FIND_FILE(HAVE_SYS_RESOURCE_H "sys/resource.h")
FIND_FILE(HAVE_SYS_UTSNAME_H "sys/utsname.h")
FIND_FILE(HAVE_SYS_MTIO_H "sys/mtio.h")
FIND_FILE(HAVE_SYS_MMAN_H "sys/mman.h")
FIND_FILE(HAVE_POLL_H "poll.h")
FIND_FILE(HAVE_SYS_POLL_H "sys/poll.h")
FIND_FILE(HAVE_NETDB_H "netdb.h")
FIND_FILE(HAVE_SYS_SOCKET_H "sys/socket.h")
FIND_FILE(HAVE_LINUX_PG_H "linux.pg.h")
FIND_FILE(HAVE_ARPA_INET_H "arpa.inet.h")
FIND_FILE(HAVE_ELF_H "elf.h")
FIND_FILE(HAVE_AOUTHDR_H "aouthdr.h")
FIND_FILE(HAVE_A_OUT_H "a.out.h")
FIND_FILE(HAVE_SYS_IPC_H "sys/ipc.h")

INCLUDE(CheckCSourceCompiles)


SET(CMAKE_REQUIRED_LIBRARIES )
SET(TESTSRC " 
#include <stdlib.h>
int main() { return ( (int) drand48()  )  ; }
")
CHECK_C_SOURCE_COMPILES("${TESTSRC}" HAVE_DRAND48)
SET(TESTSRC " 
#include <stdlib.h>
int main() { return ( (int) drand()  )  ; }
")
CHECK_C_SOURCE_COMPILES("${TESTSRC}" HAVE_DRAND)
SET(TESTSRC " 
#include <sys/socket.h>
#include <netdb.h>
int main(int argc, char **argv) {
struct sockaddr sa;
return getnameinfo(&sa, 1, argv[0], 1, argv[1], 1, 0);
}
")
CHECK_C_SOURCE_COMPILES("${TESTSRC}" HAVE_GETNAMEINFO)
SET(TESTSRC " 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
int main(int a, char **b) {
struct in_addr in;
inet_ntoa(in);
return 0;
}
")
CHECK_C_SOURCE_COMPILES("${TESTSRC}" HAVE_INET_NTOA)

CONFIGURE_FILE( xconfig.h.in xconfig.h)

