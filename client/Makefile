all : throttlectl dumphold

throttlectl : client.c client.h syscall_names.h
	gcc -Wall -Wextra client.c -o throttlectl

dumphold : dumphold.c client.h
	gcc -Wall -Wextra dumphold.c -o dumphold

# Syscall name table, taken from the x86_64 syscall numbers of the system's headers
syscall_names.h :
	( echo 'static const struct { const char *name ; unsigned long nr ; } syscall_names[] = {' ; \
	  gcc -E -dM -include asm/unistd_64.h -x c /dev/null | \
	  sed -n 's/^\#define __NR_\([A-Za-z0-9_]*\) \([0-9][0-9]*\)$$/    { "\1", \2 },/p' ; \
	  echo '} ;' ) > $@ || ( rm -f $@ ; false )
