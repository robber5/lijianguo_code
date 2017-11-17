AUTOMAKE_OPTIONS = foreign

lib_LTLIBRARIES = librecord.la
librecord_la_SOURCES = record.cpp  record_search_debug.c record_search.c #snapshot.cpp
#librecord_la_LDFLAGS = -L./mp4muxer-custom -L../output/lib -lmp4muxer -lmedia
librecord_la_CFLAGS = -L./mp4muxer-ffmepg -L../output/lib -lfmp4 -lmedia -lstorage -lpthread -lstdc++ -lm -Wl,-rpath $(libdir)
librecord_la_CPPFLAGS = -I../output/include -I./mp4muxer-ffmpeg/
include_HEADERS = record_list.h record.h record_search.h
SUBDIRS = mp4muxer-ffmpeg storage
