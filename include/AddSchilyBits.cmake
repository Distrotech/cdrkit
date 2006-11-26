IF(NOT CHECKED_rols)
   SET(CHECKED_rols 1)

   LIST(APPEND EXTRA_LIBS "rols")

# not the proper place but ok, because it is linked from everywhere

ENDIF(NOT CHECKED_rols)

