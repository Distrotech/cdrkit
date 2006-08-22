
all: Makefile
	$(MAKE) -f Makefile $(MAKE_FLAGS) all

distclean: Makefile
	$(MAKE) -f Makefile $(MAKE_FLAGS) clean
	rm -rf install_manifest.txt progress.make CMakeFiles CMakeCache.txt cmake_install.cmake */CMakeFiles */CMakeCache.txt */cmake_install.cmake */progress.make

%: Makefile
	$(MAKE) -f Makefile $(MAKE_FLAGS) $@

Makefile:
	cmake .
