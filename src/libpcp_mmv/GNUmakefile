#
# Copyright (C) 2001,2009 Silicon Graphics, Inc.  All Rights Reserved.
#
# This library is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation; either version 2.1 of the License, or
# (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
# License for more details.

#
# libpcp_mmv.so - performance data export through shared memory
#	(used in conjunction with the mmv pmda)
#

TOPDIR = ../..

include $(TOPDIR)/src/include/builddefs

BASE	= libpcp_mmv.a

SUBDIRS = src

default install: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)

default_pcp: default

install_pcp: install

