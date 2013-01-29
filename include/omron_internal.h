/*
 * Internal declarations for the libomron library.
 *
 * Note: This file is not intended to be distributed as part of the API, it is
 * only used for building the library itself.
 *
 * Copyright (c) 2009-2010 Kyle Machulis <kyle@nonpolynomial.com>
 *
 * More info on Nonpolynomial Labs @ http://www.nonpolynomial.com
 *
 * Sourceforge project @ http://www.github.com/qdot/libomron/
 *
 * This library is covered by the BSD License
 * Read LICENSE_BSD.txt for details.
 */

#ifndef _OMRON_INTERNAL_H
#define _OMRON_INTERNAL_H

#include "libomron/omron.h"
#include <stdio.h>

///////////////////////////////////////////////////////////////////////////////
//
// Debugging definitions
//
///////////////////////////////////////////////////////////////////////////////

extern int _omron_debug_level;

#define DEBUG_DEFAULT_LEVEL 0
#define DEBUG_ENV_VAR "LIBOMRON_DEBUG"

#define IF_DEBUG(level, stmt)     if (_omron_debug_level >= (level)) { stmt; }
#define DPRINTF(level, fmt, ...)  \
        IF_DEBUG(level, fprintf(stderr, "%s: " fmt, __func__, ##__VA_ARGS__))

#define MSG_ERROR(...)  DPRINTF(OMRON_DEBUG_ERROR, __VA_ARGS__)
#define MSG_WARN(...)   DPRINTF(OMRON_DEBUG_WARNING, __VA_ARGS__)
#define MSG_INFO(...)   DPRINTF(OMRON_DEBUG_INFO, __VA_ARGS__)
#define MSG_DETAIL(...) DPRINTF(OMRON_DEBUG_DETAIL, __VA_ARGS__)
#define MSG_PROTO(...)  DPRINTF(OMRON_DEBUG_PROTO, __VA_ARGS__)
#define MSG_DEVIO(...)  DPRINTF(OMRON_DEBUG_DEVIO, __VA_ARGS__)

#define MSG_HEXDUMP(level, msg, data, len) \
        IF_DEBUG(level, fprintf(stderr, "%s: %s", __func__, msg); omron_hexdump(data, len);)

///////////////////////////////////////////////////////////////////////////////
//
// Platform Specific Functions
//
///////////////////////////////////////////////////////////////////////////////

omron_device* omron_create_device(void);

///////////////////////////////////////////////////////////////////////////////
//
// Utility functions called from the platform-specific C files
//
///////////////////////////////////////////////////////////////////////////////

void omron_hexdump(const uint8_t *data, int n_bytes);

#endif // _OMRON_INTERNAL_H
