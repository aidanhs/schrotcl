.PHONY: prep build default

default: build

prep:
	git submodule init
	git submodule update
	cd tcl8.6/unix && ./configure --prefix=$$(pwd)/dist
	cd tcl8.6/unix && make
	cd tcl8.6/unix && make install

build:
	test -f tcl8.6/unix/dist/lib/tclConfig.sh
	. tcl8.6/unix/dist/lib/tclConfig.sh && \
	gcc \
		-fPIC -shared schrotcl.c -o schro.so \
		-I$$TCL_SRC_DIR/generic -I$$TCL_SRC_DIR/unix \
		$$TCL_LIB_SPEC \
