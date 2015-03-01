.PHONY: default

default:
	(\
		. tcl8.6/unix/dist/lib/tclConfig.sh && \
		gcc \
			-fPIC -shared ndtcl.c -o ndtcl.so \
			-I$$TCL_SRC_DIR/generic -I$$TCL_SRC_DIR/unix \
			$$TCL_LIB_SPEC \
	)
	echo "load ndtcl.so; puts [nd]; nd; puts {}" | tclsh
	echo && tclsh test.tcl
