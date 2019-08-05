#ifndef WD_COMMON_H
#define WD_COMMON_H

#ifdef ZLIB_COMPAT
#include "zlib.h"
#else
#include "zlib-ng.h"
#endif
#include "zutil.h"

void ZLIB_INTERNAL *wd_alloc_state(PREFIX3(streamp) strm, uInt items, uInt size);
void ZLIB_INTERNAL wd_copy_state(void *dst, const void *src, uInt size);

#endif
