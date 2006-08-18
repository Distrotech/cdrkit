#ident @(#)shlunls.mk	1.2 05/06/13 
###########################################################################
SRCROOT=	..
RULESDIR=	RULES
CCOM=		cc
include		$(SRCROOT)/$(RULESDIR)/rules.top
###########################################################################

SUBARCHDIR=	/pic
INSDIR=		lib
TARGETLIB=	unls
#CPPOPTS +=	-Istdio
include		Targets
LIBS=		-lc

###########################################################################
include		$(SRCROOT)/$(RULESDIR)/rules.shl
###########################################################################
