AM_CPPFLAGS = $(CXX17_FLAGS) $(SSL_CFLAGS) -I$(srcdir)/../util -I$(srcdir)/../http
AM_CXXFLAGS = $(PICKY_CXXFLAGS)

bin_PROGRAMS = mycached

mycached_SOURCES = mycached.cc
mycached_LDADD = ../http/libmushhttp.a ../util/libmushutil.a $(SSL_LIBS)
