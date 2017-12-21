AUTOMAKE_OPTIONS = foreign

lib_LTLIBRARIES = librecord.la
librecord_la_SOURCES = record.cpp  record_search_debug.c record_search.c snapshot.cpp
#librecord_la_LDFLAGS = -L./mp4muxer-custom -L../output/lib -lmp4muxer -lmedia
librecord_la_CPPFLAGS =  -I./mp4muxer-ffmpeg/ -I./storage -I$(includedir) -Wall -Werror
include_HEADERS = record_list.h record.h record_search.h snapshot.h
SUBDIRS = mp4muxer-ffmpeg storage
