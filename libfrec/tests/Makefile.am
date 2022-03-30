# Activate testing mechanism and select executables to test
TESTS = check_boyer_moore check_heuristic check_interface_single

# Only build these executables when 'make check' is called
check_PROGRAMS = check_boyer_moore \
                 check_heuristic \
                 check_interface_single

# Configure sources and dependencies for the test executables
check_boyer_moore_SOURCES = check_boyer_moore.c
check_boyer_moore_CFLAGS = --std=c99 -I../include -I../lib
check_boyer_moore_LDFLAGS = -L../lib
check_boyer_moore_LDADD = -ltre -lfrec @CHECK_LIBS@

check_heuristic_SOURCES = check_heuristic.c
check_heuristic_CFLAGS = --std=c99 -I../include -I../lib
check_heuristic_LDFLAGS = -L../lib
check_heuristic_LDADD = -ltre -lfrec @CHECK_LIBS@

check_interface_single_SOURCES = check_interface_single.c
check_interface_single_CFLAGS = --std=c99 -I../include -I../lib
check_interface_single_LDFLAGS = -L../lib
check_interface_single_LDADD = -ltre -lfrec @CHECK_LIBS@
