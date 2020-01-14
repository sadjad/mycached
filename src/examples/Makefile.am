AM_CPPFLAGS = $(CXX17_FLAGS) $(SSL_CFLAGS) -I$(srcdir)/../util
AM_CXXFLAGS = $(PICKY_CXXFLAGS)

LAMBDA_FLAGS = -ldl -s -static -Wl,--whole-archive -lpthread -Wl,--no-whole-archive

bin_PROGRAMS = fun cat udp-reflect

fun_SOURCES = fun.cc
fun_LDADD = ../util/libmushutil.a $(SSL_LIBS)

cat_SOURCES = cat.cc
cat_LDADD = ../util/libmushutil.a $(SSL_LIBS)

udp_reflect_SOURCES = udp-reflect.cc
udp_reflect_LDADD = ../util/libmushutil.a
udp_reflect_LDFLAGS = $(LAMBDA_FLAGS)
