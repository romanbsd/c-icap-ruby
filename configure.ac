dnl Process this file with autoconf to produce a configure script.


AC_INIT(c_icap_ruby, 0.1)
AC_CONFIG_MACRO_DIR([m4])

dnl AM_MAINTAINER_MODE
AM_CONFIG_HEADER(autoconf.h)

AM_INIT_AUTOMAKE([subdir-objects])

AC_PROG_CC
AC_PROG_CXX
AM_PROG_CC_C_O

AC_DISABLE_STATIC
AC_LIBTOOL_DLOPEN
AC_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)

CFLAGS="$CFLAGS -Wall"

AC_CHECK_HEADERS(locale.h)
AC_CHECK_FUNCS(pthread_get_stackaddr_np)
saved_CFLAGS=$CFLAGS
CFLAGS="$CFLAGS -pthread"
AC_CHECK_FUNCS(pthread_attr_getstack pthread_getattr_np)
CFLAGS=$saved_CFLAGS

#check for c-icap
AC_ARG_WITH(c-icap, [AS_HELP_STRING([--with-c-icap], [Where to find c-icap])],
[
    cicapdir="$withval"
    if test -f $withval/bin/c-icap-config; then
       cicapflags=`$withval/bin/c-icap-config --cflags`
       cicaplibs=`$withval/bin/c-icap-config --libs`
    else
       cicapflags=`c-icap-config --cflags 2>/dev/null`
       cicaplibs=`c-icap-config --libs 2>/dev/null` 
    fi
],
)

if test -z "$cicapdir"; then
    cicapflags=`c-icap-config --cflags 2>/dev/null`
    cicaplibs=`c-icap-config --libs 2>/dev/null` 
fi

if test -z "$cicapflags"; then
	AC_MSG_FAILURE([Cannot find c-icap-config])
fi

CFLAGS="$CFLAGS $cicapflags"
MODULES_LIBADD="$cicaplibs"
AC_SUBST(MODULES_LIBADD)

AC_ARG_WITH(ruby, [AS_HELP_STRING([--with-ruby], [Path to ruby 1.9 binary])],
[
case "$withval" in
  yes)
       rubybin="ruby1.9"
  ;;                        
  no )
       rubybin=""
  ;;
  * )
      rubybin=$withval
  ;;
  esac
],
[
   rubybin=""
]
)

if test a"$rubybin" != a; then 
       rubylib=`$rubybin -rrbconfig -e 'print "#{RbConfig::CONFIG["LIBRUBYARG_STATIC"]} #{RbConfig::CONFIG["LIBS"]}"'`
       rubyccflags=`$rubybin -rrbconfig -e 'print "-I#{RbConfig::CONFIG["rubyhdrdir"]} -I#{RbConfig::CONFIG["rubyhdrdir"]}/#{RbConfig::CONFIG["arch"]}"'`
       rubyldflags=`$rubybin -rrbconfig -e 'print "#{RbConfig::CONFIG["LDFLAGS"]}"'`
       save_CPPFLAGS=$CPPFLAGS
       CPPFLAGS="$CPPFLAGS $rubyccflags"
       AC_CHECK_HEADER(ruby.h,
       [
       AC_CACHE_CHECK(for ruby_bind_stack, ac_cv_have_ruby_bind_stack,
         [
	 saved_CFLAGS=$CFLAGS
	 CFLAGS="$CFLAGS -Werror-implicit-function-declaration"
       	 AC_TRY_COMPILE([#include <ruby.h>], [ruby_bind_stack(0,0)],
	   ac_cv_have_ruby_bind_stack="yes",
	   ac_cv_have_ruby_bind_stack="no")
           ])
         CFLAGS=$saved_CFLAGS
       if test $ac_cv_have_ruby_bind_stack = "no" ; then
           AC_MSG_FAILURE([Needs a ruby_bind_stack patch from http://redmine.ruby-lang.org/issues/show/2294])
       fi
       ],
       [
       AC_MSG_WARN([Unable to find header ruby.h. The Ruby module will not be built])
       rubylib=
       ])
       CPPFLAGS=$save_CPPFLAGS
fi

AC_SUBST(rubylib)
AC_SUBST(rubyccflags)
AC_SUBST(rubyldflags)

# Now determine which modules will going to build .....
AM_CONDITIONAL(USERUBY, [test a"$rubylib" != a])

AC_OUTPUT([Makefile modules/Makefile])
