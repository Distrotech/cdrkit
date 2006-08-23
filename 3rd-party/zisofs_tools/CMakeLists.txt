PROJECT (ZISOFSTOOLS)
ADD_DEFINITIONS(-Wall -W -Wpointer-arith -Wbad-function-cast -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -Winline -Wshadow -Wcast-align)
ADD_EXECUTABLE (mkzftree mkzftree.c walk.c workers.c util.c hash.c iso9660.c compress.c uncompress.c)
TARGET_LINK_LIBRARIES(mkzftree z)
SET_TARGET_PROPERTIES(mkzftree PROPERTIES COMPILE_FLAGS -O2 SKIP_BUILD_RPATH TRUE)

