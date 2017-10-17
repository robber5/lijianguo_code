UTOMAKE_OPTIONS = foreign


bin_PROGRAMS = muxer


muxer_SOURCES = main.cpp Mp4Encoder.cpp Mp4Encoder.h

muxer_LDADD = -L./mp4lib -lmp4
INCLUDES = -I./mp4lib/

include_HEADERS = mp4_encoder.h

SUBDIRS = mp4lib




