plugindir = $(PLUGINDIR)
plugindatadir = $(PLUGIN_DATA_DIR)
plugin_LTLIBRARIES = libfilesystem.la

libfilesystem_la_SOURCES = \
	ario-filesystem.c \
	ario-filesystem.h \
	ario-filesystem-plugin.c \
        ario-filesystem-plugin.h
	
libfilesystem_la_LDFLAGS = $(PLUGIN_LIBTOOL_FLAGS)

uixmldir = $(plugindatadir)
uixml_DATA = filesystem-ui.xml

INCLUDES = 						\
	-DLOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	$(DEPS_CFLAGS)   				\
	-I$(top_srcdir) 				\
	-I$(top_srcdir)/src      			\
	-I$(top_srcdir)/src/lib                        	\
	-I$(top_srcdir)/src/plugins                   	\
	-I$(top_srcdir)/src/sources                   	\
	-I$(top_srcdir)/src/shell                       \
	$(WARNINGS)

plugin_in_files = filesystem.ario-plugin.desktop.in

%.ario-plugin: %.ario-plugin.desktop.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

plugin_DATA = $(plugin_in_files:.ario-plugin.desktop.in=.ario-plugin)

EXTRA_DIST = $(plugin_in_files)\
             $(uixml_DATA)

CLEANFILES = $(plugin_DATA)
DISTCLEANFILES = $(plugin_DATA)
