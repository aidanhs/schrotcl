.PHONY: default

default:
	(\
		. tcl8.6/unix/dist/lib/tclConfig.sh && \
		gcc \
			-fPIC -shared schrotcl.c -o schro.so \
			-I$$TCL_SRC_DIR/generic -I$$TCL_SRC_DIR/unix \
			$$TCL_LIB_SPEC \
	)
	echo "load schro.so; puts [schro]; schro; puts {}" | tclsh
	echo && tclsh test.tcl
