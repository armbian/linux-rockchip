// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef CTS_STRERROR_H
#define CTS_STRERROR_H

#define CTS_ERR_FMT_STR         "%d(%s)"
#define CTS_ERR_ARG(errno)      cts_strerror(errno)
char *cts_strerror(int errno);

#endif

