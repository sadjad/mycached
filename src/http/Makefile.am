AM_CPPFLAGS = $(CXX17_FLAGS) -I$(srcdir)/../util
AM_CXXFLAGS = $(PICKY_CXXFLAGS)

noinst_LIBRARIES = libmushhttp.a

libmushhttp_a_SOURCES = body_parser.hh \
	chunked_parser.cc chunked_parser.hh \
	http_header.cc http_header.hh \
	http_message.cc http_message.hh \
	http_message_sequence.hh \
	http_request.cc http_request.hh \
	http_request_parser.hh \
	http_response.cc http_response.hh \
	http_response_parser.cc http_response_parser.hh \
	mime_type.cc mime_type.hh \
	http_client.hh
