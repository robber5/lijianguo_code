AUTOMAKE_OPTIONS = foreign


lib_LTLIBRARIES = libmp4muxer.la

libmp4muxer_la_SOURCES = Mp4Encoder.cpp Mp4Encoder.h

#libmp4muxer_la_LDFLAGS = -L./mp4lib -lmp4
#libmp4muxer_la_LIBADD =-lmp4
libmp4muxer_la_CFLAGS = -L./mp4lib -lmp4 -Wl,-rpath $(libdir)
libmp4muxer_la_CPPFLAGS = -I./mp4lib/

#include_HEADERS = Mp4Encoder.h

SUBDIRS = mp4lib




