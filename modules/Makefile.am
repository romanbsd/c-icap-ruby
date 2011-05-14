pkglibdir=$(libdir)/c_icap

pkglib_LTLIBRARIES=

if USERUBY
pkglib_LTLIBRARIES += ruby_handler.la
endif

ruby_handler_la_LIBADD = @MODULES_LIBADD@ @rubylib@
ruby_handler_la_CFLAGS = -I../include/ @rubyccflags@
ruby_handler_la_LDFLAGS = -module -avoid-version @rubyldflags@
ruby_handler_la_SOURCES = ruby_handler.c
