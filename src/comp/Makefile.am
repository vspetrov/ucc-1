#
# Copyright (c) Huawei Technologies Co., Ltd. 2019.  ALL RIGHTS RESERVED.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

SUBDIRS = xucg

noinst_HEADERS = ucc_comp.c ucc_comp_xucg.h

component_noinst  =
component_install = libucc_comp.la ucc_comp_lib_xucg.la

ucc_comp_lib_xucg_la_SOURCES  = ucc_comp_xucg.c
ucc_comp_lib_xucg_la_CPPFLAGS = $(AM_CPPFLAGS)
ucc_comp_lib_xucg_la_LDFLAGS  = -module -avoid-version
ucc_comp_lib_xucg_la_LIBADD   = -lucg

libucc_comp_la_SOURCES  = ucc_comp.c
libucc_comp_la_CPPFLAGS = $(AM_CPPFLAGS)
libucc_comp_la_LDFLAGS  = -ldl -version-info $(SOVERSION) --no-undefined

pkglib_LTLIBRARIES = $(component_install)
