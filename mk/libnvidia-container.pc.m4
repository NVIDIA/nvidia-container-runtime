divert(-1)
define(m4_prefix, esyscmd(`printf $prefix'))
define(m4_exec_prefix, esyscmd(`printf $exec_prefix'))
define(m4_libdir, esyscmd(`printf $libdir'))
define(m4_includedir, esyscmd(`printf $includedir'))
define(m4_version, indir(`$VERSION'))
define(m4_privatelibs, indir(`$PRIVATE_LIBS'))
divert(0)dnl
prefix=m4_prefix
exec_prefix=m4_exec_prefix
libdir=m4_libdir
includedir=m4_includedir

Name: libnvidia-container
Description: NVIDIA container library
Version: m4_version
Libs: -L${libdir} -lnvidia-container
Libs.private: m4_privatelibs
Cflags: -I${includedir}
