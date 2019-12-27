AM_CPPFLAGS = $(CXX17_FLAGS) $(SSL_CFLAGS) -I$(srcdir)/../util
AM_CXXFLAGS = $(PICKY_CXXFLAGS)

bin_PROGRAMS = fun

fun_SOURCES = fun.cc
fun_LDADD = ../util/libmushutil.a $(SSL_LIBS)
