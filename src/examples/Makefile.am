AM_CPPFLAGS = $(CXX17_FLAGS) $(SSL_CFLAGS) -I$(srcdir)/../util
AM_CXXFLAGS = $(PICKY_CXXFLAGS)

bin_PROGRAMS = fun cat

fun_SOURCES = fun.cc
fun_LDADD = ../util/libmushutil.a $(SSL_LIBS)

cat_SOURCES = cat.cc
cat_LDADD = ../util/libmushutil.a $(SSL_LIBS)
