
DISTNAME=talibus-$(shell cat VERSION)

all: Makefile
	$(MAKE) -f Makefile $(MAKE_FLAGS) all

distclean: Makefile
	$(MAKE) -f Makefile $(MAKE_FLAGS) clean
	rm -rf install_manifest.txt progress.make CMakeFiles CMakeCache.txt cmake_install.cmake */CMakeFiles */CMakeCache.txt */cmake_install.cmake */progress.make

%: Makefile
	$(MAKE) -f Makefile $(MAKE_FLAGS) $@

release: distclean
#	if test "$(shell svn status | grep -v -i make)" ; then echo Uncommited files found. Run \"svn status\" to display them. ; exit 1 ; fi
	@if test -f ../$(DISTNAME).tgz ; then echo ../$(DISTNAME).tgz exists, not overwritting ; exit 1; fi
	mkdir tmp && svn export . tmp/$(DISTNAME) && cd tmp && tar -f - -c $(DISTNAME) | gzip -9 > ../../$(DISTNAME).tgz && cd .. && rm -rf tmp

Makefile:
	cmake .
