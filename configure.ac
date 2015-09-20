# Copyright (C) 2015 Philip Cronje. All rights reserved.
AC_INIT([panopticon], [1.0], [syke107@gmail.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_REQUIRE_AUX_FILE([tap-driver.sh])

AC_PROG_CXX

PKG_CHECK_MODULES([SSL], [libssl])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile])

AC_OUTPUT
