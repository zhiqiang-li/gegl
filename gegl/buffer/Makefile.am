AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir)/gegl \
	-I$(top_srcdir)/gegl \
	-I$(top_builddir)/gegl/buffer \
	-I$(top_srcdir)/gegl/buffer \
	-I$(top_builddir)/gegl/graph \
	-I$(top_srcdir)/gegl/graph \
	-I$(top_builddir)/gegl/operation \
	-I$(top_srcdir)/gegl/operation \
	-I$(top_builddir)/gegl/opencl \
	-I$(top_srcdir)/gegl/opencl \
	-I$(top_builddir)/gegl/process \
	-I$(top_srcdir)/gegl/process \
	-I$(top_builddir)/gegl/property-types \
	-I$(top_srcdir)/gegl/property-types	\
	-DG_LOG_DOMAIN=\"GEGL\"

EXTRA_DIST = gegl-algorithms-boxfilter.inc gegl-algorithms-2x2-downscale.inc gegl-algorithms-bilinear.inc

AM_CFLAGS = $(DEP_CFLAGS) $(BABL_CFLAGS)

noinst_LTLIBRARIES = libbuffer.la

libbuffer_la_SOURCES = \
    gegl-algorithms.c \
    gegl-algorithms.h \
    gegl-buffer.c		\
    gegl-buffer-access.c	\
    gegl-buffer-config.c	\
    gegl-buffer-config.h	\
    gegl-buffer-enums.c		\
    gegl-buffer-enums.h		\
    gegl-buffer-matrix2.c	\
    gegl-buffer-matrix2.h	\
    gegl-buffer-index.h		\
    gegl-buffer-iterator.c	\
    gegl-buffer-iterator2.c	\
    gegl-buffer-iterator2.h	\
    gegl-buffer-linear.c	\
    gegl-rectangle.c	\
	gegl-buffer-load.c	\
    gegl-buffer-save.c		\
    gegl-buffer-swap.c		\
    gegl-sampler.c		\
    gegl-sampler-cubic.c	\
    gegl-sampler-linear.c	\
    gegl-sampler-nearest.c	\
    gegl-sampler-nohalo.c       \
    gegl-sampler-lohalo.c       \
    gegl-tile.c			\
    gegl-tile-source.c		\
    gegl-tile-storage.c		\
    gegl-tile-backend.c		\
    gegl-tile-backend-buffer.c	\
	gegl-tile-backend-file-async.c	\
    gegl-tile-backend-ram.c	\
	gegl-tile-backend-swap.c \
    gegl-tile-handler.c		\
    gegl-tile-handler-private.h	\
    gegl-tile-handler-cache.c	\
    gegl-tile-handler-chain.c	\
    gegl-tile-handler-empty.c	\
    gegl-tile-handler-log.c	\
    gegl-tile-handler-zoom.c	\
    \
    gegl-buffer.h		\
    gegl-buffer-private.h	\
    gegl-buffer-iterator.h	\
    gegl-buffer-iterator-private.h	\
    gegl-rectangle.h	\
    gegl-buffer-types.h		\
    gegl-buffer-formats.h	\
    gegl-buffer-swap.h		\
    gegl-buffer-swap-private.h	\
    gegl-sampler.h		\
    gegl-sampler-cubic.h	\
    gegl-sampler-linear.h	\
    gegl-sampler-nearest.h	\
    gegl-sampler-nohalo.h       \
    gegl-sampler-lohalo.h       \
    gegl-tile.h			\
    gegl-tile-source.h		\
    gegl-tile-storage.h		\
    gegl-tile-backend.h		\
    gegl-tile-backend-buffer.h	\
    gegl-tile-backend-file.h	\
	gegl-tile-backend-swap.h \
    gegl-tile-backend-ram.h	\
    gegl-tile-handler.h		\
    gegl-tile-handler-chain.h	\
    gegl-tile-handler-cache.h	\
    gegl-tile-handler-empty.h	\
    gegl-tile-handler-log.h	\
    gegl-tile-handler-zoom.h

