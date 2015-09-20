# Copyright (C) 2015 Philip Cronje. All rights reserved.
AM_CXXFLAGS = -std=c++11

bin_PROGRAMS = panopticon
panopticon_SOURCES = panopticon.cpp net.cpp ssl.cpp stat-cpu.cpp
panopticon_CFLAGS = $(SSL_CFLAGS)
panopticon_LDADD = $(SSL_LIBS)

LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) $(top_srcdir)/tap-driver.sh
TEST_LOG_DRIVER = $(LOG_DRIVER)
TESTS = test-stat-cpu
check_PROGRAMS = $(TESTS)

test_stat_cpu_SOURCES = test-stat-cpu.cpp stat-cpu.cpp
