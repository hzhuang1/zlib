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
void ZLIB_INTERNAL *wd_alloc_window(PREFIX3(streamp) strm, uInt items, uInt size);
void ZLIB_INTERNAL wd_free_window(PREFIX3(streamp) strm, void *w);

#define ZALLOC_STATE wd_alloc_state

#define ZFREE_STATE ZFREE

#define ZCOPY_STATE wd_copy_state

#define ZALLOC_WINDOW wd_alloc_window

#define ZFREE_WINDOW wd_free_window

#define TRY_FREE_WINDOW wd_free_window

#endif
