# Activate testing mechanism and select executables to test
TESTS = check_boyer_moore

# Only build these executables when 'make check' is called
check_PROGRAMS = check_boyer_moore

# Configure sources and dependencies for the test executables
check_boyer_moore_SOURCES = check_boyer_moore.c
check_boyer_moore_CFLAGS = --std=c99 -I../lib/libfrec
check_boyer_moore_LDFLAGS = -L../lib/libfrec
check_boyer_moore_LDADD = -ltre -lfrec @CHECK_LIBS@
