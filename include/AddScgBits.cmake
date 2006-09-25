
LIST(APPEND EXTRA_LIBS "scg")

INCLUDE(CheckIncludeFiles)
CHECK_INCLUDE_FILES("stdio.h;camlib.h" HAVE_CAMLIB_H)

if(HAVE_CAMLIB_H)

   # quick an dirty, should better become a variable used by libscg only,
   # analogous to SCG_SELF_LIBS
   ADD_DEFINITIONS(-DHAVE_CAMLIB_H)

   LIST(APPEND EXTRA_LIBS "cam")
   LIST(APPEND SCG_SELF_LIBS "cam")

endif(HAVE_CAMLIB_H)

