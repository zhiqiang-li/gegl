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

AM_CFLAGS = $(DEP_CFLAGS) $(BABL_CFLAGS)

noinst_LTLIBRARIES = libgraph.la

#libgraph_publicdir = $(includedir)/gegl-$(GEGL_API_VERSION)/graph
#libgraph_public_HEADERS = #

libgraph_la_SOURCES = \
	gegl-cache.c			\
	gegl-connection.c		\
	gegl-node.c			\
	gegl-pad.c			\
	gegl-visitor.c			\
	gegl-callback-visitor.c		\
	gegl-visitable.c		\
	gegl-node-output-visitable.c	\
	gegl-region-generic.c		\
	\
	gegl-cache.h			\
	gegl-region.h			\
	gegl-region-generic.h		\
	gegl-connection.h		\
	gegl-node.h			\
	gegl-node-private.h		\
	gegl-pad.h			\
	gegl-visitor.h			\
	gegl-callback-visitor.h		\
	gegl-visitable.h		\
	gegl-node-output-visitable.h

#libgraph_la_SOURCES = $(libgraph_sources) $(libgraph_public_HEADERS)

