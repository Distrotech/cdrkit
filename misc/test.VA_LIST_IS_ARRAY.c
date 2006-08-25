#include "confdefs.h"

#ifdef  HAVE_STDARG_H
#       include <stdarg.h>
#else
#       include <varargs.h>
#endif

int main() {

va_list a, b;

a = b;
; return 0; }

