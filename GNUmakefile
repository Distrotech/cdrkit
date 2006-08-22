GNUmakefile: Makefile
	$(MAKE) -f Makefile $(MAKE_FLAGS) all

%: Makefile
	$(MAKE) -f Makefile $(MAKE_FLAGS) $@

Makefile:
	cmake .
