
# various checks add additional of extra libs, most likely for SunOS

INCLUDE(CheckCSourceCompiles)

SET(TESTSRC "
#include <sys/types.h>
#include <sys/socket.h>

int main(int argc, char **argv) {
return socket(AF_INET, SOCK_STREAM, 0);
}
")

SET(CMAKE_REQUIRED_LIBRARIES )
CHECK_C_SOURCE_COMPILES("${TESTSRC}" LIBC_SOCKET)

IF(NOT LIBC_SOCKET)
   LIST(APPEND EXTRA_LIBS socket)
   #MESSAGE("Using libsocket for socket functions")
ENDIF(NOT LIBC_SOCKET)

SET(TESTSRC "
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int main(int argc, char **argv) {
struct hostent *h = gethostbyname(argv[0]);
return sizeof(h);
}
")

CHECK_C_SOURCE_COMPILES("${TESTSRC}" LIBC_NLS)

IF(NOT LIBC_NLS)
   LIST(APPEND EXTRA_LIBS nls)
   #MESSAGE("Using libsocket for socket functions")
ENDIF(NOT LIBC_NLS)

