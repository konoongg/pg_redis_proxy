#contrib/pg_redis_proxy/Makefile


MODULE_big = pg_redis_proxy
OBJS = \
	$(WIN32RES) \
	redis_proxy.o \
	work_with_socket/work_with_socket.o \
	redis_reqv_converter/redis_reqv_converter.o \
	configure_proxy/configure_proxy.o \
	work_with_db/work_with_db.o \
	postgres_reqv_converter/postgres_reqv_converter.o \
	proxy_hash/proxy_hash.o \
	send_req_postgres/send_req_postgres.o \
	logger/logger.o


EXTENSION = pg_redis_proxy
DATA = pg_redis_proxy--1.0.sql

SHLIB_LINK += -lpq -lev
PG_CPPFLAGS = -lpq -lev

override CPPFLAGS += -I$(CURDIR)/redis_reqv_parser  -I$(CURDIR)/redis_reqv_converter -I$(CURDIR)configure_proxy -I/usr/include/postgresql

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
