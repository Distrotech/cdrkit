#ident %W% %E% %Q%
###########################################################################
# Makefile for zisofs_tools in cdrtools package for debian
###########################################################################
SRCROOT=	../
RULESDIR=	RULES
include		$(SRCROOT)/$(RULESDIR)/rules.top
###########################################################################

INSDIR=		bin
TARGET=		mkzftree
CPPOPTS +=	-DUSE_LARGEFILES

CFILES = 	mkzftree.c walk.c workers.c util.c hash.c iso9660.c \
	   		compress.c uncompress.c
HFILES =    config.h iso9660.h mkzftree.h version.h
LIBS=		-lz
XMK_FILE=	Makefile.man

GNU_CC_WARNINGS = -Wall -W -Wpointer-arith -Wbad-function-cast -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -Winline -Wshadow -Wcast-align

###########################################################################
include		$(SRCROOT)/$(RULESDIR)/rules.cmd
###########################################################################
