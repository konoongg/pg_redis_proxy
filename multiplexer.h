#ifndef MULTIPLEXER_H
#define MULTIPLEXER_H

#include "ev.h"

void on_accept_cb(EV_P_ struct ev_io* io_handle, int revents);

#endif