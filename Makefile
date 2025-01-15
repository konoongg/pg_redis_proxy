#contrib/pg_redis_proxy/Makefile

MODULE_big = pg_redis_proxy

OBJS = \
	$(WIN32RES) \
	alloc.o \
	cache.o \
	command_processor.o \
	config.o \
	data_parser.o \
	hash.o \
	multiplexer.o \
	redis_proxy.o \
	resp_creater.o \
	socket_wrapper.o \
	strings.o \
	worker.o


EXTENSION = pg_redis_proxy
DATA = pg_redis_proxy--1.1.sql

SHLIB_LINK += -lev

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