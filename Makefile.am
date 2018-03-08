AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS} -I m4
AM_CFLAGS = -std=gnu11 -g -O3 -flto -fuse-linker-plugin -Isrc -Isrc/common
AM_CXXFLAGS = -std=gnu++14 -g -O3 -flto -fuse-linker-plugin -Isrc -Isrc/common -Isrc/decklink-sdk

EXTRA_DIST = \
README.rst

bin_PROGRAMS = bin/sdi-capture bin/st2110-send

SDI_CAPTURE_SOURCE_FILES = \
src/common/frame.c \
src/common/capture.c \
src/common/capture_mock.c \
src/common/server.c \
src/sdi-capture/app.c \
src/sdi-capture/main.c

SDI_CAPTURE_HEADER_FILES = \
src/common/capture.h

bin_sdi_capture_SOURCES = $(SDI_CAPTURE_SOURCE_FILES) $(SDI_CAPTURE_HEADER_FILES)
bin_sdi_capture_LDADD = -lreactor -ldynamic -lpthread -ldl

ST2110_SEND_SOURCE_FILES = \
src/common/frame.c \
src/common/client.c \
src/common/packet.c \
src/common/rtp.c \
src/common/reader.c \
src/st2110-send/app.c \
src/st2110-send/main.c

ST2110_SEND_HEADER_FILES = \
src/common/frame.h \
src/common/client.h \
src/common/packet.h \
src/common/rtp.h \
src/common/reader.h \
src/st2110-send/app.h

bin_st2110_send_SOURCES = $(ST2110_SEND_SOURCE_FILES) $(ST2110_SEND_HEADER_FILES)
bin_st2110_send_LDADD = -lreactor -ldynamic -lpthread -ldl

MAINTAINERCLEANFILES = aclocal.m4 config.h.in configure Makefile.in
maintainer-clean-local:; rm -rf autotools m4

CLEANFILES = {.,src}/*.{gcno,gcda,gcov}
