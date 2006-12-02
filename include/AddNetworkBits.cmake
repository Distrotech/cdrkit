
# various checks add additional of extra libs, most likely for SunOS
# using own checks and not CHECK_SYMBOLE because of spurios problems seen with
# it

INCLUDE(CheckCSourceCompiles)

SET(TESTSRC "
#include <sys/types.h>
#include <sys/socket.h>

int main(int argc, char **argv) {
return socket(AF_INET, SOCK_STREAM, 0);
}
")

SET(CMAKE_REQUIRED_LIBRARIES )
CHECK_C_SOURCE_COMPILES("${TESTSRC}" USE_LIBC_SOCKET)

IF(NOT USE_LIBC_SOCKET)
   LIST(APPEND EXTRA_LIBS socket)
   #MESSAGE("Using libsocket for socket functions")
   # enough testing. If it breaks, then it breaks, end of story
ENDIF(NOT USE_LIBC_SOCKET)

SET(TESTSRC "
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int main(int argc, char **argv) {
struct hostent *h = gethostbyname(argv[0]);
return sizeof(h);
}
")

CHECK_C_SOURCE_COMPILES("${TESTSRC}" USE_LIBC_NLS)
IF(NOT USE_LIBC_NLS)
   SET(CMAKE_REQUIRED_LIBRARIES nls)
   CHECK_C_SOURCE_COMPILES("${TESTSRC}" USE_LIBNLS)
   IF(USE_LIBNLS)
      LIST(APPEND EXTRA_LIBS nls)
   ELSE(USE_LIBNLS)
      #      SET(CMAKE_REQUIRED_LIBRARIES xnet) # no more testing, enough
      LIST(APPEND EXTRA_LIBS xnet)
   ENDIF(USE_LIBNLS)
ENDIF(NOT USE_LIBC_NLS)
SET(CMAKE_REQUIRED_LIBRARIES )

