.PHONY: prep build default

default: build

prep:
	git submodule init
	git submodule update
	cd tcl8.6/unix && ./configure --prefix=$$(pwd)/dist
	cd tcl8.6/unix && make
	cd tcl8.6/unix && make install

build:
	(\
		. tcl8.6/unix/dist/lib/tclConfig.sh && \
		gcc \
			-fPIC -shared schrotcl.c -o schro.so \
			-I$$TCL_SRC_DIR/generic -I$$TCL_SRC_DIR/unix \
			$$TCL_LIB_SPEC \
	)
	echo "load schro.so; puts [schro]; schro; puts {}" | tclsh
	echo && tclsh test.tcl
