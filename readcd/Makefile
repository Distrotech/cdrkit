#ident @(#)readcd.mk	1.4 04/05/25 
###########################################################################
SRCROOT=	..
RULESDIR=	RULES
include		$(SRCROOT)/$(RULESDIR)/rules.top
###########################################################################

INSDIR=		bin
TARGET=		readcd
CPPOPTS +=	-I../cdrecord
CPPOPTS +=	-DCLONE_WRITE
CPPOPTS +=	-DUSE_LARGEFILES
CFILES=		readcd.c cd_misc.c io.c \
		scsi_scan.c \
		movesect.c \
		defaults.c \
		scsi_cdr.c \
		scsi_mmc.c \
		modes.c misc.c getnum.c
HFILES=		
#LIBS=		-lschily $(LIB_SOCKET)
LIBS=		-lrscg -lscg $(LIB_VOLMGT) -ldeflt -lschily $(SCSILIB) $(LIB_SOCKET)
XMK_FILE=	Makefile.man

###########################################################################
include		$(SRCROOT)/$(RULESDIR)/rules.cmd
###########################################################################
count:	$(CFILES) $(HFILES)
	count $r1
