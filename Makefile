#contrib/pg_redis_proxy/Makefile

MODULE_big = pg_redis_proxy
OBJS = \
	$(WIN32RES) \
	redis_proxy.o \
	redis_reqv_parser/redis_reqv_parser.o \
	redis_reqv_converter/redis_reqv_converter.o \
	configure_proxy/configure_proxy.o


EXTENSION = pg_redis_proxy
DATA = pg_redis_proxy--1.0.sql

override CPPFLAGS += -I$(CURDIR)/redis_reqv_parser  -I$(CURDIR)/redis_reqv_converter -I$(CURDIR)configure_proxy

ifdef USE_PGXS
	PG_CONFIG = pg_config
	PGXS := $(shell $(PG_CONFIG) --pgxs)
	include $(PGXS)
else
	subdir = contrib/pg_redis_proxy
	top_builddir = ../..
	include $(top_builddir)/src/Makefile.global
    include $(top_srcdir)/contrib/contrib-global.mk
endif
