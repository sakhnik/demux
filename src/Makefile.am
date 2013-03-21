bin_PROGRAMS = feed sink

noinst_LIBRARIES = libcommon.a

libcommon_a_SOURCES = tcp.h tcp.c protocol.h protocol.c \
		      message.h message.c

feed_SOURCES = feed.c
feed_CFLAGS = $(LIBAVFORMAT_CFLAGS) $(LIBAVCODEC_CFLAGS) $(LIBAVUTIL_CFLAGS)
feed_LDADD = libcommon.a \
	     $(LIBAVFORMAT_LIBS) $(LIBAVCODEC_LIBS) $(LIBAVUTIL_LIBS)

sink_SOURCES = sink.c writer.h writer.c
sink_CFLAGS = $(PTHREAD_CFLAGS)
sink_LDADD = libcommon.a $(PTHREAD_LIBS)
