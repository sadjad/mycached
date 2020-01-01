AM_CPPFLAGS = $(CXX17_FLAGS) $(SSL_CFLAGS) -I$(srcdir)/../util
AM_CXXFLAGS = $(PICKY_CXXFLAGS)

check_PROGRAMS = ringbuffer.test

ringbuffer_test_SOURCES = ringbuffer-test.cc
ringbuffer_test_LDADD = ../util/libmushutil.a

TESTS = ringbuffer.test
