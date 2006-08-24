
all: build/Makefile
	$(MAKE) -C build $(MAKE_FLAGS) all

DISTNAME=botox-$(shell cat VERSION)

build/Makefile:
	-mkdir build 2>/dev/null
	cd build && cmake ..

cmakepurge:
	rm -rf install_manifest.txt progress.make CMakeFiles CMakeCache.txt cmake_install.cmake */CMakeFiles */CMakeCache.txt */cmake_install.cmake */progress.make

clean:
	rm -rf build include/align.h

%: build/Makefile
	$(MAKE) -C build $(MAKE_FLAGS) $@

release: distclean
#	if test "$(shell svn status | grep -v -i make)" ; then echo Uncommited files found. Run \"svn status\" to display them. ; exit 1 ; fi
	@if test -f ../$(DISTNAME).tgz ; then echo ../$(DISTNAME).tgz exists, not overwritting ; exit 1; fi
	mkdir tmp && svn export . tmp/$(DISTNAME) && cd tmp && tar -f - -c $(DISTNAME) | gzip -9 > ../../$(DISTNAME).tgz && cd .. && rm -rf tmp

