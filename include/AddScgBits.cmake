
LIST(APPEND SCG_LIBS "scg")
INCLUDE(CheckIncludeFile)
CHECK_INCLUDE_FILE("camlib.h" HAVE_CAMLIB_H)
if(HAVE_CAMLIB_H)

   # quick an dirty, should better be a var used by libscg only, analogous to
   # SCG_SELF_LIBS
   ADD_DEFINITIONS(-DHAVE_CAMLIB_H)

   LIST(APPEND SCG_LIBS "cam")
   LIST(APPEND SCG_SELF_LIBS "cam")
endif(HAVE_CAMLIB_H)

