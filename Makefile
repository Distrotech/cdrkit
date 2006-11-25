
all: build/Makefile
ifneq ($(CFLAGS),)
	cmake build -DCMAKE_C_FLAGS="$(CFLAGS)"
endif
	$(MAKE) -C build $(MAKE_FLAGS) all

DISTNAME=cdrkit-$(shell cat VERSION)

build/Makefile:
	@-mkdir build 2>/dev/null
	cd build && cmake ..
ifneq ($(CFLAGS),)
	cmake build -DCMAKE_C_FLAGS="$(CFLAGS)"
endif

cmakepurge:
	rm -rf install_manifest.txt progress.make CMakeFiles CMakeCache.txt cmake_install.cmake 
	rm -rf */install_manifest.txt */progress.make */CMakeFiles */CMakeCache.txt */cmake_install.cmake 
	rm -rf */*/install_manifest.txt */*/progress.make */*/CMakeFiles */*/CMakeCache.txt */*/cmake_install.cmake 
	rm */Makefile */*/Makefile

clean:
	rm -rf build

ifneq ($(PREFIX),)
install: build/Makefile
	cd build && cmake .. -DCMAKE_INSTALL_PREFIX="$(PREFIX)" && $(MAKE) $(MAKE_FLAGS) install
endif

release:
#	if test "$(shell svn status | grep -v -i make)" ; then echo Uncommited files found. Run \"svn status\" to display them. ; exit 1 ; fi
	@if test -f ../$(DISTNAME).tar.gz ; then echo ../$(DISTNAME).tar.gz exists, not overwritting ; exit 1; fi
	rm -rf tmp
	mkdir tmp
	svn export . tmp/$(DISTNAME)
	rm -rf tmp/$(DISTNAME)/debian
	tar -f - -c -C tmp $(DISTNAME) | gzip -9 > ../$(DISTNAME).tar.gz
	rm -rf tmp
	test -e /etc/debian_version && ln -f ../$(DISTNAME).tar.gz ../cdrkit_$(shell cat VERSION | sed -e "s,pre,~pre,").orig.tar.gz || true

%::
	$(MAKE) build/Makefile
ifneq ($(CFLAGS),)
	cmake build -DCMAKE_C_FLAGS="$(CFLAGS)"
endif
	$(MAKE) -C build $(MAKE_FLAGS) $@
