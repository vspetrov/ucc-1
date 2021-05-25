#
# Copyright (C) Mellanox Technologies Ltd. 2020-2021.  ALL RIGHTS RESERVED.
#

sources =           \
	mc_cpu.h        \
	mc_cpu_reduce.h \
	mc_cpu.c        \
	mc_cpu_max.c   \
	mc_cpu_min.c   \
	mc_cpu_sum.c \
	mc_cpu_prod.c\
	mc_cpu_land.c \
	mc_cpu_lor.c \
	mc_cpu_lxor.c \
	mc_cpu_bor.c\
	mc_cpu_band.c \
	mc_cpu_bxor.c

module_LTLIBRARIES        = libucc_mc_cpu.la
libucc_mc_cpu_la_SOURCES  = $(sources)
libucc_mc_cpu_la_CPPFLAGS = $(AM_CPPFLAGS) $(BASE_CPPFLAGS)
libucc_mc_cpu_la_CFLAGS   = $(BASE_CFLAGS)
libucc_mc_cpu_la_LDFLAGS  = -version-info $(SOVERSION) --as-needed
libucc_mc_cpu_la_LIBADD   = $(UCC_TOP_BUILDDIR)/src/libucc.la

include $(top_srcdir)/config/module.am
