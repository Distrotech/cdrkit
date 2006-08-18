#ident @(#)libunls_p.mk	1.2 04/06/17 
###########################################################################
SRCROOT=	..
RULESDIR=	RULES
include		$(SRCROOT)/$(RULESDIR)/rules.top
###########################################################################

SUBARCHDIR=	/profiled
INSDIR=		lib
TARGETLIB=	unls_p
#CPPOPTS +=	-Istdio
COPTS +=	$(COPTGPROF)
include		Targets
LIBS=		

###########################################################################
include		$(SRCROOT)/$(RULESDIR)/rules.lib
###########################################################################
