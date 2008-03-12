# The build logic isn't really in this makefile, it's in bash scripts in
# the make directory.  This just provides a familiar user interface.

all native i386 arm c67 win32:
	make/make.sh $@

install clean test:
	make/$@.sh

.PHONY: all i386 arm c67 win32 install clean test
