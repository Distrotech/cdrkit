#ident @(#)skel.mk	1.1 04/05/09 
###########################################################################
SRCROOT=	..
RULESDIR=	RULES
include		$(SRCROOT)/$(RULESDIR)/rules.top
###########################################################################

INSDIR=		bin
TARGET=		skel
CPPOPTS +=	-DUSE_LARGEFILES
CFILES=		skel.c \
		io.c cd_misc.c scsi_cdr.c \
		scsi_scan.c \
		modes.c misc.c getnum.c \
		defaults.c

HFILES=		
LIBS=		-lrscg -lscg $(LIB_VOLMGT) -ldeflt -lschily $(SCSILIB) $(LIB_SOCKET)
XMK_FILE=	Makefile.man

###########################################################################
include		$(SRCROOT)/$(RULESDIR)/rules.cmd
###########################################################################
count:	$(CFILES) $(HFILES)
	count $r1

